#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <map>


using namespace std;

#include "utils.h"
#include "replacement_state.h"
#include "cache.h"
#include "trace.h"
#include "model.h"

#define N	1000

// L1 private caches: 32KB

// L3 shared cache: 4MB

#ifndef LLC_CAPACITY
#define LLC_CAPACITY	(4 * 1024 * 1024)
#endif
#define LLC_BLOCKSIZE	64
#define LLC_ASSOC	16
#define LLC_NSETS	(LLC_CAPACITY/(LLC_BLOCKSIZE*LLC_ASSOC))

#define MAX_CORES	16
#define MAX_THREADS	256

cache LLC;
FILE *mintracefp = NULL;
tracereader *readers[MAX_THREADS];
trace *traces[MAX_THREADS];
unsigned long long int 
	l3_misses[MAX_CORES], 
	l3_misses_at_warming[MAX_CORES],
	l3_accesses = 0;
int ncores, nthreads;
bool warming = true;

map<unsigned long long int, unsigned int> index_of_next_access;
int tracecount = 0;

struct mintrace {
	unsigned long long int block_address;
	unsigned int index_of_next_access;
};

long long int last_insts[MAX_THREADS];

unsigned long long int cycles[MAX_THREADS], cycles_at_warming[MAX_THREADS], insts_at_warming[MAX_THREADS];

void print_stats (void);
double getipc (const char *);
int dan_set_shift = 0, dan_warm_inst = 500000000, dan_policy = 0;
unsigned long long int 
	//dan_max_inst = 1000000000, 
	dan_max_inst = 1000000000, 
	//dan_max_cycle = 1000000000000ull;
	dan_max_cycle = 1;
char benchmark_name[1000];

#define GET_PARAM(name,var) { \
                char *s = getenv (name); \
                if (!s) { if (0) fprintf (stderr, "warning: parameter %s not found in environment\n", name);} \
                else { sscanf (s, "%d", &var); fprintf (stderr, "%s=%d\n", name, var); } }

#define GET_LL_PARAM(name,var) { \
                char *s = getenv (name); \
                if (!s) { if (0) fprintf (stderr, "warning: parameter %s not found in environment\n", name);} \
                else { sscanf (s, "%lld", &var); fprintf (stderr, "%s=%lld\n", name, var); } }

FILE *traceout = NULL;

mintrace *mintraces = NULL;

int main (int argc, char *argv[]) {
	int i;

	assert (argc >= 2);
	ncores = argc - 1;
	nthreads = ncores;
	if (ncores > MAX_CORES) ncores = MAX_CORES;

	// initialize private caches and trace readers

	for (i=0; i<nthreads; i++) {
		readers[i] = new tracereader (argv[i+1]);
	}
	GET_PARAM ("DAN_POLICY", dan_policy);
	GET_LL_PARAM ("DAN_MAX_INST", dan_max_inst);
	GET_LL_PARAM ("DAN_MAX_CYCLE", dan_max_cycle);
	GET_PARAM ("DAN_WARM_INST", dan_warm_inst);
	GET_PARAM ("DAN_SET_SHIFT", dan_set_shift);
	char *s = getenv ("BENCHMARK_NAME");
	if (s) strcpy (benchmark_name, s); else strcpy (benchmark_name, "unknown");

	// initialize last-level cache

	init_cache (
		&LLC, 		// pointer to last-level cache data structure
		LLC_NSETS, 	// number of sets in last-level cache
		LLC_ASSOC, 	// last-level cache associativity
		LLC_BLOCKSIZE, 	// last-level cache block size
		dan_policy, 	// last-level cache replacement policy; 0=lru, 1=rand, etc. as in CRC
		dan_set_shift);	// number of lower-order bits in set index to ignore; safe to set to 0 here

	printf ("LLC %d bytes, %d assoc\n", LLC_NSETS * LLC_ASSOC * LLC_BLOCKSIZE, LLC_ASSOC);
	// prime the traces

	for (i=0; i<nthreads; i++) {
		traces[i] = readers[i]->read();
		assert (traces[i]);
		cycles[i] = traces[i]->cycle;
	}

	// read a lot of traces
	// currently, the trace reader just sets the number of cycles equal to the number of instructions in that thread.
	// after the simulation is done we translate this to estimated cycles using misses and a linear model.
	
	long long int iterations = 0;
	bool done_cycle = false;
	bool done_inst = false;
	for (;;) {

		// see which trace comes first in terms of cycle count (i.e. instruction count for now)

		int min_cycle_thread = -1;
		for (int j=0; j<nthreads; j++) {
			if (min_cycle_thread == -1) {
				if (traces[j]) min_cycle_thread = j;
			} else {
				if (traces[j] && (traces[j]->cycle < traces[min_cycle_thread]->cycle)) min_cycle_thread = j;
			}
			last_insts[j] = traces[j]->instr;// readers[j]->get_icount();
			if (warming && last_insts[j] > dan_warm_inst) {
				warming = false;
				fprintf (stderr, "stopped warming at thread %d with %lld instructions...\n", j, last_insts[j]);
				fflush (stderr);
				for (int i=0; i<ncores; i++) {
					l3_misses_at_warming[i] = l3_misses[i];
				}
				memcpy (cycles_at_warming, cycles, sizeof (cycles));
				for (int z=0; z<nthreads; z++) {
					insts_at_warming[z] = readers[z]->get_icount();
				}
			}
		}
		// all traces have been read, we're done

		if (min_cycle_thread == -1) {
			fprintf (stderr, "all done\n");
			for (int i=0; i<ncores; i++) printf ("icount core %d: %lld\n", i, readers[i]->get_icount());
			break;
		}

		// make t point to the oldest trace

		trace *t = traces[min_cycle_thread];

		// figure out what kind of operation this is; if it is a
		// branch then we don't need to know that.  if it is a iread
		// or dread, or write, then we need it.

		// put the core ID in the address so we have no coherence issues 

		t->address &= 0x00ffffffffffffffull;
		t->address |= (((unsigned long long) min_cycle_thread % MAX_CORES) << 56);

		bool use_cache = true;
		bool use_br = false;
		switch (t->cmd) {
			case DAN_IREAD: 
			case DAN_PREFETCH:
			case DAN_DREAD:
			case DAN_WRITEBACK:
			case DAN_WRITE: use_cache = true; break;
			case DAN_BRTAKEN: 
			case DAN_BRUNTAKEN: 
			case DAN_BRIND:
				assert (0);
				use_cache = false; 
				use_br = true; break;
			default: assert (use_br && 0);
		}
		if (use_cache) {
			// simulate memory access with this trace

			unsigned int miss;
			miss = memory_access (NULL, NULL, &LLC, t->address, t->pc, t->size, t->cmd, min_cycle_thread % MAX_CORES);
			if (miss & 4) l3_misses[min_cycle_thread%MAX_CORES]++;
		}

		// replace the oldest trace with a new trace from the same trace file

		if (traces[min_cycle_thread]) {
			traces[min_cycle_thread] = readers[min_cycle_thread]->read();
			if (traces[min_cycle_thread]) 
				cycles[min_cycle_thread] = traces[min_cycle_thread]->cycle;
		}
		if (iterations && iterations % 100000000 == 0) {
			printf ("core 0 icount = %lld\n", readers[0]->get_icount());
			print_stats ();
		}
		iterations++;

		// see if we are done in terms of getting to the maximum number of instructions for some thread

		//done_cycle = true;
		done_cycle = false;
		done_inst = false;
		// all threads must have executed at least this many cycles or we're not done,
		// or at least one thread must have executed at least this many instructions or we're not done
		for (int j=0; j<nthreads; j++) {
			if (readers[j]->get_cycles() < dan_max_cycle) {
				done_cycle = false;
			}
			if (readers[j]->get_icount() >= dan_max_inst) {
				printf ("thread %d reached %lld instructions; stopping\n", j, readers[j]->get_icount());
				done_inst = true;
			}
		}
		if (done_cycle) {
			printf ("all threads have reached at least %lld cycles; stopping\n", dan_max_cycle);
			break;
		}
		if (done_inst) break;
	}
	print_stats ();
	if (traceout) fclose (traceout);
	//for (i=0; i<ncores; i++) delete readers[i];
	if (mintracefp) fclose (mintracefp);
	return 0;
}

void print_stats (void) {
	int i;

	LLC.repl->PrintStats (cout);
	// estimate number of instructions executed so far using IPC from original simulations

	double sum = 0.0;
	for (i=0; i<ncores; i++)
		sum += last_insts[i];

	// compute estimated MPKIs

	char hostname[100];
	gethostname (hostname, 100);
	printf ("hostname %s\n", hostname);
	fflush (stdout);

	// printf ("L3 counts: %lld %lld %lld %lld ", LLC.counts[0], LLC.counts[1], LLC.counts[2], LLC.counts[6]);
	printf ("L3 instructions: ");
	for (i=0; i<ncores; i++) printf ("core %d: %lld ", i, last_insts[i]-insts_at_warming[i]);
	printf ("\nL3 misses: ");
	for (i=0; i<ncores; i++) printf ("core %d: %lld ", i, (l3_misses[i]-l3_misses_at_warming[i]));
	printf ("\nL3 mpki: ");
	for (i=0; i<ncores; i++) printf ("core %d: %0.4f ", i, 1000.0 * (l3_misses[i]-l3_misses_at_warming[i]) / (double) (last_insts[i]-insts_at_warming[i]));
	printf ("\n");
	if (!warming) for (i=0; i<ncores; i++) {
#if 0
#define L3_MISS_PENALTY	270
		double cpi = 
			  ( L3_MISS_PENALTY * ((l3_misses[i]-l3_misses_at_warming[i]) / 
(double) (last_insts[i]-insts_at_warming[i])) )
			+ 0.33333;
#else
#endif
		const char *name = readers[i]->getname ();
		model *m = NULL;
		double cpi;
		for (int j=0; models[j].name; j++) {
			if (strstr (name, models[j].name)) {
				m = &models[j];
				break;
			}
		}
		if (!m) {
			fprintf (stderr, "no model! defaulting to stupid model.\n");
#define L3_MISS_PENALTY	270
			cpi = ( L3_MISS_PENALTY * ((l3_misses[i]-l3_misses_at_warming[i]) / (double) (last_insts[i]-insts_at_warming[i])) ) + 0.33333;
		} else {
			double mpki = 1000.0 * ((l3_misses[i]-l3_misses_at_warming[i]) / (double) (last_insts[i]-insts_at_warming[i]));
			cpi = mpki * m->m + m->b;
		}
		printf ("core %d: %0.4f IPC\n", i, 1 / cpi);
	}
	fflush (stdout);
}

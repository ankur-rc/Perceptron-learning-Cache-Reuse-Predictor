// trace reader
#include <unistd.h>
#include <zlib.h>
#include <map>

using namespace std;

struct trace {
        int cmd;
        unsigned int size;
        unsigned long long int pc;
        unsigned long long int address;
        unsigned long long int instr;
        unsigned long long int cycle;
};

class tracereader {
	gzFile tracefp;
	trace t;
	unsigned long long int icount, current_cycle, current_instr, cyclecount;
	unsigned long long int insts_upto_restart, cycles_upto_restart;
	char filename[1000];
	long long restart_cycles;

public:

	unsigned long long int get_icount (void) { return icount; }
	unsigned long long int get_cycles (void) { return cyclecount; }

	// open a trace file

	void open (const char *name) {
		tracefp = gzopen (name, "r");
		if (!tracefp) {
			char hostname[1000];
			gethostname (hostname, 1000);
			fprintf (stderr, "%s: ", hostname);
			perror (name);
			fflush (stderr);
		}
		assert (tracefp);
	}

	int gzfread (void *buf, int size, int n, gzFile f) {
		return gzread (f, buf, size * n) / size;
	}

	const char *getname (void) {
		return filename;
	}

	void restart (void) {
		insts_upto_restart += current_instr;
		cycles_upto_restart += current_cycle;
		// printf ("restarting \"%s\" at cycle %lld\n", filename, cycles_upto_restart);
		// fflush (stdout);
		if (tracefp) gzclose (tracefp);
		open (filename);
	}

	trace *read (void) {
	startover:
		unsigned int a = gzfread (&t, sizeof (t), 1, tracefp);
		if (a == 0) {
			// printf ("restarting before %lld cycles!\n", restart_cycles);
			restart_cycles = current_cycle;
			restart ();
			goto startover;
		}
#if 0
		// this code was used to generate truncated traces
		{
			static FILE *f = NULL;
			if (!f) {
				f = fopen ("/tmp/foo", "w");
			}
			fwrite (&t, 1, sizeof (t), f);
			if (t.instr > 1000000000) {
				fprintf (stderr, "stopping at %lld\n", t.instr);
				fclose (f);
				exit (0);
			}
		}
#endif

		// heartbeat

		if (t.cycle >= (unsigned long long int) restart_cycles) {
			restart ();
			goto startover;
		}
		// this is stupid but we have to translate from CMP$im to DAN_* and back
		int cmd = t.cmd;
		switch (cmd) {
			case ACCESS_IFETCH: t.cmd = DAN_IREAD; break;
			case ACCESS_LOAD: t.cmd = DAN_DREAD; break;
			case ACCESS_STORE: t.cmd = DAN_WRITE; break;
			case ACCESS_PREFETCH: t.cmd = DAN_PREFETCH; break;
			case ACCESS_WRITEBACK: t.cmd = DAN_WRITEBACK; break;
			default: assert (0);
		}
#if 0
		printf ("cmd=%d; pc=%llx; address=%llx; instr=%llx; cycle=%llx\n",
			t.cmd, t.pc, t.address, t.instr, t.cycle);
#endif
		current_cycle = t.cycle;
		current_instr = t.instr;
		t.cycle += cycles_upto_restart;
		t.instr += insts_upto_restart;
		cyclecount = t.cycle;
		if (t.instr - icount >= 100000000) {
			icount = t.instr;
			printf ("icount = %lld, cycles = %lld\n", icount, cyclecount);
			fflush (stdout);
		}
		return & t;
	}

	// constructor

	tracereader (const char *name, long long int _restart_cycles = 1000000000) {
		restart_cycles = _restart_cycles;
		current_cycle = 0;
		current_instr = 0;
		cycles_upto_restart = 0;
		insts_upto_restart = 0;
		icount = 0;
		cyclecount = 0;
		strcpy (filename, name);
		open (filename);
		printf ("opened \"%s\"\n", filename);
		fflush (stdout);
	}

	void close (void) {
		gzclose (tracefp);
		tracefp = NULL;
	}

	// destructor

	~tracereader () {
		close ();
	}
};

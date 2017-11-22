// simulate a random or LRU cache

#include <stdio.h>
#include <assert.h>
#include "utils.h"
#include "replacement_state.h"
#include "cache.h"

using namespace std;

static unsigned int random_counter = 0;

void place (cache *c, unsigned long long int pc, unsigned int set, block *b, int offset) {
	// which pc filled this block

	b->filling_pc = pc;

	// which *byte* offset filled this block

	b->offset = offset;
}

// log base 2

int lg2 (int n) {
	int i, m = n, c = -1;
	for (i=0; m; i++) {
		m /= 2;
		c++;
	}
	assert (n == 1<<c);
	return c;
}

// make a cache.  hope blocksize and nsets are a power of 2.

void init_cache (cache *c, int nsets, int assoc, int blocksize, int replacement_policy, int set_shift) {
	int i, j;
	c->sets = new set[nsets];
	c->replacement_policy = replacement_policy;
	c->repl = new CACHE_REPLACEMENT_STATE (nsets, assoc, replacement_policy);
	c->set_shift = set_shift;
	c->nsets = nsets;
	c->assoc = assoc;
	c->blocksize = blocksize;
	c->offset_bits = lg2 (blocksize);
	c->index_bits = lg2 (nsets);
	c->tagshiftbits = c->offset_bits + c->index_bits;
	c->index_mask = nsets - 1;
	c->misses = 0;
	c->accesses = 0;
	memset (c->counts, 0, sizeof (c->counts));
	for (i=0; i<nsets; i++) {
		for (j=0; j<assoc; j++) {
			block *b = &c->sets[i].blocks[j];
			b->tag = 0;
			b->valid = 0;
			b->dirty = 0;
		}
		c->sets[i].valid = 0;
	}
}

// move a block to the MRU position

void move_to_mru (block *v, int i) {
	int j;
	block b = v[i];
	for (j=i; j>=1; j--) v[j] = v[j-1];
	v[0] = b;
}

// access a cache, return true for miss, false for hit

#define check_writeback(b) { if (writeback_address && v[(b)].valid && v[(b)].dirty) *writeback_address = ((v[(b)].tag << c->index_bits) + set) << c->offset_bits; }

bool cache_access (cache *c, unsigned long long int address, unsigned long long int pc, unsigned int size, int op, unsigned int core, unsigned long long int *writeback_address = NULL) {
	c->counts[op]++;
	int i, assoc = c->assoc;
	block *v;
	unsigned int offset = address & (c->blocksize - 1);
	unsigned long long int block_addr = address >> c->offset_bits;
	unsigned int set = (block_addr >> c->set_shift) & c->index_mask;

	// note this doesn't generate the right tag if we have a non-zero set shift
	// we *do* need the right tag value for things like the sampler to work
	// because the sampler recontstructs the physical address from the tag & index

	unsigned long long int tag = block_addr >> c->index_bits;

	// this will be true if the current set contains only valid blocks, false otherwise

	int set_valid = c->sets[set].valid;
	c->accesses++;
	v = &c->sets[set].blocks[0];
	LINE_STATE ls;
	if (writeback_address) *writeback_address = 0;
	AccessTypes at;
	switch (op) {
		case DAN_PREFETCH: at = ACCESS_PREFETCH; break;
		case DAN_DREAD: at = ACCESS_LOAD; break;
		case DAN_WRITE: at = ACCESS_STORE; break;
		case DAN_WRITEBACK: at = ACCESS_WRITEBACK; break;
		case DAN_IREAD: at = ACCESS_IFETCH; break;
		default: at = ACCESS_LOAD;
		printf ("op is %d!\n", op); fflush (stdout);
		assert (0);
	}
	
	// tag match?

	for (i=0; i<assoc; i++) {
		if (v[i].tag == tag) {
			if (at == ACCESS_STORE || at == ACCESS_WRITEBACK) v[i].dirty = true;
			if (c->replacement_policy == REPLACEMENT_POLICY_LRU) {
				// move this block to the mru position
				if (i != 0) move_to_mru (v, i);
				assert (i >= 0 && i < assoc);
				// update CRC's LRU policy (for instrumentation)
				ls.tag = tag;
				if (at != ACCESS_WRITEBACK)
#ifdef DANSHIP
					c->repl->UpdateReplacementState (set, i, &ls, core, pc, at, true, address);
#else
					c->repl->UpdateReplacementState (set, i, &ls, core, pc, at, true);
#endif
			} else if (c->replacement_policy >= REPLACEMENT_POLICY_CRC) {
				ls.tag = tag;
				assert (i >= 0 && i < assoc);
				if (at != ACCESS_WRITEBACK)
#ifdef DANSHIP
					c->repl->UpdateReplacementState (set, i, &ls, core, pc, at, true, address);
#else
					c->repl->UpdateReplacementState (set, i, &ls, core, pc, at, true);
#endif
			}
			return false;
		}
	}
	c->misses++;

	// a miss.
	// find a block to replace

	if (!set_valid) {
		for (i=0; i<assoc; i++) {
			if (v[i].valid == 0) break;
		}
		if (i == assoc) {
			c->sets[set].valid = 1; // mark this set as having only valid blocks so we don't search it again
			set_valid = 1;
		}
		// at this point, i indicates an invalid block, or assoc if there is no invalid block
	}
	if (c->replacement_policy == REPLACEMENT_POLICY_RANDOM) {

		// if no invalid block, choose a random one

		if (set_valid) i = (random_counter++) % assoc; // replace
		check_writeback (i);
		if (at == ACCESS_STORE || at == ACCESS_WRITEBACK) 
			v[i].dirty = true;
		else
			v[i].dirty = false;
		v[i].tag = tag;
		v[i].valid = 1;
		place (c, pc, set, &v[i], offset);
	} else if (c->replacement_policy == REPLACEMENT_POLICY_LRU) {

		// if no invalid block, use the lru one (the one in the last position)

		if (set_valid) i = assoc - 1; // replace LRU block
		check_writeback (i);
		if (i != 0) move_to_mru (v, i);
		if (at == ACCESS_STORE || at == ACCESS_WRITEBACK) 
			v[0].dirty = true;
		else
			v[0].dirty = false;
		v[0].tag = tag;
		v[0].valid = 1;
		place (c, pc, set, &v[0], offset);

		// update CRC's LRU policy (for instrumentation)
		ls.tag = tag;
		if (at != ACCESS_WRITEBACK) {
			// find LRU way
			int lru = -1;
			for (int z=0; z<(int)assoc; z++) if (c->repl->repl[set][z].LRUstackposition == (unsigned) assoc-1) { lru = z; break; }
			assert (lru >= 0);
#ifdef DANSHIP
			c->repl->UpdateReplacementState (set, lru, &ls, core, pc, at, false, address);
#else
			c->repl->UpdateReplacementState (set, lru, &ls, core, pc, at, false);
#endif
		}
	} else {
		// assume we are using CRC replacement policy, see what it wants to replace
		if (set_valid) {
			i = c->repl->GetVictimInSet (core, set, NULL, assoc, pc, address, at); // replace
		}
		ls.tag = tag;

		// -1 means bypass

		if (i != -1) {
			check_writeback (i);
			if (at == ACCESS_STORE || at == ACCESS_WRITEBACK) 
				v[i].dirty = true;
			else
				v[i].dirty = false;
			v[i].tag = tag;
			v[i].valid = 1;
			assert (i >= 0 && i < assoc);
#ifdef DANSHIP
			c->repl->UpdateReplacementState (set, i, &ls, core, pc, at, false, address);
#else
			c->repl->UpdateReplacementState (set, i, &ls, core, pc, at, false);
#endif
			place (c, pc, set, &v[i], offset);
		}
	}
	// only count as a miss if the block is not a writeback block or prefetch
	return (at != ACCESS_WRITEBACK) && (at != ACCESS_PREFETCH);
}

// access the memory, returning an integer that has:
// bit 0 set if there is a miss in L1
// bit 1 set if there is a miss in L2
// bit 2 set if there is a miss in L3

// private L1 and L2, shared L3

unsigned int memory_access (cache **L1, cache **L2, cache *L3, unsigned long long int address, unsigned long long int pc, unsigned int size, int op, unsigned int core) {
	// access the memory hierarchy, returning latency of access
	unsigned int miss = 0;
	// unsigned long long int writeback_address;
	if (L3) {
		// L3 shared between everyone
		bool missL3 = cache_access (L3, address, pc, size, op, core, NULL);
		if (missL3) miss |= 4;
	}
	return miss;
}

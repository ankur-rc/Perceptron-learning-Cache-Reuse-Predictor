// quick and dirty cache simulation

#define MAX_SETS	(1<<19)
#define MAX_ASSOC	16
#define WORDSIZE	4

#define DAN_IREAD       0
#define DAN_DREAD       1
#define DAN_WRITE       2
#define DAN_BRTAKEN     3
#define DAN_BRUNTAKEN   4
#define DAN_BRIND       5
#define DAN_WRITEBACK   6
#define DAN_PREFETCH	7
#define DAN_MAX		8

#define OP_READ		DAN_DREAD
#define OP_IREAD	DAN_IREAD
#define OP_WRITE	DAN_WRITE
#define OP_WRITEBACK	DAN_WRITEBACK

#define REPLACEMENT_POLICY_LRU		0
#define REPLACEMENT_POLICY_RANDOM	1
#define REPLACEMENT_POLICY_CRC		2

struct block {
	unsigned int lru_stack_position;
	unsigned long long int tag;
	unsigned char valid, dirty;
	unsigned long long int filling_pc; // pc that filled this block
	int offset; // offset of *byte* that caused this line to be filled

	block (void) {
		offset = 0;
		dirty = false;
		valid = false;
		tag = 0;
	}
};

struct set {
	block blocks[MAX_ASSOC];
	unsigned char valid; // means entire set is valid

	set (void) {
		valid = false;
		for (int i=0; i<MAX_ASSOC; i++) {
			blocks[i].lru_stack_position = i;
		}
	}
};

struct cache {
	int	nsets, assoc, blocksize, set_shift;
	int	offset_bits, index_bits, replacement_policy, tagshiftbits;
	unsigned int index_mask;
	unsigned long long misses, accesses;
	set	*sets;
	long long int counts[DAN_MAX];

	CACHE_REPLACEMENT_STATE *repl;

	cache (void) {
		misses = 0;
		accesses = 0;
		index_mask = 0;
		repl = NULL;
	}
};

void init_cache (cache *c, int nsets, int assoc, int blocksize, int policy, int set_shift);
bool cache_access (cache *c, unsigned long long int address, unsigned long long int, unsigned int, int op, unsigned int core);
unsigned int memory_access (cache **l1, cache **l2, cache *l3, unsigned long long int address, unsigned long long int, unsigned int, int op, unsigned int);

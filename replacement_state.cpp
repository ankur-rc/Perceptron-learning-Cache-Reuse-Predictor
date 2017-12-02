#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <map>
#include <iostream>

using namespace std;

#include "replacement_state.h"

// this code compiles cleanly in g++-4.1.2/i386 and 64-bit g++-4.2.4/x86_64
// development and testing were all done on x86_64

// some run-time constants

int
    // sampler associativity (changed for 4MB cache)

    dan_sampler_assoc = 12,

    // number of bits used to index predictor; determines number of
    // entries in prediction tables (changed for 4MB cache)

    dan_predictor_index_bits = 12,

    // number of prediction tables

    dan_predictor_tables = 3,

    // width of prediction saturating counters

    dan_counter_width = 2,

    // predictor must meet this threshold to predict a block is dead

    dan_threshold = 8,

    // number of partial tag bits kept per sampler entry

    dan_sampler_tag_bits = 16,

    // number of trace (partial PC) bits kept per sampler entry

    dan_sampler_trace_bits = 16,

    // number of entries in prediction table; derived from # of index bits

    dan_predictor_table_entries,

    // maximum value of saturating counter; derived from counter width

    dan_counter_max,

    // total number of bits used by all structures; computed in sampler::sampler

    total_bits_used;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This file is distributed as part of the Cache Replacement Championship     //
// workshop held in conjunction with ISCA'2010.                               //
//                                                                            //
//                                                                            //
// Everyone is granted permission to copy, modify, and/or re-distribute       //
// this software.                                                             //
//                                                                            //
// Please contact Aamer Jaleel <ajaleel@gmail.com> should you have any        //
// questions                                                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/*
** This file implements the cache replacement state. Users can enhance the code
** below to develop their cache replacement ideas.
**
*/

////////////////////////////////////////////////////////////////////////////////
// The replacement state constructor:                                         //
// Inputs: number of sets, associativity, and replacement policy to use       //
// Outputs: None                                                              //
//                                                                            //
// DO NOT CHANGE THE CONSTRUCTOR PROTOTYPE                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
CACHE_REPLACEMENT_STATE::CACHE_REPLACEMENT_STATE(UINT32 _sets, UINT32 _assoc, UINT32 _pol)
{

    numsets = _sets;
    assoc = _assoc;
    replPolicy = _pol;

    mytimer = 0;

    InitReplacementState();
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// The function prints the statistics for the cache                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
ostream &CACHE_REPLACEMENT_STATE::PrintStats(ostream &out)
{

    out << "==========================================================" << endl;
    out << "=========== Replacement Policy Statistics ================" << endl;
    out << "==========================================================" << endl;

    // CONTESTANTS:  Insert your statistics printing here

    return out;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function initializes the replacement policy hardware by creating      //
// storage for the replacement state on a per-line/per-cache basis.           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::InitReplacementState()
{
    // Create the state for sets, then create the state for the ways
    repl = new LINE_REPLACEMENT_STATE *[numsets];
    // ensure that we were able to create replacement state
    assert(repl);

    // Create the state for the sets
    for (UINT32 setIndex = 0; setIndex < numsets; setIndex++)
    {
        repl[setIndex] = new LINE_REPLACEMENT_STATE[assoc];

        for (UINT32 way = 0; way < assoc; way++)
        {
            // initialize stack position (for true LRU)
            repl[setIndex][way].LRUstackposition = way;
        }
    }

    if (replPolicy != CRC_REPL_CONTESTANT)
        return;

    // Contestants:  ADD INITIALIZATION FOR YOUR HARDWARE HERE

    samp = new sampler(numsets, assoc);
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache on every cache miss. The input        //
// argument is the set index. The return value is the physical way            //
// index for the line being replaced.                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::GetVictimInSet(UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType)
{
    // If no invalid lines, then replace based on replacement policy
    if (replPolicy == CRC_REPL_LRU)
    {
        return Get_LRU_Victim(setIndex);
    }
    else if (replPolicy == CRC_REPL_RANDOM)
    {
        return Get_Random_Victim(setIndex);
    }
    else if (replPolicy == CRC_REPL_CONTESTANT)
    {
        // Contestants:  ADD YOUR VICTIM SELECTION FUNCTION HERE
        return Get_Sampler_Victim(tid, setIndex, vicSet, assoc, PC, paddr, accessType);
    }

    // We should never here here
    assert(0);
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache after every cache hit/miss            //
// The arguments are: the set index, the physical way of the cache,           //
// the pointer to the physical line (should contestants need access           //
// to information of the line filled or hit upon), the thread id              //
// of the request, the PC of the request, the accesstype, and finall          //
// whether the line was a cachehit or not (cacheHit=true implies hit)         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateReplacementState(
    UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine,
    UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit)
{
    // What replacement policy?
    if (replPolicy == CRC_REPL_LRU)
    {
        UpdateLRU(setIndex, updateWayID);
    }
    else if (replPolicy == CRC_REPL_RANDOM)
    {
        // Random replacement requires no replacement state update
    }
    else if (replPolicy == CRC_REPL_CONTESTANT)
    {
        // Contestants:  ADD YOUR UPDATE REPLACEMENT STATE FUNCTION HERE
        // Feel free to use any of the input parameters to make
        // updates to your replacement policy
        // if (accessType != ACCESS_WRITEBACK)
        UpdateSampler(setIndex, currLine->tag, tid, PC, updateWayID, cacheHit);
    }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//////// HELPER FUNCTIONS FOR REPLACEMENT UPDATE AND VICTIM SELECTION //////////
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds the LRU victim in the cache set by returning the       //
// cache block at the bottom of the LRU stack. Top of LRU stack is '0'        //
// while bottom of LRU stack is 'assoc-1'                                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_LRU_Victim(UINT32 setIndex)
{
    // Get pointer to replacement state of current set
    LINE_REPLACEMENT_STATE *replSet = repl[setIndex];

    INT32 lruWay = 0;

    // Search for victim whose stack position is assoc-1
    for (UINT32 way = 0; way < assoc; way++)
    {
        if (replSet[way].LRUstackposition == (assoc - 1))
        {
            lruWay = way;
            break;
        }
    }

    // return lru way
    return lruWay;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds a random victim in the cache set                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_Random_Victim(UINT32 setIndex)
{
    INT32 way = (rand() % assoc);

    return way;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function implements the LRU update routine for the traditional        //
// LRU replacement policy. The arguments to the function are the physical     //
// way and set index.                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateLRU(UINT32 setIndex, INT32 updateWayID)
{
    // Determine current LRU stack position
    UINT32 currLRUstackposition = repl[setIndex][updateWayID].LRUstackposition;

    // Update the stack position of all lines before the current line
    // Update implies incremeting their stack positions by one
    for (UINT32 way = 0; way < assoc; way++)
    {
        if (repl[setIndex][way].LRUstackposition < currLRUstackposition)
        {
            repl[setIndex][way].LRUstackposition++;
        }
    }

    // Set the LRU stack position of new line to be zero
    repl[setIndex][updateWayID].LRUstackposition = 0;
}

////////////////////////////////////////////////////////////////////////////////
// Jimenez's code
////////////////////////////////////////////////////////////////////////////////

// make a trace from a PC (just extract some bits)

unsigned int make_trace(UINT32 tid, predictor *pred, Addr_t PC)
{
    return PC & ((1 << dan_sampler_trace_bits) - 1);
}

// called when there is an access to an LLC cache block

void CACHE_REPLACEMENT_STATE::UpdateSampler(UINT32 setIndex, Addr_t tag, UINT32 tid, Addr_t PC, INT32 way, bool hit)
{

    // determine if this is a sampler set

    if (setIndex % samp->sampler_modulus == 0)
    {

        // this is a sampler set.  access the sampler.

        int set = setIndex / samp->sampler_modulus;
        if (set >= 0 && set < samp->nsampler_sets)
            samp->access(tid, set, tag, PC);
    }

    // update default replacement policy

    UpdateLRU(setIndex, way);

    // make the trace

    unsigned int trace = make_trace(tid, samp->pred, PC);

    // get the next prediction for this block using that trace

    repl[setIndex][way].prediction = samp->pred->get_prediction(tid, trace, setIndex);
}

// called to select a victim.  returns victim way, or -1 if the block should bypass

INT32 CACHE_REPLACEMENT_STATE::Get_Sampler_Victim(UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType)
{

    // select a victim using default LRU policy

    int r = Get_LRU_Victim(setIndex);

    // look for a predicted dead block

    for (unsigned int i = 0; i < assoc; i++)
    {
        if (repl[setIndex][i].prediction)
        {

            // found a predicted dead block; this is our new victim

            r = i;
            break;
        }
    }

    // predict whether this block is "dead on arrival"

    unsigned int trace = make_trace(tid, samp->pred, PC);
    int prediction = samp->pred->get_prediction(tid, trace, setIndex);

    // if block is predicted dead, then it should bypass the cache

    if (prediction)
        r = -1; // -1 means bypass

    // return the selected victim

    return r;
}

// constructor for a sampler set

sampler_set::sampler_set(void)
{

    // allocate some sampler entries

    blocks = new sampler_entry[dan_sampler_assoc];

    // initialize the LRU replacement algorithm for these entries

    for (int i = 0; i < dan_sampler_assoc; i++)
        blocks[i].lru_stack_position = i;
}

// access the sampler with an LLC tag

void sampler::access(UINT32 tid, int set, Addr_t tag, Addr_t PC)
{

    // get a pointer to this set's sampler entries

    sampler_entry *blocks = &sets[set].blocks[0];

    // get a partial tag to search for

    unsigned int partial_tag = tag & ((1 << dan_sampler_tag_bits) - 1);

    // assume we do not miss

    bool miss = false;

    // this will be the way of the sampler entry we end up hitting or replacing

    int i;

    // search for a matching tag

    for (i = 0; i < dan_sampler_assoc; i++)
        if (blocks[i].valid && (blocks[i].tag == partial_tag))
        {

            // we know this block is not dead; inform the predictor

            pred->block_is_dead(tid, blocks[i].trace, false);
            break;
        }

    // did we find a match?

    if (i == dan_sampler_assoc)
    {

        // no, so this is a miss in the sampler

        miss = true;

        // look for an invalid block to replace

        for (i = 0; i < dan_sampler_assoc; i++)
            if (blocks[i].valid == false)
                break;

        // no invalid block?  look for a dead block.

        if (i == dan_sampler_assoc)
        {
            // find the LRU dead block
            for (i = 0; i < dan_sampler_assoc; i++)
                if (blocks[i].prediction)
                    break;
        }

        // no invalid or dead block?  use the LRU block

        if (i == dan_sampler_assoc)
        {
            int j;
            for (j = 0; j < dan_sampler_assoc; j++)
                if (blocks[j].lru_stack_position == (unsigned int)(dan_sampler_assoc - 1))
                    break;
            assert(j < dan_sampler_assoc);
            i = j;
        }

        // previous trace leads to block being dead; inform the predictor

        pred->block_is_dead(tid, blocks[i].trace, true);

        // fill the victim block

        blocks[i].tag = partial_tag;
        blocks[i].valid = true;
    }

    // record the trace

    blocks[i].trace = make_trace(tid, pred, PC);

    // get the next prediction for this entry

    blocks[i].prediction = pred->get_prediction(tid, blocks[i].trace, -1);

    // now the replaced entry should be moved to the MRU position

    unsigned int position = blocks[i].lru_stack_position;
    for (int way = 0; way < dan_sampler_assoc; way++)
        if (blocks[way].lru_stack_position < position)
            blocks[way].lru_stack_position++;
    blocks[i].lru_stack_position = 0;
}

// constructor for sampler

sampler::sampler(int nsets, int assoc)
{
    // four-core version gets slightly different parameters

    if (nsets == 4096)
    {
        dan_sampler_assoc = 13;
        dan_predictor_index_bits = 14;
    }

    // here, we figure out the total number of bits used by the various
    // structures etc.  along the way we will figure out how many
    // sampler sets we have room for

    // figure out number of entries in each table

    dan_predictor_table_entries = 1 << dan_predictor_index_bits;

    // compute the total number of bits used by the replacement policy

    // total number of bits available for the contest

    int nbits_total = (nsets * assoc * 8 + 1024);

    // the real LRU policy consumes log(assoc) bits per block

    int nbits_lru = assoc * nsets * (int)log2(assoc);

    // the dead block predictor consumes (counter width) * (number of tables)
    // * (entries per table) bits

    int nbits_predictor =
        dan_counter_width * dan_predictor_tables * dan_predictor_table_entries;

    // one prediction bit per cache block.

    int nbits_cache = 1 * nsets * assoc;

    // some extra bits we account for to be safe; figure we need about 85 bits
    // for the various run-time constants and variables the CRC guys might want
    // to charge us for.  in reality we leave a bigger surplus than this so we
    // should be safe.

    int nbits_extra = 85;

    // number of bits left over for the sampler sets

    int nbits_left_over =
        nbits_total - (nbits_predictor + nbits_cache + nbits_lru + nbits_extra);

    // number of bits in one sampler set: associativity of sampler * bits per sampler block entry

    int nbits_one_sampler_set =
        dan_sampler_assoc
        // tag bits, valid bit, prediction bit, trace bits, lru stack position bits
        * (dan_sampler_tag_bits + 1 + 1 + 4 + dan_sampler_trace_bits);

    // maximum number of sampler of sets we can afford with the space left over

    nsampler_sets = nbits_left_over / nbits_one_sampler_set;

    // compute the maximum saturating counter value; predictor constructor
    // needs this so we do it here

    dan_counter_max = (1 << dan_counter_width) - 1;

    // make a predictor

    pred = new predictor();

    // we should have at least one sampler set

    assert(nsampler_sets >= 0);

    // make the sampler sets

    sets = new sampler_set[nsampler_sets];

    // figure out what should divide evenly into a set index to be
    // considered a sampler set

    sampler_modulus = nsets / nsampler_sets;

    // compute total number of bits used; we can print this out to validate
    // the computation in the paper

    total_bits_used =
        (nbits_total - nbits_left_over) + (nbits_one_sampler_set * nsampler_sets);
    //fprintf (stderr, "total bits used %d\n", total_bits_used);
}

// constructor for the predictor

predictor::predictor(void)
{

    // make the tables

    tables = new int *[dan_predictor_tables];

    // initialize each table to all 0s

    for (int i = 0; i < dan_predictor_tables; i++)
    {
        tables[i] = new int[dan_predictor_table_entries];
        memset(tables[i], 0, sizeof(int) * dan_predictor_table_entries);
    }
}

// hash three numbers into one

unsigned int mix(unsigned int a, unsigned int b, unsigned int c)
{
    a = a - b;
    a = a - c;
    a = a ^ (c >> 13);
    b = b - c;
    b = b - a;
    b = b ^ (a << 8);
    c = c - a;
    c = c - b;
    c = c ^ (b >> 13);
    return c;
}

// first hash function

unsigned int f1(unsigned int x)
{
    return mix(0xfeedface, 0xdeadb10c, x);
}

// second hash function

unsigned int f2(unsigned int x)
{
    return mix(0xc001d00d, 0xfade2b1c, x);
}

// generalized hash function

unsigned int fi(unsigned int x, int i)
{
    return f1(x) + (f2(x) >> i);
}

// hash a trace, thread ID, and predictor table number into a predictor table index

unsigned int predictor::get_table_index(UINT32 tid, unsigned int trace, int t)
{
    unsigned int x = fi(trace ^ (tid << 2), t);
    return x & ((1 << dan_predictor_index_bits) - 1);
}

// inform the predictor that a block is either dead or not dead

void predictor::block_is_dead(UINT32 tid, unsigned int trace, bool d)
{

    // for each predictor table...

    for (int i = 0; i < dan_predictor_tables; i++)
    {

        // ...get a pointer to the corresponding entry in that table

        int *c = &tables[i][get_table_index(tid, trace, i)];

        // if the block is dead, increment the counter

        if (d)
        {
            if (*c < dan_counter_max)
                (*c)++;
        }
        else
        {

            // otherwise, decrease the counter

            if (i & 1)
            {
                // odd numbered tables decrease exponentially

                (*c) >>= 1;
            }
            else
            {
                // even numbered tables decrease by one
                if (*c > 0)
                    (*c)--;
            }
        }
    }
}

// get a prediction for a given trace

bool predictor::get_prediction(UINT32 tid, unsigned int trace, int set)
{

    // start the confidence sum as 0

    int conf = 0;

    // for each table...
    for (int i = 0; i < dan_predictor_tables; i++)
    {

        // ...get the counter value for that table...

        int val = tables[i][get_table_index(tid, trace, i)];

        // and add it to the running total

        conf += val;
    }

    // if the counter is at least the threshold, the block is predicted dead

    return conf >= dan_threshold;
}

// nothing

CACHE_REPLACEMENT_STATE::~CACHE_REPLACEMENT_STATE(void)
{
}

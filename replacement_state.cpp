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

#define BLOCKSIZE 64

#define SAMPLER_ASSOC 16
#define SAMPLER_SET 64

#define NUM_FEATURES 6
#define NUM_WEIGHTS 256
#define MAX_WEIGHT 31
#define MIN_WEIGHT -32

#define THETA 74
#define TAU_BYPASS 3
#define TAU_REPLACE 124

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
    // plru = new bitset<15>[numsets];

    // ensure that we were able to create replacement state

    assert(repl);

    // Create the state for the sets
    for (UINT32 setIndex = 0; setIndex < numsets; setIndex++)
    {
        repl[setIndex] = new LINE_REPLACEMENT_STATE[assoc];
        // plru[setIndex] = 0;

        for (UINT32 way = 0; way < assoc; way++)
        {
            // initialize stack position (for true LRU)
            repl[setIndex][way].LRUstackposition = way;
            repl[setIndex][way].reuse_bit = false;
            repl[setIndex][way].lru = way;
        }
    }

    if (replPolicy != CRC_REPL_CONTESTANT)
        return;

    // Contestants:  ADD INITIALIZATION FOR YOUR HARDWARE HERE

    // Initialize the sampler
    sampler_sets = new sampler *[SAMPLER_SET];
    for (int i = 0; i < SAMPLER_SET; i++)
    {
        sampler_sets[i] = new sampler[SAMPLER_ASSOC];
        for (int j = 0; j < SAMPLER_ASSOC; j++)
        {
            sampler_sets[i][j].lru = j;
            sampler_sets[i][j].partial_tag = 0;
            sampler_sets[i][j].valid = false;
            sampler_sets[i][j].y_out = 0;

            sampler_sets[i][j].features.PC_0 = 0;
            sampler_sets[i][j].features.PC_1 = 0;
            sampler_sets[i][j].features.PC_2 = 0;
            sampler_sets[i][j].features.PC_3 = 0;
            sampler_sets[i][j].features.tag_rs_4 = 0;
            sampler_sets[i][j].features.tag_rs_7 = 0;
        }
    }

    // Initialize the weight tables
    weight_table = new int *[NUM_FEATURES];
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        weight_table[i] = new int[NUM_WEIGHTS];
        for (int j = 0; j < NUM_WEIGHTS; j++)
        {
            weight_table[i][j] = 0;
        }
    }

    // Initialize the PC history
    for (int i = 0; i < 4; i++)
    {
        pc_hist[i] = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache on every cache miss. The input        //
// argument is the set index. The return value is the physical way            //
// index for the line being replaced.                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::GetVictimInSet(UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet,
                                              UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType)
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
        return Get_My_Victim(setIndex, PC, paddr);
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
    //fprintf (stderr, "ain't I a stinker? %lld\n", get_cycle_count ());
    //fflush (stderr);
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
        UpdateMyPolicy(setIndex, updateWayID, currLine, PC, cacheHit);
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
//                                                                            //
//                     Get a victim using CRC policy                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

INT32 CACHE_REPLACEMENT_STATE::Get_My_Victim(UINT32 setIndex, Addr_t PC, Addr_t address)
{
    int way = 0;
    // Compute current features
    Features current_features = compute_features(PC, address, false);

    // Predict using current features
    int prediction_output = predict(current_features);

    if (prediction_output > TAU_BYPASS) //bypass if reuse prediction is false
    {
        update_PCs(PC); // update the PC recency stack
        way = -1;       // set way to -1
    }
    else // if reuse prediction is true; i.e do not bypass
    {
        bool found_dead = false;
        //search the set for a dead block
        for (unsigned int i = 0; i < assoc; i++)
        {
            if (repl[setIndex][i].reuse_bit == false)
            {
                found_dead = true;
                way = i; // return the way with the dead block
                break;
            }
        }

        if (!found_dead) // if no dead block was found, evict using PseudoLRU
        {
            way = get_cache_LRU_index(setIndex);
        }
    }

    return way;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Update CRC book-keeping structures                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void CACHE_REPLACEMENT_STATE::UpdateMyPolicy(UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine,
                                             Addr_t PC, bool cacheHit)
{
    // update the PC recency stack
    update_PCs(PC);

    // compute current features
    Features features = compute_features(PC, currLine->tag, true);

    //check to see if current set is a sampler set
    if (setIndex % (numsets / SAMPLER_SET) == 0)
    {
        int index = (setIndex / (numsets / SAMPLER_SET));   // sampler index
        bitset<15> tag = (currLine->tag) & ((1 << 15) - 1); // sampler tag
        bool block_exists = false;

        for (int i = 0; i < SAMPLER_ASSOC; i++) // check if entry exists for current tag
        {
            if ((sampler_sets[index][i].partial_tag == tag) && (sampler_sets[index][i].valid)) // there was a match
            {
                if (sampler_sets[index][i].y_out > (-THETA) || repl[setIndex][updateWayID].reuse_bit == false) // train if greater than (-theta)
                {
                    train(sampler_sets[index][i].features, false); // train predictor on decrement
                }

                block_exists = true;
                sampler_sets[index][i].features = features;                              // update the features
                update_LRU_state(index, i);                                              // update the LRU state
                sampler_sets[index][i].y_out = predict(sampler_sets[index][i].features); // get prediction on new features
                break;
            }
        }

        if (!block_exists) // there was no match; eviction required in sampler
        {
            int way = -1;

            //look for an invalid block
            for (unsigned int i = 0; i < assoc; i++)
            {
                if (sampler_sets[index][i].valid == false)
                {
                    way = i;
                    break;
                }
            }

            // if not found, search for dead block within the sampler
            if (way == -1)
            {
                for (unsigned int i = 0; i < assoc; i++)
                {
                    if (sampler_sets[index][i].y_out > TAU_REPLACE)
                    {
                        way = i;
                        break;
                    }
                }
            }

            // if not found, use LRU to find the eviction candidate
            if (way == -1)
                way = get_LRU_index(index);

            // train if sampler block y_out is less than theta or if prediction was incorrect
            if (sampler_sets[index][way].y_out < THETA || repl[setIndex][updateWayID].reuse_bit == true)
            {
                train(sampler_sets[index][way].features, true); // train on increment
            }

            sampler_sets[index][way].partial_tag = tag;
            sampler_sets[index][way].features = features;                                // update the features
            update_LRU_state(index, way);                                                // update the LRU state
            sampler_sets[index][way].y_out = predict(sampler_sets[index][way].features); // get prediction on new features
            sampler_sets[index][way].valid = true;                                       // set valid bit
        }
    }

    // update PLRU state
    update_cache_LRU_state(setIndex, updateWayID);

    // set reuse bit
    int y_out = predict(features);
    repl[setIndex][updateWayID].reuse_bit = (y_out < TAU_REPLACE ? true : false);
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Utility functions                                      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void CACHE_REPLACEMENT_STATE::update_PCs(const Addr_t current_PC)
{
    for (int i = 3; i > 0; i--)
        pc_hist[i] = pc_hist[i - 1];

    pc_hist[0] = current_PC;
}

Features CACHE_REPLACEMENT_STATE::compute_features(const Addr_t PC, const Addr_t address, const bool PC_is_updated)
{
    Features current_features;
    Addr_t PCs[4];

    // if PC is already updated we do not need to shift new PC
    PCs[0] = (PC_is_updated ? pc_hist[0] : PC);
    PCs[1] = (PC_is_updated ? pc_hist[1] : pc_hist[0]);
    PCs[2] = (PC_is_updated ? pc_hist[2] : pc_hist[1]);
    PCs[3] = (PC_is_updated ? pc_hist[3] : pc_hist[2]);

    Addr_t mask_8 = ((1 << 8) - 1); // mask to extract 8 bits

    current_features.PC_0 = ((PCs[0] >> 2) ^ PC) & mask_8; // feature 1
    current_features.PC_1 = ((PCs[1] >> 1) ^ PC) & mask_8; // feature 2
    current_features.PC_2 = ((PCs[2] >> 2) ^ PC) & mask_8; // feature 3
    current_features.PC_3 = ((PCs[3] >> 3) ^ PC) & mask_8; // feature 4

    // Get the tag from the address
    int num_index_bits = log2(numsets);
    int num_offset_bits = log2(BLOCKSIZE);
    Addr_t tag = (PC_is_updated ? address                                           // if being called from update repl state, we already have the tag
                                : (address >> (num_index_bits + num_offset_bits))); // otherwise find tag from address

    current_features.tag_rs_4 = ((tag >> 4) ^ PC) & mask_8; // feature 5
    current_features.tag_rs_7 = ((tag >> 7) ^ PC) & mask_8; // feature 6

    return current_features;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                    Predict and train functions                             //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

int CACHE_REPLACEMENT_STATE::predict(const Features &features)
{
    int perceptron_output = 0;

    perceptron_output += weight_table[0][features.PC_0.to_ulong()];
    perceptron_output += weight_table[1][features.PC_1.to_ulong()];
    perceptron_output += weight_table[2][features.PC_2.to_ulong()];
    perceptron_output += weight_table[3][features.PC_3.to_ulong()];
    perceptron_output += weight_table[4][features.tag_rs_4.to_ulong()];
    perceptron_output += weight_table[5][features.tag_rs_7.to_ulong()];

    return perceptron_output;
}

void CACHE_REPLACEMENT_STATE::train(const Features &features, bool increment)
{
    if (increment == false)
    {
        int wt_1 = weight_table[0][features.PC_0.to_ulong()];
        weight_table[0][features.PC_0.to_ulong()] = (wt_1 > MIN_WEIGHT ? --wt_1 : MIN_WEIGHT);

        int wt_2 = weight_table[1][features.PC_1.to_ulong()];
        weight_table[1][features.PC_1.to_ulong()] = (wt_2 > MIN_WEIGHT ? --wt_2 : MIN_WEIGHT);

        int wt_3 = weight_table[2][features.PC_2.to_ulong()];
        weight_table[2][features.PC_2.to_ulong()] = (wt_3 > MIN_WEIGHT ? --wt_3 : MIN_WEIGHT);

        int wt_4 = weight_table[3][features.PC_3.to_ulong()];
        weight_table[3][features.PC_3.to_ulong()] = (wt_4 > MIN_WEIGHT ? --wt_4 : MIN_WEIGHT);

        int wt_5 = weight_table[4][features.tag_rs_4.to_ulong()];
        weight_table[4][features.tag_rs_4.to_ulong()] = (wt_5 > MIN_WEIGHT ? --wt_5 : MIN_WEIGHT);

        int wt_6 = weight_table[5][features.tag_rs_7.to_ulong()];
        weight_table[5][features.tag_rs_7.to_ulong()] = (wt_6 > MIN_WEIGHT ? --wt_6 : MIN_WEIGHT);
    }
    else if (increment)
    {
        int wt_1 = weight_table[0][features.PC_0.to_ulong()];
        weight_table[0][features.PC_0.to_ulong()] = (wt_1 < MAX_WEIGHT ? ++wt_1 : MAX_WEIGHT);

        int wt_2 = weight_table[1][features.PC_1.to_ulong()];
        weight_table[1][features.PC_1.to_ulong()] = (wt_2 < MAX_WEIGHT ? ++wt_2 : MAX_WEIGHT);

        int wt_3 = weight_table[2][features.PC_2.to_ulong()];
        weight_table[2][features.PC_2.to_ulong()] = (wt_3 < MAX_WEIGHT ? ++wt_3 : MAX_WEIGHT);

        int wt_4 = weight_table[3][features.PC_3.to_ulong()];
        weight_table[3][features.PC_3.to_ulong()] = (wt_4 < MAX_WEIGHT ? ++wt_4 : MAX_WEIGHT);

        int wt_5 = weight_table[4][features.tag_rs_4.to_ulong()];
        weight_table[4][features.tag_rs_4.to_ulong()] = (wt_5 < MAX_WEIGHT ? ++wt_5 : MAX_WEIGHT);

        int wt_6 = weight_table[5][features.tag_rs_7.to_ulong()];
        weight_table[5][features.tag_rs_7.to_ulong()] = (wt_6 < MAX_WEIGHT ? ++wt_6 : MAX_WEIGHT);
    }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Cache LRU get way and update functions                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

int CACHE_REPLACEMENT_STATE::get_cache_LRU_index(const int index)
{
    int way = 0;
    for (unsigned int i = 0; i < assoc; i++)
    {
        if (repl[index][i].lru.to_ulong() == assoc - 1)
        {
            way = i;
            break;
        }
    }

    return way;
}

void CACHE_REPLACEMENT_STATE::update_cache_LRU_state(const unsigned int index, const unsigned int way)
{
    unsigned int lru_position = repl[index][way].lru.to_ulong();

    for (unsigned int i = 0; i < assoc; i++)
    {
        if (repl[index][i].lru.to_ulong() < lru_position)
            repl[index][i].lru = repl[index][i].lru.to_ulong() + 1;
    }

    repl[index][way].lru = 0;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     LRU get way and update functions                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

int CACHE_REPLACEMENT_STATE::get_LRU_index(const int index)
{
    int way = 0;
    for (int i = 0; i < SAMPLER_ASSOC; i++)
    {
        if (sampler_sets[index][i].lru.to_ulong() == SAMPLER_ASSOC - 1)
        {
            way = i;
            break;
        }
    }

    return way;
}

void CACHE_REPLACEMENT_STATE::update_LRU_state(const int index, const int way)
{
    unsigned int lru_position = sampler_sets[index][way].lru.to_ulong();

    for (int i = 0; i < SAMPLER_ASSOC; i++)
    {
        if (sampler_sets[index][i].lru.to_ulong() < lru_position)
            sampler_sets[index][i].lru = sampler_sets[index][i].lru.to_ulong() + 1;
    }

    sampler_sets[index][way].lru = 0;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Class destructor                                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

CACHE_REPLACEMENT_STATE::~CACHE_REPLACEMENT_STATE(void)
{
}

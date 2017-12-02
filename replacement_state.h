#ifndef REPL_STATE_H
#define REPL_STATE_H

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

#include <cstdlib>
#include <cassert>
#include "utils.h"
#include "crc_cache_defs.h"
#include <iostream>

using namespace std;

// Replacement Policies Supported
typedef enum {
  CRC_REPL_LRU = 0,
  CRC_REPL_RANDOM = 1,
  CRC_REPL_CONTESTANT = 2
} ReplacemntPolicy;

// Replacement State Per Cache Line
typedef struct
{
  UINT32 LRUstackposition;

  // CONTESTANTS: Add extra state per cache line here

  bool prediction;

} LINE_REPLACEMENT_STATE;

struct sampler; // Jimenez's structures

// The implementation for the cache replacement policy
class CACHE_REPLACEMENT_STATE
{
public:
  LINE_REPLACEMENT_STATE **repl;
  sampler *samp;

private:
  void register_prediction(bool mis);

  UINT32 numsets;
  UINT32 assoc;
  UINT32 replPolicy;

  COUNTER mytimer; // tracks # of references to the cache

  // CONTESTANTS:  Add extra state for cache here

  // Jimenez's code

  // sampler data structure

public:
  ostream &PrintStats(ostream &out);

  // The constructor CAN NOT be changed
  CACHE_REPLACEMENT_STATE(UINT32 _sets, UINT32 _assoc, UINT32 _pol);

  INT32 GetVictimInSet(UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType);

  void UpdateReplacementState(UINT32 setIndex, INT32 updateWayID);

  void SetReplacementPolicy(UINT32 _pol) { replPolicy = _pol; }
  void IncrementTimer() { mytimer++; }

  void UpdateReplacementState(UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine,
                              UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit);

  ~CACHE_REPLACEMENT_STATE(void);

private:
  void InitReplacementState();
  INT32 Get_Random_Victim(UINT32 setIndex);

  INT32 Get_LRU_Victim(UINT32 setIndex);
  void UpdateLRU(UINT32 setIndex, INT32 updateWayID);

  // Jimenez's code

  //INT32  Get_Sampler_Victim( UINT32 setIndex );
  INT32 Get_Sampler_Victim(UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType);

  void UpdateSampler(UINT32 setIndex, Addr_t tag, UINT32 tid, Addr_t PC, INT32 updateWayID, bool hit);
};

// Jimenez's sampler code

struct sampler_entry
{
  unsigned int
      lru_stack_position,
      tag,
      trace,
      prediction;

  bool
      valid;

  // constructor for sampler entry

  sampler_entry(void)
  {
    lru_stack_position = 0;
    valid = false;
    tag = 0;
    trace = 0;
    prediction = 0;
  };
};

// one sampler set (just a pointer to the entries)

struct sampler_set
{
  sampler_entry *blocks;

  sampler_set(void);
};

// the dead block predictor

struct predictor
{
  int **tables; // tables of two-bit counters

  predictor(void);
  unsigned int get_table_index(UINT32 tid, unsigned int, int t);
  bool get_prediction(UINT32 tid, unsigned int trace, int set);
  void block_is_dead(UINT32 tid, unsigned int, bool);
};

// the sampler

struct sampler
{
  sampler_set *sets;
  int
      nsampler_sets,   // number of sampler sets
      sampler_modulus; // determines which LLC sets are sampler sets

  predictor *pred;
  sampler(int nsets, int assoc);
  void access(UINT32 tid, int set, Addr_t tag, Addr_t PC);
};
#endif

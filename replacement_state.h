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
#include <bitset>

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
  bool reuse_bit;
  bitset<4> lru;

} LINE_REPLACEMENT_STATE;

struct sampler
{
  bool valid;
  bitset<4> lru;
  Addr_t trace;
  bool reuse;
  bitset<16> partial_tag;
}; // Jimenez's structures

// The implementation for the cache replacement policy
class CACHE_REPLACEMENT_STATE
{
public:
  LINE_REPLACEMENT_STATE **repl;
  sampler **sampler_sets;
  unsigned int **weight_table;

private:
  UINT32 numsets;
  UINT32 assoc;
  UINT32 replPolicy;

  COUNTER mytimer; // tracks # of references to the cache

  // CONTESTANTS:  Add extra state for cache here

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
  INT32 Get_My_Victim(UINT32 setIndex, Addr_t PC, Addr_t paddr);
  void UpdateLRU(UINT32 setIndex, INT32 updateWayID);
  void UpdateMyPolicy(UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine,
                      Addr_t PC, bool cacheHit);

  // utilities
  bitset<16> JSHash(bitset<16> hash, bitset<16> trace);
  bitset<16> *compute_traces(Addr_t PC);

  // prediction and training
  bool predict(const bitset<16> *traces);
  void train(const bitset<16> *traces, bool increment);

  // Cache LRU get and update
  int get_cache_LRU_index(const int index);
  void update_cache_LRU_state(const int unsigned index, const unsigned int way);

  // LRU get and update
  int get_LRU_index(const int index);
  void update_LRU_state(const int index, const int way);
};

#endif

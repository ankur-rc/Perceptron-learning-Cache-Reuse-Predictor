Welcome to the cache replacement and bypass competition for CSCE 614, Fall
2015. You will be implementing a cache replacement and bypass policy in the
"efectiu" infrastructure.

"Efectiu" means "cash" in Catalan, a play on the English pronunciation
of the word "cache" and an allusion to the older cache simulator "Dinero"
which means money in Spanish. You may pronounce it as "affect you."

This infrastructure uses a simple linear performance model to translate
misses to cycles. It is based on the 2010 JILP CRC infrastructure but
replaces the cache simulator with a simple model that only tracks last-level
cache accesses. It is configured to simulate a 4MB last-level cache.

To implement your replacement policy, you may modify only
replacement_state.cpp and replacement_state.h . These files already come
with LRU and random implemented as policies numbers 0 and 1. Your policy
is number two. Modify or replace the declarations and definitions for
GetMyVictim and UpdateMyPolicy in these files, and add whatever other
code you like to implement your replacement and bypass policy. To get the
simulator to use your policy instead of LRU, set the environment variable
DAN_POLICY to 2. For example, in Bourne shell or bash, you would write:

export DAN_POLICY=2; ./efectiu <trace-file-name>.gz

A typical way to approach implementing a cache replacement and bypass policy
would be to modify LINE_REPLACEMENT_STATE to include your per-block metadata,
e.g. prediction bits, counters, or whatever, then put your other state as
fields in CACHE_REPLACEMENT_STATE. Modify or replace GetMyVictim to return
a way number from 0 through 15 giving the block in the set to be replaced,
or -1 if no block should be replace i.e. for bypassing. Modify UpdateMyPolicy
to handle whatever update you need to do when a block is accessed. Right now,
those methods accept minimal information as parameters, but you can modify
their type signatures to accept and of the data available from the calling
methods which are GetVictimInSet and UpdateReplacementState. For example,
you can get the thread ID, set index, address (PC) of the memory access
instruction, whether the access was a hit or miss, and the type of access
e.g. demand read, write, prefetch, writeback, or instruction cache read.

Do not modify any of the other files. We will only evaluate your
replacement_state.cpp and replacement_state.h files.

27 traces from SPEC CPU 2006 have been provided in the "traces"
directory. These traces are the last-level cache accesses for one billion
instructions on a machine with a three-level cache hierarchy where the first
level is 32KB split I+D and, level is a unified 256KB, and the third level
is the 4MB cache you are optimizing. Your goal is to maximize geometric
mean speedup over LRU. That is, for each benchmark compute the IPC with
your technique, divide that by the IPC from LRU to get the speedup, then
take the geometric mean of all the speedups.

For this project, let us not focus on the hardware budget; rather, use
what resources you need to use to implement a reasonable replacement
and bypass policy. Don't try to cheat by implementing extra cache space
(I don't know how you would even do that but don't try).

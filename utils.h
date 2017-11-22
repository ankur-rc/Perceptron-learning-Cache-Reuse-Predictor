#ifndef __UTILS_H
#define __UTILS_H
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef unsigned long long int UINT64;
typedef long long int INT64;
typedef unsigned int UINT32;
typedef int INT32;
typedef unsigned long long int COUNTER;
typedef unsigned long long int Addr_t;

struct LINE_STATE {
	Addr_t tag;
};

#endif

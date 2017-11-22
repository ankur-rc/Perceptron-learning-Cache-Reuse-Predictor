//#define ACCESS_WRITEBACK	6
typedef enum
{   
    ACCESS_IFETCH      = 0,
    ACCESS_LOAD        = 1,
    ACCESS_STORE       = 2,
    ACCESS_UNSUPPORT0  = 3,
    ACCESS_UNSUPPORT1  = 4,
    ACCESS_PREFETCH    = 5,
    ACCESS_WRITEBACK   = 6,
    ACCESS_MAX         = 7
} AccessTypes;

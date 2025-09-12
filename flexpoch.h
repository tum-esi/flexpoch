#ifndef _FLEXPOCH_H
#define _FLEXPOCH_H

// define feature macros for platform specific C functions. Need to be available
#define _XOPEN_SOURCE // function "strptime" is platform specific. 
#define _GNU_SOURCE   // function "timegm" is platform specific.

#include <stdio.h>    // printf
#include <stdint.h>   // for int64_t
#include <stdlib.h>   // strtoul, abs
#include <string.h>
#include <ctype.h>    // isdigit
#include <time.h>
#include <math.h>
#include <stdbool.h>


#define FP_VERSION "1.1"



// types
// ============================================================================
typedef int64_t FP_NumType;

// Flexpoch_Type
typedef enum {
    FMT_ABS_SEC = 0,
    FMT_ABS_YEAR = 1,
    FMT_REL_SEC = 2,
    FMT_REL_FRAC = 3,
    FMT_CUSTOM = 4,
    FMT_LOGICAL = 5,
} FPFormat;

// error codes
typedef enum {
    SUCCESS = 0,
    ERR_INVALID_1ST_BYTE = -1,
    ERR_OUT_OF_RANGE = -2,
    ERR_INVALID_LEAPSECOND = -5,
    ERR_INVALID_PRECISION = -6,
    ERR_NON_ZERO_AFTER_YEAR = -8,
    ERR_INVALID_YEAR = -9,
    ERR_INVALID_ISO = -10,
    ERR_INVALID_OFFSET = -11,
    ERR_OFFSET_AND_LEAPSECOND = -12,
    ERR_INCOMPATIBLE_OUTPUT = -13,
    ERR_INVALID_6TH_BYTE = -106,
    ERR_CUSTOM_FORMAT = -64,
    ERR_RESERVED_FORMAT = -128,
} ErrNo;

typedef enum {
    PRC_YOCTOSEC = -24,
    PRC_NANOSEC  = -9,
    PRC_23BIT  = -7,
    PRC_MICROSEC = -6,
    PRC_15BIT = -5,
    PRC_MILLISEC = -3,
    PRC_SECOND = 0,
    PRC_MINUTE = 1,
    PRC_HOUR   = 2,
    PRC_DAY    = 3,
    PRC_WEEK   = 4,
    PRC_MONTH  = 5,
    PRC_QUATER = 6,
    PRC_TRIMESTER  = 7,
    PRC_SEMESTER   = 8,
    PRC_YEAR       = 9,
    PRC_DECADE     = 10,
    PRC_CENTURY    = 11,
    PRC_MILLENNIUM = 12,
    PRC_UNKNOWN = 99,
} Precision;

typedef struct {
    bool is_dst;
    FPFormat fmt;
    bool is_leapsecond;
    Precision precision;
    float year;
    int64_t seconds;
    uint32_t ns;  
    int32_t tz_offset;
    uint64_t hr_frac;
    int64_t rawdata;
} FP_Components;



// Functions
// ============================================================================

void FP_init(FP_Components *fpc);

// return "empty" FP struct
FP_Components FP_new();


// Input formats
// ----------------------------------------------------------------------------

void FP_from_ts(struct timespec *ts, FP_Components *out);

void FP_from_tm(struct tm *tm, FP_Components *out);

// validate and parse 64 bit flexpoch number. Return negative number if invalid.
ErrNo FP_from_fp(int64_t flexpoch, FP_Components *out) ;

ErrNo FP_from_iso(char *isostr, FP_Components *out);

ErrNo FP_from_unix(int64_t unixtime, FP_Components *out);

ErrNo FP_from_java(int64_t javatime, FP_Components *out);

ErrNo FP_from_logic(int64_t logictime, FP_Components *out);


// Output formats
// ----------------------------------------------------------------------------

ErrNo FP_to_fp(FP_Components *fpc, FP_NumType* out);

ErrNo FP_to_unix(FP_Components *fpc, int64_t* out);

ErrNo FP_to_java(FP_Components *fpc, int64_t* out);

ErrNo FP_to_logic(FP_Components *fpc, int64_t* out);

ErrNo FP_to_iso(FP_Components *fpc, char *out);


// Helper functions
// ----------------------------------------------------------------------------

// Function to out the individual Flexpoch components to stdout
void FP_print_components(FP_Components *fpc);

bool FP_is_year(FP_NumType flexpoch);

bool FP_is_sec(FP_NumType flexpoch);

uint64_t FP_ns_to_precision(uint64_t ns, Precision prc);

void FP_precision_name(Precision prc, char *out);


#endif // _FLEXPOCH_H
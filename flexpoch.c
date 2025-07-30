#include "flexpoch.h"

#define CP_FLOAT_SIGN 0x0000000080000000
#define CP_UNDEFINED_FP -0x8000000000000000
#define NS_PER_SEC 1000000000

#define FP_YEAR_MAX 19254
#define FP_YEAR_MIN -2250

#define TZ_BIN_OFFSET 1024    // binary offset
#define TZ_LEAPSEC 1023    // special offset value for leapsecond

// Constants
// ============================================================================

// codepoint (CP) bytes
const int8_t CP_ABS_YEAR_POS = 0x7F;  // codepoint absolute years
const int8_t CP_ABS_YEAR_NEG = -0x20; // codepoint absolute years: -0x20 or 0xE0
const int8_t CP_REL_SEC = 0b1101;   // 4-bit codepoint relative time 0xD0
const int8_t CP_REL_FRAC = 0b1100;   // 4-bit codepoint relative high-res fraction 0xC0
const int8_t CP_CUSTOM   = 0b1011;   // 4-bit codepoint for custom code 0xB0
const int8_t CP_LOGICAL  = 0b1010;   // 4-bit codepoint for logical clocks 0xA0
const int8_t CP_RESERVED = 0b100;    // 3-bit codepoint for reserved (0x8 or 0x9)


// Functions
// ============================================================================

void FP_init(FP_Components *fpc){
    fpc->is_dst = false;
    fpc->is_leapsecond = false;
    fpc->precision = PRC_UNKNOWN;
    fpc->seconds = 0;
    fpc->tz_offset = 0;
    fpc->year = 0.0;
    fpc->ns = -1;
    fpc->rawdata = CP_UNDEFINED_FP;
}

// return "empty" FP struct
FP_Components FP_new(){
    FP_Components fpc = {0};
    FP_init(&fpc);
    return fpc;
}



uint32_t ns2frac(uint32_t nanoseconds) {
    uint64_t temp = (uint64_t)((uint64_t)(nanoseconds) * 1000000000 + 59604644775) / 119209289551;
    return (uint32_t) temp;
}

// convert fraction to ns. This only works for small fraction (e.g. 20bit)
uint32_t frac2ns(uint64_t binary) {
    uint64_t temp = (uint64_t)((binary>>1) * 119209289551 + 500000000) / 1000000000;  // 2^64 ~ 10^19. add 0.5e9 for correct rounding
    return (uint32_t)(temp);
}

int16_t FP_tz_offset_to_bin(int16_t tz_offset){
    tz_offset = tz_offset & 0x7FF;
    return tz_offset ^ 1<<10;
}

int16_t FP_tz_offset_from_bin(int16_t tz_code){
    tz_code = tz_code & 0x7FF; // strip to 11 bit
    tz_code ^= (1<<10); // flip bit at index 10 (11th bit)
    tz_code += 0xF800 * (tz_code>>10); // set all preceding bits if 11th is set
    return tz_code;
}

// Input formats
// ----------------------------------------------------------------------------

// validate and parse 64 bit flexpoch number. Return negative number if invalid.
ErrNo FP_from_fp(int64_t flexpoch, FP_Components *out) {
    int8_t first_byte = (flexpoch >> 56) & 0xFF;
    out->rawdata = flexpoch;

    if (first_byte == CP_ABS_YEAR_NEG || first_byte == CP_ABS_YEAR_POS) {
        bool is_positive = (first_byte == CP_ABS_YEAR_POS);
        if((flexpoch & 0xFFFFFF) != 0){ return ERR_NON_ZERO_AFTER_YEAR; }
        out->fmt = FMT_ABS_YEAR;
        uint32_t floatbits = (uint32_t)(flexpoch >> 24); // parse next 4B as float
        out->year = *((float*)&floatbits); // cast to float
        out->tz_offset = 0;
        if( ( is_positive && (out->year < (float)FP_YEAR_MAX+1)) ||
            (!is_positive && (out->year > (float)FP_YEAR_MIN-1)) ){
            return ERR_INVALID_YEAR;  // wrong range
        } 
    } else if ((int8_t)(CP_REL_SEC<<4) <= first_byte && first_byte < CP_ABS_YEAR_POS) {
        uint64_t fraction = 0;
        out->fmt = FMT_ABS_SEC;
        out->seconds = flexpoch >> 24;  // do not add mask to preserve sign!
        if (!(flexpoch & 0b1)) {
            fraction = (flexpoch & 0xFFFFFE); // full precision
            out->precision = PRC_23BIT;
        } else if ((flexpoch & 0b111) == 0b001){ 
            out->precision = PRC_MICROSEC; // 20bit
            out->tz_offset = 0;
            fraction = (flexpoch & 0xFFFFF0);
        }  else if ((flexpoch & 0b111) == 0b011){ 
            out->precision = PRC_15BIT; // 15bit, RTC
            out->tz_offset = 0;
            fraction = (flexpoch & 0xFFFE00);
        }  else if ((flexpoch & 0b111) == 0b101){ 
            out->precision = PRC_MILLISEC; // 10bit
            fraction = (flexpoch & 0xFFC000);
            out->tz_offset = FP_tz_offset_from_bin(flexpoch >> 3);
        }  else if ((flexpoch & 0b111) == 0b111){   // sec or above sec precision
            out->tz_offset = FP_tz_offset_from_bin(flexpoch >> 13);
            out->precision = (flexpoch & 0x78) >> 3;
            if(out->precision > 12){
                return ERR_INVALID_PRECISION;
            }
        }
        if(out->tz_offset == TZ_LEAPSEC){
            out->is_leapsecond = true;
            out->tz_offset = 0;
        }
        if (((first_byte >> 4) & 0xF) == CP_REL_SEC){
            out->seconds = out->seconds & 0x0FFFFFFFFF;
            out->fmt = FMT_REL_SEC;
        }
        // printf("precision %i", out->precision);
        out->ns = frac2ns(fraction);
    } else if (((first_byte >> 4) & 0xF) == CP_REL_FRAC){
        out->fmt = FMT_REL_FRAC;
        out->hr_frac = flexpoch << 4;
        printf("WARNING: High-res relative fractions currently not implemented!");
        return ERR_RESERVED_FORMAT;
    } else if (((first_byte >> 4) & 0xF) == CP_CUSTOM){
        out->fmt = FMT_CUSTOM;
        return ERR_CUSTOM_FORMAT;
    } else if (((first_byte >> 4) & 0xF) == CP_LOGICAL){
        out->fmt = FMT_LOGICAL;
        out->seconds = flexpoch & 0x0FFFFFFFFFFFFFFF;
    } else if (((first_byte >> 5) & 0x7) == CP_RESERVED){
        return ERR_RESERVED_FORMAT;
    } else {
        // this should not happen
        return ERR_INVALID_1ST_BYTE;
    }
    return SUCCESS;
}


void FP_from_ts(struct timespec *ts, FP_Components *out){
    out->seconds = ts->tv_sec;
    out->ns = ts->tv_nsec;
    out->precision = PRC_MILLISEC;
}

void FP_from_tm(struct tm *tm, FP_Components *out){
    // printf("tm: %i-%i-%i %i:%i:%i s, dst:%i", tm->tm_year, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_isdst);
    out->seconds = timegm(tm);
    out->is_dst = tm->tm_isdst;
    out->precision = PRC_SECOND;
};

ErrNo FP_from_iso(char *isostr, FP_Components *out){
    struct tm tm = {0};
    tm.tm_mday = 1;  // important! Otherwise strptime can fail

    const char *formats[] = {
        "%Y-%m-%dT%H:%M:%S.%f%z",
        "%Y-%m-%dT%H:%M:%S.%fZ",
        "%Y-%m-%dT%H:%M:%S.%f",
        "%Y-%m-%dT%H:%M:%S%z",
        "%Y-%m-%dT%H:%M:%SZ",
        "%Y-%m-%dT%H:%M:%S",
        "%Y-%m-%dT%H:%M",
        "%Y-%m-%dT%H",
        "%Y-%m-%d",
        "%Y-W%W",
        "%Y-%m",
        "%Y",
    };

    
    size_t num_formats = 12;
    bool is_match = false;
    
    for (size_t i = 0; i < num_formats; i++) {
        tm.tm_mday = 1; 
        if (strptime(isostr, formats[i], &tm) != NULL) {
            tm.tm_isdst = 0; // ensure that tm is interpreted as UTC!
            if (strlen(isostr) >= 19 && (strncmp(&isostr[17], "60", 2) == 0)){
                out->is_leapsecond = true;
                tm.tm_sec = 59;
            }
            FP_from_tm(&tm, out);
            is_match = true;
            if(i == 11){ 
                out->precision = PRC_YEAR;
            } else if (i == 3 || i == 4) {
                out->precision = PRC_SECOND;

            } else {
                out->precision = i - 5;
            }
            break;
        }
    }

    if (!is_match){
        return ERR_INVALID_ISO;
    }

    // Parse timezone offset if present
    char *tz_sign = strrchr(isostr, '+');
    if (!tz_sign && strlen(isostr) > 16) {
        tz_sign = strrchr(isostr + 16, '-');
    }

    // Find the position of the subsecond part
    const char *dot = strchr(isostr, '.');
    int numsubsdigits = 0;
    if (dot) {
        while (*(dot+1+numsubsdigits) && isdigit(*(dot+1+numsubsdigits))){
            numsubsdigits++;
        }
        char subseconds[10];
        strncpy(subseconds, dot + 1, numsubsdigits);
        subseconds[numsubsdigits] = '\0'; // Null-terminate the string
        if(numsubsdigits == 0){ 
            out->precision = PRC_SECOND; 
        } else if(numsubsdigits <= 3){ 
            out->precision = PRC_MILLISEC; 
            out->ns = atoi(subseconds) * 1000000; 
        } else if(numsubsdigits <= 6){ 
            out->precision = PRC_MICROSEC; 
            out->ns = atoi(subseconds) * 1000; 
        } else if(numsubsdigits <= 9){ 
            out->precision = PRC_NANOSEC; 
            out->ns = atoi(subseconds); 
        }
    }


    if (tz_sign) {
        int tz_hour = 0, tz_min = 0;
        sscanf(tz_sign, "%c%d:%d", tz_sign, &tz_hour, &tz_min);
        int offset = (tz_hour * 60 + tz_min) * (*tz_sign == '+' ? 1 : -1);
        out->tz_offset = offset;
        out->seconds -= offset*60;
    }

    return SUCCESS; 
};


ErrNo FP_from_unix(int64_t unixtime, FP_Components *out){
    if(unixtime <= (int64_t)CP_ABS_YEAR_NEG<<32 || (int64_t)CP_ABS_YEAR_POS<<32 <= unixtime ){
        return ERR_OUT_OF_RANGE;
    }
    out->seconds = unixtime;
    out->ns = 0;
    out->is_leapsecond = 0;
    out->precision = PRC_SECOND;
    return FP_to_fp(out, &(out->rawdata));
};

ErrNo FP_from_logic(int64_t logictime, FP_Components *out){
    if(logictime <= (int64_t)-1<<60 || (int64_t)1<<60 < logictime ){
        return ERR_OUT_OF_RANGE;
    }
    out->seconds = logictime;
    out->fmt = FMT_LOGICAL;
    return FP_to_fp(out, &(out->rawdata));
};

// output formats
// ----------------------------------------------------------------------------


ErrNo FP_to_fp(FP_Components *fpc, FP_NumType* out){
    if(fpc->tz_offset < -1020 || 1020 < fpc->tz_offset){ // actually -1024 .. 1022 but reserve
        return ERR_INVALID_OFFSET;
    }
    if (fpc->is_leapsecond && fpc->tz_offset){
        return ERR_OFFSET_AND_LEAPSECOND;
    }
    if(fpc->fmt == FMT_LOGICAL){ 
        *out = fpc->seconds + ((uint64_t)CP_LOGICAL<<60);
        return SUCCESS;
    }
    if(fpc->year != 0 && fpclassify(fpc->year) == FP_NORMAL) { 
        uint32_t lower32 = *((uint32_t*)&(fpc->year));
        if(fpc->year < 0.0){*out = ((int64_t)CP_ABS_YEAR_NEG << 56) + lower32;}
        if(fpc->year > 0.0){*out = ((int64_t)CP_ABS_YEAR_POS << 56) + lower32;}
    }
    *out = fpc->seconds << 24;
    int16_t tz_value = fpc->is_leapsecond? TZ_LEAPSEC : fpc->tz_offset;
    if (fpc->precision >= 0) {
        *out += ((fpc->precision & 0xF)<<3);
        *out += (FP_tz_offset_to_bin(tz_value)<<13); 
        *out += 0b111; // set sec+ precision
    } else if (fpc->precision == PRC_MILLISEC) {
        *out += (ns2frac(fpc->ns) >> 13) << 14; 
        *out += (FP_tz_offset_to_bin(tz_value)<<3);
        *out += 0b101; // set ms precision bits
    } else if (fpc->precision == PRC_15BIT) {
        *out += (ns2frac(fpc->ns) >> 8) << 9;  
        *out += 0b011;
    } else if (fpc->precision == PRC_MICROSEC) {
        *out += (ns2frac(fpc->ns) >> 3) << 4;  
        *out += 0b001;
    } else if (fpc->precision == PRC_23BIT || fpc->precision == PRC_NANOSEC) {
        *out += (ns2frac(fpc->ns)) << 1;  
        *out += 0b0;
    } else {
        return ERR_INVALID_PRECISION;
    }

    return SUCCESS;
}

ErrNo FP_to_unix(FP_Components *fpc, int64_t* out){
    *out = fpc->seconds;
    if (fpc->is_leapsecond){ return -1; }
    if (fpc->tz_offset){ return -1; }
    return fpc->precision; // as "error" for precision loss
}

ErrNo FP_to_logic(FP_Components *fpc, int64_t* out){
    *out = fpc->seconds;
    if (fpc->fmt != FMT_LOGICAL){
        return ERR_INCOMPATIBLE_OUTPUT;
    }
    return SUCCESS;
}


// Convert FP_Components to ISO date time format string
ErrNo FP_to_iso(FP_Components *fpc, char *out) {
    if (fpc->year != 0){
        sprintf(out, "%.1f", fpc->year);
        return SUCCESS;
    }
    time_t rawtime = fpc->seconds + fpc->tz_offset*60;
    int idx = 0;
    if (fpc->fmt == FMT_REL_SEC){
        if(fpc->precision <= PRC_SECOND){
            idx = sprintf(out+idx, "PT%lu", fpc->seconds);
            uint32_t decimals = FP_ns_to_precision(fpc->ns, fpc->precision);
            switch(fpc->precision){
                case PRC_NANOSEC: ;;
                case PRC_23BIT: sprintf(out+idx, ".%09uS", decimals); break;
                case PRC_MICROSEC: ;;
                case PRC_15BIT: sprintf(out+idx, ".%06uS", decimals); break;
                case PRC_MILLISEC: sprintf(out+idx, ".%03uS", decimals); break;
                case PRC_SECOND: sprintf(out+idx, "S"); break;
                default: return ERR_INVALID_PRECISION;
            }
        } else {
            uint64_t value = FP_ns_to_precision(fpc->seconds*NS_PER_SEC, fpc->precision);
            char unit[3];
            FP_precision_name(fpc->precision, unit);
            if(fpc->precision <= PRC_HOUR){
                sprintf(out+idx, "PT%lu%s", value, unit);
            } else {
                sprintf(out+idx, "P%lu%s", value, unit);
            }
        }
        return SUCCESS;
    }
    struct tm *tm = gmtime(&rawtime); 
    if (fpc->is_leapsecond && tm->tm_hour == 23 && tm->tm_min == 59 && tm->tm_sec == 59){
        tm->tm_sec += 1;
    }
    if (fpc->precision == PRC_MILLENNIUM){  
        idx += sprintf(out+idx, "%01dxxx", (tm->tm_year + 1900)/1000);
    }
    if (fpc->precision == PRC_CENTURY){  
        idx += sprintf(out+idx, "%02dxx", (tm->tm_year + 1900)/100);
    }
    if (fpc->precision == PRC_DECADE){  
        idx += sprintf(out+idx, "%03dx", (tm->tm_year + 1900)/10);
    }
    if (fpc->precision <= PRC_YEAR){  
        idx += sprintf(out+idx, "%04d", tm->tm_year + 1900);
    }
    if (fpc->precision == PRC_QUATER){ 
        idx += sprintf(out+idx, "-Q%d", (tm->tm_mon+1) / 4 + 1);
    }
    if (fpc->precision <= PRC_MONTH && !(fpc->precision == PRC_WEEK)){ 
        idx += sprintf(out+idx, "-%02d", tm->tm_mon + 1);
    }
    if (fpc->precision == PRC_WEEK){ 
        idx += strftime(out+idx, 5, "-W%W", tm);
    }
    if (fpc->precision <= PRC_DAY){  
        idx += sprintf(out+idx, "-%02d", tm->tm_mday);
    }
    if (fpc->precision <= PRC_HOUR){  
        idx += sprintf(out+idx, "T%02d", tm->tm_hour);
    }
    if (fpc->precision <= PRC_MINUTE){  
        idx += sprintf(out+idx, ":%02d", tm->tm_min);
    }
    if (fpc->precision <= PRC_SECOND){  
        idx += sprintf(out+idx, ":%02d", tm->tm_sec);
    }
    if (fpc->precision == PRC_MILLISEC){ 
        idx += sprintf(out+idx, ".%03u", (fpc->ns + 500000) / 1000000);
    }
    if (fpc->precision == PRC_MICROSEC || fpc->precision == PRC_15BIT ){ 
        idx += sprintf(out+idx, ".%06u", (fpc->ns + 500) / 1000);
    }
    if (fpc->precision == PRC_NANOSEC || fpc->precision == PRC_23BIT){ 
        idx += sprintf(out+idx, ".%09u", fpc->ns);
    }
    if (fpc->tz_offset != 0) {
        int hours = abs(fpc->tz_offset) / 60;
        int minutes = (abs(fpc->tz_offset) % 60);
        idx += sprintf(out+idx, "%c%02d:%02d", fpc->tz_offset < 0 ? '-' : '+', hours, minutes);
    }
    if (fpc->tz_offset == 0 && fpc->precision <= PRC_HOUR){
        idx += sprintf(out+idx, "Z");
    }
    if (fpc->is_dst) {
        idx += sprintf(out+idx, " DST");
    }
    return SUCCESS;
}



// Function to write the individual Flexpoch components to stdout
void FP_print_components(FP_Components *fpc) {
    if(fpc->rawdata == CP_UNDEFINED_FP){
        FP_to_fp(fpc, &(fpc->rawdata));
    }
    printf("\nInternal Representation:\nFP=");
    if (fpclassify(fpc->year) == FP_NORMAL) {
        printf("%02lX.%06lX.%08lX", fpc->rawdata >> 56, (fpc->rawdata >> 32) & 0xFFFFFF, fpc->rawdata & 0xFFFFFFFF);
        printf(", T=%f y", fpc->year);
        printf("\n   -+ ---+-- ----+---\n    |    |       +----> float: %f\n    |    +------------> ignored\n    +-----------------> '7F|E0' YEAR_MARKER",  fpc->year);
    } else {
        char prcname[5] = "no\0";
        FP_precision_name(fpc->precision, prcname);
        printf("%010lX", (fpc->rawdata >> 24) & 0xFFFFFFFFFF);
        printf(".%06lX", fpc->rawdata & 0xFFFFFF);
        printf(", T=");
        printf("%li s, %i ns", fpc->seconds, fpc->ns);
        printf(" (PRC:%i='%s'%s%s)", 
            fpc->precision, prcname, fpc->is_leapsecond ? ",LS" : "", fpc->is_dst ? ",DST" : "");
        if(fpc->tz_offset){ printf(", tz: %d min", fpc->tz_offset); }

        printf("\n   -----+---- -+-  |\n        |      |   +-> ");
            printf("subsec precision: 0x%01lX means P=%i=%s", fpc->rawdata & 0xF, fpc->precision, prcname);
        printf("\n        |      +-----> ");
        if(fpc->tz_offset){
            printf("tz_offset: 0x%05X -> %i", fpc->tz_offset, fpc->tz_offset);
        } else {
            if(fpc->precision == PRC_23BIT){
                printf("2^-23 s: 0x%06lX = %u ns", (fpc->rawdata & 0xFFFFFE), fpc->ns);
            } else if(fpc->precision == PRC_MICROSEC){
                printf("microsec: 0x%05lX = %u us", (fpc->rawdata & 0xFFFFF0) >> 4, fpc->ns / 1000);
            } else if(fpc->precision == PRC_MILLISEC){
                printf("millisec: 0x%03lX = %u ms", (fpc->rawdata & 0xFFC000) >> 12, fpc->ns / 1000000);
            } else if (fpc->precision >= PRC_SECOND){
                printf("0x%01lX is lowres precision:P=%u=%s", (fpc->rawdata & 0x0F0000) >> 16, fpc->precision, prcname);
            }
        }

        printf("\n        +------------> seconds: %li", fpc->seconds);
    }
    printf("\n\n");
}


bool FP_is_year(FP_NumType flexpoch){
    return ((((flexpoch >> 56) & 0xFF) == CP_ABS_YEAR_POS) ||
            (((flexpoch >> 56) & 0xFF) == CP_ABS_YEAR_NEG));
}

bool FP_is_sec(FP_NumType flexpoch){
    return (((int64_t)CP_ABS_YEAR_NEG)<<56 < flexpoch && 
            flexpoch < ((int64_t)CP_ABS_YEAR_POS)<<56);
}


uint64_t FP_ns_to_precision(uint64_t ns, Precision prc){
    uint64_t frac = ((uint64_t)(ns) * 1000000000 + 59604644775) / 119209289551;
    switch (prc) {
        case PRC_NANOSEC:  return ns;
        case PRC_MICROSEC: return ns / 1000;
        case PRC_MILLISEC: return ns / 1000000;
        case PRC_SECOND:   return ns / NS_PER_SEC;
        case PRC_MINUTE:   return ns / NS_PER_SEC / 60;
        case PRC_HOUR:     return ns / NS_PER_SEC / 60 / 60;
        case PRC_DAY:      return ns / NS_PER_SEC / 60 / 60 / 24;
        case PRC_WEEK:     return ns / NS_PER_SEC / 60 / 60 / 24 / 7;
        case PRC_MONTH:    return ns / NS_PER_SEC / 60 / 60 / 24 / 30;
        case PRC_QUATER:   return ns / NS_PER_SEC / 60 / 60 / 24;
        case PRC_YEAR:     return ns / NS_PER_SEC / 60 / 60 / 24 * 100 / 36525;
        case PRC_23BIT:    return frac;
        case PRC_15BIT:    return frac >> 8;
        default: return -1;
    }
}

void FP_precision_name(Precision prc, char *out){
    switch (prc) {
        case PRC_NANOSEC: strcpy(out, "ns"); break;
        case PRC_23BIT: strcpy(out, "23b"); break;
        case PRC_MICROSEC: strcpy(out, "us"); break;
        case PRC_15BIT: strcpy(out, "15b"); break;
        case PRC_MILLISEC: strcpy(out, "ms"); break;
        case PRC_SECOND: strcpy(out, "S"); break;
        case PRC_MINUTE: strcpy(out, "M"); break;  // ambigious with month! ISO requires "T" before
        case PRC_HOUR: strcpy(out, "H"); break;
        case PRC_DAY: strcpy(out, "D"); break;
        case PRC_WEEK: strcpy(out, "W"); break;
        case PRC_MONTH: strcpy(out, "M"); break;
        case PRC_QUATER: strcpy(out, "Q"); break;
        case PRC_YEAR: strcpy(out, "Y"); break;
        case PRC_DECADE: strcpy(out, "Dec"); break;
        case PRC_CENTURY: strcpy(out, "Cen"); break;
        case PRC_MILLENNIUM: strcpy(out, "Mil"); break;
        default: strcpy(out, "N/A"); break;
    }
}
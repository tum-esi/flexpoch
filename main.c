#include <stdio.h>
#include <stdbool.h>
#include <string.h> // Include this header for memcpy
#include <ctype.h> // isxdigit
#include <locale.h>  // set locale to UTF-8

#include "flexpoch.h"

#define ARG_FROM_ISO "--from-iso"
#define ARG_FROM_FP "--from-fp"
#define ARG_FROM_UNIX "--from-unix"
#define ARG_FROM_LOGICAL "--from-logical"
#define ARG_TO_ISO "--to-iso"
#define ARG_TO_FP "--to-fp"
#define ARG_TO_UNIX "--to-unix"
#define ARG_JSON "--json"
#define ARG_VERBOSE "--verbose"
#define ARG_HELP "--help"
#define ARG_VERSION "--version"

typedef enum {
    UNKNOWN = -1,
    FP  = 0,
    ISO = 1,
    UNIX = 2,
    LOGICAL = 3,
} TimeFormat;

void print_version(){
    printf("Flexpoch v%s\n", FP_VERSION);
}

void print_usage(){
    print_version();
    printf("\nUsage: \"fp --from-FMT VALUE --to-FMT (--json|--help|--verbose)\"\n");
    printf("  with FMT = iso|unix|fp\n\n");
    printf("Examples:\n");
    printf("fp 0xXXXXXXXXXXXXXXXX            // fp hex  -> ISO str\n");
    printf("fp 0xXXXXXXXXXXXXXXXX --to-unix  // fp hex  -> unix int\n");
    printf("fp --from-iso ISO_STR            // ISO str -> fp hex\n");
}

int get_current_time(FP_Components* fpc){
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0){
    // if (timespec_get(&ts, TIME_UTC) == NULL){
        FP_from_ts(&ts, fpc);
    } else {
        return -1;
    }

    // offset
    time_t now;
    struct tm local_tm;
    time(&now);
    localtime_r(&now, &local_tm);
    fpc->tz_offset = local_tm.tm_gmtoff / 60;
    return 0;
}


bool is_hex_str(const char *str) {
    while (*str) { 
        if (!isxdigit(*str)) { return false; }
        str++;
    }
    return true; 
}

int try_parse_fp_hex(char *argstr, int64_t* fp){
    size_t len=strlen(argstr);
    char* hexstr = NULL;
    if (len > 2 && strncmp(argstr, "0x", 2) == 0){
        hexstr=argstr+2;
    } else {
        hexstr=argstr;
    }
    len=strlen(hexstr);
    if (len == 16 && is_hex_str(hexstr)){
        *fp = strtoul(hexstr, NULL, 16);
        return 0;
    } else {
        return -1;
    }
    return 0;
} 

int try_parse_unix(char *argstr, int64_t *unixtime){
    char *endptr;
    *unixtime = strtol(argstr, &endptr, 10);
    if(*endptr != '\0'){ 
        return -1; 
    } else {
        return 0;
    }
}


int guess_single_arg(char *argstr, TimeFormat *infmt, TimeFormat *outfmt){
    size_t len=strlen(argstr);
    int64_t unixtime;
    if (len == 18 && strncmp(argstr, "0x", 2) == 0){
        *infmt = FP;
        if(*outfmt == UNKNOWN){ *outfmt = ISO; }
        if(argstr[2] == 'A'){ *outfmt = LOGICAL; }
    } else if (len > 5 && (argstr[4] == '-')){
        *infmt = ISO;
        if(*outfmt == UNKNOWN){ *outfmt = FP; }
    } else if (try_parse_unix(argstr, &unixtime) == 0){
        *infmt = UNIX;
        if(*outfmt == UNKNOWN){ *outfmt = FP; }
    } else {
        printf("Argument does not match any known format: %s\n", argstr);
    }
    return 0;
}



int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");  // UTF-8

    TimeFormat infmt = UNKNOWN;
    TimeFormat outfmt = UNKNOWN;
    bool is_json_out = false;
    bool is_verbose = false;
    int payload_arg_idx = 0;
    int error = 0;
    FP_Components fpc = FP_new();

    // parse arguements
    for (int i = 1; i < argc; i++){ // Start from 1 to skip the program name
        if (strncmp(argv[i], ARG_FROM_FP, strlen(ARG_FROM_FP)) == 0){
            infmt = FP;
        } else if (strncmp(argv[i], ARG_FROM_ISO, strlen(ARG_FROM_ISO)) == 0){
            infmt = ISO;
        } else if (strncmp(argv[i], ARG_FROM_UNIX, strlen(ARG_FROM_UNIX)) == 0){
            infmt = UNIX;
        } else if (strncmp(argv[i], ARG_FROM_LOGICAL, strlen(ARG_FROM_LOGICAL)) == 0){
            infmt = LOGICAL;
        } else if (strncmp(argv[i], ARG_TO_FP, strlen(ARG_TO_FP)) == 0){
            outfmt = FP;
        } else if (strncmp(argv[i], ARG_TO_ISO, strlen(ARG_TO_ISO)) == 0){
            outfmt = ISO;
        } else if (strncmp(argv[i], ARG_TO_UNIX, strlen(ARG_TO_UNIX)) == 0){
            outfmt = UNIX;
        } else if (strncmp(argv[i], ARG_JSON, strlen(ARG_JSON)) == 0){
            is_json_out = true;
        } else if (strncmp(argv[i], ARG_VERBOSE, strlen(ARG_VERBOSE)) == 0){
            is_verbose = true;
        } else if (strncmp(argv[i], ARG_HELP, strlen(ARG_HELP)) == 0){
            print_usage();
            return 0;
        } else if (strncmp(argv[i], ARG_VERSION, strlen(ARG_VERSION)) == 0){
            print_version();
            return 0;
        } else {  // payload argument, could be number or ISO time string
            payload_arg_idx = i;
        }
    }

    if(is_verbose){ print_version(); }

    if(payload_arg_idx){
        if(infmt == UNKNOWN){
            if(is_verbose){ printf("No in-format specified, guessing...\n"); }
            guess_single_arg(argv[payload_arg_idx], &infmt, &outfmt);
        }

        switch(infmt){
            case ISO: 
                if(is_verbose){ printf("in-format=ISO\n"); }
                error = FP_from_iso(argv[payload_arg_idx], &fpc);
                if (error == 0) {
                    if (outfmt == ISO || outfmt == UNKNOWN){ outfmt = FP; }
                } else {
                    printf("Unable to parse ISO sring %s", argv[payload_arg_idx]);
                }
                break;
            case FP:
                if(is_verbose){ printf("in-format=Flexpoch\n"); }
                int64_t flexpoch = 0;
                if (try_parse_fp_hex(argv[payload_arg_idx], &flexpoch) == 0) {
                    error = FP_from_fp(flexpoch, &fpc);
                } else {
                    printf("Unable to parse FP hex!");
                }
                break;               
            case UNIX:
                if(is_verbose){ printf("in-format=UNIX\n"); }
                if (outfmt == UNKNOWN){ outfmt = FP; }
                int64_t unixtime;
                if (try_parse_unix(argv[payload_arg_idx], &unixtime) == 0) {
                    error = FP_from_unix(unixtime, &fpc);
                } else {
                    printf("Unable to parse integer time: %s!", argv[payload_arg_idx]);
                }
                break;
            case LOGICAL:
                if(is_verbose){ printf("in-format=LOGICAL\n"); }
                if(outfmt == UNKNOWN){ outfmt = FP; }
                int64_t logictime;
                if (try_parse_unix(argv[payload_arg_idx], &logictime) == 0) {
                    error = FP_from_logic(logictime, &fpc);
                } else {
                    printf("Unable to parse integer time: %s!", argv[payload_arg_idx]);
                }
                break;
            default:
                printf("Unknown in-FMT: \"%s\"", argv[payload_arg_idx]);
                break;
        }
    } else {
        if(is_verbose){ printf("No value for time format provided. Using system time.\n"); }
        get_current_time(&fpc);
        outfmt = FP;
    }


    if(is_verbose){
        FP_print_components(&fpc);
    }

    switch(outfmt){
        case ISO:
            if(is_verbose){ printf("out-format=ISO\n"); }
            if(error){ break; }
            char iso_string[50];
            FP_to_iso(&fpc, iso_string);
            if(is_json_out){
                printf("{\"iso_time\": \"%s\"}\n", iso_string);
            } else {
                printf("%s\n", iso_string);
            }
            break;
        case FP:
            if(is_verbose){ printf("out-format=Flexpoch\n"); }
            if(error){ break; }
            int64_t fp;
            error = FP_to_fp(&fpc, &fp);
            if(error){ break; }
            if(is_json_out){
                printf("{\"fp_time\": \"0x%016lX\"}\n", fp);
            } else {
                printf("0x%016lX\n", fp);
            }
            break;
        case UNIX:
            if(is_verbose){ printf("out-format=UNIX\n"); }
            if(error){ break; }
            int64_t unixtime = 0;
            FP_to_unix(&fpc, &unixtime);
            if(is_json_out){
                printf("\"unix_time\": %li\n", unixtime);
            } else {
                printf("%li\n", unixtime);
            }
            break;
        case LOGICAL:
            if(is_verbose){ printf("out-format=LOGICAL\n"); }
            if(error){ break; }
            int64_t logictime = 0;
            FP_to_logic(&fpc, &logictime);
            if(is_json_out){
                printf("\"logic_time\": %li\n", logictime);
            } else {
                printf("%li\n", logictime);
            }
            break;
        default:
            printf("Unknown output-format");
            break;
    }

    if(error){ printf("Error! "); }

    switch(error){
        case 0: return 0; break;
        case ERR_INVALID_ISO: printf("ISO Error.\n"); break;
        case ERR_INVALID_OFFSET: printf("Timezone offset outside of allowed range (-17:00 .. +17:00).\n"); break;
        case ERR_OUT_OF_RANGE: printf("Value outside of encodable time range.\n"); break;
        case ERR_INVALID_YEAR: printf("Invalid Floating Point Year.\n"); break;
        case ERR_INVALID_PRECISION: printf("Precision Error.\n"); break;
        case ERR_OFFSET_AND_LEAPSECOND: printf("Timezone offset and Leapsecond cannot be encoded at the same time.\n"); break;
        case ERR_CUSTOM_FORMAT: printf("Custom codepoint range (start = 0xB). Please decode with custom decoder.\n"); break;
        case ERR_RESERVED_FORMAT: printf("Reserved codepoint range (start = 0x8 or 0x9). Not supported by this version.\n"); break;
        default:
            printf("Unkown Error: %i.\n", error);
    }

    return error;
}
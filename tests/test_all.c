#include <stdio.h>

#include <stdbool.h>
#include <string.h> // Include this header for memcpy
#include <stdlib.h> // strtoul
#include <ctype.h> // isxdigit
#include <locale.h>  // set locale to UTF-8

#include "flexpoch.h"
#include "prf.h"
#include "tests.h"

#define CFG_ROUNDS_PER_CODE 100
PRF_Profile prf;

void test_value(int64_t flexpoch){
    FP_Components fpc;
    PRF_reset(&prf);
    int result;

    for (int i = 0; i < CFG_ROUNDS_PER_CODE; i++){
        FP_init(&fpc);
        PRF_start(&prf);
        result = FP_from_fp(flexpoch, &fpc);
        PRF_stop(&prf);
    }
    if(result != 0){
        printf("Error: %i", result);
    }
}

void test_performance(){
    int64_t flexpoch;
#if defined(__aarch64__) || defined(_M_ARM64)
    printf("Running on ARM\n");
    enable_cycle_counter();
#endif
    for (size_t i = 0; i < sizeof(TEST_VALUES_POS)/8; i++) {
        flexpoch = TEST_VALUES_POS[i];
        test_value(flexpoch);
        printf("\n------------------\nTesting FP=%016lX: ", flexpoch);
        PRF_print("fp", &prf);
    }

    printf("\n\n----\nTesting negative examples (should fail)\n----\n");
    for (size_t i = 0; i < sizeof(TEST_VALUES_NEG)/8; i++) {
        flexpoch = TEST_VALUES_NEG[i];
        test_value(flexpoch);
        printf("\n------------------\nTesting FP=%016lX: ", flexpoch);
        PRF_print("fp", &prf);
    }
}

int main(int argc, char *argv[]) {
    // run_tests();
    test_performance();
    return 0;
}

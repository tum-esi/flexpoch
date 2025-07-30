
int64_t TEST_VALUES_POS[] = {
    0x0067E66C32000000, // 2025-03-28T09:30:26.000000000Z
    0x005F7AFF4F2AAAA8, // 2020-10-05T11:11:11.166666508Z
    0x005F7AFF4F5BBBB1, // us precision
    0x005F7AFF4F400003, // 15bit precision
    0x005F7AFF4F802005, // ms precision
    0x005F7AFF4F8021E5, // ms + timezone
    0x005868467FFFE007, // leapsecond
    0x005F7AFF4F801F87, // second precision
    0x005F7AFF4F00000F, // 1 min
    0x005F7AFF7F000017, // 1 hour
    0x005F7A881F00001F, // 1 day
    0x0067748580000027, // 1 week
    0x005F7AFF4F00002F, // month
    0x005E43A7BB000037, // quater
    0x005F7AFF4F00004F, // 1 year
    0x7EFFFFFFFF80001F, // just before float
    0x7F469c4000000000, // float 20000.0
    0xD000015180000000, // relative time
};

int64_t TEST_VALUES_NEG[] = {
    0x7F3F800000000000, // invalid float
    0x8000000000000000, // reserved time
    0x7F03000042000000, // Float 42.5
};
// ISO Test Strings
// 2004-09-16T23:59:60.123+01:13


// Function to read the Time Stamp Counter
static inline uint64_t read_tsc(void) {
#if defined(__aarch64__) || defined(_M_ARM64)
    // For ARM architectures
    uint64_t val;
    __asm__ __volatile__("mrs %0, pmccntr_el0" : "=r"(val));
    return val;
#else
    uint32_t hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)lo) | (((uint64_t)hi) << 32);
#endif
}


void print_short(FP_Components *fpc){
    char prcname[5] = "no\0";
    FP_precision_name(fpc->precision, prcname);
    printf("FP=%010lX", (fpc->rawdata >> 24) & 0xFFFFFFFFFF);
    printf(".%06lX", fpc->rawdata & 0xFFFFFF);
    printf(", T=");
    printf("%li s, %i ns", fpc->seconds, fpc->ns);
    printf(" (PRC:%i='%s'%s%s)", 
        fpc->precision, prcname, fpc->is_leapsecond ? ",LS" : "", fpc->is_dst ? ",DST" : "");
    if(fpc->tz_offset){ printf(", tz: %d min", fpc->tz_offset); }
    printf("\n");
}





void parse_and_print(int64_t flexpoch){
    uint64_t t_start, t_end;
    FP_Components components = {0};
    char iso_string[50];

    t_start = read_tsc();
    int result = FP_from_fp(flexpoch, &components);
    t_end = read_tsc();


    if (result == 0) {     
        FP_to_iso(&components, iso_string);
        printf("ISO Date Time: %s\n", iso_string);
    } else {
        printf("Invalid Flexpoch value %i\n", result);
    }
    // FP_print_components(&components);
    print_short(&components);
    printf("Execution cycles: %lu\n", t_end - t_start);
    printf("---------------\n");
    fflush(stdout);
}

void run_tests(){
    int64_t flexpoch;
    for (size_t i = 0; i < sizeof(TEST_VALUES_POS)/8; i++) {
        flexpoch = TEST_VALUES_POS[i];
        parse_and_print(flexpoch);
    }
}

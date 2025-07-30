#include "prf.h"
#if CFG_PROFILER_ENABLED == 1 /* otherwise skip compilation */

#define CFG_UNIT_PREFIX 'c'   // m = millisecond; u = microsecond, n = nanosec, c = cycles


#ifdef Arduino_h
extern myprintf(char *fmt, ... );

unsigned int systime(void){
    #if CFG_UNIT_PREFIX == 'm'
    return millis();
    #elif CFG_UNIT_PREFIX == 'u'
    return micros();
    #endif
}
#else
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#define myprintf printf


#if defined(__aarch64__) || defined(_M_ARM64)
void enable_cycle_counter(void) {
    printf("Trying to enable the CPU Cycle Counter...\n");

    // Enable the cycle counter
    uint64_t value;
    asm volatile ("mrs %0, pmcntenset_el0" : "=r" (value));
    value |= (1 << 31);
    asm volatile ("msr pmcntenset_el0, %0" : : "r" (value));

    printf("CPU Cycle Counter enabled.\n");
}
#endif

#if CFG_UNIT_PREFIX=='c'
// Function to read the Time Stamp Counter
static inline uint64_t systime(void) {
#if defined(__aarch64__) || defined(_M_ARM64)    
    uint64_t val;
    __asm__ __volatile__ ("mrs %0, pmccntr_el0" : "=r" (val));
    return val;
#else
    uint32_t hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)lo) | (((uint64_t)hi) << 32);
#endif
}
#elif CFG_UNIT_PREFIX=='n'
unsigned long systime(void){
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0){
    // if (timespec_get(&ts, TIME_UTC) == NULL){
        unsigned long time_in_nsec = (ts.tv_sec) * 1000000000L + (ts.tv_nsec);
        return time_in_nsec;
    } else {

        return -1;
    }

}
#else
unsigned long systime(void){
    struct timeval tv;
    gettimeofday(&tv, 0);

    #if CFG_UNIT_PREFIX=='m'
    unsigned long time_in_mill = (tv.tv_sec) * 1000L + (tv.tv_usec) / 1000;
    return time_in_mill;
    #elif CFG_UNIT_PREFIX=='u'
    unsigned long time_in_usec = (tv.tv_sec) * 1000000L + (tv.tv_usec);
    return time_in_usec;
    #endif
}
#endif

#endif


Profiler_Time_Type PRF_time(PRF_Profile* profile){
    return systime() - profile->t_start;
}


void PRF_start(PRF_Profile* profile){
    profile->t_start = systime();
}

void PRF_stop(PRF_Profile* profile){
    Profiler_Time_Type dt = systime() - profile->t_start; // arduino
    profile->t_total += dt;

    if(dt > profile->t_max){ profile->t_max = dt; }
    if(dt < profile->t_min){ profile->t_min = dt; }
    if(profile->samples == 0) profile->t_avg = dt; // initial value
    profile->samples++;
    profile->t_avg = profile->t_avg * ((PRF_AVG_WINDOW-1.0)/PRF_AVG_WINDOW) + (float)(dt)/PRF_AVG_WINDOW;
}

void PRF_reset(PRF_Profile* profile){
    profile->samples = 0;
    profile->t_min = 1000000000;  // fits 32 bit signed
    profile->t_avg = 0;
    profile->t_max = 0;
    profile->t_total = 0;
}

void PRF_print(char* name, PRF_Profile* profile){
    if (profile->samples == 0) myprintf("TIME_PROFILE %s: No samples!", name);
    Profiler_Time_Type t_avg = profile->t_total / profile->samples;
	myprintf("TIME_PROFILE %s: calls=%d, min,avg,max=%ld < %ld < %ld %cs, total: %ld %cs\n", \
        name, profile->samples, profile->t_min, t_avg, profile->t_max, CFG_UNIT_PREFIX, profile->t_total, CFG_UNIT_PREFIX);
}

#endif

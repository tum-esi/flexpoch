#include <linux/module.h>
#include <linux/kernel.h>

void enable_ccr(void *info) {
    uint32_t value;

    // Enable user-mode access to the performance counters
    asm volatile ("mrs %0, pmuserenr_el0" : "=r" (value));
    value |= 1;
    asm volatile ("msr pmuserenr_el0, %0" : : "r" (value));
}

int init_module(void) {
  // Each cpu has its own set of registers
  on_each_cpu(enable_ccr,NULL,0);
  printk (KERN_INFO "Userspace access to CCR enabled\n");
  return 0;
}

void cleanup_module(void) {
}

MODULE_LICENSE("GPL");

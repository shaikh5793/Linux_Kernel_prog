/*
 * Copyright (c) 2024 TECH VEDA(www.techveda.org)
 * Author: Raghu Bharadwaj
 * This software is dual-licensed under the MIT License and GPL v2.
 * See the accompanying LICENSE file for the full text.
*/

#include <linux/module.h>    
#include <linux/kernel.h>      
#include <linux/init.h>        
#include <linux/jiffies.h>     
#include <linux/delay.h>       
#include <linux/timekeeping.h> 

// Function to perform a busy-loop delay for approximately 'ms' milliseconds
static void busyloop_ms_delay(unsigned int ms) {
    unsigned long start_jiffies = jiffies;
    unsigned long delay_jiffies = msecs_to_jiffies(ms);
    
    pr_info("BusyLoopDelay: Starting busy-loop delay for %u ms.\n", ms);
    
    // Busy-wait loop
    while (jiffies - start_jiffies < delay_jiffies) {
        cpu_relax(); // Hint to the processor that we are in a busy-wait loop
    }
    
    pr_info("BusyLoopDelay: Completed busy-loop delay for %u ms.\n", ms);
}

// Function to perform a busy-loop delay using high-resolution timers
static void busyloop_hr_delay(unsigned int ms) {
    ktime_t start_time, current_time;
    unsigned long delay_ns = ms * 1000000; // Convert milliseconds to nanoseconds
    
    pr_info("BusyLoopDelay: Starting high-resolution busy-loop delay for %u ms.\n", ms);
    
    start_time = ktime_get();
    
    do {
        current_time = ktime_get();
    } while (ktime_to_ns(ktime_sub(current_time, start_time)) < delay_ns);
    
    pr_info("BusyLoopDelay: Completed high-resolution busy-loop delay for %u ms.\n", ms);
}

// Initialization function
static int __init delay_init(void) {
    pr_info("BusyLoopDelay: Module loaded.\n");
    
    // Perform a busy-loop delay of 1000 milliseconds (1 second) using jiffies
    busyloop_ms_delay(1000);
    
    // Perform a busy-loop delay of 500 milliseconds (0.5 seconds) using high-resolution timers
    busyloop_hr_delay(500);
    
    pr_info("BusyLoopDelay: All busy-loop delays completed.\n");
    
    return 0; 
}

// Exit function
static void __exit delay_exit(void) {
    pr_info("BusyLoopDelay: Module unloaded.\n");
}

module_init(delay_init);
module_exit(delay_exit);


MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Raghu Bharadwaj");
MODULE_DESCRIPTION("A Linux kernel module to demonstrate busy-loop delays");

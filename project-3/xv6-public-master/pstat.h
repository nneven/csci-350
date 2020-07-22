#ifndef _PSTAT_H_
#define _PSTAT_H_

/* NSCHEDSTATS is the number of sched_stat_t slots per process. The scheduler fills in the slots  as it schedules processes to record information about scheduling */

#define NSCHEDSTATS 1500

/*
 * responsible for recording the scheduling state per process at a particular tick 
 * e.g. a process can have an array of sched_stat_t's, with each of them holding the info 
 * of a scheduling round of the process 
 */

struct sched_stat_t
{
	int start_tick; // the number of ticks when this process is scheduled
	int duration;   // number of ticks the process is running before it gives up the CPU
	int priority;   // the priority of the process when it's scheduled

	// you may add more fields for debugging purposes
};

#endif
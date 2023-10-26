#ifndef _PSTAT_H_ 
#define _PSTAT_H_
#include "param.h"

struct pstat {
	int inuse[NPROC]; 	// BOOLEAN: is the PTE in use?
	int tickets[NPROC]; 	// number of tickets the process has.
	int pid[NPROC]; 	// the pid of the process.
	int ticks[NPROC]; 	// Number of tickets each process has accumulated.
	/*
	 * Current assumption is that ticks is the overall amount of tickets
	 * that a process has seen. tickets contains the current tickets of
	 * the process.
	 *
	 * An example entry is shown in the instructions:
	 * | inuse | tickets | pid | ticks |
	 * |    1  |    10   |  3  |  1010 |
	 * */
};


#endif //_PSTAT_H_

#include "pstat.h"
#include "user.h"

//this program will display the currently running processes on the system & some information about them
int main(int argc, char* argv[]) {
	struct pstat table; 	//the table holding the process stats
	getpinfo(&table); 	//load the table
	//print the table data
	printf(1," used | pid  |tickets| ticks\n");
	for (uint index = 0; index < NPROC; index++) { 	//for-each process
		int a = (table.inuse[index] 	!= 0); 	//if the program is in use
		int b = (table.pid[index] 	!= 0); 	//if the pid is not 0
		int c = (table.tickets[index] 	!= 0); 	//if the process has tickets
		int d = (table.ticks[index] 	!= 0); 	//if the process has ever run
		//
		if (a || b || c || d)
			printf(1, "   %d  |   %d  |   %d   |  %d\n",
				table.inuse[index],
				table.pid[index],
				table.tickets[index],
				table.ticks[index]
			);
	}
	exit(); 	//exit the process
}

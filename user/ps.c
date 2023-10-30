#include "pstat.h"
#include "user.h"

void print_as_table(int inuse, int pid, int tickets, int ticks) {
	printf(1, "   %d  |   %d  |   %d   |  %d\n",
		inuse,
		pid,
		tickets,
		ticks
	);
}

void print_as_csv(int inuse, int pid, int tickets, int ticks) {
	printf(1, "%d,%d,%d,%d\n",
		inuse,
		pid,
		tickets,
		ticks
	);
}

//this program will display the currently running processes on the system & some information about them
int main(int argc, char* argv[]) {
	settickets(100); 		//give the program a pretty high priority so that we get our output faster
	int csv_flag = (argc>1); 	//if there is any text after ps, just do CSV.
					
	struct pstat table; 		//the table holding the process stats
	getpinfo(&table); 		//load the table
	//print the table data
	printf(1," used | pid  |tickets| ticks\n");
	for (uint index = 0; index < NPROC; index++) { 	//for-each process
		int a = (table.inuse[index] 	!= 0); 	//if the program is in use
		int b = (table.pid[index] 	!= 0); 	//if the pid is not 0
		int c = (table.tickets[index] 	!= 0); 	//if the process has tickets
		int d = (table.ticks[index] 	!= 0); 	//if the process has ever run
		//
		if (a || b || c || d) {
			if (csv_flag) print_as_csv( 	//if csv_flag is set, print in csv format
					table.inuse[index],
					table.pid[index],
					table.tickets[index],
					table.ticks[index]
				);
			else print_as_table( 		//if csv_flag is not set, print in a tabular format
					table.inuse[index],
					table.pid[index],
					table.tickets[index],
					table.ticks[index]
				);
				
		}
	}
	exit(); 	//exit the process
}

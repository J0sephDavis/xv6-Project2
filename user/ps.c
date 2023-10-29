#include "pstat.h"
#include "user.h"
//this program will display the currently running processes on the system & some information about them
int main() {
	//allocate table
	struct pstat *table;
	table = malloc(sizeof(struct pstat));
	//load the table
	getpinfo(table);
	//print results
	printf(1," used | pid  |tickets| ticks\n");
	for (uint index = 0; index < NPROC; index++) {
		if (table->inuse[index] == 1)
			printf(0, "   %d  |   %d  |   %d   |  %d\n",
				table->inuse[index],
				table->pid[index],
				table->tickets[index],
				table->ticks[index]
			);
	}
	//dellocate table
	free(table);
}

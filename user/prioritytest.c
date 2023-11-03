#include "user.h"
//code to test the lottery system
//using fork(), spawn 5 new processes
//set the tickets to: 10,20,30,40, & 50, one per fork.
int main() {
	int forkVal,iterator;
	int value = 0;
	for (iterator = 1; iterator < 10; iterator++) {
		forkVal = fork();
		if (forkVal < 0) {
			printf(2,"<fork failed>\n");
			return -1;
		}
		else if (forkVal == 0) {//child
			setpriority(49+iterator);
			//printf(0,"%d:FORK HERE, IM TRYING!\n",iterator);
			for (int a = 0; a < iterator*1000000*iterator; a++)
				value++; 	//keeps the process running for eternity (so we can see how many ticks it has compared to the others)
			exit(); 		//so that the fork doesn't somehow spawn its own children.
		}
	}
	while (wait() != -1) { continue; };
	printf(1,"<PRIORITY TEST END>\n");
	exit(); //without a proper exit we incur trap penalties
}

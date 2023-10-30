#include "user.h"
//code to test the lottery system
//using fork(), spawn 5 new processes
//set the tickets to: 10,20,30,40, & 50, one per fork.
int main() {
	int forkVal,iterator;
	int value = 0;
	for (iterator = 1; iterator <= 5; iterator++) {
		forkVal = fork();
		if (forkVal < 0) {
			printf(2,"<fork failed>\n");
			return -1;
		}
		else if (forkVal == 0) {//child
			settickets(iterator*10);
			for (;;)  value++; 	//keeps the process running for eternity (so we can see how many ticks it has compared to the others)
			break; 			//so that the fork doesn't somehow spawn its own children.
		}
	}
	printf(0,"exit\n");
	exit(); //without a proper exit we incur trap penalties
}

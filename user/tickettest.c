#include "user.h"
//code to test the lottery system
//using fork(), spawn 5 new processes
//within each fork, set 10,20,30,40, then finally 50 tickets
//(all other processes will have their 1 tickets they began with on the system).
int main() {
	int forkVal,iterator;
	for (iterator = 1; iterator < 5; iterator++) {
		forkVal = fork();
		if (forkVal == 0) {//child
			settickets(iterator*10);
		}
	}
}

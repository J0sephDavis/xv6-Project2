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
			if (iterator >5) {
				setpriority(200); 
				for (;;) value++;
			}
			else {
				setpriority(180);
				for (;;) value++;
			}
			exit(); 		//so that the fork doesn't somehow spawn its own children.
		}
	}
	while (wait() != -1) { continue; };
	printf(1,"<PRIORITY TEST END>\n");
	exit(); //without a proper exit we incur trap penalties
}

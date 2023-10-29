#include "pstat.h"
#include "user.h"
//this program will display the currently running processes on the system & some information about them
int main() {
//allocate table
struct pstat *table = {0};
getpinfo(table);
printf(0, "%d", table->inuse[0]);

//call getpinfo(table)
//print results
}

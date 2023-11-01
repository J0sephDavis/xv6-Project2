#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "rand.h"
#include "pstat.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void) { initlock(&ptable.lock, "ptable"); }

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *allocproc(void) {
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  p->numTickets = 1; 		//initialize tickets
  p->numTicks = 0; 		//initialize ticks (times process has been scheduled)
  p->priority = 50; 		//initialize priority

  release(&ptable.lock);

  // Allocate kernel stack if possible.
  if ((p->kstack = kalloc()) == 0) {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// Set up first user process.
void userinit(void) {
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  acquire(&ptable.lock);
  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n) {
  uint sz;

  sz = proc->sz;
  if (n > 0) {
    if ((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if (n < 0) {
    if ((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void) {
  int i, pid;
  struct proc *np;

  // Allocate process.
  if ((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if ((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0) {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  np->numTickets =
      proc->numTickets; // new process tickets  = parent process tickets

  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void) {
  struct proc *p;
  int fd;

  if (proc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++) {
    if (proc->ofile[fd]) {
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->parent == proc) {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void) {
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for (;;) {
    // Scan through table looking for zombie children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->parent != proc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE) {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || proc->killed) {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock); // DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void) {
	struct proc* p;
	uint current_priority 	= 201; 		//the current priority(highest priority) that we are running processes at
	int total_priority = 0; 		//total processes in the priority_queue
	struct proc* priority_queue[NPROC]; 		//contains the processes that are to run
	//
	for (;;) { 				// Enable interrupts on this processor.
		//what is "sti()"?
		sti();
		//
		int tmp_highest = 201; 		//current highest priority (initialized as an impossible one)
		int count_highest = 0; 		//count of runnable programs at highest priorities
		//
		acquire(&ptable.lock); 						//get table lock
		for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) { 		//for loop to determine the highest priority & amount of processes at that priority
			if (p->state != RUNNABLE) continue;
			if (p->priority < tmp_highest){  			//if there is a process of higher priority than the current highest
				tmp_highest = p->priority; 			//set the new highest priority
				count_highest = 1; 				//set the count to 1
			}
			else if (p->priority == tmp_highest) 			//if there are processes of equal priority to the current highest
				count_highest++; 				//if we have multiple processes at max priority, count
		}
		if (count_highest == 0) {release(&ptable.lock); continue;};
		
		if (current_priority != tmp_highest) { 				//if the priority queues are absolutely different rebuild
//			cprintf("NEW PRIORITY\n");
			current_priority = tmp_highest; 			//set the new current priority
			total_priority = 0; 					//reset the size of the priority queue
			for (p = ptable.proc; p<&ptable.proc[NPROC]; p++) {
				if (p->priority != current_priority) continue; 	//if the priority of the current process isn't what we're looking for, skip
				priority_queue[total_priority++] = p; 			//set the current index of the priority_queue to the new value
			}
		}
/* Need to check if any processes have died in the queue,
 * if ALL processes died in queue,
 * 	just change current priority & let the table be rebuilt automatically
 * if SOME, but not ALL, processes have died in the qeueue,
 * 	we must reorder the queue so that the dead processes fall outside of
 * 	the range specified by total_priority or just set to 0x00 or something?
 * */
		int total_ran = 0; 						//the total processes that have run this loop
		for (int f = 0; f < total_priority; f++) { 	//loop over all processes in queue
			p = priority_queue[f];
			if (p->state != RUNNABLE) 				// if the process is not runnable, skip to next process
				continue;
			total_ran++;
			// Switch to chosen process.  It is the process's job
			// to release ptable.lock and then reacquire it
			// before jumping back to us.
			proc = p;
			switchuvm(p);
			p->state = RUNNING;
			p->numTicks++;
			swtch(&cpu->scheduler, proc->context);
			switchkvm();
			// Process is done running for now.
			// It should have changed its p->state before coming back.
			proc = 0;
		}
		//if we didn't run all the processes (meaning SOME died) just force a new table
		if (total_ran < total_priority)current_priority = 201; 		//THIS IS NOT A SOLUTION, WE SHOULD SIMPLY BE REARRANGING THE QUEUE, NOT JUST FORCING A NEW ONE
		release(&ptable.lock); 					//unlock table
	}
}
//the lottery scheduler
void lottery_scheduler(void) {
  struct proc *p;
  
  for (;;) {
	// Enable interrupts on this processor.
	//what is "sti()"?
	sti();
	
	uint ticketCounter = 0; 				// the count of all tickets held by all processes
	acquire(&ptable.lock); 				//lock table
	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) 	//for-each process in process-table, summate tickets
		if (p->state == RUNNABLE){ 			//if the process can be run
			ticketCounter = ticketCounter + p->numTickets; 	//add the processes tickets to the ticketCounter
		}
	release(&ptable.lock); 				//unlock table
	if (ticketCounter == 0) continue; 		//if we cannot do a lottery, skip.
	uint lotteryWinner = (rand() % ticketCounter) + 1; 	//The number of tickets that must be exceeded to choose the winner

	acquire(&ptable.lock); 					//lock table
	ticketCounter = 0; // reset the count
	for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) { 	// Loop over process table looking for process to run.
		if (p->state != RUNNABLE) 			// if the process is not runnable, skip to next process
			continue;
		ticketCounter += p->numTickets; 		//increment the ticket counter
		if (ticketCounter < lotteryWinner) continue;	//if this isn't a winner, skip to the next process
//		cprintf("!%d,%d|\n",ticketCounter, lotteryWinner);

		// Switch to chosen process.  It is the process's job
		// to release ptable.lock and then reacquire it
		// before jumping back to us.
		proc = p;
		switchuvm(p);
		p->state = RUNNING;
		p->numTicks++;
		swtch(&cpu->scheduler, proc->context);
		switchkvm();
		
		// Process is done running for now.
		// It should have changed its p->state before coming back.
		proc = 0;
		break;
	}
	release(&ptable.lock); 					//unlock table
  }
}

// Set a processes tickets to the passed amount. Cannot be less than 1
int settickets(uint numTickets) {
  // return -1 if the caller requested <1 tickets
  if (numTickets < 1)
    return -1;
  // updated process tickets
  proc->numTickets = numTickets;
  // return successful
  return 0;
}

/* getpinfo(struct pstat *), updates the variables of the pointer to include the process-information
 * return 0 on success and -1 on FAILURE*/
int getpinfo(struct pstat *referenced_table){
	if (referenced_table == NULL) return -1; //if the pointer is NULL, return FAILURE

	struct proc *process; 	//points to a process structure
	int index = 0; 		//current index in pstat referenced_table
	//
	acquire(&ptable.lock); 	//acquire a lock to the table so that it isn't touched while we use it
	for (process = ptable.proc; process < &ptable.proc[NPROC]; process++) { //for-each process in the process-table
		if(process->state == ZOMBIE || process->state == EMBRYO)	//if process ZOMBIE or EMBRYO, skip.
			continue;
		//update the table with in-use, PID, numTickets, & numTicks
		referenced_table->inuse[index] = (process->state != UNUSED); //1 if used, 0 if unused
		referenced_table->pid[index] = process->pid;
		referenced_table->tickets[index] = process->numTickets;
		referenced_table->ticks[index] = process->numTicks;
		referenced_table->priority[index] = process->priority;
		//increment counter
		index++;
	} release(&ptable.lock); 	//released the lock so that the table can be used
	//

	return 0; //if function has reached end of execution, return SUCCESS
}
// Set a processes priority, cannot be less than 0 or greater than 200
int setpriority(int priority) {
  // return -1 if the caller requested <1 tickets
  if (priority < 0 || priority > 200)
    return -1;
  // updated process tickets
  proc->priority = priority;
  // return successful
  return 0;
}
// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void sched(void) {
  int intena;

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (cpu->ncli != 1)
    panic("sched locks");
  if (proc->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void) {
  acquire(&ptable.lock); // DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void) {
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk) {
  if (proc == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock) { // DOC: sleeplock0
    acquire(&ptable.lock);  // DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock) { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1(void *chan) {
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan) {
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid) {
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid) {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void) {
  static char *states[] = {
      [UNUSED] "unused",   [EMBRYO] "embryo",  [SLEEPING] "sleep ",
      [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING) {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

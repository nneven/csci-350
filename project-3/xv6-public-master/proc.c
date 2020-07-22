#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
// #include "pstat.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// added by Sean and Nicolas
int c0 = -1;
int c1 = -1;
int c2 = -1;
struct proc* q0[NPROC];
struct proc* q1[NPROC];
struct proc* q2[NPROC];

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  
  
  // added by Sean and Nicolas
  if (p->pid > 0) {
    for (int i = 0; i < c0; i++) {
      if (p == q0[i]) {
        // shift all left
        for (int j = i; j <= c0-1; j++) {
          q0[j] = q0[j+1];
        }
        // delete q0[i]
        q0[c0] = 0;
        c0--;
        break;
      }
    }
    for (int i = 0; i < c1; i++) {
      if (p == q1[i]) {
        // shift all left
        for (int j = i; j <= c1-1; j++) {
          q1[j] = q1[j+1];
        }
        // delete q0[i]
        q1[c1] = 0;
        c1--;
        break;
      }
    }
    for (int i = 0; i < c2; i++) {
      if (p == q2[i]) {
        // shift all left
        for (int j = i; j <= c2-1; j++) {
          q2[j] = q2[j+1];
        }
        // delete q0[i]
        q2[c2] = 0;
        c2--;
        break;
      }
    }
  }

  // initiate proc properties
  p->tick_count = 0;
  p->total_ticks = 0;
  p->num_stats_used = 0;
  p->wait_time = 0;

  c0++;
  q0[c0] = p; // add p into first priority queue

  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
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
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

void
boost(int first_process, int duration)
{
  //Increase wait time of all RUNNABLE processes in q2 according to last process' duration
  int i;
  for(i = first_process; i <= c2; i++) {
    if(q2[i]->state == RUNNABLE) {
      q2[i]->wait_time += duration;
      // cprintf("%d", q2[i]->wait_time);
      //If any of the process' wait time passes 50 boost it into the end of first priority queue
      if (q2[i]->wait_time > 50)
      {
        // cprintf("Boosting process %d from Q2 -> Q0 \n", q2[i]->pid);
        struct proc* p_to_boost;
        p_to_boost = q2[i];
        p_to_boost->tick_count = 0;
        p_to_boost->wait_time = 0;
        //Change priority of p
        struct sched_stat_t *s = &p_to_boost->sched_stats[p_to_boost->num_stats_used];
        s->priority = 0;
        c0++;
        q0[c0] = p_to_boost;
        //Delete from q2 and shift left
        for (int j = i; j <= c2-1; j++) {
          q2[j] = q2[j+1];
        }
        // don't forget counter-- and last element of array
        q2[c2] = 0;
        c2--;
      }
    }
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();
    /*
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // cprintf("before %d \n", ticks);
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();
      // cprintf("after %d \n", ticks);

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    } */

    
    acquire(&ptable.lock);

    if (c0 != -1) {
      int i;
      for(i = 0; i <= c0; i++) {
        if(q0[i]->state != RUNNABLE) {
          continue;
        }

        // cprintf("before %d \n", ticks);

        p = q0[i];
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;
        // cprintf("Q0 \n");

        // write stats
        struct sched_stat_t *s = &p->sched_stats[p->num_stats_used];
        s->start_tick = ticks;
        s->priority = 0;

        swtch(&(c->scheduler), p->context);
        switchkvm();

        // cprintf("after %d \n", ticks);

        // these two values should NOT be the same
        // cprintf("%d %d \n", ticks, s->start_tick);

        s->duration = ticks - s->start_tick;
        // cprintf("D: %d \n", s->duration);
        p->num_stats_used++;
        p->times[0]++;
        p->ticks[0] = s->duration;
        p->total_ticks += s->duration;
        p->tick_count = s->duration;

        if (p->tick_count >= 1) {
          p->tick_count = 0;
          // increase c1 count (order?)
          c1++;
          // copy proc into lower priority queue
          q1[c1] = p;
          // other changes necessary (?)

          // delete proc from q0
          q0[i] = 0;
          // shift all left
          for (int j = i; j <= c0-1; j++) {
            q0[i] = q0[i+1];
          }
            // don't forget counter-- and last element of array
            q0[c0] = 0;
            c0--;
        }

        // Make sure above code is correct before Q1/2 implementation...

        boost(0, s->duration);
        // cprintf("Q0 boost finish \n");

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
    }

    
    if(c1 != -1) {

      int i;
      for(i = 0; i <= c1; i++) {
        if(q1[i]->state != RUNNABLE) {
          continue;
        }

        p = q1[i];
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        // write stats
        struct sched_stat_t *s = &p->sched_stats[p->num_stats_used];
        s->start_tick = ticks;
        s->priority = 1;

        swtch(&(c->scheduler), p->context);
        switchkvm();

        s->duration = ticks - s->start_tick;
        p->num_stats_used++;
        p->times[1]++;
        p->ticks[1] = s->duration;
        p->total_ticks += s->duration;
        p->tick_count = s->duration;

        if (p->tick_count >= 2) {
          p->tick_count = 0;
          // increase c2 count (order?)
          c2++;
          // copy proc into lower priority queue
          q2[c2] = p;
          // other changes necessary (?)

          // delete proc from q1
          q1[i] = 0;
          // shift all left
          for (int j = i; j <= c1-1; j++) {
            q1[i] = q1[i+1];
          }
            // don't forget counter-- and last element of array
            q1[c1] = 0;
            c1--;
        }

        // Make sure above code is correct before Q1/2 implementation...

        boost(0, s->duration);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
    }

    
    if(c2 != -1) {

      int i;
      for(i = 0; i <= c2; i++) {
        if(q2[i]->state != RUNNABLE) {
          continue;
        }

        p = q2[i];
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        // write stats
        struct sched_stat_t *s = &p->sched_stats[p->num_stats_used];
        s->start_tick = ticks;
        s->priority = 2;

        swtch(&(c->scheduler), p->context);
        switchkvm();

        s->duration = ticks - s->start_tick;
        p->num_stats_used++;
        p->times[2]++;
        p->ticks[2] = s->duration;
        p->total_ticks += s->duration;
        p->tick_count = s->duration;

        if (p->tick_count >= 8) {
          p->tick_count = 0;
          // increase c1 count (order?)
          // c1++;
          // copy proc into lower priority queue
          // q1[c1] = p;
          // other changes necessary (?)

          // delete proc from q0
          // q0[i] = 0;
          
          q2[i]=0;
          int j;
          for(j=i;j<=c2-1;j++)
          q2[j] = q2[j+1];

          q2[c2] = p;
        }

        // Make sure above code is correct before Q1/2 implementation...
        boost(i, s->duration);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
    }
    
    release(&ptable.lock);

  }
}

int
getpinfo(int pid)
{
  int n;
  int priority = 0;
  acquire(&ptable.lock);
  struct proc *p;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid) break;
  }
  cprintf("\nname=%s, pid=%d \n", p->name, p->pid);
  cprintf("wait_time=%d \n", p->wait_time);
  cprintf("ticks={%d, %d, %d} \n", p->ticks[0], p->ticks[1], p->ticks[2]);
  cprintf("times={%d, %d, %d} \n", p->times[0], p->times[1], p->times[2]);
  for (n = 0; n < p->num_stats_used; n++)
  {
    struct sched_stat_t *s = &p->sched_stats[n];
    if (s->priority == 0) break;
  }
  for (int i = n; i < p->num_stats_used; i++)
  {	
  	
    struct sched_stat_t *s = &p->sched_stats[i];
    if (i == n) priority = s->priority;
    if (i > n && (&p->sched_stats[i-1])->priority == 2 && s->priority == 0) {
      cprintf("[boosted process %d from Q2 -> Q0] \n", pid);
    }
    if (s->priority - priority != -1) {
      priority = s->priority;
   	  cprintf("start=%d, duration=%d, priority=%d \n", s->start_tick, s->duration, s->priority);
   	}
  }
  release(&ptable.lock);
  return 0;
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

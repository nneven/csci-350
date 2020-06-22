#ifndef XV6_PUBLIC_KTHREAD_H
#define XV6_PUBLIC_KTHREAD_H

#define NTHREAD			16	// maximum number of threads per process
#define MAX_MUTEXES		64	// added by Sean and Nicolas

int kthread_create(void*(*start_func)(), void* stack, int stack_size);
int kthread_id();
void kthread_exit();
int kthread_join(int thread_id);

// added by Sean and Nicolas
int kthread_mutex_alloc();
int kthread_mutex_dealloc(int mutex_id);
int kthread_mutex_lock(int mutex_id);
int kthread_mutex_unlock(int mutex_id);

#endif

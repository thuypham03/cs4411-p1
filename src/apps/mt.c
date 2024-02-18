/* Threads and semaphores in user space.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <egos/queue.h>
#include <egos/context.h>

/**** THREADS AND SEMAPHORES ****/
typedef enum state {
	RUNNING,
	RUNNABLE,
	TERMINATED
} thread_state;

typedef struct thread {
	char *sp;
	address_t base;
	thread_state state;

	void (*func)(void *);
	void *arg;
} thread_t;

thread_t *current;
thread_t *next;
struct queue *runnable_queue;

void thread_init() {
	// Initialize threading package
	current = malloc(sizeof(thread_t));
	runnable_queue = malloc(sizeof(struct queue));

	next = NULL;
	current->base = NULL;
	queue_init(runnable_queue);
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stack_size) {
	current->state = RUNNABLE;
	queue_add(runnable_queue, current);

	next = malloc(sizeof(thread_t));
	next->func = f;
	next->arg = arg;
	next->sp = malloc(stack_size); // Top of stack
	next->base = (address_t) &next->sp[stack_size]; // Bottom of stack
	next->state = RUNNING;

	ctx_start(&current->sp, ???)
	current = next;
}

void thread_yield();
void thread_exit();

struct sema {
    // your code here
};

void sema_init(struct sema *sema, unsigned int count);
void sema_dec(struct sema *sema);
void sema_inc(struct sema *sema);
bool sema_release(struct sema *sema);

/**** TEST SUITE ****/

// your test code here, such as producer/consumer, read/write locks
// dining philosophers and barber shop

int main(int argc, char **argv){
    thread_init();
    // your code here
    thread_exit();
    return 0;
}
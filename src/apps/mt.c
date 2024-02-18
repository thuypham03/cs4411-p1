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
} thread_t;

thread_t *current;
thread_t *next;
struct queue *runnable_queue;

void thread_init() {
	// Initialize threading package
	current = malloc(sizeof(thread_t));
	runnable_queue = malloc(sizeof(struct queue));

	current->base = NULL;
	queue_init(runnable_queue);
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
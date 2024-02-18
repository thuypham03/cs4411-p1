/* Threads and semaphores in user space.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <egos/queue.h>
#include <egos/context.h>

/**** THREADS AND SEMAPHORES ****/
typedef enum state
{
	RUNNING,
	RUNNABLE,
	TERMINATED
} thread_state;

typedef struct thread
{
	char *sp;
	address_t base;
	thread_state state;

	void (*func)(void *);
	void *arg;
} thread_t;

thread_t *current;
thread_t *next;
struct queue *runnable_queue;

void ctx_entry() {}

void thread_init()
{
	// Initialize threading package
	current = malloc(sizeof(thread_t));
	runnable_queue = malloc(sizeof(struct queue));

	next = NULL;
	current->base = NULL;
	queue_init(runnable_queue);
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stack_size)
{
	current->state = RUNNABLE;
	queue_add(runnable_queue, current);

	next = malloc(sizeof(thread_t));
	next->func = f;
	next->arg = arg;															 // When will we run the function?
	next->sp = malloc(stack_size);								 // Top of stack
	next->base = (address_t)&next->sp[stack_size]; // Bottom of stack
	next->state = RUNNING;

	// Switch from current to newly created thread (stack top = next->sp)
	ctx_start(&current->sp, next->sp); // Recheck for Top of stack = next->sp
	current = next;
}

void thread_yield()
{
	assert(current->state == RUNNING);
	if (queue_empty(runnable_queue))
		return;

	current->state = RUNNABLE;
	queue_add(runnable_queue, current);
	next = (thread_t *)queue_get(runnable_queue);
	next->state = RUNNING;

	ctx_switch(&current->sp, next->sp);
	current = next;
}

void thread_exit()
{
	if (queue_empty(runnable_queue))
		exit(0);

	current->state = TERMINATED;
	next = (thread_t *)queue_get(runnable_queue);
	next->state = RUNNING;

	ctx_switch(&current->sp, next->sp);
	current = next;
	// Next thread clean up last thread?
}

// struct sema {
//     // your code here
// };

// void sema_init(struct sema *sema, unsigned int count);
// void sema_dec(struct sema *sema);
// void sema_inc(struct sema *sema);
// bool sema_release(struct sema *sema);

/**** TEST SUITE ****/

// your test code here, such as producer/consumer, read/write locks
// dining philosophers and barber shop

static void test_code(void *arg)
{
	int i;
	for (i = 0; i < 10; i++)
	{
		printf("%s here: %d\n", arg, i);
		thread_yield();
	}
	printf("%s done\n", arg);
}

int main(int argc, char **argv)
{
	thread_init();
	thread_create(test_code, "thread 1", 16 * 1024);
	thread_create(test_code, "thread 2", 16 * 1024);
	test_code("main thread");
	thread_exit();
	return 0;
}
/* Threads and semaphores in user space.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <egos/queue.h>
#include <egos/context.h>
#include <earth/earth.h>

void ctx_entry();
void thread_init();
void thread_create(void (*f)(void *arg), void *arg, unsigned int stack_size);
void thread_yield();
void thread_exit();

/**** THREADS AND SEMAPHORES ****/
typedef enum state
{
	RUNNING,
	RUNNABLE,
	TERMINATED
} thread_state;

typedef struct thread
{
	int id;
	address_t sp;
	char *base;
	thread_state state;

	void (*f)(void *arg);
	void *arg;
} thread_t;

typedef struct scheduler
{
	thread_t *current;
	thread_t *next;
	struct queue *runnable_queue;
	struct queue *terminated_queue;
} scheduler;

static scheduler *master;
static int thread_id_counter = 0;
static int debug = 0;

void ctx_entry()
{
	if (debug) sys_print("ctx_entry()\n");

	master->current->state = RUNNING;
	master->current->f(master->current->arg);
	thread_exit();
}

void thread_init()
{
	if (debug) sys_print("thread_init()\n");

	// Initialize threading package
	master = malloc(sizeof(scheduler));
	master->runnable_queue = malloc(sizeof(struct queue));
	queue_init(master->runnable_queue);
	master->terminated_queue = malloc(sizeof(struct queue));
	queue_init(master->terminated_queue);

	master->current = malloc(sizeof(thread_t));
	master->current->base = NULL;
	master->current->id = thread_id_counter;
	master->next = NULL;
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stack_size)
{
	if (debug) sys_print("thread_create()\n");

	master->current->state = RUNNABLE;
	queue_add(master->runnable_queue, master->current);

	master->next = malloc(sizeof(thread_t));
	master->next->f = f;
	master->next->arg = arg;															 
	master->next->base = malloc(stack_size);
	// master->next->sp = (address_t) &master->next->base[stack_size];
	master->next->sp = (address_t) (master->next->base + stack_size - sizeof(address_t));
	master->next->state = RUNNING;

	thread_id_counter++;
	master->next->id = thread_id_counter;
	
	if (master->current->base == NULL) {
		master->current = master->next;
		ctx_entry();
	} else {
		// Switch from current to newly created thread (stack top = next->sp)
		ctx_start(&master->current->sp, master->next->sp);
		master->current = master->next;
	}
}

void thread_yield()
{
	assert(master->current->state == RUNNING);
	if (queue_empty(master->runnable_queue))
		return;

	master->current->state = RUNNABLE;
	queue_add(master->runnable_queue, master->current);
	master->next = (thread_t *)queue_get(master->runnable_queue);
	master->next->state = RUNNING;

	ctx_switch(&master->current->sp, master->next->sp);
	master->current = master->next;
}

void thread_exit()
{
	if (queue_empty(master->runnable_queue)) exit(0);

	master->current->state = TERMINATED;
	queue_add(master->terminated_queue, master->current);

	master->next = (thread_t *) queue_get(master->runnable_queue);
	master->next->state = RUNNING;

	ctx_switch(&master->current->sp, master->next->sp);
	master->current = master->next;

	// Next thread clean up last thread?
	while (!queue_empty(master->terminated_queue)) {
		thread_t *terminated_thread = (thread_t *) queue_get(master->terminated_queue);
		free(terminated_thread->base);		
		free((void *) terminated_thread);
	}
}

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
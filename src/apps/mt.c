

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

typedef enum state
{
	RUNNING,
	RUNNABLE,
	TERMINATED,
} thread_state;

typedef struct thread
{
	thread_state state;
	char *sp;
	address_t base;
	void (*f)(void *arg);
	void *arg;
	int id;
} thread;

typedef struct scheduler
{
	struct queue *runnable_queue;
	struct queue *terminated_queue;
	thread *running_thread;
	thread *next_thread;
} scheduler;

static scheduler *master;
static int thread_id = 0;

void ctx_entry()
{
	if (master->running_thread->base == NULL)
	{
		master->running_thread->state = TERMINATED;
		queue_add(master->terminated_queue, master->running_thread);
	}
	else
	{
		master->running_thread->state = RUNNABLE;
		queue_add(master->runnable_queue, master->running_thread);
	}
	master->running_thread = master->next_thread;
	master->next_thread = NULL;
	master->running_thread->state = RUNNING;
	(master->running_thread->f)(master->running_thread->arg);
	thread_exit();
}

void thread_init()
{
	master = malloc(sizeof(scheduler));
	master->runnable_queue = malloc(sizeof(struct queue));
	queue_init(master->runnable_queue);
	master->terminated_queue = malloc(sizeof(struct queue));
	queue_init(master->terminated_queue);
	master->running_thread = malloc(sizeof(thread));
	master->running_thread->state = RUNNING;
	master->running_thread->base = NULL;
	master->running_thread->id = thread_id;
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stack_size)
{
	thread *new_thread = malloc(sizeof(thread));
	new_thread->state = RUNNABLE;
	new_thread->sp = malloc(stack_size);
	new_thread->base = (address_t)&new_thread->sp[stack_size];
	new_thread->f = f;
	new_thread->arg = arg;
	thread_id++;
	new_thread->id = thread_id;
	if (master->next_thread != NULL)
	{
		queue_insert(master->runnable_queue, master->next_thread);
	}
	master->next_thread = new_thread;
	if (master->running_thread->base == NULL)
	{
		ctx_entry();
	}
	else
	{
		ctx_start(&master->running_thread->base, new_thread->base);
	}
}

void thread_yield()
{
	if (master->running_thread->state == TERMINATED)
	{
		queue_add(master->terminated_queue, master->running_thread);
	}
	if (master->next_thread != NULL)
	{
		master->running_thread = master->next_thread;
		master->next_thread = NULL;
	}
	else if (!queue_empty(master->runnable_queue))
	{
		master->next_thread = (thread *)queue_get(master->runnable_queue);
	}
	else
	{
		return;
	}
	master->running_thread->state = RUNNABLE;
	queue_add(master->runnable_queue, master->running_thread);
	ctx_switch(master->running_thread->base, master->next_thread->base);
	master->running_thread = master->next_thread;
	master->running_thread->state = RUNNING;
	master->next_thread = NULL;
}

void thread_exit()
{
	master->running_thread->state = TERMINATED;
	thread_yield();
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
	return 0;
}



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
	READY,
	RUNNING,
	RUNNABLE,
	FINISHED,
} thread_state;

typedef struct thread
{
	thread_state state;
	address_t sp;
	char *base;
	void (*f)(void *arg);
	void *arg;
	int id;
} thread;

typedef struct scheduler
{
	struct queue *runnable_queue;
	struct queue *terminated_queue;
	thread *running_thread;
} scheduler;

static scheduler *master;
static int thread_id = 0;

static int debug = 0;

void ctx_entry()
{
	if (debug)
	{
		sys_print("ctx_entry()\n");
	}
	master->running_thread->state = RUNNING;
	master->running_thread->f(master->running_thread->arg);
	thread_exit();
}

void thread_init()
{
	if (debug)
	{
		sys_print("thread_init()\n");
	}
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
	if (debug)
	{
		sys_print("thread_create()\n");
	}
	thread *new_thread = malloc(sizeof(thread));
	new_thread->state = READY;
	new_thread->base = malloc(stack_size);
	new_thread->sp = (address_t)&new_thread->base[stack_size];
	new_thread->f = f;
	new_thread->arg = arg;
	thread_id++;
	new_thread->id = thread_id;
	queue_add(master->runnable_queue, new_thread);
}

void thread_yield()
{
	if (debug)
	{
		sys_print("thread_yield()\n");
	}
	thread *current_thread = master->running_thread;
	if (current_thread->state == FINISHED)
	{
		queue_add(master->terminated_queue, current_thread);
	}
	else
	{
		queue_add(master->runnable_queue, current_thread);
	}
	while (!queue_empty(master->terminated_queue))
	{
		thread *terminated_thread = (thread *)queue_get(master->terminated_queue);
		free(terminated_thread->sp);
		free(terminated_thread);
	}
	if (!queue_empty(master->runnable_queue))
	{
		thread *next_thread = (thread *)queue_get(master->runnable_queue);
		master->running_thread = next_thread;
		if (next_thread->state == READY)
		{
			if (current_thread->base != NULL)
			{
				if (debug)
				{
					sys_print("ctx_start()\n");
				}
				ctx_start(&current_thread->sp, next_thread->sp);
				master->running_thread->state = RUNNING;
			}
			else
			{
				if (debug)
				{
					sys_print("ctx_entry()\n");
				}
				ctx_entry();
			}
		}
		else
		{
			if (debug)
			{
				sys_print("ctx_switch()\n");
			}
			ctx_switch(&current_thread->sp, next_thread->sp);
			master->running_thread->state = RUNNING;
		}
	}
}

void thread_exit()
{
	if (debug)
	{
		sys_print("thread_exit()\n");
	}
	master->running_thread->state = FINISHED;
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
	thread_exit();
	return 0;
}

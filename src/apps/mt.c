/* Threads and semaphores in user space.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <egos/queue.h>
#include <egos/context.h>
#include <earth/earth.h>

void thread_init();
void thread_create(void (*f)(void *arg), void *arg, unsigned int stack_size);
void thread_yield();
void thread_exit();

enum state
{
	RUNNING,
	RUNNABLE,
	TERMINATED,
};

typedef struct thread
{
	enum state;
	char stack[];
	address_t base;
	void (*f)(void *arg) f;
	void *arg;
} *thread;

typedef struct scheduler
{
	thread *running_thread;
	queue *run_queue;
	queue *terminated_queue;
} *scheduler_t;

static scheduler_t scheduler;

void thread_init()
{
	scheduler = malloc(sizeof(scheduler_t))

			thread current_thread = malloc(sizeof(thread));
	if (currentThread == NULL)
	{
		exit(1);
	}
	current_thread->state = RUNNABLE;
	current_thread->base = NULL;
	current_thread->stack = NULL;
	current_thread->f = NULL;
	current_thread->arg = NULL;

	struct queue *run_queue = malloc(sizeof(queue));
	if (run_queue == NULL)
	{
		exit(1);
	}
	queue_init(run_queue);

	struct queue *terminated_queue = malloc(sizeof(queue));
	if (terminated_queue == NULL)
	{
		exit(1);
	}

	scheduler->current_thread = current_thread;
	scheduler->run_queue = run_queue;
	scheduler->terminated_queue = terminated_queue;
}

void ctx_entry()
{
	scheduler->current_thread->f(scheduler->current_thread->arg);
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stack_size)
{
	thread new_thread = malloc(sizeof(thread));
	if (new_thread == NULL)
	{
		exit(1);
	}
	new_thread->stack = malloc(stack_size);
	if (new_thread->stack == NULL)
	{
		exit(1);
	}
	new_thread->state = RUNNABLE;
	new_thread->base = (address_t)&new_thread->stack[stack_size] new_thread->f = f;
	new_thread->arg = arg;

	queue_add(scheduler->run_queue, scheduler->new_thread);

	// switch to new thread
	scheduler->current_thread->state = RUNNABLE;
	queue_add(scheduler->run_queue, current_thread);
	ctx_start(&scheduler->current_thread->base, &new_thread->base);
	scheduler->current_thread = new_thread;
};

void thread_yield()
{
	if (queue_empty(scheduler->run_queue))
	{
		while (!queue_empty(scheduler->terminated_queue))
		{
			thread dead_thread = (thread)queue_get(scheduler->terminated_queue);
			free(dead_thread->stack);
			free(dead_thread);
		}
		if (scheduler->current_thread->state == TERMINATED)
		{
			free(scheduler->current_thread->stack);
			free(scheduler->current_thread);
			exit(0);
		}
	}
	else
	{
		thread next_thread = queue_get(scheduler->run_queue);
		scheduler->current_thread->state = RUNNABLE;
		queue_add(scheduler->run_queue, scheduler->current_thread);
		ctx_switch(&scheduler->current_thread->base, &next_thread->base)
				scheduler->current_thread = next_thread;
	}

	while (!queue_empty(scheduler->terminated_queue))
	{
		thread_t dead_thread = (thread_t)queue_get(scheduler->terminated_queue);
		free(dead_thread->stack);
		free(dead_thread);
	}
}

void thread_exit()
{
	scheduler->current_thread->state = TERMINATED;
	queue_add(scheduler->terminated_queue, current_thread);
	thread_yield();
}

int main(int argc, char **argv)
{
	return 0;
}

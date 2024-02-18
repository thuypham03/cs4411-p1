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
	enum state state;
	address_t base;
	void (*f)(void *arg);
	void *arg;
	char *stack;
} *thread_t;

typedef struct scheduler
{
	thread_t running_thread;
	struct queue *run_queue;
	struct queue *terminated_queue;
} *scheduler_t;

static scheduler_t scheduler;

void ctx_entry()
{
	scheduler->running_thread->f(scheduler->running_thread->arg);
}

void thread_init()
{
	scheduler = malloc(sizeof(struct scheduler));
	if (scheduler == NULL)
	{
		exit(1);
	}

	thread_t new_thread = malloc(sizeof(struct thread));
	new_thread->state = RUNNING;
	new_thread->stack = NULL;
	new_thread->base = NULL;
	new_thread->f = NULL;
	new_thread->arg = NULL;

	struct queue *run_queue = malloc(sizeof(struct queue));
	if (run_queue == NULL)
	{
		exit(1);
	}
	queue_init(run_queue);

	struct queue *terminated_queue = malloc(sizeof(struct queue));
	if (terminated_queue == NULL)
	{
		exit(1);
	}

	scheduler->running_thread = new_thread;
	scheduler->run_queue = run_queue;
	scheduler->terminated_queue = terminated_queue;
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stack_size)
{
	thread_t new_thread = malloc(sizeof(struct thread));
	if (new_thread == NULL)
	{
		exit(1);
	}
	new_thread->stack = malloc(stack_size);
	if (new_thread->stack == NULL)
	{
		free(new_thread);
		exit(1);
	}
	new_thread->state = RUNNABLE;
	new_thread->base = (address_t)&new_thread->stack[stack_size];
	new_thread->f = f;
	new_thread->arg = arg;

	// switch to new thread
	scheduler->running_thread->state = RUNNABLE;
	queue_add(scheduler->run_queue, scheduler->running_thread);
	ctx_start(&scheduler->running_thread->base, &new_thread->base);
	scheduler->running_thread = new_thread;
};

void thread_yield()
{
	if (!queue_empty(scheduler->run_queue))
	{
		thread_t next_thread = queue_get(scheduler->run_queue);
		if (scheduler->running_thread->state != TERMINATED)
		{
			scheduler->running_thread->state = RUNNABLE;
			queue_add(scheduler->run_queue, scheduler->running_thread);
		}
		thread_t current_thread = scheduler->running_thread;
		scheduler->running_thread = next_thread;
		ctx_switch(&current_thread->base, &next_thread->base);
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
	scheduler->running_thread->state = TERMINATED;
	queue_add(scheduler->terminated_queue, scheduler->running_thread);
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
	thread_create(test_code, "thread 1", 16 * 1024);
	test_code("main thread");
	// thread_exit();
	return 0;
}

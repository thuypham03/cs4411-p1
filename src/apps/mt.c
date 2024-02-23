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
	RUNNABLE,
	RUNNING,
	BLOCKED,
	TERMINATED
} thread_state;

typedef struct thread
{
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
static int debug = 0;

void ctx_entry()
{
	if (debug)
		printf("ctx_entry()\n");

	master->current = master->next;
	master->current->state = RUNNING;
	master->current->f(master->current->arg);
	thread_exit();
}

void thread_init()
{
	if (debug)
		printf("thread_init()\n");

	// Initialize threading package
	master = malloc(sizeof(scheduler));
	master->runnable_queue = malloc(sizeof(struct queue));
	queue_init(master->runnable_queue);
	master->terminated_queue = malloc(sizeof(struct queue));
	queue_init(master->terminated_queue);

	master->current = malloc(sizeof(thread_t));
	master->current->base = NULL;
	master->next = NULL;
}

void clean_up_zombies()
{
	// Next thread clean up last thread
	while (!queue_empty(master->terminated_queue))
	{
		thread_t *terminated_thread = (thread_t *)queue_get(master->terminated_queue);
		free(terminated_thread->base);
		free((void *)terminated_thread);
	}
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stack_size)
{
	if (debug)
		printf("thread_create()\n");

	master->current->state = RUNNABLE;
	queue_add(master->runnable_queue, master->current);

	master->next = malloc(sizeof(thread_t));
	master->next->f = f;
	master->next->arg = arg;
	master->next->base = malloc(stack_size);
	master->next->sp = (address_t)&master->next->base[stack_size];
	master->next->state = RUNNING;

	// Switch from current to newly created thread (stack top = next->sp)
	if (debug)
		printf("ctx_start()\n");
	ctx_start(&master->current->sp, master->next->sp);
	master->current = master->next;
	clean_up_zombies();
}

void thread_yield()
{
	if (debug)
		printf("thread_yield()\n");

	if (queue_empty(master->runnable_queue))
	{
		if (master->current->state == BLOCKED || master->current->state == TERMINATED)
		{
			clean_up_zombies();
			queue_release(master->runnable_queue);
			queue_release(master->terminated_queue);
			free(master->runnable_queue);
			free(master->terminated_queue);
			free(master);
			exit(0);
		}
		else
			return;
	}

	if (master->current->state == RUNNING)
	{
		master->current->state = RUNNABLE;
		queue_add(master->runnable_queue, master->current);
	}

	master->next = (thread_t *)queue_get(master->runnable_queue);
	master->next->state = RUNNING;

	if (debug)
		printf("ctx_switch()\n");
	ctx_switch(&master->current->sp, master->next->sp);
	master->current = master->next;
	clean_up_zombies();
}

void thread_exit()
{
	if (debug)
		printf("thread_exit()\n");
	master->current->state = TERMINATED;
	queue_add(master->terminated_queue, master->current);

	if (queue_empty(master->runnable_queue))
	{
		clean_up_zombies();
		queue_release(master->runnable_queue);
		queue_release(master->terminated_queue);
		free(master->runnable_queue);
		free(master->terminated_queue);
		free(master);
		exit(0);
	}

	master->next = (thread_t *)queue_get(master->runnable_queue);
	master->next->state = RUNNING;

	if (debug)
		printf("ctx_switch()\n");
	ctx_switch(&master->current->sp, master->next->sp);
	master->current = master->next;
	clean_up_zombies();
}

typedef struct sema
{
	unsigned int count;
	struct queue *waiting_queue;
} sema_t;

void sema_init(struct sema *sema, unsigned int count)
{
	sema->count = count;
	sema->waiting_queue = malloc(sizeof(struct queue));
	queue_init(sema->waiting_queue);
}

void sema_dec(struct sema *sema)
{
	if (sema->count > 0)
	{
		sema->count--;
		return;
	}

	// Block the thread and add it to the semaphore's wait queue
	master->current->state = BLOCKED;
	queue_add(sema->waiting_queue, master->current);
	thread_yield();
}

void sema_inc(struct sema *sema)
{
	if (!queue_empty(sema->waiting_queue))
	{
		// Unblock a thread from the wait queue
		thread_t *unblock_thread = (thread_t *)queue_get(sema->waiting_queue);
		unblock_thread->state = RUNNABLE;
		queue_add(master->runnable_queue, unblock_thread);
	}
	else
		sema->count++;
}

bool sema_release(struct sema *sema)
{
	if (queue_empty(sema->waiting_queue))
	{
		queue_release(sema->waiting_queue);
		return true;
	}
	return false;
}

/**** TEST SUITE ****/

/**
 * Test for thread scheduling
 */
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

void test_thread()
{
	thread_init();
	thread_create(test_code, "thread 1", 16 * 1024);
	thread_create(test_code, "thread 2", 16 * 1024);
	test_code("main thread");
	thread_exit();
}

/**
 * Test for producer/consumer
 */
#define NSLOTS 3

static struct sema s_empty, s_full, s_lock;
static unsigned int in, out;
static char *slots[NSLOTS];

static void producer(void *arg)
{
	for (;;)
	{
		// first make sure there’s an empty slot.
		sema_dec(&s_empty);

		// now add an entry to the queue
		sema_dec(&s_lock);
		slots[in++] = arg;
		if (in == NSLOTS)
			in = 0;
		// printf("%s put\n", arg);
		sema_inc(&s_lock);

		// finally, signal consumers
		sema_inc(&s_full);
	}
}

static void consumer(void *arg)
{
	unsigned int i;
	for (i = 0; i < 5; i++)
	{
		// first make sure there’s something in the buffer
		sema_dec(&s_full);

		// now grab an entry to the queue
		sema_dec(&s_lock);
		void *x = slots[out++];
		printf("%s: got %s\n", arg, x);
		if (out == NSLOTS)
			out = 0;
		sema_inc(&s_lock);

		// finally, signal producers
		sema_inc(&s_empty);
	}
}

void test_producer_consumer()
{
	thread_init();
	sema_init(&s_lock, 1);
	sema_init(&s_full, 0);
	sema_init(&s_empty, NSLOTS);

	thread_create(consumer, "consumer 1", 16 * 1024);
	thread_create(consumer, "consumer 2", 16 * 1024);
	producer("producer 1");
	producer("producer 2");
	// Code should never reach here since producer is an infinite loop
	thread_exit();
}

/**
 * Test for philosopher
 */
#define N_philo 5 // Number of philosophers and forks
sema_t forks[N_philo];

void philosopher(void *num)
{
	int i = (int)num;

	for (int k = 0; k < 3; ++k)
	{
		// Think
		sema_dec(&forks[i]);								 // Pick up left fork
		sema_dec(&forks[(i + 1) % N_philo]); // Pick up right fork

		printf("philosopher %d eats\n", i);

		sema_inc(&forks[i]);								 // Put down left fork
		sema_inc(&forks[(i + 1) % N_philo]); // Put down right fork

		thread_yield();
	}
}

void test_philosopher()
{
	thread_init();
	for (int i = 0; i < N_philo; i++)
	{
		sema_init(&forks[i], 1);
	}
	// Create philosopher threads
	for (int i = 0; i < N_philo; i++)
	{
		thread_create(philosopher, (void *)i, 16 * 1024);
	}
	for (int i = 0; i < N_philo; i++)
	{
		sema_release(&forks[i]);
	}
	thread_exit();
}

/**
 * Test for barber
 */
#define N 1 // Maximum number of customers that can wait

sema_t barberReady;
sema_t accessWaiting;
sema_t customerReady;
int waiting = 0; // Number of customers waiting

void barber(void *arg)
{
	for (;;)
	{
		sema_dec(&customerReady); // Try to acquire a customer - if none, sleep
		sema_dec(&accessWaiting); // Lock access to 'waiting'
		waiting = waiting - 1;		// Decrement count of waiting customers
		sema_inc(&barberReady);		// The barber is ready to cut hair
		printf("The barber is cutting hair\n");
		sema_inc(&accessWaiting); // Release 'waiting'
	}
}

void customer(void *arg)
{
	sema_dec(&accessWaiting); // Lock access to 'waiting'
	if (waiting < N)
	{												 // Check if there's room to wait
		waiting = waiting + 1; // Increment count of waiting customers
		printf("Customer %ld arrived and is waiting\n", (long)arg);
		sema_inc(&customerReady); // Notify barber that a customer is ready
		sema_inc(&accessWaiting); // Release 'waiting'
		sema_dec(&barberReady);		// Wait for the barber to be ready
	}
	else
	{
		printf("Customer %ld left because the waiting room is full\n", (long)arg);
		sema_inc(&accessWaiting); // Release 'waiting'
	}
}

void test_barber()
{
	thread_init();
	sema_init(&barberReady, 0);
	sema_init(&accessWaiting, 1);
	sema_init(&customerReady, 0);

	thread_create(barber, NULL, 16 * 1024);
	for (long i = 1; i <= 10; i++)
	{
		thread_create(customer, (void *)i, 16 * 1024);
	}

	sema_release(&barberReady);
	sema_release(&accessWaiting);
	sema_release(&customerReady);
	thread_exit();
}

int main(int argc, char **argv)
{
	// test_thread();
	// test_producer_consumer();
	// test_philosopher();
	test_barber();
	return 0;
}
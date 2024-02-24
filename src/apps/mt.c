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

// Define possible states for a thread
typedef enum state
{
	RUNNABLE,	 // Ready to run
	RUNNING,	 // Currently running
	BLOCKED,	 // Waiting for an event or resource
	TERMINATED // Finished execution
} thread_state;

// Thread structure, representing each thread in the system
typedef struct thread
{
	address_t sp;				// Stack pointer for context switching
	char *base;					// Base address of the stack
	thread_state state; // Current state of the thread

	void (*f)(void *arg); // Function to run in the thread
	void *arg;						// Argument to the function
} thread_t;

// Scheduler structure, managing threads and scheduling
typedef struct scheduler
{
	thread_t *current;							// Currently running thread
	thread_t *next;									// Next thread to run
	struct queue *runnable_queue;		// Queue of runnable threads
	struct queue *terminated_queue; // Queue of terminated threads
} scheduler;

static scheduler *master; // Global scheduler instance
static int debug = 0;			// Debug flag for logging

void ctx_entry()
{
	if (debug)
		printf("ctx_entry()\n");

	// Set the current thread to the next one and mark it as running
	master->current = master->next;
	master->current->state = RUNNING;

	// Execute the thread function
	master->current->f(master->current->arg);
	thread_exit();
}

void thread_init()
{
	if (debug)
		printf("thread_init()\n");

	// Allocate and initialize the master scheduler and its queues
	master = malloc(sizeof(scheduler));
	master->runnable_queue = malloc(sizeof(struct queue));
	queue_init(master->runnable_queue);
	master->terminated_queue = malloc(sizeof(struct queue));
	queue_init(master->terminated_queue);

	// Allocate a dummy current thread (needed for the first thread_create call)
	master->current = malloc(sizeof(thread_t));
	master->current->base = NULL;
	master->next = NULL;
}

// Clean up resources used by terminated threads
void clean_up_zombies()
{
	// Freeing resources for each terminated thread
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

	// Mark the current thread as runnable and add it to the runnable queue
	master->current->state = RUNNABLE;
	queue_add(master->runnable_queue, master->current);

	// Allocate and initialize the next thread
	master->next = malloc(sizeof(thread_t));
	master->next->f = f;
	master->next->arg = arg;
	// Allocate stack memory and set the stack pointer
	master->next->base = malloc(stack_size);
	master->next->sp = (address_t)&master->next->base[stack_size];
	master->next->state = RUNNING;

	// Switch from current to newly created thread (stack top = next->sp)
	if (debug)
		printf("ctx_start()\n");
	ctx_start(&master->current->sp, master->next->sp);
	master->current = master->next;

	// After the switch, clean up any terminated threads
	clean_up_zombies();
}

void thread_yield()
{
	if (debug)
		printf("thread_yield()\n");

	// If no threads are runnable, check for termination conditions
	if (queue_empty(master->runnable_queue))
	{
		if (master->current->state == BLOCKED || master->current->state == TERMINATED)
		{
			// If the current thread is not runnable, clean up and exit
			clean_up_zombies();
			queue_release(master->runnable_queue);
			free(master->runnable_queue);
			queue_release(master->terminated_queue);
			free(master->terminated_queue);
			free(master);
			exit(0);
		}
		else
			return; // If the current thread is still runnable, do nothing
	}

	// If the current thread is running, make it runnable again
	if (master->current->state == RUNNING)
	{
		master->current->state = RUNNABLE;
		queue_add(master->runnable_queue, master->current);
	}

	// Switch to the next runnable thread
	master->next = (thread_t *)queue_get(master->runnable_queue);
	master->next->state = RUNNING;

	if (debug)
		printf("ctx_switch()\n");
	ctx_switch(&master->current->sp, master->next->sp);
	master->current = master->next;

	// Clean up any terminated threads after switching
	clean_up_zombies();
}

void thread_exit()
{
	if (debug)
		printf("thread_exit()\n");

	// Mark the current thread as terminated and add it to the terminated queue
	master->current->state = TERMINATED;
	queue_add(master->terminated_queue, master->current);

	// If no threads are runnable, clean up and exit
	if (queue_empty(master->runnable_queue))
	{
		clean_up_zombies();
		queue_release(master->runnable_queue);
		free(master->runnable_queue);
		queue_release(master->terminated_queue);
		free(master->terminated_queue);
		free(master);
		exit(0);
	}

	// Switch to the next runnable thread
	master->next = (thread_t *)queue_get(master->runnable_queue);
	master->next->state = RUNNING;

	if (debug)
		printf("ctx_switch()\n");
	ctx_switch(&master->current->sp, master->next->sp);
	master->current = master->next;

	// Clean up any terminated threads after switching
	clean_up_zombies();
}

typedef struct sema
{
	unsigned int count;					 // Counter to track the semaphore's value
	struct queue *waiting_queue; // Queue of threads waiting on this semaphore
} sema_t;

void sema_init(struct sema *sema, unsigned int count)
{
	// Initialize value and empty waiting queue
	sema->count = count;
	sema->waiting_queue = malloc(sizeof(struct queue));
	queue_init(sema->waiting_queue);
}

void sema_dec(struct sema *sema)
{
	if (sema->count > 0)
	{
		// Decrement the count if it's positive
		sema->count--;
		return;
	}

	// Block the thread and add it to the semaphore's wait queue
	master->current->state = BLOCKED;
	queue_add(sema->waiting_queue, master->current);

	// Yield control, as the current thread is now blocked
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
		// If no threads are waiting, increment the count
		sema->count++;
}

bool sema_release(struct sema *sema)
{
	if (queue_empty(sema->waiting_queue))
	{
		// If no threads are waiting, free the waiting queue and return true
		queue_release(sema->waiting_queue);
		free(sema->waiting_queue);
		return true;
	}
	return false;
}

/**** TEST SUITE ****/

/**
 * Unit Tests
 */
static int did_thread_run[] = {0, 0};
static int zero = 0;
static int one = 1;
static incr(int *idx)
{
	did_thread_run[*idx] = 1;
}

// Test thread_init() and thread_exit()
void thread_test1()
{
	thread_init();
	if (master == NULL || master->current == NULL || master->runnable_queue == NULL || master->terminated_queue == NULL)
	{
		printf("thread_init() failed\n");
		exit(1);
	}
	thread_exit();
}

// Test thread_create() thread_yield()
void thread_test2()
{
	thread_init();
	if (master->current == NULL || master->runnable_queue == NULL || master->terminated_queue == NULL)
	{
		printf("thread_init() failed\n");
		exit(1);
	}

	thread_create(incr, &zero, 16 * 1024);
	thread_create(incr, &one, 16 * 1024);
	if (did_thread_run[0] == 0 || did_thread_run[1] == 0)
	{
		printf("thread_create() failed\n");
		exit(1);
	}
	thread_exit();
}

static sema_t lock;

// Tests sema_init() and sema_release()
void sema_test1()
{
	sema_init(&lock, 1);
	thread_t *mock_thread = malloc(sizeof(thread_t));

	queue_add((&lock)->waiting_queue, mock_thread); // obtain lock on queue
	if (sema_release(&lock))
	{
		printf("sema_release() released when shouldn't\n");
		exit(1);
	}
	queue_get((&lock)->waiting_queue); // release lock from queue
	if (!sema_release(&lock))
	{
		printf("sema_release() did not release when should\n");
		exit(1);
	}
}

// Tests sema_inc() and sema_dec()
void sema_test2()
{
	thread_init();
	sema_init(&lock, 1);
	thread_create(sema_dec, &lock, 1024 * 16);
	thread_create(sema_inc, &lock, 1024 * 16);
	if (!sema_release(&lock))
	{
		printf("sema_release() did not release when should\n");
		exit(1);
	}
	printf("passed sema_test2()\n");
}

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
	sema_release(&s_lock);
	sema_release(&s_full);
	sema_release(&s_empty);
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
	if (waiting < N)					// Check if there's room to wait
	{
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

	// Code should never reach here since baber is an infinite loop
	sema_release(&barberReady);
	sema_release(&accessWaiting);
	sema_release(&customerReady);
	thread_exit();
}

int main(int argc, char **argv)
{
	// UNIT TESTS
	// thread_test1();
	// thread_test2();
	// sema_test1();
	// sema_test2();

	// INTEGRATION TESTS
	test_thread();
	// test_producer_consumer();
	// test_philosopher();
	// test_barber();
	return 0;
}
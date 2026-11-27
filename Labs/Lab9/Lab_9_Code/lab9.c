/*
 lab 9 (Duck Park passengers): simplified passenger threads from Project 3

 Each passenger thread:
   1. explores the park for a random time (1-3s)
   2. gets a ride ticket at the ticket booth (critical section, mutex lock)
   3. enters the ride queue
  
  [Additional Question]: lab9_ai.c is an AI-generated program for this lab,
  but it contains a deadlock. Identify where the deadlock occurs and explain how to fix it.

  [Your Answer]:
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_THREADS 10

// -- Passenger data --
typedef struct {
    int id;
    int has_ticket; // bool
    unsigned int seed; // per-thread seed for rand_r()
} passenger_t;

passenger_t *passengers = NULL;

// -- Shared variables, mutexes --
time_t start_time;

// Ticket booth mutex (one ticket at a time)
pthread_mutex_t ticket_mutex = PTHREAD_MUTEX_INITIALIZER;
int tickets_sold = 0; // protected by ticket_mutex

// Ride queue mutex (protects ride_queue_size)
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
int ride_queue_size = 0; // protected by queue_mutex

// Seconds since the program started, use it in every print: "[Time: %d] ..."
int get_time() {
    return (int)(time(NULL) - start_time);
}

void explore_park(passenger_t *p) {
    // TODO: print "[Time: t] Passenger i is exploring the park"
    printf("[Time: %d] Passenger %d is exploring the park\n", get_time(), p->id);
    // TODO: sleep for a random time of 1-3 seconds, use rand_r() with p->seed
    int s_time = (rand_r(p->seed) % 3) + 1;
    sleep(s_time);
    // TODO: print "[Time: t] Passenger i finished exploring, entering the ticket booth"
    printf("[Time: %d] Passenger %d finished exploring, entering the ticket booth\n", get_time(), p->id);
}

void get_ride_ticket(passenger_t *p) {
    // TODO: Allocate the ticket booth mutex lock (ticket_mutex)
    // TODO: print "[Time: t] First Print: Passenger i is buying a ticket"
    // TODO: sleep for one second
    // TODO: increment tickets_sold and set p->has_ticket to 1
    // TODO: print "[Time: t] Second Print: Passenger i acquired ticket #k" (k = tickets_sold)
    // TODO: Release the ticket booth mutex lock

    // TODO: You may also comment out the lock and unlock and see how the output changes,
    // describe your observation below:

}

void enter_ride_queue(passenger_t *p) {
    // TODO: Allocate the ride queue mutex lock (queue_mutex)
    // TODO: increment ride_queue_size
    // TODO: print "[Time: t] Passenger i has entered the ride queue (queue size: s)"
    // TODO: Release the ride queue mutex lock
}

void *passenger_thread(void *arg) {
    // TODO: get this passenger's passenger_t from arg
    // TODO: print "[Time: t] Passenger i entered the park"
    // TODO: call explore_park(), get_ride_ticket(), then enter_ride_queue()

    return NULL;
}

int main(int argc, char *argv[]) {
    int n = 0;
    if (argc < 2) {
        printf("Please add the number of threads to the command line\n");
        exit(1);
    }
    n = atoi(argv[1]);
    if (n > MAX_THREADS) n = MAX_THREADS;
    if (n < 1) n = 1;

    printf("- Number of passenger threads: %d\n\n", n);

    srand(time(NULL));
    start_time = time(NULL);

    // TODO: Allocate an array of n passenger_t on the heap with malloc()
    passenger_t passengers[n] = malloc(sizeof(passenger_t) * n);
    // TODO: For each passenger i: set id to i, has_ticket to 0, and seed to rand()
    for (int i = 0; i < n; i++) {
        passengers[i].id = 0;
        passengers[i].has_ticket = 0;
        passengers[i].seed = rand();
    }

    // TODO: Create n threads, each calling passenger_thread with a pointer to its own passenger_t

    // TODO: Wait for all n threads to finish with pthread_join()

    // TODO: print "\n[Time: t] All n passengers have tickets (tickets sold: k, ride queue size: s)"
    // TODO: free the passenger array and destroy both mutexes

    return 0;
}

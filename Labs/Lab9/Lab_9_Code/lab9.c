/*
 lab 9 (Duck Park passengers): simplified passenger threads from Project 3

 Each passenger thread:
   1. explores the park for a random time (1-3s)
   2. gets a ride ticket at the ticket booth (critical section, mutex lock)
   3. enters the ride queue
  
  [Additional Question]: lab9_ai.c is an AI-generated program for this lab,
  but it contains a deadlock. Identify where the deadlock occurs and explain how to fix it.

  If the queue is full, get_ride_ticket will return. However, this is before it releases
  the lock on the ticket mutex. That means that the next thread to want to get a ride ticket
  will get stuck on its first line waiting for the lock to be released. We can verify this by 
  compiling the code and running it with five passengers - it hangs after filling the queue. 

  This can be fixed by only locking and unlocking when strictly necessary - right around the increment and
  read of tickets_sold. It could also be done by unlocking before returning when the queue is full, but
  that doesn't seem as elegant. 
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
    // print "[Time: t] Passenger i is exploring the park"
    printf("[Time: %d] Passenger %d is exploring the park\n", get_time(), p->id);
    // sleep for a random time of 1-3 seconds, use rand_r() with p->seed
    int s_time = (rand_r(&p->seed) % 3) + 1;
    sleep(s_time);
    // print "[Time: t] Passenger i finished exploring, entering the ticket booth"
    printf("[Time: %d] Passenger %d finished exploring, entering the ticket booth\n", get_time(), p->id);
}

void get_ride_ticket(passenger_t *p) {
    // TODO: Allocate the ticket booth mutex lock (ticket_mutex)
    pthread_mutex_lock(&ticket_mutex);
    // print "[Time: t] First Print: Passenger i is buying a ticket"
    printf("[Time: %d] First Print: Passenger %d is buying a ticket\n", get_time(), p->id);
    // sleep for one second
    sleep(1);
    // TODO: increment tickets_sold and set p->has_ticket to 1
    tickets_sold++;
    p->has_ticket = 1;
    // print "[Time: t] Second Print: Passenger i acquired ticket #k" (k = tickets_sold)
    printf("[Time: %d] Second Print: Passenger %d acquired ticket #%d\n", get_time(), p->id, tickets_sold);
    // Release the ticket booth mutex lock
    pthread_mutex_unlock(&ticket_mutex);

}

void enter_ride_queue(passenger_t *p) {
    // Allocate the ride queue mutex lock (queue_mutex)
    //pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_lock(&queue_mutex);
    // increment ride_queue_size
    ride_queue_size++;
    // print "[Time: t] Passenger i has entered the ride queue (queue size: s)"
    printf("[Time: %d] Passenger %d has entered the ride queue (queue size: %d)\n", get_time(), p->id, ride_queue_size);
    // Release the ride queue mutex lock
    pthread_mutex_unlock(&queue_mutex);
}

void *passenger_thread(void *arg) {
    // get this passenger's passenger_t from arg
    passenger_t *pass = (passenger_t*) arg;
    // print "[Time: t] Passenger i entered the park"
    printf("[Time: %d] Passenger %d entered the park\n", get_time(), pass->id);
    // call explore_park(), get_ride_ticket(), then enter_ride_queue()
    explore_park(pass);
    get_ride_ticket(pass);
    enter_ride_queue(pass);

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

    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&ticket_mutex, NULL);

    printf("- Number of passenger threads: %d\n\n", n);

    srand(time(NULL));
    start_time = time(NULL);

    // Allocate an array of n passenger_t on the heap with malloc()
    passenger_t *passengers = malloc(sizeof(passenger_t) * n);
    // For each passenger i: set id to i, has_ticket to 0, and seed to rand()
    for (int i = 0; i < n; i++) {
        passengers[i].id = i;
        passengers[i].has_ticket = 0;
        passengers[i].seed = rand();
    }

    // Create n threads, each calling passenger_thread with a pointer to its own passenger_t
    pthread_attr_t attr[n];
    pthread_t threads[n];
    for (int i = 0; i < n; i++) {
        pthread_attr_init(&attr[i]);
        int rc = pthread_create(&threads[i], &attr[i], passenger_thread, &passengers[i]);
        if (rc) {
            printf("Error creating thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
    // Wait for all n threads to finish with pthread_join()
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }
    // print "\n[Time: t] All n passengers have tickets (tickets sold: k, ride queue size: s)"
    printf("\n[Time: %d] All %d passengers have tickets (tickets sold: %d, ride queue size: %d)\n", 
                get_time(), n, tickets_sold, ride_queue_size);
    // free the passenger array and destroy both mutexes
    free(passengers);

    pthread_mutex_destroy(&ticket_mutex);
    pthread_mutex_destroy(&queue_mutex);

    return 0;
}

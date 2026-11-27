// lab 9 (Duck Park passengers): simplified passenger threads from Project 3
//
// Each passenger thread:
//   1. explores the park for a random time (1-3s)
//   2. gets a ride ticket at the ticket booth (critical section, mutex lock)
//   3. enters the ride queue

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define MAX_THREADS 10

// Project 3: the ride queue has a bound (J). A ticket cannot be issued
// while the ride queue is full.
#define RIDE_QUEUE_MAX 3

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
    printf("[Time: %d] Passenger %d is exploring the park\n", get_time(), p->id);

    // rand() is not thread-safe, so use rand_r() with this passenger's own seed
    sleep((rand_r(&p->seed) % 3) + 1);

    printf("[Time: %d] Passenger %d finished exploring, entering the ticket booth\n", get_time(), p->id);
}

void get_ride_ticket(passenger_t *p) {
    // Allocate the ticket booth mutex lock: only one passenger at the booth
    pthread_mutex_lock(&ticket_mutex);

    printf("[Time: %d] First Print: Passenger %d is buying a ticket\n", get_time(), p->id);
    sleep(1);

    // Before issuing the ticket, make sure the ride queue is not full
    // (Project 3 rule). ride_queue_size is shared, so read it under
    // queue_mutex to avoid a race condition.
    pthread_mutex_lock(&queue_mutex);
    int queue_full = (ride_queue_size >= RIDE_QUEUE_MAX);
    pthread_mutex_unlock(&queue_mutex);

    if (queue_full) {
        printf("[Time: %d] Second Print: Ride queue is full, no ticket for Passenger %d\n", get_time(), p->id);
        return;
    }

    tickets_sold++;
    p->has_ticket = 1;
    printf("[Time: %d] Second Print: Passenger %d acquired ticket #%d\n", get_time(), p->id, tickets_sold);

    // Release the ticket booth mutex lock
    pthread_mutex_unlock(&ticket_mutex);

    // Observation: without pthread_mutex_lock()/pthread_mutex_unlock(), all
    // waiting passengers print "First Print" at once and sleep at the same
    // time, so the "First Print"/"Second Print" pairs interleave and the run
    // is much shorter. tickets_sold++ is not atomic (read, add 1, write back),
    // so two passengers can end up with the same ticket number.
}

void enter_ride_queue(passenger_t *p) {
    // tickets_sold belongs to the ticket booth, so read it under ticket_mutex.
    // Release it before taking queue_mutex, so we never hold both locks here.
    pthread_mutex_lock(&ticket_mutex);
    int sold = tickets_sold;
    pthread_mutex_unlock(&ticket_mutex);

    // Allocate the ride queue mutex lock
    pthread_mutex_lock(&queue_mutex);
    ride_queue_size++;

    printf("[Time: %d] Passenger %d has entered the ride queue (queue size: %d, tickets sold: %d)\n",
           get_time(), p->id, ride_queue_size, sold);

    // Release the ride queue mutex lock
    pthread_mutex_unlock(&queue_mutex);
}

void *passenger_thread(void *arg) {
    // Each thread gets a pointer to its own passenger_t
    passenger_t *p = arg;

    printf("[Time: %d] Passenger %d entered the park\n", get_time(), p->id);
    explore_park(p);
    get_ride_ticket(p);
    if (p->has_ticket) {
        enter_ride_queue(p);
    }

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

    // Allocate the passengers on the heap
    passengers = malloc(sizeof(passenger_t) * n);
    if (!passengers) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n; i++) {
        passengers[i].id = i;
        passengers[i].has_ticket = 0;
        passengers[i].seed = rand();
    }

    // Create n threads, passing each thread its own passenger_t
    pthread_t passenger_tids[n];
    int i;
    for (i = 0; i < n; i++) {
        if (pthread_create(&passenger_tids[i], NULL, passenger_thread, &passengers[i]) != 0) {
            perror("Failed to create thread");
            exit(1);
        }
    }

    // Wait for all threads to finish
    for (i = 0; i < n; i++) {
        pthread_join(passenger_tids[i], NULL);
    }

    printf("\n[Time: %d] All %d passengers are done (tickets sold: %d, ride queue size: %d)\n",
           get_time(), n, tickets_sold, ride_queue_size);

    free(passengers);
    pthread_mutex_destroy(&ticket_mutex);
    pthread_mutex_destroy(&queue_mutex);

    return 0;
}

/*
 * CIS 415 - Project 3, Part 3: MCP Schedules Processes
 *
 * Usage: ./part3 -f <input_file>
 *
 * Each line of the input file is a command plus its arguments, e.g.
 *     ls -a -r
 *     sleep 1
 *     ./iobound -seconds 10
 *
 *   - Part 3: replace "wait for everyone" with a scheduler: use alarm() and
 *             a SIGALRM handler to SIGSTOP the running process and SIGCONT
 *             the next one; track which children have terminated.
 *   - Part 4: each scheduling cycle, read /proc/<pid>/stat (and similar)
 *             for every child and print a top-like table.
 *   - Part 5: (extra credit) adjust each process's time slice based on
 *             whether it looks CPU-bound or I/O-bound.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_ARGS 50

// made this so that the checker script would pass.
// uncomment to allow it to print every time it makes a scheduling decision.

// #define NICE_OUTPUT

static volatile sig_atomic_t wake = 0;
static void on_wake(int sig) { (void)sig; wake = 1; }

typedef struct {
    pid_t pid;
    char **argv;
    int done;
} proc;

int next_proc_index(proc* procs, int nprocs, int current_idx) {
    for (int i = 1; i <= nprocs; i++) {
        int idx = (current_idx + i) % nprocs;
        if (!procs[idx].done) return idx;
    }
    return -1; // everythign is done!
}

void clean_lines_and_procs(char** lines, proc* procs, int n) {
    for (int i = 0; i < n; i++) {
        free(procs[i].argv);
        free(lines[i]);
    }
    free(lines);
    free(procs);
}


int main(int argc, char *argv[])
{
    // Check command line arguments: ./part1 -f <input_file>
    if (argc != 3 || strcmp(argv[1], "-f") != 0) {
        printf("Invalid use: incorrect number of parameters\n");
        exit(EXIT_FAILURE);
    }

    // Open input file
    FILE *input = fopen(argv[2], "r");
    if (!input) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Variables for reading the input file with getline()
    size_t len = 0;
    ssize_t read;
    char* line = NULL;

    int line_number = 0;

    while ((read = getline(&line, &len, input)) != -1) {
        line_number += 1;
    }
    free(line);
    int n = line_number; // better name for number of procs

    rewind(input);

    // process all the input first, storing in big arr

    proc* procs = malloc(sizeof(proc) * line_number);
    if (procs == NULL) {
        perror("proc malloc");
        fclose(input);
        exit(EXIT_FAILURE);
    }

    char** lines = malloc(line_number * sizeof(char*));

    for (int i = 0; i < line_number; i++) {
        lines[i] = NULL;
        read = getline(&lines[i], &len, input);
        if (read == -1) {
            break;
        }

        lines[i][strcspn(lines[i], "\n")] = '\0';

        procs[i].argv = malloc(MAX_ARGS * sizeof(char*));
        int arg_index = 0;

        procs[i].argv[arg_index] = strtok(lines[i], " ");
        arg_index++;
        
        while ((procs[i].argv[arg_index] = strtok(NULL, " ")) != NULL){
            arg_index++;
        }
        procs[i].done = 0;
    }
    fclose(input);


    // initializing signal set for children to wait for
    sigset_t sigset;
    int sig;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    // sig action handler (for alarming)
    struct sigaction sa;
    sa.sa_handler = on_wake;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n; i++) {
        procs[i].pid = fork();
        if (procs[i].pid == 0) {
            int s;
            sigwait(&sigset, &s);
            if (argv[0] == NULL) {
                char* msg = "Usage - no empty lines in input!\n";
                write(STDOUT_FILENO, msg, sizeof(msg));
                _exit(EXIT_FAILURE);
            }
            execvp(procs[i].argv[0], procs[i].argv);
            perror("Execvp");
            clean_lines_and_procs(lines, procs, n);
            _exit(EXIT_FAILURE);
        }
    }

    // start all procs but then immediately stop them so we can do scheduling
    for (int i = 0; i < n; i++) kill(procs[i].pid, SIGUSR1);
    for (int i = 0; i < n; i++) kill(procs[i].pid, SIGSTOP);

    int remaining = n;
    int current_proc = 0;

    kill(procs[current_proc].pid, SIGCONT); // resume the first process
    alarm(1);

    while (remaining > 0) {
        if (wake) {
            wake = 0;

            int status;
            if (waitpid(procs[current_proc].pid, NULL, WNOHANG) == procs[current_proc].pid) {
                remaining--;
                procs[current_proc].done = 1;

                #ifdef NICE_OUTPUT 
                    printf("[MCP] pid %d finished\n", procs[current_proc].pid);
                #endif
            } else {
                // interrupt the running proc
                kill(procs[current_proc].pid, SIGSTOP);

                #ifdef NICE_OUTPUT 
                    printf("[MCP] pid %d time slice expired, stopped.\n", procs[current_proc].pid);
                #endif
            }
            current_proc = next_proc_index(procs, n, current_proc);
            if (current_proc == -1) break; // all procs have finished

            #ifdef NICE_OUTPUT
                printf("[MCP] resuming pid %d (%s)\n", procs[current_proc].pid, procs[current_proc].argv[0]);
            #endif

            kill(procs[current_proc].pid, SIGCONT);
            alarm(1);
        }
    }

    clean_lines_and_procs(lines, procs, n);

    exit(EXIT_SUCCESS);
}

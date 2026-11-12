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

static volatile sig_atomic_t wake = 0;
static void on_wake(int sig) { (void)sig; wake = 1; }

typedef struct {
    pid_t pid;
    char **argv;
    int done;
} proc;

int next_proc_index(proc* procs, int nprocs, int current_idx) {
    for (int i = 1; i <= nprocs; i++) {
        int idx = (current_idx + i) % n;
        if (!procs[idx].done) return idx;
    }
    return -1; // everythign is done!
}


int select_and_run_next_proc() {
    if (pid_statuses[current_pid_idx] == NOT_BEGUN) {
        pid_array[current_pid_idx] = fork();

        //   - If fork() fails (< 0): report the error with perror(),
        //     free resources, and exit(EXIT_FAILURE).
        if (pid_array[current_pid_idx] < 0) {
            perror("Fork");
            free(pid_array);
            free(pid_statuses);
            exit(EXIT_FAILURE);
        }
        //   - In the child (== 0): call execvp(args[0], args).
        //     execvp() only returns on failure. In that case:
        //         perror("Execvp");
        //     then free memory, close the file, and exit(EXIT_FAILURE).
        //     (Why must the child exit here? What happens if it doesn't?)
        else if (pid_array[current_pid_idx] == 0) {
            int received_signal;
            if (sigwait(&sigset, &received_signal) == 0) {
                execvp(args_for_procs[current_pid_idx][0], args_for_procs[current_pid_idx]);
                perror("Execvp");
                free(pid_array);
                free(pid_statuses);
                exit(EXIT_FAILURE);
            } else {
                perror("sigwait");
                free(pid_array); free(pid_statuses); 
                exit(EXIT_FAILURE);
            }
        } else {
            // parent branch
            current_pid_idx++;
        }

    } else if (pid_statuses[current_pid_idx] == PAUSED) {
        // resume process
        if ()
    } else if (pid_statuses[current_pid_idx] == FINISHED) {
        current_pid_idx++;
    }
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
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    int line_number = 0;

    while ((read = getline(&line, &len, input)) != -1) {
        line_number += 1;
    }
    int n = line_number; // better name for number of procs

    rewind(input);

    // process all the input first, storing in big arr

    proc* procs = malloc(sizeof(proc) * line_number);
    if (procs == NULL) {
        perror("proc malloc");
        fclose(input);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < line_number; i++) {
        read = getline(&line, &len, input);
        if (read == -1) {
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        procs[i].argv = malloc(MAX_ARGS * sizeof(char*));
        int arg_index = 0;

        procs[i].argv[arg_index] = strdup(strtok(line, " "));
        arg_index++;
        
        while ((procs[i].argv[arg_index] = strdup(strtok(NULL, " "))) != NULL){
            arg_index++;
        }
        procs[i].done = 0;
    }

    free(line);

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
            sigwait(&usr1, &s);
            execvp(procs[i].argv[0], procs[i].argv);
            perror("execvp");
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
            // if it has changed state (to finished?), will return its pid
            if (waitpid(procs[current_proc].pid, &status, WNOHANG) == procs[current_proc].pid) {
                remaining--;
                procs[current_proc].done = 1;
                printf("[MCP] pid %d finished\n", procs[current_proc].pid);
            } else {
                // interrupt the running proc
                kill(procs[current_proc].pid, SIGSTOP);
                printf("[MCP] pid %d time slice expired, stopped.\n", procs[current_proc].pid);
            }
            current_proc = next_proc_index(procs, n, current_proc);
            if (current_proc == -1) break; // all procs have finished
            printf("[MCP] resuming pid %d (%s)\n", procs[current_proc].pid, procs[current_proc].argv[0]);
            kill(procs[current_proc].pid, SIGCONT);
            alarm(1);
        }
    }
    fclose(input);

    for (int i = 0; i < n; i++) {
        free(procs[i].argv);
    }
    free(procs);
    exit(EXIT_SUCCESS);
}

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

int current_pid_idx;
int finished_procs;

pid_t* pid_array;
int* pid_statuses;
char*** args_for_procs;


enum childStatus {
    NOT_BEGUN,
    RUNNING,
    PAUSED,
    FINISHED
};


void send_to_children(pid_t* pid_array, int num_children, int signal) {
    pid_t childPID;
    for (int i = 0; i < num_children; i++) {
        childPID = pid_array[i];
        kill(childPID, signal);
    }
}

void sigalrm_handler(int signum) {
    char* message = "received signal alarm\n";
    write(STDOUT_FILENO, message, strlen(message));
    kill(pid_array[current_pid_idx], SIGSTOP);

    return;
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

    rewind(input);

    // process all the input first, storing in big arr
    args_for_procs = malloc(sizeof(char**) * line_number);

    for (int i = 0; i < line_number; i++) {
        read = getline(&line, &len, input);
        if (read == -1) {
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        args_for_procs[i] = malloc(MAX_ARGS * sizeof(char*));
        int arg_index = 0;

        args_for_procs[i][arg_index] = strtok(line, " ");
        arg_index++;
        
        while ((args_for_procs[i][arg_index] = strtok(NULL, " ")) != NULL){
            printf("%s\n", args_for_procs[i][arg_index]);
            arg_index++;
        }
    }

    pid_array = malloc(sizeof(pid_t*) * line_number);
    pid_statuses = malloc(sizeof(int) * line_number);

    if (pid_array == NULL || pid_statuses == NULL) {
	    perror("malloc");
	    return 1;
    }

    // initializing signal set for children to wait for
    sigset_t sigset;
    int sig;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    // sig action handler (for alarming)
    struct sigaction sa;
    sa.sa_handler = sigalrm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < line_number; i++) {
        pid_statuses[i] = NOT_BEGUN;
    }

    // int finished_procs = 0;
    // int current_child_index = 0;


    // while (finished_procs != line_number) {
    //     if (pid_statuses[current_child_index] == NOT_BEGUN) {
    //         read = getline(&line, &len, input);
    //         if (read == -1) {
    //             break;
    //         }

    //         line[strcspn(line, "\n")] = '\0';

    //         char *args[MAX_ARGS];
    //         int arg_index = 0;

    //         args[arg_index] = strtok(line, " ");
    //         arg_index++;
            
    //         while ((args[arg_index] = strtok(NULL, " ")) != NULL){
    //             arg_index++;
    //         }

    //         pid_array[i] = fork();


    //         //   - If fork() fails (< 0): report the error with perror(),
    //         //     free resources, and exit(EXIT_FAILURE).
    //         if (pid_array[i] < 0) {
    //             perror("Fork");
    //             free(line);
    //             free(pid_array);
    //             free(pid_statuses);
    //             fclose(input);
    //             exit(EXIT_FAILURE);
    //         }
    //         //   - In the child (== 0): call execvp(args[0], args).
    //         //     execvp() only returns on failure. In that case:
    //         //         perror("Execvp");
    //         //     then free memory, close the file, and exit(EXIT_FAILURE).
    //         //     (Why must the child exit here? What happens if it doesn't?)
    //         else if (pid_array[i] == 0) {
    //             int received_signal;
    //             if (sigwait(&sigset, &received_signal) == 0) {
    //                 execvp(args[0], args);
    //                 perror("Execvp");
    //                 free(line);
    //                 free(pid_array);
    //                 free(pid_statuses);
    //                 fclose(input);
    //                 exit(EXIT_FAILURE);
    //             } else {
    //                 perror("sigwait");
    //                 free(line); free(pid_array); free(pid_statuses); 
    //                 fclose(input);
    //                 exit(EXIT_FAILURE);
    //             }
    //         } else {
    //             // parent branch
    //             current_child_index++;
    //         }

    //     } else if (pid_statuses[current_child_index] == PAUSED) {
    //         // resume process
    //         if ()
    //     } else if (pid_statuses[current_child_index] == FINISHED) {
    //         current_child_index++;
    //     }
    //     //   - In the parent (> 0): do NOT wait here! Continue the loop so
    //     //     that all processes run at the same time.

    //     //   (Part 2 hint: this child branch is where the child will later
    //     //    sigwait() for SIGUSR1 before calling execvp().)

    // }

    // // while (finished_procs != line_number) {
    // //     if (pid_statuses[current_child_index] == NOT_BEGUN) {
    // //         kill(pid_array)
    // //     }
    // //     printf("current child index is %d\n", current_child_index);
    // //     kill(pid_array[current_child_index], SIGSTOP);
    // //     pid_statuses

    // //     current_child_index = (current_child_index + 1) % line_number;
    // //     if 

    // //     kill(pid_array[current_child_index], SIGCONT);
    // //     alarm(1);
    // // }

    // // send each process a SIGUSR1 signal
    // send_to_children(pid_array, line_number, SIGUSR1);
    // sleep(1);

    // // i have to 

    // // send each process a SIGSTOP signal
    // send_to_children(pid_array, line_number, SIGSTOP);
    // sleep(1);

    // send_to_children(pid_array, line_number, SIGCONT);

    // int status;
    // for (int i = 0; i < line_number; i++) {
    //     waitpid(pid_array[i], &status, 0);
    // }


    // fclose(input);
    // free(line);
    // free(pid_array);

    // exit(EXIT_SUCCESS);

}

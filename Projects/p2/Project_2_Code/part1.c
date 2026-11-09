/*
 * CIS 415 - Project 2, Part 1: MCP Launches the Workload (MCP v1.0)
 *
 * Usage: ./part1 -f <input_file>
 *
 * Each line of the input file is a command plus its arguments, e.g.
 *     ls -a -r
 *     sleep 1
 *     ./iobound -seconds 10
 *
 * Your MCP v1.0 must:
 *   1. Read every command from the input file.
 *   2. For each command, fork() a child that runs it with execvp().
 *   3. After ALL children have been launched, wait for each one (waitpid).
 *   4. Free all memory, close the file, and exit().
 *
 * Notes on output (the test script compares output character by character):
 *   - Do NOT print extra debug messages in your final submission.
 *   - If execvp() fails, report it with exactly: perror("Execvp");
 *   - Bad arguments must print exactly:
 *         "Invalid use: incorrect number of parameters\n"
 *
 * Always check your code with valgrind:
 *     valgrind --leak-check=full ./part1 -f input.txt
 *
 * Looking ahead: Parts 2, 3, 4 and 5 are completed by EXTENDING this file.
 * Once part1 works (and is valgrind-clean), copy it to part2.c and build on
 * it, then copy part2.c to part3.c, and so on. Keep the code clean and
 * modular now (e.g. a helper that parses a line into args[]) so that later
 * parts are easy to add:
 *   - Part 2: in the child, block SIGUSR1 and sigwait() for it before
 *             execvp(); the parent then sends SIGUSR1, SIGSTOP and SIGCONT
 *             to every child before waiting.
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

    // TODO 1: Count the number of lines (commands) in the input file.
    //         Hint: loop over getline() until it returns -1,
    //         then rewind() the file so you can read it again.
    int line_number = 0;

    while ((read = getline(&line, &len, input)) != -1) {
        line_number += 1;
    }

    rewind(input);

    // TODO 2: Allocate an array to hold one pid_t per command.
    //         Remember to check the return value of malloc().
    pid_t *pid_array = NULL;
    pid_array = malloc(sizeof(pid_t*) * line_number);

    if (pid_array == NULL) {
	    perror("malloc");
	    return 1;
    }

    // Launch one process per line
    for (int i = 0; i < line_number; i++) {
        read = getline(&line, &len, input);
        if (read == -1) {
            break;
        }


        // TODO 3: Remove the trailing newline character (if any).
        line[strcspn(line, "\n")] = '\0';

        // TODO 4: Split the line into tokens (strtok with " ") and store
        //         them in args[]. The array must end with a NULL pointer,
        //         since execvp() expects a NULL-terminated argument list.
        char *args[MAX_ARGS];
        int arg_index = 0;

        args[arg_index] = strtok(line, " ");
        arg_index++;
        
        while ((args[arg_index] = strtok(NULL, " ")) != NULL){
            arg_index++;
        }

        args[++arg_index] = NULL;

        // TODO 5: fork() a child process and save its pid in pid_array[i].
        pid_array[i] = fork();

        //   - If fork() fails (< 0): report the error with perror(),
        //     free resources, and exit(EXIT_FAILURE).
        if (pid_array[i] < 0) {
            perror("Fork");
            free(line);
            free(pid_array);
            fclose(input);
            exit(EXIT_FAILURE);
        }
        //   - In the child (== 0): call execvp(args[0], args).
        //     execvp() only returns on failure. In that case:
        //         perror("Execvp");
        //     then free memory, close the file, and exit(EXIT_FAILURE).
        //     (Why must the child exit here? What happens if it doesn't?)
        else if (pid_array[i] == 0) {
            execvp(args[0], args);
            perror("Execvp");
            free(line);
            free(pid_array);
            fclose(input);
            exit(EXIT_FAILURE);
        } 
        //   - In the parent (> 0): do NOT wait here! Continue the loop so
        //     that all processes run at the same time.
        else {
            continue;
        }
        //   (Part 2 hint: this child branch is where the child will later
        //    sigwait() for SIGUSR1 before calling execvp().)

    }

    // TODO 6: Now that every process has been launched, wait for each
    //         child to terminate using waitpid() on pid_array[i].
    int status;
    for (int i = 0; i < line_number; i++) {
        waitpid(pid_array[i], NULL, 1);
    }
    //   (Part 2/3 hint: the signaling and scheduling logic for later parts
    //    goes here, between launching the children and the final cleanup.)


    // TODO 7: Clean up: free line and pid_array, close the input file.
    fclose(input);
    free(line);
    free(pid_array);

    exit(EXIT_SUCCESS);

}

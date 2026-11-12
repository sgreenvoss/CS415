/*
 * CIS 415 - Project 4, Part 4: MCP Knows All
 *
 * Usage: ./part4 -f <input_file>
 *
 * Each line of the input file is a command plus its arguments, e.g.
 *     ls -a -r
 *     sleep 1
 *     ./iobound -seconds 10
 *
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

void read_memory(pid_t pid, long* _vmrss, long* _vmsize) {
    long vmrss = -1, vmsize = -1;

    int pathname_max = 255;
    char pathname[pathname_max];

    if (snprintf(pathname, pathname_max, "/proc/%d/status", pid) < 0) {
        perror("Building path");
    };

    FILE* proc_status = fopen(pathname, "r");

    if (proc_status == NULL) {
        perror("Couldn't open file");
        return;
    }

    int line_max = 512; // setting low just for now, can revise later
    char line[line_max];

    while (fgets(line, line_max, proc_status) != NULL) {
        if (strncmp("VmSize", line, 6) == 0) {
            sscanf(line, "VmSize:     %ld kB", &vmsize);
        } else if (strncmp("VmRSS", line, 5) == 0) {
            sscanf(line, "VmRSS:      %ld kB", &vmrss);
        }
    }

    fclose(proc_status);
    *_vmrss = vmrss;
    *_vmsize = vmsize;

}


// Read CPU execution time: utime + stime
//
// /proc/<pid>/stat is a single line of space-separated fields. Some of them:
//     field 1:  pid
//     field 2:  comm (executable name in parentheses)
//     field 3:  state (one character, e.g. R, S)
//     ...
//     field 14: utime - time spent in user mode,   in clock ticks
//     field 15: stime - time spent in kernel mode, in clock ticks
//
// See "man 5 proc" for the full list and each field's type.
double read_time(pid_t pid) {
    double total_sec = 0.0;
    double utime = 0.0, stime = 0.0;

    int pathname_max = 255;
    char pathname[pathname_max];

    if (snprintf(pathname, pathname_max, "/proc/%d/stat", pid) < 0) {
        perror("Building path");
        exit(EXIT_FAILURE);
    };

    FILE* proc_stat = fopen(pathname, "r");

    if (proc_stat == NULL) {
        perror("Couldn't open file");
        exit(EXIT_FAILURE);
    }

    int buf_max = 4098;
    char buf[buf_max];

    if (fgets(buf, buf_max, proc_stat) == NULL) {
        perror("Reading file");
        exit(EXIT_FAILURE);
    }

    sscanf(buf,
           "%*d %*s %*c "
           "%*d %*d %*d %*d %*d "
           "%*u %*u %*u %*u %*u "
           "%lf %lf",
           &utime, &stime);

    total_sec = (utime + stime) / (double) sysconf(_SC_CLK_TCK);


    fclose(proc_stat);

    return total_sec;
}


// Read I/O statistics: read_bytes, write_bytes
//
// /proc/<pid>/io has one "key: value" pair per line, e.g.
//     rchar: 3919
//     wchar: 510
//     ...
//     read_bytes: 0
//     write_bytes: 4096
//
//   read_bytes  - bytes this process actually caused to be fetched from storage
//   write_bytes - bytes this process caused to be sent to storage
void read_io(pid_t pid, long* read, long* write) {
    long read_b = -1, write_b = -1;

    int pathname_max = 255;
    char pathname[pathname_max];

    if (snprintf(pathname, pathname_max, "/proc/%d/io", pid) < 0) {
        perror("Building path");
    };

    FILE* proc_io = fopen(pathname, "r");

    if (proc_io == NULL) {
        perror("Couldn't open file");
        return;
    }

    int line_max = 512;
    char line[line_max];

    while (fgets(line, line_max, proc_io) != NULL) {
        if (strncmp("read_bytes", line, 10) == 0) {
            sscanf(line, "read_bytes: %ld", &read_b);
        } else if (strncmp("write_bytes", line, 11) == 0) {
            sscanf(line, "write_bytes: %ld", &write_b);
        }
    }

    fclose(proc_io);

    *read = read_b;
    *write = write_b;
}

void get_name(pid_t pid, char* line, size_t buff_size) {
    int pathname_max = 255;
    char pathname[pathname_max];

    if (snprintf(pathname, pathname_max, "/proc/%d/comm", pid) < 0) {
        perror("Building path");
    };

    FILE* proc_comm = fopen(pathname, "r");

    if (proc_comm == NULL) {
        perror("Couldn't open file");
        return;
    }

    if (fgets(line, buff_size, proc_comm) != NULL) {
        line[strcspn(line, "\n")] = '\0';
    }

    fclose(proc_comm);
}


void read_proc_info(pid_t child) {
    printf("\n%-8s | %-8s | %-8s | %-8s | %-8s | %-8s | %-8s\n", "pid","name","read","write","seconds","vmrss","vmsize");
    long read, write, vmrss, vmsize;
    double seconds;
    char name[255];

    read_memory(child, &vmrss, &vmsize);
    seconds = read_time(child);
    read_io(child, &read, &write);
    
    get_name(child, name, sizeof(name));

    printf("%-8d | %-8s | %-8ld | %-8ld | %-8lf | %-8ld | %-8ld \n", child, name, read, write, seconds, vmrss, vmsize);
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

    char** lines = malloc(line_number * MAX_ARGS);

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
            execvp(procs[i].argv[0], procs[i].argv);
            perror("execvp");
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
            // if it has changed state (to finished?), will return its pid
            if (waitpid(procs[current_proc].pid, &status, WNOHANG) == procs[current_proc].pid) {
                remaining--;
                procs[current_proc].done = 1;

                #ifdef NICE_OUTPUT 
                    printf("[MCP] pid %d finished\n", procs[current_proc].pid);
                #endif
            } else {
                // interrupt the running proc
                read_proc_info(procs[current_proc].pid);
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

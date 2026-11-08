/*
 * CIS 415 - Lab 6: Exploring the Linux /proc Filesystem
 *
 * The kernel exposes information about every running process under
 * /proc/<pid>/. In this lab the parent process forks a child that does
 * some CPU work and file I/O, then the parent inspects the child through /proc.
 *
 * Your task: complete the three functions marked TODO:
 *   1. read_memory() - parse /proc/<pid>/status for VmRSS and VmSize
 *   2. read_time()   - parse /proc/<pid>/stat   for utime and stime
 *   3. read_io()     - parse /proc/<pid>/io     for read_bytes and write_bytes
 *
 * Useful references:
 *   man 5 proc      (look for /proc/pid/status, /proc/pid/stat, /proc/pid/io)
 *   man 3 sysconf   (_SC_CLK_TCK)
 *
 * Tip: look at the real files first, e.g.
 *   cat /proc/self/status
 *   cat /proc/self/stat
 *   cat /proc/self/io
 *
 * Compile: gcc -Wall -o lab6 lab6_student.c
 * Run:     ./lab6
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>

// Read memory info: VmRSS and VmSize
//
// /proc/<pid>/status is a text file with one "Key:   value" pair per line, e.g.
//     VmSize:     2680 kB
//     VmRSS:      1024 kB
//
//   VmSize - total virtual memory size of the process
//   VmRSS  - resident set size (memory actually in physical RAM)
void read_memory(pid_t pid) {
    long vmrss = -1, vmsize = -1;

    int pathname_max = 255;
    char pathname[pathname_max];

    // TODO 1: Build the path "/proc/<pid>/status" (hint: snprintf).
    if (snprintf(pathname, pathname_max, "/proc/%d/status", pid) < 0) {
        perror("Building path");
    };
    // TODO 2: Open the file with fopen(). If it fails, print an error
    //         with perror() and return.
    FILE* proc_status = fopen(pathname, "r");

    if (proc_status == NULL) {
        perror("Couldn't open file");
        return;
    }

    // TODO 3: Read the file line by line (hint: fgets). For each line:
    //           - if it starts with "VmSize", parse the number into vmsize
    //           - if it starts with "VmRSS",  parse the number into vmrss
    //         (hint: strncmp to match the prefix, sscanf to parse the value)
    int line_max = 512; // setting low just for now, can revise later
    char line[line_max];

    while (fgets(line, line_max, proc_status) != NULL) {
        if (strncmp("VmSize", line, 6) == 0) {
            sscanf(line, "VmSize:     %ld kB", &vmsize);
        } else if (strncmp("VmRSS", line, 5) == 0) {
            sscanf(line, "VmRSS:      %ld kB", &vmrss);
        }
    }


    // TODO 4: Close the file.
    fclose(proc_status);

    printf("[Memory] VmRSS: %ld kB, VmSize: %ld kB\n", vmrss, vmsize);
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
void read_time(pid_t pid) {
    double total_sec = 0.0;
    double utime = 0.0, stime = 0.0;

    // TODO 1: Build the path "/proc/<pid>/stat" and open it.
    //         Handle the error case as in read_memory().
    int pathname_max = 255;
    char pathname[pathname_max];

    if (snprintf(pathname, pathname_max, "/proc/%d/stat", pid) < 0) {
        perror("Building path");
    };

    FILE* proc_stat = fopen(pathname, "r");

    if (proc_stat == NULL) {
        perror("Couldn't open file");
        return;
    }

    // TODO 2: Read the whole line into a buffer (hint: fgets).
    int buf_max = 4098;
    char buf[buf_max];

    if (fgets(buf, buf_max, proc_stat) == NULL) {
        perror("Reading file");
    }

    // TODO 3: Extract utime (field 14) and stime (field 15).
    //         Hint: in sscanf, "%*d" or "%*s" reads a field and discards it,
    //         so you can skip the first 13 fields.

    sscanf(buf,
           "%*d %*s %*c "
           "%*d %*d %*d %*d %*d "
           "%*u %*u %*u %*u %*u "
           "%lf %lf",
           &utime, &stime);

    // TODO 4: utime and stime are in clock ticks. Convert their sum to seconds.
    //         Hint: sysconf(_SC_CLK_TCK) gives the number of ticks per second.

    total_sec = (utime + stime) / (double) sysconf(_SC_CLK_TCK);

    // TODO 5: Close the file.

    fclose(proc_stat);

    printf("[CPU] Total CPU time: %.4f sec\n", total_sec);
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
//
// Question to think about: how do rchar/wchar differ from read_bytes/write_bytes?
void read_io(pid_t pid) {
    long read_b = -1, write_b = -1;

    // TODO 1: Build the path "/proc/<pid>/io" and open it.
    //         Handle the error case as in read_memory().
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

    // TODO 2: Read line by line and parse "read_bytes" into read_b
    //         and "write_bytes" into write_b.
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

    // TODO 3: Close the file.

    printf("[IO] read_bytes: %ld bytes, write_bytes: %ld bytes\n", read_b, write_b);
}


// ===================== Do not modify below this line =====================

// Parent reads /proc information of the child
void read_proc_info(pid_t child) {
    printf("\n========= Reading /proc/%d =========\n", child);
    read_memory(child);
    read_time(child);
    read_io(child);
    printf("====================================\n\n");
}


// Main
int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    // Child process: simulate some workload
    if (pid == 0) {
        printf("Child PID: %d running workload...\n", getpid());

        // 1) CPU work
        long sum = 0;
        for (long i = 0; i < 200000000; i++) {
            sum += (i & 7);
        }

        // 2) File write
        int fd = open("child_output.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) {
            char buf[] = "This is a test line written by the child process.\n";
            for (int i = 0; i < 10; i++) {
                write(fd, buf, strlen(buf));
            }
            close(fd);
        }

        // 3) File read
        fd = open("child_output.txt", O_RDONLY);
        if (fd >= 0) {
            // Force kernel to drop cached pages for this file
            posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);

            char buffer[1024];
            ssize_t n;
            while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
                fwrite(buffer, 1, n, stdout);
            }
            close(fd);
        }

        sleep(2);
        printf("Child finished: sum=%ld\n", sum);
        return 0;
    }

    // Parent process
    printf("Parent: child PID = %d\n", pid);

    sleep(1);

    read_proc_info(pid);

    waitpid(pid, NULL, 0);

    printf("Parent: child exited.\n");
    return 0;
}

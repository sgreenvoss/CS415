/*
 * CIS 415 Lab 5 - Signal Processing in C
 *
 * Goal:
 *   1. Fork <num_processes> children (at least 5). Each child prints a message,
 *      then WAITS for SIGUSR1 before calling exec() on ./iobound.
 *   2. Write signaler(), which sleeps 3 seconds, then sends one signal to
 *      every child in the pool.
 *   3. The parent calls signaler() four times, in this order:
 *         SIGUSR1 -> SIGSTOP -> SIGCONT -> SIGINT
 *
 * Build:  make
 * Run:    ./lab5 5
 * Check:  ./checker.sh
 *
 * Useful man pages:
 *   man 3 sigwait, man 3 sigemptyset, man 3 sigaddset, man 2 sigprocmask,
 *   man 2 kill,    man 3 sleep,       man 3 strsignal
 */
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>     // strsignal()

void signaler(pid_t* pid_ary, int size, int signal);

int main(int argc, char* argv[])
{


	if (argc < 2){

		printf("Input: ./lab5 <num_processes>\n");
		exit(-1);

	}

	//helpful print statement
	printf("the value of SIGUSR1 is: %d \n",SIGUSR1);
	printf("the value of SIGSTOP is: %d \n",SIGSTOP);
	printf("the value of SIGCONT is: %d \n",SIGCONT);
	printf("the value of SIGINT is: %d \n",SIGINT);

	// initialization of pid like lab 4

	int size = atoi(argv[1]);

	pid_t *pid_ary = (pid_t *)malloc(sizeof(pid_t) * size);

	// argument vector for execvp(): program name first, NULL-terminated
	char *args[] = {"./iobound", "-seconds", "4", 0};

	// initialize sigset
	sigset_t sigset;
	int sig;        // sigwait() stores the number of the received signal here

	/*
	 * ------------------------------------------------------------------
	 * TODO 1: Block SIGUSR1 BEFORE forking.
	 * ------------------------------------------------------------------
	 * Why: sigwait() only works for signals that are BLOCKED. If SIGUSR1 is
	 * not blocked, its default action (terminate the process) runs as soon
	 * as it arrives and the child dies instead of returning from sigwait().
	 *
	 * Why before fork(): a child inherits its parent's signal mask. Blocking
	 * here, once, means every child starts with SIGUSR1 already blocked, so
	 * there is no window where a SIGUSR1 could kill a child too early.
	 *
	 * Steps:
	 *   a. sigemptyset(&sigset);                  -> start with an empty set
	 *   b. sigaddset(&sigset, SIGUSR1);           -> put SIGUSR1 in the set
	 *   c. sigprocmask(SIG_BLOCK, &sigset, NULL); -> block every signal in the set
	 *
	 * Tip: each of these returns -1 on error; you can check it and call perror().
	 */

	// create an empty sigset_t
	if (sigemptyset(&sigset) == -1) {
		perror("sigemptyset failed");
	};
	// use sigaddset() to add the SIGUSR1 signal to the set
	if (sigaddset(&sigset, SIGUSR1) == -1) {
		perror("sigaddset failed");
	};
	// use sigprocmask() to add the signal set in the sigset for blocking
	if (sigprocmask(SIG_BLOCK, &sigset, NULL) == -1) {
		perror("sigprocmask failed");
	};

	for(int i = 0; i < size; i++)
	{

		pid_ary[i] = fork();

		if(pid_ary[i] == 0)
		{
			/*
			 * --------------------------------------------------------------
			 * TODO 2: Child process code.
			 * --------------------------------------------------------------
			 * Remember: inside the child, pid_ary[i] is 0. Use getpid() to
			 * get the child's own PID for the messages below.
			 */

			// print: Child Process: <pid> - Waiting for SIGUSR1...
			//   e.g. printf("Child Process: %d - Waiting for SIGUSR1...\n", getpid());
			printf("Child process: %d - waiting for SIGUSR1...\n", getpid());

			// wait for the signal
			//   - Call sigwait(&sigset, &sig);
			//   - The child sleeps here until a signal in sigset (SIGUSR1) arrives.
			//   - It returns 0 on success, and sig then holds the signal number (10).
			//   - Only continue to exec() if sigwait() returned 0.
			if (sigwait(&sigset, &sig) != 0) {
				perror("sigwait didn't return 0");
				exit(1);
			};

			// print: Child Process: <pid> - Received signal: SIGUSR1 - Calling exec().
			//   Tip: stdout may be buffered; call fflush(stdout) before exec(),
			//   otherwise text still sitting in the buffer is lost when exec()
			//   replaces the process image.
			printf("Child Process: %d - Received signal: SIGUSR1 - calling exec().\n", getpid());
			fflush(stdout);

			// call execvp with ./iobound like in lab 4
			//   - execvp(args[0], args);   (args is declared above main's loop)
			//   - execvp() only returns if it FAILED, so print an error with
			//     perror("execvp") right after it.
			execvp(args[0], args);
			perror("execvp");

			exit(0);    // only reached if exec failed (or sigwait failed)

		}
		else if (pid_ary[i] < 0) {
			// fork failed error
			//   - perror("fork"); free(pid_ary); then exit with a non-zero status
			perror("fork");
			free(pid_ary);
			exit(-1);
		}
	}

	/*
	 * ----------------------------------------------------------------------
	 * TODO 3: Parent sends the signals, one round per call to signaler().
	 * ----------------------------------------------------------------------
	 * Call signaler(pid_ary, size, <SIGNAL>) four times, in THIS order:
	 *   SIGUSR1 -> children leave sigwait() and exec ./iobound
	 *   SIGSTOP -> children are paused (state 'T' in `ps`)
	 *   SIGCONT -> paused children resume running
	 *   SIGINT  -> children are terminated (same as pressing CTRL-C)
	 */

	// send SIGUSR1
	signaler(pid_ary, size, SIGUSR1);
	// send SIGSTOP
	signaler(pid_ary, size, SIGSTOP);
	// send SIGCONT
	signaler(pid_ary, size, SIGCONT);
	// send SIGINT
	signaler(pid_ary, size, SIGINT);


	free(pid_ary);

	return 0;
}

/*
 * signaler - send one signal to every child process in the pool.
 *
 * pid_ary : array of child PIDs (the process pool)
 * size    : number of children in pid_ary
 * signal  : the signal to send (SIGUSR1, SIGSTOP, SIGCONT or SIGINT)
 */
void signaler(pid_t* pid_ary, int size, int signal) {
	/*
	 * --------------------------------------------------------------------
	 * TODO 4: Implement signaler().
	 * --------------------------------------------------------------------
	 */

	// sleep for three seconds
	//   - sleep(3);  gives the children time to reach sigwait() / run iobound
	//     before the next round of signals is sent.

	for(int i = 0; i < size; i++)
	{
		// print: Parent process: <pid> - Sending signal: <signal> to child process: <pid>
		//   - parent pid : getpid()
		//   - child pid  : pid_ary[i]
		//   - <signal>   : use strsignal(signal) to print the signal's name
		//                  (e.g. "User defined signal 1") instead of the number.
		//   e.g. printf("Parent process: %d - Sending signal: %s to child process: %d\n",
		//               getpid(), strsignal(signal), pid_ary[i]);
		printf("Parent process: %d - Sending signal: %s to child process: %d\n", 
				getpid(), strsignal(signal), pid_ary[i]);
		

		// send the signal
		//   - kill(pid_ary[i], signal);
		//   - kill() returns -1 on failure; if so, report it with perror("kill").
		if (kill(pid_ary[i], signal) == -1) {
			perror("kill");
		};

	}
}

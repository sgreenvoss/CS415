/*
 * main_student.c - CS415 Lab2 starter code
 *
 * Your program runs in one of two modes:
 *
 *   1. Interactive mode:   ./lab2
 *        - Print the prompt ">>> ", read one command from stdin, run it,
 *          and repeat until the user types "exit".
 *
 *   2. File mode:          ./lab2 -f <input_file>
 *        - Read commands from <input_file>, one per line.
 *        - All output goes to "output.txt" (in the current directory),
 *          NOT to the terminal.
 *
 * Supported commands:
 *   ls    -> call listDir() (see command.h / command.c)
 *   exit  -> free everything and end the program
 *   other -> print exactly:  "Error! Unrecognized command: <cmd>\n"
 *
 * Example (interactive mode, run from inside files/):
 *   >>> ls
 *   1_poem.txt 2_lyrics.txt 3_DE_Code.py
 *   >>> cat
 *   Error! Unrecognized command: cat
 *   >>> exit
 *
 * Helpers you already have:
 *   - str_filler() / free_command_line() from Lab 1 (string_parser.h).
 *     str_filler(line, " ") splits a line into tokens and removes the
 *     trailing '\n'. cmd.command_list[0] is the command name.
 *     Every command_line returned by str_filler() must be freed with
 *     free_command_line().
 *   - listDir() from command.h.
 *
 * Requirements:
 *   - Use write(STDOUT_FILENO, ...) for your own output.
 *   - Your program must pass valgrind with NO memory errors and with
 *     "All heap blocks were freed" (this includes closing every FILE*).
 *   - Run ./checker.sh to test your work.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "command.h"
#include "string_parser.h"


int main(int argc, char* argv[]){
    size_t len = 1024;
    char *line = malloc(len * sizeof(char));
    size_t size;

    /* ------------------------------------------------------------------ */
    /*  Interactive mode                                                   */
    /* ------------------------------------------------------------------ */
    if (argc == 1) {
        while (1) {
            /* TODO 1: Print the prompt ">>> " using write().
             * Hint: the prompt is 4 bytes and has no newline. */
		write(STDOUT_FILENO, ">>> ", 4);

            /* TODO 2: Read one line from stdin with getline().
             * Hint: getline(&line, &len, stdin) returns -1 on EOF
             *       (e.g. the user presses Ctrl-D). */
		line = NULL;

		ssize_t res = getline(&line, &size, stdin);
		if (res == -1) {
			// do something to return
			return 0;
		}
            /* TODO 3: Split the line into tokens with str_filler().
             * Hint: use " " as the delimiter. */
		command_line cmd;
		cmd = str_filler(line, " ");

            /* TODO 4: If the line was empty (cmd.num_token == 0), there is
             * no command to run. Skip to the next loop iteration.
             * Hint: what happens if you read cmd.command_list[0] here? */
		if (cmd.num_token == 0) {
			continue;
		}

            /* TODO 5: Run the command.
             *   - "ls"   -> call listDir()
             *   - "exit" -> free everything you allocated, then exit(0)
             *   - other  -> print "Error! Unrecognized command: <cmd>\n"
             * Hint: compare strings with strcmp(), not ==.
             * Hint: before exit(0), think about which heap memory is still
             *       allocated (cmd? line?). valgrind will tell you if you
             *       forget something. */
		
		if (strcmp(cmd.command_list[0], "ls") == 0) {
			listDir();
		} else if (strcmp(cmd.command_list[0], "exit") == 0) {
			free(line);
			free_command_line(&cmd);
			exit(0);
		} else {
			printf("Error! Unrecognized command: %s\n", cmd.command_list[0]);
		}

            /* TODO 6: Free this command's tokens before the next loop. */
		free_command_line(&cmd);
		free(line);
        }
    }

    /* ------------------------------------------------------------------ */
    /*  File mode                                                          */
    /* ------------------------------------------------------------------ */
    else if (argc == 3 && strcmp(argv[1], "-f") == 0) {
        /* TODO 7: Open the input file (argv[2]) for reading with fopen().
         * Hint: check for NULL and print an error with perror() if the
         *       file cannot be opened. */
        FILE *fin = NULL;
	fin = fopen(argv[2], "r");
	if (fin == NULL) {
		perror("File cannot be opened");
		exit(1);
	}

        /* TODO 8: Redirect stdout to "output.txt" so that everything the
         * program prints (including listDir()) goes into that file.
         * Hint: look up freopen(). Use mode "w" so the file starts empty
         *       each run. */
	FILE *fout = NULL;
	fout = freopen("output.txt", "w", stdout);

        /* TODO 9: Read the input file line by line until EOF.
         * Hint: same getline() call as interactive mode, but read from
         *       fin instead of stdin. For each line, do the same steps as
         *       TODO 3-6 (no prompt in file mode). */

	while (getline(&line, &size, fin) != -1) {
		command_line cmd;
		cmd = str_filler(line, " ");

		if (cmd.num_token == 0) {
			continue;
		}	
		if (strcmp(cmd.command_list[0], "ls") == 0) {
			listDir();	
		} else if (strcmp(cmd.command_list[0], "exit") == 0) {
			free_command_line(&cmd);
			break;	
		} else {
			printf("Error! Unrecognized command: %s\n", cmd.command_list[0]);
		}
		free_command_line(&cmd);
        }
	fclose(fout);
	fclose(fin);
    }

    /* TODO 11 (optional): Handle bad arguments, e.g. "./lab2 -x" or
     * "./lab2 -f" with no file. Print a usage message and return 1.
     * Remember to free line on this path too. */
    free(line);

    return 0;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "command.h"
#include "string_parser.h"
#include "helper.h"

int main(int argc, char* argv[]){
    size_t len = 1024;
    char *line = malloc(len * sizeof(char));
    size_t size;
    int exiting = 0;

    /* ------------------------------------------------------------------ */
    /*  Interactive mode                                                   */
    /* ------------------------------------------------------------------ */
    if (argc == 1) {
        while (1) {

		    write(STDOUT_FILENO, ">>> ", 4);

		    ssize_t res = getline(&line, &len, stdin);
            if (res == -1) {
                // do something to return
                return 0;
            }

            command_line cmd;
            command_line cmds;

            cmds = str_filler(line, ";");

            if (exiting) {
                free_command_line(&cmds);
                free(line);
                exit(0);
            }

            for (int i = 0; i < cmds.num_token; i++) {
                cmd = str_filler(cmds.command_list[i], " ");

                if (cmd.num_token == 0) {
                    free_command_line(&cmd);
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
                    if (cmd.num_token > 1) {
                        writeMsg("Error: too many arguments for ls.\n", STDOUT_FILENO);
                    } else listDir();

                } else if (strcmp(cmd.command_list[0], "pwd") == 0) {
                    if (cmd.num_token > 1) {
                        writeMsg("Error: too many arguments for pwd.\n", STDOUT_FILENO);
                    } else showCurrentDir();

                } else if (strcmp(cmd.command_list[0], "mkdir") == 0) {
                    if (cmd.num_token != 2) {
                        writeMsg("Error: incorrect number of arguments for mkdir.\n", STDOUT_FILENO);
                    } else makeDir(cmd.command_list[1]);

                } else if (strcmp(cmd.command_list[0], "cd") == 0) {
                    if (cmd.num_token != 2) {
                        writeMsg("Error: incorrect number of arguments for cd.\n", STDOUT_FILENO);
                    } else changeDir(cmd.command_list[1]);

                } else if (strcmp(cmd.command_list[0], "cp") == 0) {
                    if (cmd.num_token != 3) {
                        writeMsg("Error: incorrect number of arguments for cp.\n", STDOUT_FILENO);
                    } else copyFile(cmd.command_list[1], cmd.command_list[2]);

                } else if (strcmp(cmd.command_list[0], "rm") == 0) {
                    if (cmd.num_token != 2) {
                        writeMsg("Error: incorrect number of arguments for rm.\n", STDOUT_FILENO);
                    } else deleteFile(cmd.command_list[1]);

                } else if (strcmp(cmd.command_list[0], "mv") == 0) {
                    if (cmd.num_token != 3) {
                        writeMsg("Error: incorrect number of arguments for mv.\n", STDOUT_FILENO);
                    } else moveFile(cmd.command_list[1], cmd.command_list[2]);

                } else if (strcmp(cmd.command_list[0], "cat") == 0) {
                    if (cmd.num_token != 2) {
                        writeMsg("Error: incorrect number of arguments for cat.\n", STDOUT_FILENO);
                    } else displayFile(cmd.command_list[1]);

                } else if (strcmp(cmd.command_list[0], "exit") == 0) {
                    free_command_line(&cmd);
                    // hacky, but need to break out of both loops here. 
                    exiting = 1;
                    break;
                }  else {
                    write(STDOUT_FILENO, "Error! Unrecognized command: ", 29);
                    write(STDOUT_FILENO, cmd.command_list[0], strlen(cmd.command_list[0]));
                    write(STDOUT_FILENO, "\n", 1);
                }

                free_command_line(&cmd);
            }
            free_command_line(&cmds);
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

	    while (getline(&line, &len, fin) != -1) {
            command_line cmd;
            cmd = str_filler(line, " ");

            if (cmd.num_token == 0) {
                free_command_line(&cmd);
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
                if (cmd.num_token > 1) {
                    writeMsg("Error: too many arguments for ls.\n", STDOUT_FILENO);
                } else listDir();

            } else if (strcmp(cmd.command_list[0], "pwd") == 0) {
                if (cmd.num_token > 1) {
                    writeMsg("Error: too many arguments for pwd.\n", STDOUT_FILENO);
                } else showCurrentDir();

            } else if (strcmp(cmd.command_list[0], "mkdir") == 0) {
                if (cmd.num_token != 2) {
                    writeMsg("Error: incorrect number of arguments for mkdir.\n", STDOUT_FILENO);
                } else makeDir(cmd.command_list[1]);

            } else if (strcmp(cmd.command_list[0], "cd") == 0) {
                if (cmd.num_token != 2) {
                    writeMsg("Error: incorrect number of arguments for cd.\n", STDOUT_FILENO);
                } else changeDir(cmd.command_list[1]);

            } else if (strcmp(cmd.command_list[0], "cp") == 0) {
                if (cmd.num_token != 3) {
                    writeMsg("Error: incorrect number of arguments for cp.\n", STDOUT_FILENO);
                } else copyFile(cmd.command_list[1], cmd.command_list[2]);

            } else if (strcmp(cmd.command_list[0], "rm") == 0) {
                if (cmd.num_token != 2) {
                    writeMsg("Error: incorrect number of arguments for rm.\n", STDOUT_FILENO);
                } else deleteFile(cmd.command_list[1]);

            } else if (strcmp(cmd.command_list[0], "exit") == 0) {
                free_command_line(&cmd);
                break;
            }  else {
                write(STDOUT_FILENO, "Error! Unrecognized command: ", 29);
                write(STDOUT_FILENO, cmd.command_list[0], strlen(cmd.command_list[0]));
                write(STDOUT_FILENO, "\n", 1);
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

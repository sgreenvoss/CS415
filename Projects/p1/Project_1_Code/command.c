/*
 * command_student.c - CS415 Lab2 starter code
 *
 * Implement listDir(), which is called when the user types "ls".
 *
 * listDir() prints, on ONE line, the names of the files in the current
 * directory that:
 *   - end with ".txt" or ".py",
 *   - start with a digit (0-9),
 *   - are not "output.txt" or "output_reference.txt".
 * The names must be sorted, each followed by one space, and the line
 * ends with '\n'.
 *
 * Example (run from inside files/):
 *   >>> ls
 *   1_poem.txt 2_lyrics.txt 3_DE_Code.py
 *   (there is a space after every name, including the last one)
 *
 * Requirements:
 *   - Use write(STDOUT_FILENO, ...) for output, not printf(). In file
 *     mode, stdout is redirected to output.txt, and write() goes straight
 *     to that file.
 *   - Your program must pass valgrind with NO memory errors and with
 *     "All heap blocks were freed". Every malloc/strdup needs a
 *     free(), and every opendir() needs a closedir().
 *   - Run ./checker.sh to test your work.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_FILES 1024

/* Comparison function for qsort() on an array of char* (file names).
 * qsort() passes POINTERS to the array elements, so a and b are really
 * char** and must be dereferenced once to get the strings. */
int compare_files(const void *a, const void *b)
{
    const char *file_a = *(const char **)a;
    const char *file_b = *(const char **)b;

    return strcmp(file_a, file_b);
}

void listDir()
{
    /* TODO 1: Get the current directory with getcwd().
     * Hint: use a char array such as char cwd[1024]. If getcwd() returns
     *       NULL, print an error with perror() and exit. */
	char cwd[1024];
	size_t size = 1024;
	if (getcwd(cwd, size) == NULL) {
		perror("getcwd");
	}

    /* TODO 2: Open the directory with opendir().
     * Hint: opendir() returns a DIR*. Check for NULL. */
	DIR* dir = opendir(cwd);
	if (dir == NULL) {
		perror("Not a valid directory");
	}

    /* A growable array of file names. It starts empty and grows by one
     * slot for each file you keep. */
	char *files[MAX_FILES];
	int file_count = 0;

    /* TODO 3: Read every entry of the directory with readdir().
     * Hint: while ((entry = readdir(dir)) != NULL) { ... }
     *       entry->d_name is the file name.
     *
     * For each name:
      *   a) Skip all unreadable files and directories using stat() and S_ISREG()
     *   b) Skip "output.txt" and "output_reference.txt" using strcmp().
     *   c) Otherwise, add it to files:
     *        - if file_count larger than MAX_FILES, break.
     *        - store a COPY of the name with strdup().
     *      Hint: you need a copy because entry->d_name is overwritten by
     *            the next readdir() call and is gone after closedir(). */
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		struct stat sb;

 		if (stat(entry->d_name, &sb) == -1) {
			perror("lstat");
			// free stuff here
			exit(1);
		} else {
			if (!S_ISREG(sb.st_mode)) continue;
			if (entry->d_name[0] == '.') continue;	
		}

		if (strcmp("output.txt", entry->d_name) == 0 || 
		    strcmp("output_reference.txt", entry->d_name) == 0) {
			continue;
		}

		file_count += 1;
	       	if (file_count > MAX_FILES) break;

		files[file_count - 1] = strdup(entry->d_name);
	}

    /* TODO 4: Close the directory with closedir(). */
	closedir(dir);

    /* TODO 5: Sort the names with qsort() and compare_files().
     * Hint: qsort(array, number_of_elements, size_of_one_element, compare)
     *       Each element is a char*, so the size is sizeof(char *). */
	qsort(files, file_count, sizeof(char*), compare_files);

    /* TODO 6: Print the names with write(): each name followed by " ",
     * then one "\n" at the end.
     * Hint: write(STDOUT_FILENO, str, strlen(str)) */
	for (int i = 0; i < file_count; i++) {
		write(STDOUT_FILENO, files[i], strlen(files[i]));
		write(STDOUT_FILENO, " ", 1);
	} write(STDOUT_FILENO, "\n", 1);

    /* TODO 7: Free the memory.
     * Hint: free each files[i] (from strdup). */
	for (int i = 0; i < file_count; i++) {
		free(files[i]);
	}
}

void showCurrentDir() /*for the pwd command*/
{
	char cwd[1024];
	size_t size = 1024;
	if (getcwd(cwd, size) == NULL) {
		perror("getcwd");
	}

	write(STDOUT_FILENO, cwd, strlen(cwd));
}

void makeDir(char *dirName) 
{
	continue;
}

void changeDir(char *dirName) /*for the cd command*/
{ 
	continue;
}

void copyFile(char *sourcePath, char *destinationPath) /*for the cp command*/
{
	continue;
}

void moveFile(char *sourcePath, char *destinationPath) /*for the mv command*/
{
	continue;
}

void deleteFile(char *filename) /*for the rm command*/
{
	continue;
}

void displayFile(char *filename) /*for the cat command*/
{
	continue;
}

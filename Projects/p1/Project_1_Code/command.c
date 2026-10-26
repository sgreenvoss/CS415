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
#include <libgen.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include "helper.h"

#define MAX_FILES 1024

void writeMsg(char* error_msg, int loc) {
    write(loc, error_msg, strlen(error_msg));
}

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
		return;
	}

    /* TODO 2: Open the directory with opendir().
     * Hint: opendir() returns a DIR*. Check for NULL. */
	DIR* dir = opendir(cwd);
	if (dir == NULL) {
		perror("Not a valid directory");
		return;
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
		} //else {
			// if (!S_ISREG(sb.st_mode)) continue;
			// if (entry->d_name[0] == '.') continue;	
		//}

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
	write(STDOUT_FILENO, "\n", 1);
}

void makeDir(char *dirName) 
{
	if (mkdir(dirName, 777) != 0) {
		if (errno == EEXIST) {
			char* message = "Error: Directory already exists.\n";
			write(STDOUT_FILENO, message, strlen(message));
		} else {
			perror("Error creating directory\n");
		}
	}
}

void changeDir(char *dirName) /*for the cd command*/
{ 
	if (chdir(dirName) != 0) {
		char* error_message;
		if (errno == ENOENT) {
			error_message = "Error: Directory does not exist.\n";
		} else if (errno == EACCES) {
			error_message = "Error: Permission denied.\n";
		} else {
			error_message = "Error opening directory.\n";
		}
		write(STDOUT_FILENO, error_message, strlen(error_message));
	}
}

void copyFile(char *sourcePath, char *destinationPath) /*for the cp command*/
{
	int fd = open(sourcePath, O_RDONLY);
	int ofd;

	char* error_message = "";
	
	// what is the dest path? directory or file?
	struct stat d_st; //dest stat
	if (stat(destinationPath, &d_st) == -1) {
		if (errno != ENOENT) {
			error_message = "Error: could not retrieve file stats.\n";
			write(STDOUT_FILENO, error_message, strlen(error_message));
			close(fd);
			return;
		}
	}

	char fname[1024];
	strcpy(fname, destinationPath);

	if (S_ISDIR(d_st.st_mode)) {
		// directory
		strcat(fname, "/");
		strcat(fname, basename(sourcePath));
	}

	ofd = open(fname, O_WRONLY | O_CREAT, 0644);

	if (fd < 0) {
		error_message = "Error: could not open source file.\n";
	} else if (ofd < 0) {
		error_message = "Error: could not open destination file\n";
		close(fd);
	}
	if (strlen(error_message) != 0) {
		write(STDOUT_FILENO, error_message, strlen(error_message));
		return;
	}

	struct stat st;
	if (fstat(fd, &st) == -1) {
		error_message = "Error: could not retrieve file stats.\n";
		write(STDOUT_FILENO, error_message, strlen(error_message));
		close(fd); close(ofd);
		return;
	}

	off_t file_size = st.st_size;
	char* buffer = malloc(file_size + 1);
	if (buffer == NULL) {
		error_message = "Error: memory allocation failed.\n";
		write(STDOUT_FILENO, error_message, strlen(error_message));
		close(fd); close(ofd);
		return;
	}

	if (read(fd, buffer, file_size) == -1) {
		error_message = "Error: read failed.\n";
		write(STDOUT_FILENO, error_message, strlen(error_message));
		close(fd); close(ofd);
		free(buffer);
		return;
	}

	if (write(ofd, buffer, file_size) == -1) {
		error_message = "Error: write failed.\n";
		write(STDOUT_FILENO, error_message, strlen(error_message));
		close(fd); close(ofd);
		free(buffer);
		return;
	} 
	close(fd); close(ofd);
	free(buffer);
}

void deleteFile(char *filename) /*for the rm command*/
{
	struct stat sb;
	char* error_msg;
	if (stat(filename, &sb) == -1) {
		writeMsg("Error: could not stat file.\n", STDOUT_FILENO);
		return;
	}

	if (S_ISDIR(sb.st_mode)) {
		writeMsg("Error: cannot remove directory.\n", STDOUT_FILENO);
		return;
	}

	if (remove(filename) == -1) {
		writeMsg("Error: could not remove file.\n", STDOUT_FILENO);
	}
}

void moveFile(char *sourcePath, char *destinationPath) /*for the mv command*/
{
	int fd = open(sourcePath, O_RDONLY);
	int ofd = open(destinationPath, O_WRONLY | O_CREAT, 0644);
	char* error_message = "";

	if (fd < 0) {
		error_message = "Error: could not open source file.\n";
	} else if (ofd < 0) {
		error_message = "Error: could not open destination file\n";
		close(fd);
	}
	if (strlen(error_message) != 0) {
		write(STDOUT_FILENO, error_message, strlen(error_message));
		return;
	}

	struct stat st;
	if (fstat(fd, &st) == -1) {
		error_message = "Error: could not retrieve file stats.\n";
		write(STDOUT_FILENO, error_message, strlen(error_message));
		close(fd); close(ofd);
		return;
	}

	off_t file_size = st.st_size;
	char* buffer = malloc(file_size + 1);
	if (buffer == NULL) {
		error_message = "Error: memory allocation failed.\n";
		write(STDOUT_FILENO, error_message, strlen(error_message));
		close(fd); close(ofd);
		return;
	}

	if (read(fd, buffer, file_size) == -1) {
		error_message = "Error: read failed.\n";
		write(STDOUT_FILENO, error_message, strlen(error_message));
		close(fd); close(ofd);
		free(buffer);
		return;
	}

	if (write(ofd, buffer, file_size) == -1) {
		error_message = "Error: write failed.\n";
		write(STDOUT_FILENO, error_message, strlen(error_message));
		close(fd); close(ofd);
		free(buffer);
		return;
	} 

	close(fd);
	// yay reduce reuse recycle
	deleteFile(sourcePath);

	close(ofd);
	free(buffer);

}


void displayFile(char *filename) /*for the cat command*/
{
	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		writeMsg("Error: could not open file to cat.\n", STDOUT_FILENO);
		return;
	}

	struct stat st;
	if (fstat(fd, &st) == -1) {
		writeMsg("Error: could not retrieve file stats.\n", STDOUT_FILENO);
		close(fd);
		return;
	}

	off_t file_size = st.st_size;
	char* buffer = malloc(file_size + 1);
	if (buffer == NULL) {
		writeMsg("Error: memory allocation failed.\n", STDOUT_FILENO);
		close(fd);
		return;
	}

	if (read(fd, buffer, file_size) == -1) {
		writeMsg("Error: read failed.\n", STDOUT_FILENO);
		fprintf(stdout, "%s", strerror(errno));
		close(fd);
		free(buffer);
		return;
	}

	writeMsg(buffer, STDOUT_FILENO);

	close(fd);
	free(buffer);
}

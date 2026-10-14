/*
 * string_parser.c
 *
 *  Created on: Nov 25, 2020
 *      Author: gguan, Monil
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GUN_SOURCE

int count_token (char* buf, const char* delim)
{
	//TODO：
	/*
	*	#1.	Check for NULL string
	*	#2.	iterate through string counting tokens
	*		Cases to watchout for
	*			a.	string start with delimeter
	*			b. 	string end with delimeter
	*			c.	account NULL for the last token
	*	#3. return the number of token (note not number of delimeter)
	*/
	if(buf == NULL || delim == NULL){
		return 0;
	}
	int count = 0;
	char *token = strtok(buf, delim);

	while(token != NULL){
		count++;
		token = strtok(NULL, delim);
	}

	return count;
}

command_line str_filler (char* buf, const char* delim)
{
	//TODO：
	/*
	*	#1.	create command_line variable to be filled and returned
	*	#2.	count the number of tokens with count_token function, set num_token. 
    *           one can use strtok_r to remove the \n at the end of the line.
	*	#3. malloc memory for token array inside command_line variable
	*			based on the number of tokens.
	*	#4.	use function strtok_r to find out the tokens 
    *   #5. malloc each index of the array with the length of tokens,
	*			fill command_list array with tokens, and fill last spot with NULL.
	*	#6. return the variable.
	*/

	command_line cmd;
	cmd.num_token = 0;
	cmd.command_list = NULL;

	if(buf == NULL || delim == NULL){
		return cmd;
	}

	size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') {
        buf[len-1] = '\0';
    }

	char *tmp = strdup(buf);
	if (tmp == NULL) {
        // perror("strdup");
        return cmd;
    }

	// count the number of tokens
	char* saveptr;
    char* token = strtok_r(tmp, delim, &saveptr);
    while (token != NULL) {
        cmd.num_token++;
        token = strtok_r(NULL, delim, &saveptr);
    }
    free(tmp);

	if (cmd.num_token == 0) {
        return cmd;
    }

	cmd.command_list = (char**)malloc((cmd.num_token + 1) * sizeof(char*));
    if (cmd.command_list == NULL) {
        // perror("malloc command_list");
        cmd.num_token = 0;
        return cmd;
    }

	int i = 0;
    token = strtok_r(buf, delim, &saveptr);
    while (token != NULL) {
        cmd.command_list[i] = (char*)malloc(strlen(token) + 1);
        if (cmd.command_list[i] == NULL) {
            // perror("malloc token");
            for (int j = 0; j < i; j++) free(cmd.command_list[j]);
            free(cmd.command_list);
            cmd.command_list = NULL;
            cmd.num_token = 0;
            return cmd;
        }
        strcpy(cmd.command_list[i], token);
        i++;
        token = strtok_r(NULL, delim, &saveptr);
    }
    cmd.command_list[i] = NULL;

    return cmd;

}


void free_command_line(command_line* command)
{
	//TODO：
	/*
	*	#1.	free the array base num_token
	*/
	if (command == NULL || command->command_list == NULL) {
        return;
    }

    for (int i = 0; i < command->num_token; i++) {
        free(command->command_list[i]);
    }

    free(command->command_list);

    command->command_list = NULL;
    command->num_token = 0;
}

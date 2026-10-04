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
	if (buf == NULL || delim == NULL || *delim == '\0') {
		return 0;
	}

	int tokens = 0;
	int in_token = 0;
	
	for (const char* p = buf; *p != '\0' && *p != '\n'; p++) {
		// is the current char part of the delimiter?
		if (strchr(delim, *p) != NULL) {
			in_token = 0;
		// if not, did we just finish parsing a delimiter? 
		} else if (!in_token) { 
			in_token = 1;
			tokens++;
		}
	}
	return tokens;
}

command_line str_filler (char* buf, const char* delim)
{
	//TODO：
	/*
	*	#1.	create command_line variable to be filled and returned
	*	#2.	count the number of tokens with count_token function, set num_token. 
  *     one can use strtok_r to remove the \n at the end of the line.
	*	#3. malloc memory for token array inside command_line variable
	*			based on the number of tokens.
	*	#4.	use function strtok_r to find out the tokens 
  * #5. malloc each index of the array with the length of tokens,
	*			fill command_list array with tokens, and fill last spot with NULL.
	*	#6. return the variable.
	*/
	command_line cmd;
	int tokens = count_token(buf, delim);
	char* real_buf;
	char* _fake;
	real_buf = strtok_r(buf, "\n", &_fake); 
	
	cmd.command_list = malloc(sizeof(char*) * (tokens+1));
	cmd.num_token = tokens;	
	
	if (real_buf == NULL) {
		return cmd;
	}

	char* token;
	char* saveptr = NULL;
	token = strtok_r(real_buf, delim, &saveptr);
		
	for (int i = 0; ; i++) { 

		if (token == NULL || i == tokens) {
			cmd.command_list[i] = NULL;
			break;
		}

		char* new_token = malloc(sizeof(char) * (strlen(token)+1));
		strcpy(new_token, token);
		cmd.command_list[i] = new_token;
		
		token = strtok_r(NULL, delim, &saveptr);
	}

	return cmd;
}


void free_command_line(command_line* command)
{
	//TODO：
	/*
	*	#1.	free the array base num_token
	*/

	for (int i = 0; i < command->num_token; i++) {
		free(command->command_list[i]);
	}

	free(command->command_list);
}

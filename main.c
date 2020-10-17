/**
* @file main.c
* @brief Nano Shell code
* @date 2020-10-10
* @author André Azevedo 2182634
* @author Alexandre Santos - 2181593
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "debug.h"
#include "memory.h"
#include "args.h"

//TODO REDIRECIONAMENTO
//TODO TRATAR SINAIS

/**
 * DEFINITIONS
 */

#define NANO_TOKENS_BUFSIZE 32 //Size for tokes buffer

/* Global Variable declaration*/
int status = 0; // Status for terminating nanoShell

/*Function Declaration*/
char *nano_read_command(char *line);
char **nano_split_lineptr(char *lineptr, char **tokens, int buffersize);
void nano_verify_pointer(char **ptr);
void nano_exec_commands(char **tokens);
int nano_verify_char(char *lineptr);
void nano_verify_terminate(char **args);

/**
 * Function to verify termination of nanoShell
 * 
 * @return Function returns nothing
 */
void nano_verify_terminate(char **args)
{
	size_t size = sizeof(args) / sizeof(args[0]);

	for (size_t i = 0; i < size; i++)
	{
		if (strcmp(args[i], "bye") == 0)
		{
			printf("[INFO] bye command detected. Terminating nanoShell\n");
			exit(0);
		}
	}
}

/**
 * Function to verify unsupported characters
 * 
 * @return int value 0 for OK -1 for NOT OK
 * 
 */

int nano_verify_char(char *lineptr)
{
	int res = 0;
	size_t length = strlen(lineptr);
	size_t i;

	int verify[] = {33, 34, 35, 36, 38, 39, 40, 41, 42, 44, 58, 59, 60, 61, 63, 64, 91, 92, 93, 94, 96, 123, 124, 126};
	size_t verlength = sizeof(verify) / sizeof(verify[0]);
	for (i = 0; i < length; i++)
	{
		for (size_t j = 0; j < verlength; j++)
		{
			if (lineptr[i] == verify[j])
			{
				res = -1;
				j = verlength;
				i = length;
			}
		}
	}
	return res;
}

/**
 * Function to execute the commands
 * 
 * @return Function return nothing
 */

void nano_exec_commands(char **tokens)
{

	pid_t pid;

	pid = fork();

	if (pid == -1)
	{
		ERROR(3, "Error executing fork().\n");
	}
	else if (pid == 0)
	{
		execvp(tokens[0], tokens);
		ERROR(4, "Error executing execvp.\n");
	}
	else
	{
		wait(NULL);
	}
}

/**
 * Function to verify pointers memory allocation
 * 
 * @return Function does not return anything
 */
void nano_verify_pointer(char **ptr)
{
	if (ptr == NULL)
	{
		ERROR(2, "[ERROR]Memory Allocation Failed\n");
	}
}

/**
 * Function to parse and split the given
 *  commands from liteprt into tokens
 * 
 * @return Function returns a pointer to a string array with the commands
 */
char **nano_split_lineptr(char *lineptr, char **tokens, int buffersize)
{

	char *token;

	int pos = 0;

	if (tokens == NULL)
	{
		ERROR(2, "[ERROR]Memory Allocation Failed\n");
	}

	token = strtok(lineptr, " \r\n");
	while (token != NULL)
	{
		tokens[pos] = token;
		pos++;

		if (pos >= buffersize)
		{
			buffersize = buffersize + NANO_TOKENS_BUFSIZE;
			tokens = realloc(tokens, buffersize * sizeof(char *));

			if (tokens == NULL)
			{
				ERROR(2, "[ERROR]Memory Allocation Failed\n");
			}
		}
		token = strtok(NULL, " \r\n");
	}

	tokens[pos] = NULL;

	return tokens;
}

/**
 * Function to read the command inserted in the shell
 * 
 * @return Function returns a pointer to char with the command.
 */
char *nano_read_command(char *line)
{

	size_t n = 0;
	ssize_t result;

	printf("nanoShell$ ");
	if ((result = getline(&line, &n, stdin)) == -1)
	{
		if (feof(stdin))
		{
			exit(EXIT_SUCCESS);
		}
		else
		{
			ERROR(1, "[ERROR]Error reading commands\n");
		}
	}
	line[strcspn(line, "\n")] = 0;

	return line;
}

/**
 * Function to start loop of the nanoShell
 * 
 * @return This function returns nothing
 */
void nano_loop(void)
{
	char *lineptr;
	char **args;

	do
	{

		char *line = NULL;

		lineptr = nano_read_command(line);

		if (lineptr[0] != 0)
		{

			int res = nano_verify_char(lineptr);
			if (res == -1)
			{
				printf("[ERROR] Wrong request ' %s", lineptr);
			}
			else
			{
				int buffersize = NANO_TOKENS_BUFSIZE;
				char **tokens = malloc(buffersize * sizeof(char *));

				args = nano_split_lineptr(lineptr, tokens, buffersize);

				free(tokens);

				nano_verify_terminate(args);

				nano_exec_commands(args);
			}
		}

		free(line);

	} while (status == 0);
}

/**
 * Main FUNCTION
 * 
 */
int main(int argc, char *argv[])
{
	/* Disable warnings */
	(void)argc;
	(void)argv;

	struct gengetopt_args_info args;

	if (cmdline_parser(argc, argv, &args) != 0)
	{
		exit(1);
	}
	if (args.no_help_given)
	{
		printf("HELP FOR NANOSHELL\n\n");
		printf("#Use simple commands without metachars and pipes \n Ex: ps aux -l \n\n");
		printf("#Use bye command to exit nanoShell\n\n\nAuthors:\n");
		printf("André Azevedo - 2182634\nAlexandre Santos - 2181593\n\n");
	}
	//TODO file parameter
	//TODO max commands
	//TODO signal file

	/* Main code */
	nano_loop();

	return 0;
}

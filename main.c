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
#include "time.h"

/**
 * DEFINITIONS
 */

#define NANO_TOKENS_BUFSIZE 32 //Size for tokes buffer
#define NANO_TIME_BUFSIZE 256 //Size for time buffer

/* Global Variable declaration*/
int status = 0; // Status for terminating nanoShell

/*Function Declaration*/
char *nano_read_command(char *line);
char **nano_split_lineptr(char *lineptr);
void nano_verify_pointer(char **ptr);
void nano_exec_commands(char **tokens);
int nano_verify_char(char *lineptr);
void nano_verify_terminate(char **args);
int nano_verify_redirect(char **args, char **outputfile);
void nano_sig_handler(int sig, siginfo_t *siginfo, void *context);

/**
 * Function to handle the signals
 * 
 */
void nano_sig_handler(int sig, siginfo_t *siginfo, void *context)
{
	(void)context;
	int aux;

	/* Copia da variavel global errno */
	aux = errno;

	if (sig == SIGUSR1)
	{

		/* 	FILE *fileptr;
		char *lineptr = NULL;
		size_t n = 0;
		ssize_t result;

		fileptr = fopen(file, "rt");
		if (fileptr == NULL)
		{
			ERROR(1, "Error opening for reading!\n");
		}

		while ((result = getline(&lineptr, &n, fileptr)) != -1)
		{
			printf("Found line with size: %zu : ", result);
			printf("%s \n", lineptr);
		}

		free(lineptr);
		fclose(fileptr); */

		/* Restaura valor da variavel global errno */
		errno = aux;
	}
	else if (sig == SIGUSR2)
	{
	}
	else if (sig == SIGINT)
	{
		printf("[INFO] Received SIGINT from PID: %ld\n", (long)siginfo->si_pid);
		printf("\n[INFO] nanoShell is terminating.\n");

		exit(0);
	}

	/* Restaura valor da variavel global errno */
	errno = aux;
}
/**
 * Function to verify if its a redirect command
 * 
 */
int nano_verify_redirect(char **args, char **outputfile)
{

	for (int i = 0; args[i] != NULL; i++)
	{
		if ((strcmp(args[i], ">") == 0))
		{
			*outputfile = args[i + 1];

			args[i] = NULL;

			return 1;
		}
		if ((strcmp(args[i], ">>") == 0))
		{
			*outputfile = args[i + 1];

			args[i] = NULL;

			return 2;
		}
		if ((strcmp(args[i], "2>") == 0))
		{
			*outputfile = args[i + 1];

			args[i] = NULL;

			return 3;
		}
		if ((strcmp(args[i], "2>>") == 0))
		{

			*outputfile = args[i + 1];

			args[i] = NULL;

			return 4;
		}
	}

	return -1;
}

/**
 * Function to verify termination of nanoShell
 * 
 * @return Function returns nothing
 */
void nano_verify_terminate(char **args)
{
	size_t size = sizeof(args) / sizeof(char **);

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
 * @return int value 0 for OK, -1 for NOT OK
 * 
 */

int nano_verify_char(char *lineptr)
{
	int res = 0;
	size_t length = strlen(lineptr);
	size_t i;

	int verify[] = {33, 34, 35, 36, 38, 39, 40, 41, 42, 44, 58, 59, 60, 61, 63, 64, 91, 92, 93, 94, 96, 123, 124, 126};

	size_t verlength = sizeof(verify) / sizeof(verify[0]);

	/* Verify SPACE and TAB and % in first char */
	if (lineptr[0] == 32 || lineptr[0] == 9 || lineptr[0] == 37)
	{
		return -1;
	}

	/* Verify other chars */
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
char **nano_split_lineptr(char *lineptr)
{

	char *token;
	int buffersize = NANO_TOKENS_BUFSIZE;
	char **tokens = malloc(buffersize * sizeof(char *));

	int pos = 0;

	if (tokens == NULL)
	{
		ERROR(2, "[ERROR]Memory Allocation Failed\n");
	}

	token = strtok(lineptr, " ");
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
		token = strtok(NULL, " ");
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

	do
	{

		char *line = NULL;
		char **args;

		lineptr = nano_read_command(line);

		if (lineptr[0] != 0)
		{

			int res = nano_verify_char(lineptr);
			if (res == -1)
			{
				printf("[ERROR] Wrong request ' %s\n", lineptr);
			}
			else
			{
				char *outputfile;
				int result;

				args = nano_split_lineptr(lineptr);

				nano_verify_terminate(args);

				pid_t pid = fork();
				if (pid == -1)
				{
					ERROR(3, "Error executing fork().\n");
				}
				else if (pid == 0)
				{
					FILE *fp;

					/* Verify if it is a redirect command */
					result = nano_verify_redirect(args, &outputfile);

					switch (result)
					{
					case 1:
						fp = freopen(outputfile, "w", stdout);
						break;
					case 2:
						fp = freopen(outputfile, "a", stdout);
						break;
					case 3:
						fp = freopen(outputfile, "w", stderr);
						break;
					case 4:
						fp = freopen(outputfile, "a", stderr);
						break;
					default:
						break;
					}
					if (fp == NULL)
					{
						printf("[ERROR]Error opening file\n");
					}

					/* Execute commands */
					execvp(args[0], args);
					ERROR(4, "Error executing execvp.\n");

					exit(0);
				}
				else
				{
					wait(NULL);
					free(args);
				}
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

	//Save time process start

	char buf[NANO_TIME_BUFSIZE] = {0};
	time_t starttime = time(NULL);

	if (starttime == -1)
	{

		printf("The time() function failed");
		return 1;
	}

	struct tm *ptm = localtime(&starttime);

	if (ptm == NULL)
	{

		printf("The localtime() function failed");
		return 1;
	}
	//printf("UTC time: %s", asctime(ptm));
	strftime(buf, NANO_TIME_BUFSIZE, "%a %d %b %G - %T %z %Z", ptm);
	printf("%s\n", buf);
    


/* 
	printf("The time is: %02d/%02d/%d - %02d:%02d:%02d\n", ptm->tm_mday, ptm->tm_mon, ptm->tm_year, ptm->tm_hour,
		   ptm->tm_min, ptm->tm_sec); */

	//SIGNAL HANDLER

	struct sigaction act;

	/* Definir a rotina de resposta a sinais */
	act.sa_sigaction = nano_sig_handler;

	/* mascara sem sinais -- nao bloqueia os sinais */
	sigemptyset(&act.sa_mask);

	act.sa_flags = SA_SIGINFO;	/*info adicional sobre o sinal */
	act.sa_flags |= SA_RESTART; /*recupera chamadas bloqueantes*/

	/* Captura do sinal SIGUSR1 */
	if (sigaction(SIGUSR1, &act, NULL) < 0)
	{
		ERROR(1, "sigaction - SIGUSR1");
	}
	/* Captura do sinal SIGUSR2 */
	if (sigaction(SIGUSR2, &act, NULL) < 0)
	{
		ERROR(1, "sigaction - SIGUSR2");
	} /* Captura do sinal SIGUSR1 */
	if (sigaction(SIGINT, &act, NULL) < 0)
	{
		ERROR(1, "sigaction - SIGINT");
	}

	//TODO file parameter
	//TODO max commands
	//TODO signal file

	/* Main code */
	nano_loop();

	return 0;
}

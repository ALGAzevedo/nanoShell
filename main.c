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
#define NANO_TIME_BUFSIZE 256  //Size for time buffer
#define C_EXIT_FAILURE -1
#define C_EXIT_SUCCESS 0
#define C_ERROR_PARSING_ARGS 1
#define NANO_TIME_ERROR 2
#define NANO_ERROR_MALLOC 3
#define NANO_ERROR_READ 4
#define NANO_ERROR_IO 5
#define NANO_ERROR_FORK 6
#define NANO_ERROR_EXECVP 7
#define NANO_ERROR_SIGACTION 8

// DEFINE GLOBAL VARIABLES
int status = 0; // Status for terminating nanoShell
struct tm *ptm;
struct tm *current;
unsigned int count_stdout = 0;
unsigned int count_stderr = 0;
unsigned int max_commands = 0;
unsigned int *executed_commandsptr;

// FUNCTIONS DECLARATION
char *nano_read_command(char *line);
char **nano_split_lineptr(char *lineptr);
void nano_verify_pointer(char **ptr);
void nano_exec_commands(char **tokens);
int nano_verify_char(char *lineptr);
void nano_verify_terminate(char **args);
int nano_verify_redirect(char **args, char **outputfile);
void nano_sig_handler(int sig, siginfo_t *siginfo, void *context);

/*****************************************************************
 * Function to handle the signals
 * 
 ****************************************************************/
void nano_sig_handler(int sig, siginfo_t *siginfo, void *context)
{
	(void)context;
	int aux;

	/* Copia da variavel global errno */
	aux = errno;

	if (sig == SIGUSR1)
	{
		char buf[NANO_TIME_BUFSIZE] = {0};
		strftime(buf, NANO_TIME_BUFSIZE, "\nnanoShell started at: %a %d %b %G - %T %z %Z", ptm);
		printf("%s\n", buf);
	}
	else if (sig == SIGUSR2)
	{

		FILE *fileptr;

		//Get the time to create the string for file

		time_t starttime = time(NULL);

		if (starttime == -1)
		{
			ERROR(NANO_TIME_ERROR, "The time() function failed");
		}

		current = localtime(&starttime);

		if (current == NULL)
		{
			ERROR(NANO_TIME_ERROR, "The localtime() function failed");
		}

		//Create string for file name
		char bufer[NANO_TIME_BUFSIZE] = {0};
		strftime(bufer, NANO_TIME_BUFSIZE, "nanoShell_status_%d.%m.%Y_%Hh:%M.%S.txt", current);

		//Open or Create file

		fileptr = fopen(bufer, "w");
		if (fileptr == NULL)
		{
			ERROR(NANO_ERROR_IO, "Error opening for writing!\n");
		}

		/* 		char buffer[256];
		snprintf(buffer, sizeof(buffer),
				 "\n%d execution(s) of applications\n%d execution(s) with STDOUT redir\n%d execution(s) with STDERR redir\n",
				 *executed_commandsptr, count_stdout, count_stderr);
		fwrite(buffer, 1, sizeof(bufer), fileptr); */

		fprintf(fileptr, "%d execution(s) of applications\n%d execution(s) with STDOUT redir\n%d execution(s) with STDERR redir\n",
				*executed_commandsptr, count_stdout, count_stderr);
		fclose(fileptr);
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
/*****************************************************************
 * Function to verify if its a redirect command
 * 
 ****************************************************************/
int nano_verify_redirect(char **args, char **outputfile)
{

	for (int i = 0; args[i] != NULL; i++)
	{
		printf("%s\n", args[i]);
		if ((strcmp(args[i], ">") == 0))
		{
			*outputfile = args[i + 1];

			args[i] = NULL;

			//Increment STDOUT redir counter
			count_stdout++;
			return 1;
		}
		if ((strcmp(args[i], ">>") == 0))
		{
			*outputfile = args[i + 1];

			args[i] = NULL;

			//Increment STDOUT redir counter
			count_stdout++;
			return 2;
		}
		if ((strcmp(args[i], "2>") == 0))
		{
			*outputfile = args[i + 1];

			args[i] = NULL;

			//Increment STDERR redir counter
			count_stderr++;
			return 3;
		}
		if ((strcmp(args[i], "2>>") == 0))
		{

			*outputfile = args[i + 1];

			args[i] = NULL;

			//Increment STDERR redir counter
			count_stderr++;
			return 4;
		}
	}
	
	//Increment Commands counter
	(*executed_commandsptr)++;
	// PERGUNTAR AO PROF: Como fazer update ao ponteiro dentro do fork()?
	//printf("[INFO] Comandos executados: %d\n", *executed_commandsptr);

	return -1;
}

/*****************************************************************
 * Function to verify termination of nanoShell
 * 
 * @return Function returns nothing
 ****************************************************************/
void nano_verify_terminate(char **args)
{
	size_t size = sizeof(args) / sizeof(char **);

	for (size_t i = 0; i < size; i++)
	{
		if (strcmp(args[i], "bye") == 0)
		{
			printf("[INFO] bye command detected. Terminating nanoShell\n");
			exit(C_EXIT_SUCCESS);
		}
	}
}

/*****************************************************************
 * Function to verify unsupported characters
 * 
 * @return int value 0 for OK, -1 for NOT OK
 * 
 ****************************************************************/
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

/*****************************************************************
 * Function to verify pointers memory allocation
 * 
 * @return Function does not return anything
 ****************************************************************/
void nano_verify_pointer(char **ptr)
{
	if (ptr == NULL)
	{
		ERROR(NANO_ERROR_MALLOC, "[ERROR]Memory Allocation Failed\n");
	}
}

/*****************************************************************
 * Function to parse and split the given
 *  commands from liteprt into tokens
 * 
 * @return Function returns a pointer to a string array with the commands
 ****************************************************************/
char **nano_split_lineptr(char *lineptr)
{

	char *token;
	int buffersize = NANO_TOKENS_BUFSIZE;
	char **tokens = malloc(buffersize * sizeof(char *));

	int pos = 0;

	nano_verify_pointer(tokens);

	token = strtok(lineptr, " ");
	while (token != NULL)
	{
		tokens[pos] = token;
		pos++;

		if (pos >= buffersize)
		{
			buffersize = buffersize + NANO_TOKENS_BUFSIZE;
			tokens = realloc(tokens, buffersize * sizeof(char *));

			nano_verify_pointer(tokens);
		}
		token = strtok(NULL, " ");
	}

	tokens[pos] = NULL;

	return tokens;
}

/*****************************************************************
 * Function to read the command inserted in the shell
 * 
 * @return Function returns a pointer to char with the command.
 ****************************************************************/
char *nano_read_command(char *line)
{

	size_t n = 0;
	ssize_t result;

	printf("nanoShell$ ");
	if ((result = getline(&line, &n, stdin)) == -1)
	{
		if (feof(stdin))
		{
			exit(C_EXIT_SUCCESS);
		}
		else
		{
			ERROR(NANO_ERROR_READ, "[ERROR]Error reading commands\n");
		}
	}
	line[strcspn(line, "\n")] = 0;

	return line;
}

/***************************************************************
 * Function to start loop of the nanoShell
 * 
 * @return This function returns nothing
 **************************************************************/
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
					ERROR(NANO_ERROR_FORK, "Error executing fork().\n");
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
					ERROR(NANO_ERROR_EXECVP, "Error executing execvp.\n");

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

		if (*executed_commandsptr == max_commands)
		{				// verifying equal because of performance and we know we are incrementing +1 everytime
			status = 1; // to stops the loop
		}

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
	unsigned int executed_commands = 0;

	struct gengetopt_args_info args;

	if (cmdline_parser(argc, argv, &args) != 0)
	{
		ERROR(C_ERROR_PARSING_ARGS, "Invalid arguments. nanoShell can't start.");
		exit(C_EXIT_FAILURE);
	}
	// Max executions
	if (args.max_given)
	{
		if (args.max_arg <= 0)
		{
			printf("ERROR: invalid value \'int\' for -m/--max.\n\n");
			// PERGUNTAR AO PROF: DEVE TERMINAR A SHELL?
		}
		else
		{
			max_commands = args.max_arg;
			executed_commandsptr = &executed_commands;
			printf("[INFO] nanoShell with terminate after %d commands\n", max_commands);
		}
	}

	/*************************************************************
	 * 
	 * HELP ARGUMENT CODE
	 * 
	 *************************************************************/
	if (args.no_help_given)
	{
		printf("HELP FOR NANOSHELL\n\n");
		printf("#Use simple commands without metachars and pipes \n Ex: ps aux -l \n\n");
		printf("#Use bye command to exit nanoShell\n\n\nAuthors:\n");
		printf("André Azevedo - 2182634\nAlexandre Santos - 2181593\n\n");
	}

	/*************************************************************
	 * 
	 * SIGNALS FILE ARGUMENT CODE
	 * 
	 *************************************************************/
	if (args.signalfile_given)
	{

		FILE *fileptr;

		fileptr = fopen("signals.txt", "w");
		if (fileptr == NULL)
		{
			ERROR(NANO_ERROR_IO, "Error opening for writing!\n");
		}

		pid_t pid = getpid();

		/* 		char buffer[64];
		snprintf(buffer, sizeof(buffer), "kill -SIGINT %d\nkill -SIGUSR1 %d\nkill -SIGUSR2 %d", pid, pid, pid);
		fwrite(buffer, 1, sizeof(buffer), fileptr);  */

		fprintf(fileptr, "kill -SIGINT %d\nkill -SIGUSR1 %d\nkill -SIGUSR2 %d", pid, pid, pid);

		fclose(fileptr);
	}

	/*************************************************************
	 * 
	 * SAVE TIMESTAMP FOR NANOSHELL STARTUP
	 * 
	 *************************************************************/
	time_t starttime = time(NULL);

	if (starttime == -1)
	{
		printf("The time() function failed");
		return 1;
	}

	ptm = localtime(&starttime);

	if (ptm == NULL)
	{
		printf("The localtime() function failed");
		return 1;
	}

	/*************************************************************
	 * 
	 * SIGNAL HANDLER
	 * 
	 *************************************************************/
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
		ERROR(NANO_ERROR_SIGACTION, "sigaction - SIGUSR1");
	}
	/* Captura do sinal SIGUSR2 */
	if (sigaction(SIGUSR2, &act, NULL) < 0)
	{
		ERROR(NANO_ERROR_SIGACTION, "sigaction - SIGUSR2");
	} /* Captura do sinal SIGUSR1 */
	if (sigaction(SIGINT, &act, NULL) < 0)
	{
		ERROR(NANO_ERROR_SIGACTION, "sigaction - SIGINT");
	}

	//TODO file parameter

	/*************************************************************
	 * 
	 * MAIN LOOP
	 * 
	 *************************************************************/
	nano_loop();

	if (args.max_arg && max_commands > 0)
	{
		printf("[INFO] nanoShell executed %d commands\n", executed_commands);
	}
	return C_EXIT_SUCCESS;
}

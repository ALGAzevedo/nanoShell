/**
* @file main.c
* @brief Nano Shell code
* @date 2020-10-10
* @author 2181593 – Alexandre Jorge Casaleiro dos Santos
* @author 2182634 - André Luís Gil De Azevedo
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
#define NANO_MAX_INVALID 9

// DEFINE GLOBAL VARIABLES
int status = 0; // Status for terminating nanoShell
struct tm *ptm;
struct tm *current;

struct NanoCounters {
	unsigned int G_count_stdout;
	unsigned int G_count_stderr;
	unsigned int G_max_commands;
	unsigned int G_count_commands;
} counters;


// FUNCTIONS DECLARATION
void nano_sig_handler(int sig, siginfo_t *siginfo, void *context);
int nano_verify_redirect(char **args, char **outputfile);
void nano_verify_terminate(char **args);
int nano_verify_char(char *lineptr);
void nano_verify_pointer(char **ptr);
char **nano_split_lineptr(char *lineptr);
char *nano_read_command(char *line);
void nano_exec_commands(char *lineptr);
void nano_loop(void);


/*******************************************************************************************************************
 * Function nano_sig_handler
 * ----------------------------------------------------------------------------------------------------------------
 *  @brief Function to handle the signals received
 * 
 * Possible signals:
 * 					-SIGUSR1 - prints in stout the starting date and time of the nanoShell
 * 
 * 					-SIGUSR2 - opens a file with the name the format "nanoShell_status_currentDate_currentHour.txt"
 * 								and write in the file:	
 * 														- number of executed commands
 * 														- number of executed commands redirected to stdout
 * 														- number of executed commands redirected to stderr
 * 
 * 					-SIGINT - Terminates the nanoShell printing the PID process that have sent the signal
 * 
 *******************************************************************************************************************/
void nano_sig_handler(int sig, siginfo_t *siginfo, void *context)
{
	(void)context;
	int aux;

	/* Copy from global variable errno */
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

		fprintf(fileptr, "%u execution(s) of applications\n%u execution(s) with STDOUT redir\n%u execution(s) with STDERR redir\n",
				counters.G_count_commands, counters.G_count_stdout, counters.G_count_stderr);

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


/*******************************************************************************************************************
 * Function nano_verify_redirect
 * ----------------------------------------------------------------------------------------------------------------
 * @brief Function receives @param args with the inserted command and verifies
 * 	if the command is a redirect command, saving the redirect destination to
 * 	@param outputfile. Then counters for the total executed commands, stdout 
 * redirect commands and stderr redirect commands are also incremented here.
 * 
 * @return Function returns an Int depending on the redirect:
 * 
 * 				-1 if it isn't a redirect command
 * 				 1 if it is a stdout redirect to new or clean file
 * 				 2 if it is a stdout redirect to append in file
 * 				 3 if it is a stderr redirect to new or clean file
 * 				 4 if it is a stderr redirect to append in file
 * 
 *******************************************************************************************************************/
int nano_verify_redirect(char **args, char **outputfile)
{

	for (int i = 0; args[i] != NULL; i++)
	{
		if ((strcmp(args[i], ">") == 0))
		{
			*outputfile = args[i + 1];

			args[i] = NULL;

			//Increment STDOUT redir counter
			counters.G_count_stdout++;
			//Increment Total commands executed
			counters.G_count_commands++;

			return 1;
		}
		if ((strcmp(args[i], ">>") == 0))
		{
			*outputfile = args[i + 1];

			args[i] = NULL;

			//Increment STDOUT redir counter
			counters.G_count_stdout++;
			//Increment Total commands executed
			counters.G_count_commands++;

			return 2;
		}
		if ((strcmp(args[i], "2>") == 0))
		{
			*outputfile = args[i + 1];

			args[i] = NULL;

			//Increment STDERR redir counter
			counters.G_count_stderr++;
			//Increment Total commands executed
			counters.G_count_commands++;

			return 3;
		}
		if ((strcmp(args[i], "2>>") == 0))
		{

			*outputfile = args[i + 1];

			args[i] = NULL;

			//Increment STDERR redir counter
			counters.G_count_stderr++;
			//Increment Total commands executed
			counters.G_count_commands++;

			return 4;
		}
	}
	counters.G_count_commands++;
	return -1;
}

/*******************************************************************************************************************
 * Function: nano_verify_terminate
 *  ----------------------------------------------------------------------------------------------------------------
 * @brief Function receives the @param args with the separated tokens
 * to verify if it was inserted the command "bye"to terminate the nanoShell.
 * 
 * @return Function returns nothing
 *******************************************************************************************************************/
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


/*******************************************************************************************************************
 * Function nano_verify_char
 * ---------------------------------------------------------------------------------------------------------------
 *  @brief Function receives @param lineptr with the inserted command to verify if it was inserted any character
 *  	that isn't supported. First verifies if there is a TAB, SPACE or % in the first char( @param lineptr[0] ), after 
 *  	validates there isn't any of those it verifies the unsupported characters in the whole @param lineptr.
 * 
 * 	Unsupported characters: !, ", #, $, &, ', (, ), , , :, ;, <, =, ?, @, [, \, ], ^, `, {, |, }, ~
 * 
 * @return Function returns @param result with 0 if all characters are OK and -1 if one unsupported character is found
 *******************************************************************************************************************/
int nano_verify_char(char *lineptr)
{
	int res = 0;
	size_t length = strlen(lineptr);
	size_t i;

	int verify[] = {33, 34, 35, 36, 38, 39, 40, 41, 42, 44, 58, 59, 60, 61, 63, 64, 91, 92, 93, 94, 96, 123, 124, 125 , 126};

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


/*******************************************************************************************************************
 * Function nano_verify_pointer
 * ---------------------------------------------------------------------------------------------------------------
 *  @brief Function receives the pointer @param ptr to verify if the memory was allocated correctly.
 *  	If the memory wasn't well allocated it sends and ERROR with a message	
 * 
 * @return Functions returns void
 *******************************************************************************************************************/
void nano_verify_pointer(char **ptr)
{
	if (ptr == NULL)
	{
		ERROR(NANO_ERROR_MALLOC, "[ERROR] Memory Allocation Failed\n");
	}
}


/*******************************************************************************************************************
 * Function nano_split_lineptr
 * ---------------------------------------------------------------------------------------------------------------
 *  @brief Function to parse and split the given string @param lineptr and split in different tokens separated by SPACE, 
 * 		adding them to @param tokens and terminate each token with NULL. The last position of @param tokens is also set 
 * 		to NULL so it can be later used in EXECVP.
 * 
 * @return Function returns a pointer to @param tokens with the necessary arguments for the EXECVP.
 *******************************************************************************************************************/
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


/*******************************************************************************************************************
 * Function nano_read_command
 * ---------------------------------------------------------------------------------------------------------------
 *  @brief Function reads the command inserted by the user using getline, and inserts the string terminator when it
 * 		finds the \n.
 * 
 * @return Function returns the string inserted by the user @param line
 *******************************************************************************************************************/
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


/*******************************************************************************************************************
 * Function nano_exec_commands
 * ---------------------------------------------------------------------------------------------------------------
 *  @brief Function receives @param lineptr with the inserted command by the user and makes the validations for
 * 		unsupported characters, splits the string in tokens to be executed in EXECVP, verifies command for terminating
 * 		nanoShell and verifies for a redirect command. After, it creates a children process to verify @param result for
 * 		the possible redirect. If there is a redirect sets @param outputfile for the destination with the options 
 * 		from the @param result. 
 * 		If there isn't any error the function executes the command with EXECVP.
 * 
 * @return Function returns void
 *******************************************************************************************************************/
void nano_exec_commands(char *lineptr)
{
	char **args;

	if (lineptr[0] != 0)
	{

		int res = nano_verify_char(lineptr);
		if (res == -1)
		{
			printf("[ERROR] Wrong request ' %s'\n", lineptr);
		}
		else
		{
			char *outputfile;
			int result;

			args = nano_split_lineptr(lineptr);

			nano_verify_terminate(args);

			/* Verify if it is a redirect command */
			result = nano_verify_redirect(args, &outputfile);

			pid_t pid = fork();
			if (pid == -1)
			{
				ERROR(NANO_ERROR_FORK, "Error executing fork().\n");
			}
			else if (pid == 0)
			{
				FILE *fp;

				switch (result)
				{
				case 1:
					printf("[INFO] stdout redirect to %s\n", outputfile);
					fp = freopen(outputfile, "w", stdout);
					break;
				case 2:
					printf("[INFO] stdout redirect to %s\n", outputfile);
					fp = freopen(outputfile, "a", stdout);
					break;
				case 3:
					printf("[INFO] stderr redirect to %s\n", outputfile);
					fp = freopen(outputfile, "w", stderr);
					break;
				case 4:
					printf("[INFO] stderr redirect to %s\n", outputfile);
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
}

/*******************************************************************************************************************
 * Function nano_loop
 * ---------------------------------------------------------------------------------------------------------------
 *  @brief Function starts the loop of the nanoShell. Calls nano_read_command to get the command from user 
 * 		to @param lineptr and sends it to nano_exec_commands to be parsed and handled.
 * 		If the nanoShell is started with -m option it also verifies if the given max executed commands were reached
 * 		to terminate nanoShell changing @param status to 1.
 * 
 * @return Function returns void
 *******************************************************************************************************************/
void nano_loop(void)
{
	char *lineptr;

	do
	{

		char *line = NULL;

		lineptr = nano_read_command(line);

		nano_exec_commands(lineptr);

		free(line);

		// If nanoShell is started with -m option
		if (counters.G_max_commands > 0 && counters.G_count_commands == counters.G_max_commands) {
			status = 1;
		}

	} while (status == 0);
}

/*******************************************************************************************************************
 * Function main
 * ---------------------------------------------------------------------------------------------------------------
 *  @brief Function main starts the commands counters from the @struct counters. 
 * 		Function handles the options given when starting nanoShell:
 * 				
 * 				-s : Creates a file "signals.txt" with the complete possible signals to send to nanoShell
 * 				-m : Terminates after a given int value of commands.
 * 				-f : Reads from a given file to execute commands.
 * 
 * 
 * @return Function returns a int value depending on the event. 0 if OK.
 *******************************************************************************************************************/
int main(int argc, char *argv[])
{
	/* Disable warnings */
	(void)argc;
	(void)argv;

	counters.G_count_commands = 0;
	counters.G_count_stderr = 0;
	counters.G_count_stdout = 0;

	struct gengetopt_args_info args;

	if (cmdline_parser(argc, argv, &args) != 0)
	{
		ERROR(C_ERROR_PARSING_ARGS, "Invalid arguments. nanoShell can't start.");
		exit(C_EXIT_FAILURE);
	}

	/*******************************************************************************************************************
	 * Help option: -h
	 * ---------------------------------------------------------------------------------------------------------------
	 *  @brief If nanoShell is started with option -h it shows the user some help regarding the nanoShell
	 * 
	 *******************************************************************************************************************/
	if (args.no_help_given)
	{

		printf("\nnanoShell, version 1.0.0 -> November 2020\n");
		printf("Authors: Alexandre Santos (2181593) & André Azevedo (2182634)\n");

		
		printf("\v\t# Use simple commands without metachars and pipes (ex: ps aux -l)\n");
		printf("\t# Use bye command to exit nanoShell\n");

		printf("\vOptions:\n");
		
		printf("\v  -f \t\tfile \t\t- run a list of commands in a file (filename argument). Each line should be a command\n");
		printf("  -h \t\thelp \t\t- shows a brief summary of options and arguments of each available option\n");
		printf("  -m \t\tmax \t\t- define the maximum number of commands the nanoShell should execute before terminating\n");
		printf("  -s \t\tsignal file \t- creates a 'signal.txt' file with all available commands that can send signals to the nanoShell.\n");

		printf("\vArguments:\n");

		printf("\v  -f, --file <fich>\n");
		printf("  -h, --help\n");
		printf("  -m, --max <int>\n");
		printf("  -s, --signalfile\n\n");

		return C_EXIT_SUCCESS;
	}
	
	/*******************************************************************************************************************
	 * Max executions option: -m {int}
	 * ---------------------------------------------------------------------------------------------------------------
	 *  @brief If option is given with a int value > 0 it starts the @struct counters @param G_max_commands with the
	 *	the given value and informs the user.
	 * 
	 *******************************************************************************************************************/
	if (args.max_given)
	{
		if (args.max_arg <= 0)
		{
			printf("[ERROR] Invalid value \'int\' for -m.\n\n");
			exit(NANO_MAX_INVALID);
		}
		else
		{
			counters.G_max_commands = args.max_arg;
			printf("[INFO] nanoShell with terminate after %d commands\n", counters.G_max_commands);
		}
	}

	/*******************************************************************************************************************
	 * Signals option: -s
	 * ---------------------------------------------------------------------------------------------------------------
	 *  @brief If nanoShell is started with option -s it creates a @file named "signals.txt" with the commands to send
	 * 		signals to nanoShell. 
	 * 		Example: kill -SIGINT {nanoShell PID}	
	 * 
	 *******************************************************************************************************************/
	if (args.signalfile_given)
	{

		FILE *fileptr;

		fileptr = fopen("signals.txt", "w");
		if (fileptr == NULL)
		{
			ERROR(NANO_ERROR_IO, "Error opening for writing!\n");
		}
		pid_t pid = getpid();

		fprintf(fileptr, "kill -SIGINT %d\nkill -SIGUSR1 %d\nkill -SIGUSR2 %d", pid, pid, pid);

		fclose(fileptr);
	}

	/*******************************************************************************************************************
	 * File option: -f {file_directory/name}
	 * ---------------------------------------------------------------------------------------------------------------
	 *  @brief If nanoShell is started with option -f it opens the given file to read every line and execute possible
	 * 		commands from the file.
	 * 		If the line starts with #, [LINE FEED], [SPACE] or [HORIZONTAL TAB] it is ignored.
	 * 		nanoShell is terminated after reading all the lines.
	 * 
	 *******************************************************************************************************************/
	if (args.file_given)
	{
		FILE *fileptr;
		char *lineptr = NULL;
		size_t n = 0;
		ssize_t result;

		fileptr = fopen(args.file_arg, "r");
		if (fileptr == NULL)
		{
			ERROR(NANO_ERROR_IO, "Error opening for reading!\n");
		}

		int i = 1;
		printf("[INFO] Executing from file %s\n", args.file_arg);

		while ((result = getline(&lineptr, &n, fileptr)) != -1)
		{
			//Verifies for #, [LINE FEED], [SPACE], [HORIZONTAL TAB] to ignore line
			if (lineptr[0] != 35 && lineptr[0] != 10 && lineptr[0] != 32 && lineptr[0] != 9)
			{
				lineptr[strcspn(lineptr, "\n")] = 0;
				printf("[command #%d]: %s\n", i, lineptr);
				nano_exec_commands(lineptr);
				i++;
			}
		}

		free(lineptr);
		fclose(fileptr);
		return C_EXIT_SUCCESS;
	}

	/*************************************************************
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
	 * SIGNAL HANDLER
	 * 
	 *************************************************************/
	struct sigaction act;

	/* Defines routine to handle signals */
	act.sa_sigaction = nano_sig_handler;

	/* empty signals mask -- dont block signals */
	sigemptyset(&act.sa_mask);

	act.sa_flags = SA_SIGINFO;	/*Additional info about signals */
	act.sa_flags |= SA_RESTART; /*recovers from blocking calls*/

	/* Captures signal SIGUSR1 */
	if (sigaction(SIGUSR1, &act, NULL) < 0)
	{
		ERROR(NANO_ERROR_SIGACTION, "sigaction - SIGUSR1");
	}
	/* Captures signal SIGUSR2 */
	if (sigaction(SIGUSR2, &act, NULL) < 0)
	{
		ERROR(NANO_ERROR_SIGACTION, "sigaction - SIGUSR2");
	} /* Captures signal SIGUSR1 */
	if (sigaction(SIGINT, &act, NULL) < 0)
	{
		ERROR(NANO_ERROR_SIGACTION, "sigaction - SIGINT");
	}


	/*************************************************************
	 * MAIN LOOP
	 * 
	 *************************************************************/
	nano_loop();

	if (args.max_arg && counters.G_max_commands > 0)
	{
		printf("[INFO] nanoShell executed %d commands\n", counters.G_count_commands);
	}

	return C_EXIT_SUCCESS;
}
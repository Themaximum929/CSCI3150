#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int shell_execute(char ** args, int argc)
{
	// char **args = arguments supplied to shell_execute() from the shell program (SimpleShell), argc = number of arguments
	int child_pid, wait_return, status;

	// If "EXIT" is inputted, exit the program
	if (strcmp(args[0], "EXIT") == 0)
		return -1; 
	
	// Declare fail if fork() cannot create child
	if ((child_pid = fork()) < 0) {
		printf("fork() error \n");
	}
	else if (child_pid == 0) {
		// Child process...

		/*
			Extract all commands (excluding pipes) into separate array

			pipe_count keeps track of how many pipes there are in the given command
			pipe_location keeps track of where the pipe appears in **args (initialized values are used for checking)
		*/
		int pipe_count = 0;
		int pipe_location[3] = {-1, -1, -1};
		printf("argc = %d\n", argc);
		// Find where the pipes are, and the number of pipes
		for (int i = 0; i < argc; i++) {
			if (args[i] != NULL) {
				if (strcmp(args[i], "|") == 0) {
					if (pipe_count < 3) {
						pipe_location[pipe_count] = i;
						pipe_count++;
					}
					else {
						printf("Too many pipes!\n");
						exit(-1);
					}
				}
			}
		}

		
		// printf("%d pipes found!\n", pipe_count);
		for (int i = 0; i < pipe_count; i++) {
			if (pipe_location[i] >= 0) {
				printf("Pipe %d found at location %d\n", i + 1, pipe_location[i]);
			}
		}
		

		// Do the cases 1 by 1
		if (pipe_count == 0) {
			printf("No pipe case\n");

			// On success execvp() does not return; return -1 on error and exit (abnormally)
			if ( execvp(args[0], args) < 0) { 
				printf("execvp() error \n");
				exit(-1);
			}
		}
		else if (pipe_count == 1) {
			printf("1 pipe case\n");

			// Separate the args
			int args1_len = pipe_location[0] + 1;
			int args2_len = argc - pipe_location[0] - 1;
			char * args1[args1_len];
			char * args2[args2_len];

			for (int i = 0; i < args1_len - 1; i++) {
				args1[i] = (char*)malloc(sizeof(args[i]));
				strcpy(args1[i], args[i]);
			}
			args1[args1_len - 1] = NULL;

			for (int i = args1_len; i < argc - 1; i++) {
				args2[i - args1_len] = (char*)malloc(sizeof(args[i]));
				strcpy(args2[i - args1_len], args[i]);
				printf("%s %s\n", args2[i - args1_len], args[i]);
			}
			args2[args2_len - 1] = NULL;
			// printf("args2[0] pre child: %s\n", args2[0]);

			
			int pipe1[2];
			if (pipe(pipe1) < 0) {
				printf("pipe() error\n");
				exit(-1);
			}

			if ((child_pid = fork()) < 0) {
				printf("fork() error \n");
				exit(-1);
			}
			else if (child_pid > 0) {
				// Parent...
				
				close(1); dup(pipe1[1]);
				close(pipe1[0]); close(pipe1[1]);
				
				if ( execvp(args1[0], args1) < 0) { 
					printf("execvp() error in args1\n");
					exit(-1);
				}
			}
			else {
				// Child...

				close(0); dup(pipe1[0]);
				close(pipe1[0]); close(pipe1[1]);

				// printf("args2[0]: %s\n", args2[0]);
				if ( execvp(args2[0], args2) < 0) { 
					printf("execvp() error in args2\n");
					exit(-1);
				}
				exit(1);
			}
			
		}
		else if (pipe_count == 2) {
			printf("2 pipe case\n");

			// Separate the args
			int args1_len = pipe_location[0] + 1;
			int args2_len = pipe_location[1] - pipe_location[0];
			int args3_len = argc - pipe_location[1] - 1;
			char * args1[args1_len];
			char * args2[args2_len];
			char * args3[args3_len];

			// Copy the arguments for command 1
			for (int i = 0; i < args1_len - 1; i++) {
				args1[i] = (char*)malloc(sizeof(args[i]));
				strcpy(args1[i], args[i]);
			}
			args1[args1_len - 1] = NULL;

			// Copy the arguments for command 2
			for (int i = args1_len; i < args1_len + args2_len - 1; i++) {
				args2[i - args1_len] = (char*)malloc(sizeof(args[i]));
				strcpy(args2[i - args1_len], args[i]);
			}
			args2[args2_len - 1] = NULL;

			// Copy the arguments for command 3
			for (int i = args1_len + args2_len; i < argc - 1; i++) {
				args3[i - args1_len - args2_len] = (char*)malloc(sizeof(args[i]));
				strcpy(args3[i - args1_len - args2_len], args[i]);
			}
			args3[args3_len - 1] = NULL;

			int pipe1[2], pipe2[2];
			if (pipe(pipe1) < 0 || pipe(pipe2) < 0) {
				printf("pipe() error\n");
				exit(-1);
			}

			if ((child_pid = fork()) < 0) {
				printf("fork() error \n");
				exit(-1);
			}
			else if (child_pid > 0) {
				// Parent...
				close(1); dup(pipe1[1]);
				close(pipe1[0]); close(pipe1[1]);

				if ( execvp(args1[0], args1) < 0) { 
					printf("execvp() error in args1\n");
					exit(-1);
				}
			}
			else {
				// Child...

				if ((child_pid = fork()) < 0) {
					printf("fork() error \n");
					exit(-1);
				}
				else if (child_pid > 0) {
					// Parent...
					close(0); dup(pipe1[0]);
					close(1); dup(pipe2[1]);
					close(pipe1[0]); close(pipe1[1]); close(pipe2[0]); close(pipe2[1]);

					if ( execvp(args2[0], args2) < 0) { 
						printf("execvp() error in args2\n");
						exit(-1);
					}
				}
				else {
					// Child...
					close(0); dup(pipe2[0]);
					close(pipe1[0]); close(pipe1[1]); close(pipe2[0]); close(pipe2[1]);

					if ( execvp(args3[0], args3) < 0) { 
						printf("execvp() error in args3\n");
						exit(-1);
					}
					exit(1);
				}
			}


		}
		else if (pipe_count == 3) {
			printf("3 pipe case\n");
			
			// Separate the args
			int args1_len = pipe_location[0] + 1;
			int args2_len = pipe_location[1] - pipe_location[0];
			int args3_len = pipe_location[2] - pipe_location[1];
			int args4_len = argc - pipe_location[2] - 1;
			char * args1[args1_len];
			char * args2[args2_len];
			char * args3[args3_len];
			char * args4[args4_len];

			// Copy the arguments for command 1
			for (int i = 0; i < args1_len - 1; i++) {
				args1[i] = (char*)malloc(sizeof(args[i]));
				strcpy(args1[i], args[i]);
			}
			args1[args1_len - 1] = NULL;

			// Copy the arguments for command 2
			for (int i = args1_len; i < args1_len + args2_len - 1; i++) {
				args2[i - args1_len] = (char*)malloc(sizeof(args[i]));
				strcpy(args2[i - args1_len], args[i]);
			}
			args2[args2_len - 1] = NULL;

			// Copy the arguments for command 3
			for (int i = args1_len + args2_len; i < args1_len + args2_len + args3_len - 1; i++) {
				args3[i - args1_len - args2_len] = (char*)malloc(sizeof(args[i]));
				strcpy(args3[i - args1_len - args2_len], args[i]);
			}
			args3[args3_len - 1] = NULL;

			// Copy the arguments for command 4
			for (int i = args1_len + args2_len + args3_len; i < argc - 1; i++) {
				args4[i - args1_len - args2_len - args3_len] = (char*)malloc(sizeof(args[i]));
				strcpy(args4[i - args1_len - args2_len - args3_len], args[i]);
			}
			args4[args4_len - 1] = NULL;

			int pipe1[2], pipe2[2], pipe3[2];
			if (pipe(pipe1) < 0 || pipe(pipe2) < 0 || pipe(pipe3) < 0) {
				printf("pipe() error\n");
				exit(-1);
			}

			if ((child_pid = fork()) < 0) {
				printf("fork() error \n");
				exit(-1);
			}
			else if (child_pid > 0) {
				// Parent...
				close(1); dup(pipe1[1]);
				close(pipe1[0]); close(pipe1[1]);

				if ( execvp(args1[0], args1) < 0) { 
					printf("execvp() error in args1\n");
					exit(-1);
				}
			}
			else {
				// Child...
				if ((child_pid = fork()) < 0) {
					printf("fork() error \n");
					exit(-1);
				}
				else if (child_pid > 0) {
					// Parent...
					close(0); dup(pipe1[0]);
					close(1); dup(pipe2[1]);
					close(pipe1[0]); close(pipe1[1]); close(pipe2[0]); close(pipe2[1]);

					if ( execvp(args2[0], args2) < 0) { 
						printf("execvp() error in args2\n");
						exit(-1);
					}
				}
				else {
					// Child...

					if ((child_pid = fork()) < 0) {
						printf("fork() error \n");
						exit(-1);
					}
					else if (child_pid > 0) {
						// Parent...
						close(0); dup(pipe2[0]);
						close(1); dup(pipe3[1]);
						close(pipe1[0]); close(pipe1[1]); close(pipe2[0]); close(pipe2[1]); close(pipe3[0]); close(pipe3[1]);

						if ( execvp(args3[0], args3) < 0) { 
							printf("execvp() error in args3\n");
							exit(-1);
						}
					}
					else {
						// Child...
						close(0); dup(pipe3[0]);
						close(pipe1[0]); close(pipe1[1]); close(pipe2[0]); close(pipe2[1]); close(pipe3[0]); close(pipe3[1]);

						if ( execvp(args3[0], args3) < 0) { 
							printf("execvp() error in args3\n");
							exit(-1);
						}
						exit(-1);
					}
				}
			}

		}
		else {
			// Should never get here (too many pipes), terminate abnormally with fallback message
			printf("Too many pipes!\n");
			exit(-1);
		}

		/*
		// On success execvp() does not return; return -1 on error and exit (abnormally)
		if ( execvp(args[0], args) < 0) { 
			printf("execvp() error \n");
			exit(-1);
		}
		*/
	}
	else {
		// Parent process...

		// Declare fail if wait terminates early (if wait() returns before the child finishes termination)
		if ((wait_return = wait(&status)) < 0)
			printf("wait() error \n"); 
	}
			
	return 0;

}

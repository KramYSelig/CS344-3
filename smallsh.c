/******************************************************************************
 * Author: Mark Giles
 * Filename: smallsh.c
 * Date Created: 11/13/2015
 * Date Last Modified: 11/13/2015
 * ***************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

// attribute to shell operation
struct Shell {
	int status[2],
		exit,
		argCount,
		sigChild;
	char cwd[1024],
		 buffer[2049],
	     *args[512];
};

// initialize global shell settings and attributes
void initShell(struct Shell *sh);
void runShell(struct Shell *sh);
void shellClrInput(struct Shell *sh);
void shellProcessArgs(struct Shell *sh);
void shellRunSysProc(struct Shell *sh);
void checkBgProcs(int signo);
void cleanup();
// global variables for signal handling
int children[200];
int numChildren = 0;
int bgCheck = 0;
char bgMessage[100];

int main(int argc, char *argv[]) {
	// create shell and allocate memory
	struct Shell *sh = malloc(sizeof(struct Shell));
	// create signal handler to avoid interruption
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	sigaction(SIGINT, &act, NULL);
	// signal handler for background processes
	struct sigaction sa;
	sa.sa_handler = checkBgProcs;
	sa.sa_flags = SA_RESTART;
	sigfillset(&sa.sa_mask);
	sigaction(SIGCHLD, &sa, NULL);
	// initialize attributes of shell
	initShell(sh);
	// shell operations and commands
	runShell(sh);
	// kill any zombie children
	cleanup();
	// free memory allocated for the shell
	free(sh);
	return 0;
}

// kill all processes created by parent
void cleanup() {
	int i = 0;
	for (i = 0; i < numChildren; i++) {
		kill(children[i], SIGKILL);
	}
}
// print message for background process that is completed including signal
void checkBgProcs(int signo) {
	int status,
		i = 0;
	pid_t exitPID = -5;
	exitPID = waitpid(-1, &status, WNOHANG);
	// read through children looking for a matching process ID
	for (i = 0; i < numChildren; i++) {
		if (exitPID == children[i]) {
			// record the status if the process exited
			if (WIFEXITED(status)) {
				bgCheck = 1;
				sprintf(bgMessage, "background pid %i is done: exit value %i\n", exitPID, status);
			} else if (WIFSIGNALED(status)) { // record the signal if process terminated by signal
				bgCheck = 1;
				sprintf(bgMessage, "background pid %i is done: terminated by signal %i\n", exitPID, status);
			}
		}
	}
}

// process system procedure not specified in this shell
void shellRunSysProc(struct Shell *sh) {
	int bg = -1,
		i = 0,
		inIndex = -1,
		outIndex = -1,
		fdRedirect = -5,
		inputFD,
		outputFD;
	char outFile[512],
		 inFile[512];
	pid_t spawnPID,
		exitPID;
	spawnPID = fork();
	
	// check if process is to run in the background
	if (strcmp(sh->args[sh->argCount - 1], "&") == 0) {
		bg = 1;
		sh->args[sh->argCount - 1] = '\0';
	}
	// look for argument cutoff index, strip out background 
	// and input/output redirection arguments
	for (i = 0; i < sh->argCount - 1; i++) {
		if (strcmp(sh->args[i], "<") == 0) {
			strcpy(inFile, sh->args[i + 1]);
			if (inIndex == -1) {
				inIndex = i;
			}
		} else if (strcmp(sh->args[i], ">") == 0) {
			strcpy(outFile, sh->args[i + 1]);
			if (outIndex == -1) {
				outIndex = i;
			}
		}
	}
	if (inIndex != -1 && outIndex != -1) {
		if (inIndex < outIndex) {
			sh->args[inIndex] = NULL;
		} else {
			sh->args[outIndex] = NULL;
		}
	} else if (inIndex != -1 && outIndex == -1) {
		sh->args[inIndex] = NULL;
	} else if (outIndex != -1 && inIndex == -1) {
		sh->args[outIndex] = NULL;
	}
	struct sigaction sach;
	// determine if current process is parent or child
	switch(spawnPID) {
		case -1:
			perror("Fork failed!");
			break;
		case 0:
			// child process, run system command based on arguments
			sigfillset(&sach.sa_mask);
			sigaction(SIGINT, &sach, NULL);
			// redirect input if specified
			if (inIndex != -1) {
				// open input file read only
				inputFD = open(inFile, O_RDONLY);
				if (inputFD == -1) {
					printf("smallsh: cannot open %s for input\n", inFile);
					exit(1);
				}
				fdRedirect = dup2(inputFD, 0);
				if (fdRedirect == -1) {
					perror("dup2");
					exit(2);
				}
			} else if (bg == 1) { // redirect background process input if not specified
				inputFD = open("/dev/null", O_RDONLY);
				if (inputFD == -1) {
					printf("smallsh: cannot open %s for input\n", inFile);
				}
				fdRedirect = dup2(inputFD, 0);
			}
			// redirect output if specified
			if (outIndex != -1) {
				// open output file write only
				outputFD = open(outFile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
				if (outputFD == -1) {
					printf("smallsh: cannot open %s for output\n", outFile);
				}
				fdRedirect = dup2(outputFD, 1);
				if (fdRedirect == -1) {
					perror("dup2");
					exit(2);
				}
			}
			// execute specified command
			sh->status[0] = execvp(sh->args[0], sh->args);
			// if this code exists, child command failed
			printf("%s: no such file or directory\n", sh->args[0]);
			exit(1);
			break;
		default:
			// parent process, check status of id
			if (bg != 1) {
				children[numChildren] = spawnPID;
				exitPID = waitpid(spawnPID, &sh->status[0], 0);
				numChildren++;
				if (WIFEXITED(sh->status[0])) {
					sh->status[1] = 0;
				} else if (WIFSIGNALED(sh->status[0])) {
					sh->status[1] = 1;
					printf("terminated by signal %i\n", sh->status[0]);
				}
			} else { // background process, no waiting, print process ID
				children[numChildren] = spawnPID;
				exitPID = waitpid(spawnPID, &sh->status[0], WNOHANG);
				numChildren++;
				printf("background pid is %i\n", spawnPID);
			}
			// before next command, check to see if background command completed
			// and print the background message
			if (bgCheck == 1) {
				printf("%s", bgMessage);
				// clear the background message for next use
				for (i = 0; i < 512; i++) {
					bgMessage[i] = '\0';
				}
				// reset background check value
				bgCheck = 0;
			}
			break;
	}
}

// initialize shell attributes
void initShell(struct Shell *sh) {
	int i = 0;
	sh->status[0] = 0;					// initial status is good (0)
	sh->status[1] = 0;					// initial type is uninterrupted exit
	sh->exit = 0;						// do not exit loop until this changes
	sh->argCount = 0;					// starting with 0 arguments
	getcwd(sh->cwd, sizeof(sh->cwd));	// sets initial working directory
	bgMessage[0] = '\0';				// message to store background process status
	bgCheck = 0;						// determines if background process completed
}

// process user arguments for shell
void shellProcessArgs(struct Shell *sh) {
	int error = -5;
	char *buff;
	// if first character of argument is #, this is a comment	
	if (strcmp(sh->args[0], "") == 0 || sh->args[0][0] == '#') {
		printf("\n");
	} else if (strcmp(sh->args[0], "exit") == 0) {
		// set exit value to 1 to avoid future command loop
		sh->exit = 1;
	} else if (strcmp(sh->args[0], "cd") == 0) {
		// process change directory internal to this shell
		sh->status[0] = 0;
		sh->status[1] = 0;
		if (sh->argCount == 1) {
			// change to directory specified in the HOME variable
			sh->status[0] = chdir(getenv("HOME"));
		} else if (sh->argCount == 2) {
			// change to the specified directory
			sh->status[0] = chdir(sh->args[1]);
		} else {
			printf("cd: Too many arguments\n");
			sh->status[0] = 1;
		}
	} else if (strcmp(sh->args[0], "status") == 0) {
		// determines status of most recent fg command
		if (sh->status[1] == 0) {
			if (sh->status[0] == 0) {
				printf("exit value 0\n");
			} else {
				printf("exit value 1\n");
			}
		} else if (sh->status[1] == 1) {
			// print this message if recent process terminated by signal
			printf("terminated by signal %i\n", sh->status[0]);
		}
	} else {
		// argument not a part of shell internal commands
		shellRunSysProc(sh);
	}
}

// run command loop and process user arguments
void runShell(struct Shell *sh) {
	int i = 0;
	char *token = malloc(sizeof(char) * 512);
	// loop for user input
	while (sh->exit == 0) {
		// create array for user arguments
		for (i = 0; i < 512; i++) {
			sh->args[i] = malloc(sizeof(char) * 512);
		}
	
		// clear shell input buffer and arguments array
		shellClrInput(sh);	
		fflush(0);
		if (bgCheck == 1) {
			printf("\n%s\n", bgMessage);
			for (i = 0; i < 512; i++) {
				bgMessage[i] = '\0';
			}
			bgCheck = 0;
		}
		// display colon for user prompt
		printf(": ");
		// read user input into shell buffer
		fgets(sh->buffer, 2048, stdin);
		// parse user input to shell arguments array
		token = strtok(sh->buffer, " ");
		// store user arguments in argument array
		while (token != NULL) {
			strcpy(sh->args[sh->argCount], token);
			sh->argCount++;
			token = strtok(NULL, " ");
		}
		
		// remove newline character from last argument
		sh->args[sh->argCount - 1][strlen(sh->args[sh->argCount - 1]) - 1] = '\0';
		// null terminate argument after the last
		sh->args[sh->argCount] = NULL;
		shellProcessArgs(sh);
		for (i = 0; i < 512; i++) {
			free(sh->args[i]);
		}
	}
	free(token);
}

// clears the input buffer and argument count between commands
void shellClrInput(struct Shell *sh) {
	int i = 0,
		j = 0;
	
	// clear input buffer for next user input
	for (i = 0; i < 2049; i++) {
		sh->buffer[i] = '\0';
	}
	// reset argument count
	sh->argCount = 0;
}


/******************************************************************************
 * Author: Mark Giles
 * Filename: smallsh.c
 * Date Created: 11/13/2015
 * Date Last Modified: 11/13/2015
 * Description: 
 *
 * Input:
 *
 * Output:
 *
 * ***************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct ChildProcess {
	pid_t pid;
	int exitStatus,
		termSignal;
};

struct Shell {
	int status[2],
		exit,
		argCount,
		numChildren;
	char cwd[1024],
		 buffer[2049],
	     *args[512];
	struct ChildProcess children[200];
};



// initialize global shell settings and attributes
void initShell(struct Shell *sh);
void runShell(struct Shell *sh);
void shellClrInput(struct Shell *sh);
void shellProcessArgs(struct Shell *sh);
void shellRunSysProc(struct Shell *sh);

int main(int argc, char *argv[]) {
	struct Shell *sh = malloc(sizeof(struct Shell));
	initShell(sh);
	runShell(sh);
	return 0;
}


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
	fflush(0);
	spawnPID = fork();
	if (strcmp(sh->args[sh->argCount - 1], "&") == 0) {
		bg = 1;
		sh->args[sh->argCount - 1] = '\0';
	}
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

		switch(spawnPID) {
		case -1:
			perror("Fork failed!");
			break;
		case 0:
			if (inIndex != -1) {
				// open input file read only
				inputFD = open(inFile, O_RDONLY);
				fdRedirect = dup2(inputFD, 0);
				if (fdRedirect == -1) {
					perror("dup2");
					exit(2);
				}
			}
			if (outIndex != -1) {
				// open output file write only
				outputFD = open(outFile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
				fdRedirect = dup2(outputFD, 1);
				if (fdRedirect == -1) {
					perror("dup2");
					exit(2);
				}
			}
			errno = 0;
			sh->status[0] = execvp(sh->args[0], sh->args);
			if (sh->status[0] == -1 && errno != 0) {
			
			}
			printf("%s: Command not found.\n", sh->args[0]);
			exit;
			break;
		default:
			sh->children[sh->numChildren].pid = spawnPID;
			sh->numChildren++;
			
			if (bg != 1) {
				exitPID = waitpid(spawnPID, &sh->status[0], 0);
			} else {
				exitPID = waitpid(spawnPID, &sh->status[0], WNOHANG);
			}
			if (WIFEXITED(sh->status[0])) {
				sh->children[sh->numChildren - 1].exitStatus = WEXITSTATUS(sh->status[0]);
				sh->status[1] = 0;
			} else if (WIFSIGNALED(sh->status[0])) {
				sh->children[sh->numChildren - 1].termSignal = WTERMSIG(sh->children[sh->numChildren - 1].exitStatus);
				sh->status[1] = 1;
			}
			break;
	}
}

void initShell(struct Shell *sh) {
	int i = 0;
	sh->status[0] = 0;					// initial status is good (0)
	sh->status[1] = 0;					// initial type is uninterrupted exit
	sh->exit = 0;						// do not exit loop until this changes
	sh->argCount = 0;					// starting with 0 arguments
	getcwd(sh->cwd, sizeof(sh->cwd));	// sets initial working directory
	sh->numChildren = 0;
}

void shellProcessArgs(struct Shell *sh) {
	int error = -5;
	char *buff;
	if (strcmp(sh->args[0], "") == 0 || sh->args[0][0] == '#') {
		
	} else if (strcmp(sh->args[0], "exit") == 0) {
		sh->exit = 1;
	} else if (strcmp(sh->args[0], "cd") == 0) {
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
		if (sh->status[1] == 0) {
			if (sh->status[0] == 0) {
				printf("exit value 0\n");
			} else {
				printf("exit value 1\n");
			}
		} else if (sh->status[1] == 1) {
			printf("terminated by signal %i\n", sh->status[0]);
		}
	} else {
		shellRunSysProc(sh);
	}
}

void runShell(struct Shell *sh) {
	int i = 0;
	char *token = malloc(sizeof(char) * 512);
	while (sh->exit == 0) {
		for (i = 0; i < 512; i++) {
			sh->args[i] = malloc(sizeof(char) * 512);
		}
	
		// clear shell input buffer and arguments array
		shellClrInput(sh);
		// display colon for user prompt
		printf(": ");
		// read user input into shell buffer
		fgets(sh->buffer, 2048, stdin);
		// parse user input to shell arguments array
		token = strtok(sh->buffer, " ");
		
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

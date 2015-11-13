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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct Shell {
	int status[2],
		exit,
		argCount;
	char cwd[1024],
		 buffer[2049],
	     args[512][512];
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

void initShell(struct Shell *sh) {
	sh->status[0] = 0;
	sh->status[1] = 0;
	sh->exit = 0;
	sh->argCount = 0;
	getcwd(sh->cwd, sizeof(sh->cwd));
}

void shellRunSysProc(struct Shell *sh) {
	printf("forks and stuff\n");
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
	char *token;
	while (sh->exit == 0) {
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
		sh->args[sh->argCount - 1][strlen(sh->args[sh->argCount - 1]) - 1] = '\0';
		shellProcessArgs(sh);
	}
}

void shellClrInput(struct Shell *sh) {
	int i = 0,
		j = 0;

	// clear shell argument strings for next command
	for (i = 0; i < sh->argCount; i++) {
		for (j = 0; j < 512; j++) {
			sh->args[sh->argCount][j] = '\0';
		}
	}
	// clear input buffer for next user input
	for (i = 0; i < 2049; i++) {
		sh->buffer[i] = '\0';
	}
	// reset argument count
	sh->argCount = 0;
}

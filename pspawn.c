/************************************************************************//**
 * \file 
 * \brief Spawns a child process using a pseudo terminal.
 *
 * Spawns a child process using a pseudo-terminal to avoid input/output
 * buffering. The standard output of the child process is redirected to
 * the standard output of the parent process.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 ****************************************************************************/
#include "pspawn.h"
#include <unistd.h>
#include <pty.h>
#include <stdio.h>
#include <sys/wait.h>
#include <errno.h>

/// Lenght of the line buffer
#define PSPAWN_LINE_BUF_LEN		256

/************************************************************************//**
 * Spawns a child process using a pseudo-terminal.
 *
 * \param[in] file File name of the process to spawn.
 * \param[in] arg  Array of the arguments passed to the new process. By
 *                 convention, argument 0 is the process name.
 * \return -1 if the process spawn failed, -2 if child didn't exit properly,
 *            or the process return code otherwise.
 ****************************************************************************/
int pspawn(const char *file, char *const arg[]) {
	char lineBuf[PSPAWN_LINE_BUF_LEN];
	int fd, stat;
	ssize_t readed;

	// Use forkpty instead of fork/exec or popen, to avoid buffering
    switch(forkpty(&fd, NULL, NULL, NULL)) {
        case -1:    // forkpty failed
            perror("pspawn: ");
            return -1;

        case 0:     // Child process spawn process
            execvp(file, arg);
			// execvp should never return unless error occurs
			perror("pspawn child: ");
            _exit(-1);

        default:    // Parent process
			// read from fd, print readed data and end when EOF
			while ((readed = read(fd, lineBuf, PSPAWN_LINE_BUF_LEN)) > 0) {
				lineBuf[readed] = '\0';
				fputs(lineBuf, stdout);
			}
			// Wait for child to exit
			wait(&stat);
			if (WIFEXITED(stat)) return WEXITSTATUS(stat);
			else return -2;
			break;
    } // switch(forkpty(...))

	return 0;
}


/************************************************************************//**
 * \file
 * \brief Spawns a child process using a pseudo terminal.
 *
 * \defgroup pspawn pspawn
 * \{
 * \brief Spawns a child process using a pseudo terminal.
 *
 * Spawns a child process using a pseudo-terminal to avoid input/output
 * buffering. The standard output of the child process is redirected to
 * the standard output of the parent process.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 ****************************************************************************/

#ifndef _PSPAWN_H_
#define _PSPAWN_H_

/************************************************************************//**
 * Spawns a child process using a pseudo-terminal.
 *
 * \param[in] file File name of the process to spawn.
 * \param[in] arg  Array of the arguments passed to the new process. By
 *                 convention, argument 0 is the process name.
 * \return 0 if OK, -1 if there was an error when trying to spawn the new
 *         process.
 ****************************************************************************/
int pspawn(const char *file, char *const arg[]);

#endif /*_PSPAWN_H_*/

/** \} */

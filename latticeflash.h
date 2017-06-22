/************************************************************************//**
 * \file
 * \brief Flash specified xcf file to FPGA.
 *
 * \defgroup latticeflash latticeflash
 * \{
 * \brief Flash specified xcf file to FPGA.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 ****************************************************************************/
#ifndef _LATTICE_FLASH_H_
#define _LATTICE_FLASH_H_

#include "util.h"

/************************************************************************//**
 * Uses lattice prgcmd to burn xcf file to FPGA
 *
 * \param[in] prgcmd Complete path to lattice programmer.
 * \param[in] xcf    XCF input file to burn to FPGA.
 *
 * \return -1 if spawning avrdude failed, -2 if avrdude didn't exit
 *         properly, -3 if any other error occurred,  or avrdude return
 *         code if otherwise.
 ****************************************************************************/
int LatticeFlash(const char prgcmd[], const char xcf[]);

#endif /*_LATTICE_FLASH_H_*/

/** \} */


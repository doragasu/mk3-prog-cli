/************************************************************************//**
 * \file
 * \brief Flash specified xcf file to FPGA.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 ****************************************************************************/
#include "latticeflash.h"
#include "pspawn.h"
#include <stdio.h>

/// Arguments used to burn the firmware of the programmer MCU
/// \note As the file to launch is a shell script, we spawn sh and let it
/// launch the script.
static const char *latFlash[] = {
	"/bin/sh", "-c", NULL, "-input", NULL, NULL
};

/// Position in latFlash argument array of the flasher binary/script
#define LATTICE_FLASH_BIN_POS	2
/// Position in latFlash argument array of the input XCF file
#define LATTICE_FLASH_XCF_POS	4

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
int LatticeFlash(const char prgcmd[], const char xcf[]) {
	int retVal;

	// Allocate memory for command line arguments
	retVal = -3;

	// Fill missing fields of the command arguments (every NULL excepting
	// the last one).
	latFlash[0] = prgcmd;
	latFlash[2] = xcf;

	// Invoke avrdude
	if ((retVal = pspawn(latFlash[0], (char *const*)latFlash))) {
		PrintErr("Lattice programmer failed!\n");
	}
	// Return error code
	return retVal;
}



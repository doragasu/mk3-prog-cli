/************************************************************************//**
 * \file
 * \brief Flashes elf files to programmer MCU or cart CIC.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 ****************************************************************************/
#include "avrflash.h"
#include "pspawn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// Arguments used to burn the firmware of the programmer MCU
static const char *avrFlash[] = {
	NULL, "-p", NULL, "-C", NULL, "-c", NULL,
	"-U", NULL, "-U", NULL, "-U", NULL, NULL
};

/// Position in avrflash argument array of executable file
#define AVR_AVRDUDE_POS		0
/// Position in avrflash argument array of the MCU to flash
#define AVR_FLASH_MCU_POS	2
/// Position in avrflash argument array of the programmer configuration file
#define AVR_FLASH_CFG_POS	4
/// Position in avrflash argument array of the programmer to use
#define AVR_FLASH_PROG_POS	6
/// Position in avrFlash argument array of the flash command
#define AVR_FLASH_CMD_POS	8
/// Position in avrFlash argument array of the high fuse command
#define AVR_FUSEH_CMD_POS	10
/// Position in avrFlash argument array of the low fuse command
#define AVR_FUSEL_CMD_POS	12

/// avrdude command to write to flash
#define AVR_FLASH_CMD		"flash:w:"
/// avrdude command to write higher fuse
#define AVR_FUSEH_CMD		"hfuse:w:"
/// avrdude command to write lower fuse
#define AVR_FUSEL_CMD		"lfuse:w:"

/************************************************************************//**
 * Uses avrdude to flash specified firmware file.
 *
 * \param[in] path Path of the avrdude binary.
 * \param[in] cfg  avrdude configuration file (-C avrdude switch).
 * \param[in] mcu  Microcontroller (-p avrdude switch argument).
 * \param[in] file Firmware to flash, elf format needed for the fuses
 *                 to work.
 * \param[in] prog Programmer (-c avrdude switch).
 *
 * \return -1 if spawning avrdude failed, -2 if avrdude didn't exit
 *         properly, -3 if malloc failed or avrdude return code if otherwise.
 ****************************************************************************/
int AvrFlash(const char path[], const char cfg[], const char mcu[],
		const char file[], const char prog[]) {
	char *flashBuf = NULL;
	char *hfuseBuf = NULL;
	char *lfuseBuf = NULL;
	int retVal;
	size_t fileLen;

	// Allocate memory for command line arguments
	retVal = -3;
	fileLen = strlen(file);
	if (!(flashBuf = (char*)malloc(sizeof(AVR_FLASH_CMD) + fileLen))) {
		perror("avrflash: ");
		goto dealloc_exit;
	}
	if (!(hfuseBuf = (char*)malloc(sizeof(AVR_FUSEH_CMD) + fileLen))) {
		perror("avrflash: ");
		goto dealloc_exit;
	}
	if (!(lfuseBuf = (char*)malloc(sizeof(AVR_FUSEL_CMD) + fileLen))) {
		perror("avrflash: ");
		goto dealloc_exit;
	}

	// Fill missing fields of the command arguments (every NULL excepting
	// the last one).
	strcpy(flashBuf, AVR_FLASH_CMD);
	strcpy(flashBuf + sizeof(AVR_FLASH_CMD) - 1, file);
	strcpy(hfuseBuf, AVR_FUSEH_CMD);
	strcpy(hfuseBuf + sizeof(AVR_FUSEH_CMD) - 1, file);
	strcpy(lfuseBuf, AVR_FUSEL_CMD);
	strcpy(lfuseBuf + sizeof(AVR_FUSEL_CMD) - 1, file);
	avrFlash[AVR_AVRDUDE_POS] = path;
	avrFlash[AVR_FLASH_MCU_POS] = mcu;
	avrFlash[AVR_FLASH_CFG_POS] = cfg;
	avrFlash[AVR_FLASH_PROG_POS] = prog;
	avrFlash[AVR_FLASH_CMD_POS] = flashBuf;
	avrFlash[AVR_FUSEH_CMD_POS] = hfuseBuf;
	avrFlash[AVR_FUSEL_CMD_POS] = lfuseBuf;

	// Invoke avrdude
	if ((retVal = pspawn(avrFlash[0], (char *const*)avrFlash))) {
		PrintErr("avrdude failed while trying fo write to flash!\n");
	}
	// Free used RAM and exit
dealloc_exit:
	if (lfuseBuf) free(lfuseBuf);
	if (hfuseBuf) free(hfuseBuf);
	if (flashBuf) free(flashBuf);
	return retVal;
}


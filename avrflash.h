/************************************************************************//**
 * \file
 * \brief Flashes elf files to programmer MCU or cart CIC.
 * 
 * \defgroup avrflash avrflash
 * \{
 * \brief Flashes elf files to programmer MCU or cart CIC.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 ****************************************************************************/
#ifndef _AVRFLASH_H_
#define _AVRFLASH_H_

#include "util.h"

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
		const char file[], const char prog[]);

#endif /*_AVRFLASH_H_*/

/** \} */


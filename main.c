/************************************************************************//**
 * \file
 * \brief Command Line Interface for the mojo-nes-mk3 programmer.
 *
 * \defgroup main main
 * \{
 * \brief Command Line Interface for the mojo-nes-mk3 programmer.
 *
 * This utility allows to manage mojo-nes-mk3 cartridges, usina a
 * mojo-nes-mk3 programmer. The utility allows to program and read flash
 * and RAM chips. A driver system allows to support several mapper chip
 * implementations.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 ****************************************************************************/
#include "util.h"
#ifdef __OS_WIN
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <signal.h>
#endif

#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <glib.h>

#include "progbar.h"
#include "cmd.h"
#include "avrflash.h"
#include "latticeflash.h"

/// Major version of the program
#define VERSION_MAJOR	0x00
/// Minor version of the program
#define VERSION_MINOR	0x04

/// Maximum file length
#define MAX_FILELEN		255

/** \addtogroup ProgChips
 *  \brief Programmable flash chips
 *  \{ */
/// Character ROM chip (CHR ROM)
#define PROG_CHIP_CHR	0
/// Program ROM chip (PRG ROM)
#define PROG_CHIP_PRG	1
/// Last value for programmable chips
#define PROG_CHIP_MAX	PROG_CHIP_PRG
/** \} */

/// Return value for Erase operation error
#define PROG_ERASE_FULL	0xFFFFFF

/// SRAM base address
#define PROG_SRAM_BASE	0x6000
/// SRAM length
#define PROG_SRAM_LEN	(8 * 1024)

/// Definition of the chip of the programmer (ATMEGA8515)
#define AVR_CHIP_MCU	"m8515"
///Definition of the CIC chip (ATTINY13)
#define AVR_CHIP_CIC	"t13"
/// avrdude binary path
#define AVR_PATH			"/usr/bin/avrdude"
/// Configuration file of the programmer to use
#define AVR_PROG_CFG		"/usr/share/mk3-prog/mk3prog.conf"
/// MCU programmer defined in mk3prog.conf
#define AVR_PROG_MCU		"mk3prog-mcu"
/// CIC programmer defined in mk3prog.conf
#define AVR_PROG_CIC		"mk3prog-cic"

/// Path to the FPGA programmer program/script
#define LATT_PROG_PATH		"/usr/local/diamond/3.7_x64/bin/lin64/pgrcmd"

/// Copies the specified address to a byte array field
#define CMD_SET_ADDR(field, addr)	do{	\
	(field)[0] = (addr)>>16;			\
	(field)[1] = (addr)>>8;				\
	(field)[2] = (addr);				\
}while(0)

/// Copies the command length to the specified byte array field
#define CMD_SET_LEN(field, len)	do{	\
	(field)[0] = (len)>>8;			\
	(field)[1] = (len);				\
}while(0)

/// Printf-like macro that prints only if condition is TRUE.
#define CondPrintf(cond, ...)	do{if(cond) printf(__VA_ARGS__);}while(0)

/// Printf-like macro that prints only if f.verbose is TRUE.
#define PrintVerb(...)			\
	do{if(f.verbose) printf(__VA_ARGS__);fflush(stdout);}while(0)

/// Definition of a file representing a memory image.
typedef struct {
	char *file;		///< Image file name
	uint32_t addr;	///< Memory address of the image
	uint32_t len;	///< Length of the memory image
} MemImage;

/// Supported option flags
typedef union {
	uint32_t all;				///< Access to all flags
	struct {
		uint8_t fwVer:1;		///< Get firmware version
		uint8_t verify:1;		///< Verify flashed files
		uint8_t verbose:1;		///< Verbose operation
		uint8_t flashId:1;		///< Get flash chip IDs
		uint8_t chrErase:1;		///< Erase CHR flash
		uint8_t prgErase:1;		///< Erase PRG flash
		uint8_t dry:1;			///< Dry run
	};
} Flags;

/*
 * Global variables.
 */

/// Options supported by the client
const static struct option opt[] = {
		{"firm-ver",	no_argument,		NULL,	'f'},
        {"flash-chr",   required_argument,  NULL,   'c'},
        {"flash-prg",   required_argument,  NULL,   'p'},
        {"read-chr",    required_argument,  NULL,   'C'},
        {"read-prg",    required_argument,  NULL,   'P'},
        {"erase-chr",   no_argument,        NULL,   'e'},
        {"erase-prg",   no_argument,        NULL,   'E'},
        {"chr-sec-er",  required_argument,  NULL,   's'},
        {"prg-sec-er",  required_argument,  NULL,   'S'},
        {"verify",      no_argument,        NULL,   'V'},
        {"flash-id",    no_argument,        NULL,   'i'},
		{"read-ram",	required_argument,  NULL,	'R'},
		{"write-ram",	required_argument,  NULL,	'W'},
		{"fpga-flash",	required_argument,	NULL,	'b'},
		{"cic-flash",	required_argument,	NULL,	'a'},
        {"firm-flash",  required_argument,  NULL,   'F'},
        {"mpsse-if",    required_argument,  NULL,   'm'},
        {"mapper",      required_argument,  NULL,   'M'},
		{"dry-run",     no_argument,		NULL,   'd'},
        {"version",     no_argument,        NULL,   'r'},
        {"verbose",     no_argument,        NULL,   'v'},
        {"help",        no_argument,        NULL,   'h'},
        {NULL,          0,                  NULL,    0 }
};

/// Descriptions of the supported options
const static char *description[] = {
	"Get programmer firmware version",
	"Flash file to CHR ROM",
	"Flash file to PRG ROM",
	"Read CHR ROM to file",
	"Read PRG ROM to file",
	"Erase CHR Flash",
	"Erase PRG Flash",
	"Erase CHR flash sector",
	"Erase PRG flash sector",
	"Verify flash after writing file",
	"Obtain flash chips identifiers",
	"Read data from RAM chip",
	"Write data to RAM chip",
	"Upload bitfile to FPGA, using .xcf file",
	"AVR CIC firmware flash",
	"Flash programmer firmware",
	"Set MPSSE interface number",
	"Set mapper: 1-NOROM, 2-MMC3, 3-NFROM",
	"Dry run: don't actually do anything",
	"Show program version",
	"Show additional information",
	"Print help screen and exit"
};

/*
 * PRIVATE FUNCTIONS
 */

#ifndef __OS_WIN
/************************************************************************//**
 * Signal handler that restores cursor and aborts program.
 * 
 * \param[in] sig Received signal causing abortion.
 ****************************************************************************/
static void Terminate(int sig) {
	PrintErr("Caught signal %d, aborting...\n", sig);
	// Restore default cursor
	printf("\e[?25h");
	exit(1);
}
#endif

/************************************************************************//**
 * Print program version.
 * 
 * \param[in] prgName Program name to print along with program version.
 ****************************************************************************/
static void PrintVersion(char *prgName) {
	printf("%s version %d.%d, doragasu 2016.\n", prgName,
			VERSION_MAJOR, VERSION_MINOR);
}

/************************************************************************//**
 * Print help message.
 * 
 * \param[in] prgName Program name to print along with help message.
 ****************************************************************************/
static void PrintHelp(char *prgName) {
	int i;

	PrintVersion(prgName);
	printf("Usage: %s [OPTIONS [OPTION_ARG]]\nSupported options:\n\n", prgName);
	for (i = 0; opt[i].name; i++) {
		printf(" -%c, --%s%s: %s.\n", opt[i].val, opt[i].name,
				opt[i].has_arg == required_argument?" <arg>":"",
				description[i]);
	}
	// Print additional info
	printf("For file arguments, it is possible to specify start address and "
		   "file length to read/write in bytes, with the following format:\n"
		   "    file_name:memory_address:file_length\n\n"
		   "Examples:\n"
		   "TODO\n"
	       "\nNOTES:\n"
		   "\t- To flash CIC and programmer firmware, avrdude must be "
		   "installed, with corresponding configuration file.\n"
		   "\t- Uploading bitfiles to FPGA, requires Lattice Diamond or \n"
		   "\t  Programmer Standalone to be installed.\n");
}

/************************************************************************//**
 * Receives a MemImage pointer with full info in file name (e.g.
 * m->file = "rom.bin:6000:1"). Removes from m->file information other
 * than the file name, and fills the remaining structure fields if info
 * is provided (e.g. info in previous example would cause m = {"rom.bin",
 * 0x6000, 1}).
 *
 * \param[inout] m Pointer to memory image argument to parse.
 *
 * \return 0 if success, non-zero on error.
 ****************************************************************************/
static int ParseMemArgument(MemImage *m) {
	int i;
	char *addr = NULL;
	char *len  = NULL;
	char *endPtr;

	// Set address and length to default values
	m->len = m->addr = 0;

	// First argument is name. Find where it ends
	for (i = 0; i < (MAX_FILELEN + 1) && m->file[i] != '\0' &&
			m->file[i] != ':'; i++);
	// Check if end reached without finding end of string
	if (i == MAX_FILELEN + 1) return 1;
	if (m->file[i] == '\0') return 0;
	
	// End of token marker, search address
	m->file[i++] = '\0';
	addr = m->file + i;
	for (; i < (MAX_FILELEN + 1) && m->file[i] != '\0' && m->file[i] != ':';
			i++);
	// Check if end reached without finding end of string
	if (i == MAX_FILELEN + 1) return 1;
	// If end of token marker, search length
	if (m->file[i] == ':') {
		m->file[i++] = '\0';
		len = m->file + i;
		// Verify there's an end of string
		for (; i < (MAX_FILELEN + 1) && m->file[i] != '\0'; i++);
		if (m->file[i] != '\0') return 1;
	}
	// Convert strings to numbers and return
	if (addr && *addr) m->addr = strtol(addr, &endPtr, 0);
	if (m->addr == 0 && addr == endPtr) return 2;
	if (len  && *len)  m->len  = strtol(len, &endPtr, 0);
	if (m->len  == 0 && len  == endPtr) return 3;

	return 0;
}

/************************************************************************//**
 * Print a memory image structure.
 * 
 * \param[in] m Pointer to memory image argument to print.
 ****************************************************************************/
static void PrintMemImage(MemImage *m) {
	printf("%s", m->file);
	if (m->addr) printf(" at address 0x%06X", m->addr);
	if (m->len ) printf(" (%d bytes)", m->len);
}

/************************************************************************//**
 * Print a memory range error message corresponding to input code.
 * 
 * \param[in] code Error code to print.
 ****************************************************************************/
static void PrintMemError(int code) {
	switch (code) {
		case 0: printf("Memory range OK.\n"); break;
		case 1: PrintErr("Invalid memory range string.\n"); break;
		case 2: PrintErr("Invalid memory address.\n"); break;
		case 3: PrintErr("Invalid memory length.\n"); break;
		default: PrintErr("Unknown memory specification error.\n");
	}
}

/************************************************************************//**
 * Obtain programmer firmware version:
 *
 * \return 0 on success, less than 0 on error.
 ****************************************************************************/
static int ProgFwGet(void) {
	Cmd cmd;
	CmdRep *rep;

	cmd.command = CMD_FW_VER;

	if (CmdSend(&cmd, 1, &rep) < 0) return -1;

	printf("Awesome MOJO-NES programmer firmware: %d.%d\n",
			rep->fwVer.ver_major, rep->fwVer.ver_minor);
	CmdRepFree(rep);
	return 0;
}

/************************************************************************//**
 * Obtain flash chip identifiers of the inserted cart.
 *
 * \return 0 on success, less than 0 on error.
 ****************************************************************************/
static int ProgFIdGet(void) {
	Cmd cmd;
	CmdRep *rep;

	cmd.command = CMD_FLASH_ID;

	if (CmdSend(&cmd, 1, &rep) < 0) return -1;

	printf("CHR --> ManID: 0x%02X. DevID: 0x%02X:%02X:%02X\n", rep->fId.chr.manId,
			rep->fId.chr.devId[0], rep->fId.chr.devId[1],
			rep->fId.chr.devId[2]);
	printf("PRG --> ManID: 0x%02X. DevID: 0x%02X:%02X:%02X\n", rep->fId.prg.manId,
			rep->fId.prg.devId[0], rep->fId.prg.devId[1],
			rep->fId.prg.devId[2]);
	CmdRepFree(rep);
	return 0;
}

/************************************************************************//**
 * Erases specified flash.
 *
 * \param[in] chip Flash chip to erase.
 * \param[in] addr Address of the sector to erase.
 *
 * \return 0 on success, less than 0 on error.
 ****************************************************************************/
static int ProgFlashErase(uint8_t chip, uint32_t addr) {
	Cmd cmd;
	CmdRep *rep;

	cmd.rdWr.cmd = CMD_CHR_ERASE + chip;
	CMD_SET_ADDR(cmd.rdWr.addr, addr);

	if (CmdSend(&cmd, 4, &rep) < 0) return -1;
	CmdRepFree(rep);
	return 0;
}

/************************************************************************//**
 * Allocates a RAM buffer, reads the specified MemImage file, and writes it
 * to the specified flash chip.
 *
 * \param[in] chip Flash chip to program.
 * \param[in] f    Memory image to program to specified chip.
 * \param[in] cols Number of columns of the terminal, used to draw the
 *                 status bar.
 *
 * \return Pointer to the raw data of the allocated and flashed image file,
 *         or NULL if error occurred.
 *
 * \warning Buffer must be externally deallocated when no longer needed,
 *          using free().
 ****************************************************************************/
static uint8_t *AllocAndFlash(uint8_t chip, MemImage *f, unsigned int cols) {
    FILE *rom;
	uint8_t *writeBuf;
	uint32_t addr;
	int toWrite;
	uint32_t i;
	Cmd cmd;
	CmdRep *rep = NULL;

	// Address string, e.g.: 0x123456
	char addrStr[9];

	if (chip > PROG_CHIP_MAX) return NULL;

	if (!(rom = fopen(f->file, "rb"))) {
		perror(f->file);
		return NULL;
	}

	// Obtain length if not specified
	if (!f->len) {

	    fseek(rom, 0, SEEK_END);
	    f->len = ftell(rom);
	    fseek(rom, 0, SEEK_SET);
	}

    writeBuf = malloc(f->len);
	if (!writeBuf) {
		perror("Allocating write buffer");
		fclose(rom);
		return NULL;
	}
	// Read the entire ROM and close file
    if (1 > fread(writeBuf, f->len, 1, rom)) {
		fclose(rom);
		free(writeBuf);
		PrintErr("Error reading ROM file!\n");
		return NULL;
	}
	fclose(rom);

   	printf("Flashing %s ROM %s starting at 0x%06X...\n", chip?"PRG":"CHR",
			f->file, f->addr);

	// Send flash command to programmer
	cmd.rdWr.cmd = CMD_CHR_WRITE + chip;
	for (i = 0, addr = f->addr; i < f->len;) {
		toWrite = MIN(32768, f->len - i);
		CMD_SET_ADDR(cmd.rdWr.addr, addr);
		CMD_SET_LEN(cmd.rdWr.len, toWrite);
		if ((CmdSendLongCmd(&cmd, sizeof(CmdRdWrHdr), writeBuf + i,
				toWrite, &rep) != CMD_OK) || (rep->command != CMD_OK)) {
			PrintErr("CMD response: %d. Couldn't write to cart!\n",
					rep->command);
			if (rep) CmdRepFree(rep);
			free(writeBuf);
			return NULL;
		}
		CmdRepFree(rep);
		// Update vars and draw progress bar
		i += toWrite;
		addr += toWrite;
   	    sprintf(addrStr, "0x%06X", addr);
   	    ProgBarDraw(i, f->len, cols, addrStr);
	}
   	putchar('\n');
	return writeBuf;
}


/************************************************************************//**
 * Allocates a RAM buffer, and reads range specified in MemImage input
 * from the specified Flash chip to the allocated buffer.
 *
 * \param[in] chip Flash chip to read.
 * \param[in] f    Memory image with the range to read.
 * \param[in] cols Number of columns of the terminal, used to draw the
 *                 status bar.
 *
 * \return Pointer to the raw data of the allocated and read image file,
 *         or NULL if error occurred.
 *
 * \warning Buffer must be externally deallocated when no longer needed,
 *          using free().
 ****************************************************************************/
uint8_t *AllocAndRead(uint8_t chip, MemImage *f, unsigned int cols) {
	uint8_t *readBuf;
	int toRead;
	uint32_t addr;
	uint32_t i;
	// Address string, e.g.: 0x123456
	char addrStr[9];
	Cmd cmd;
	CmdRep *rep = NULL;

	if (chip > PROG_CHIP_MAX) return NULL;

	readBuf = malloc(f->len);
	if (!readBuf) {
		perror("Allocating read buffer");
		return NULL;
	}
	printf("Reading %s ROM starting at 0x%06X...\n", chip?"PRG":"CHR",
			f->addr);

	fflush(stdout);
	cmd.rdWr.cmd = CMD_CHR_READ + chip;
	for (i = 0, addr = f->addr; i < f->len;) {
		toRead = MIN(32768, f->len - i);
		CMD_SET_ADDR(cmd.rdWr.addr, addr);
		CMD_SET_LEN(cmd.rdWr.len, toRead);
		if ((CmdSendLongRep(&cmd, sizeof(CmdRdWrHdr), &rep, readBuf + i,
				toRead) != toRead) || (rep->command != CMD_OK)) {
			PrintErr("CMD response: %d. Couldn't read from cart!\n", rep->command);
			if (rep) CmdRepFree(rep);
			free(readBuf);
			return NULL;
		}
		CmdRepFree(rep);
		fflush(stdout);
		// Update vars and draw progress bar
		i += toRead;
		addr += toRead;
   	    sprintf(addrStr, "0x%06X", addr);
   	    ProgBarDraw(i, f->len, cols, addrStr);
	}
	putchar('\n');
	return readBuf;
}

/************************************************************************//**
 * Allocates a RAM buffer, reads the specified MemImage file, and writes it
 * to the in-cart RAM chip.
 *
 * \param[in] f    Memory image with the range to write and file to read.
 *
 * \return Pointer to the raw data of the allocated and read image file,
 *         or NULL if error occurred.
 *
 * \warning Buffer must be externally deallocated when no longer needed,
 *          using free().
 ****************************************************************************/
uint8_t *AllocAndRamWrite(MemImage *f) {
    FILE *ram;
	uint8_t *writeBuf;
	Cmd cmd;
	CmdRep *rep = NULL;

	// Check address and length are OK
	if ((PROG_SRAM_BASE > f->addr) ||
			((PROG_SRAM_BASE + PROG_SRAM_LEN) < (f->addr + f->len))) {
		PrintErr("Wrong RAM read address:length combination!\n");
		return NULL;
	}
	if (!(ram = fopen(f->file, "rb"))) {
		perror(f->file);
		return NULL;
	}

	// Obtain length if not specified
	if (!f->len) {
	    fseek(ram, 0, SEEK_END);
	    f->len = ftell(ram);
	    fseek(ram, 0, SEEK_SET);
	}

    writeBuf = malloc(f->len);
	if (!writeBuf) {
		perror("Allocating write buffer RAM");
		fclose(ram);
		return NULL;
	}
	// Read the entire RAM file and close it.
    if (1 > fread(writeBuf, f->len, 1, ram)) {
		fclose(ram);
		free(writeBuf);
		PrintErr("Error reading RAM file!\n");
		return NULL;
	}
	fclose(ram);

   	printf("Writing SRAM %s starting at 0x%04X... ", f->file, f->addr);

	// Prepare and send RAM write command
	cmd.rdWr.cmd = CMD_RAM_WRITE;
	CMD_SET_ADDR(cmd.rdWr.addr, f->addr - PROG_SRAM_BASE);
	CMD_SET_LEN(cmd.rdWr.len, f->len);
	if ((CmdSendLongCmd(&cmd, sizeof(CmdRdWrHdr), writeBuf,
			f->len, &rep) != f->len) || (rep->command != CMD_OK)) {
		if (rep) CmdRepFree(rep);
		free(writeBuf);
		PrintErr("Couldn't write to cart!\n");
		return NULL;
	}
	CmdRepFree(rep);
	printf("OK!\n");
	return writeBuf;
}

/************************************************************************//**
 * Allocates a RAM buffer, and reads range specified in MemImage input
 * from the in-cart RAM chip.
 *
 * \param[in] f    Memory image with the range to read.
 *
 * \return Pointer to the raw data of the allocated and read image file,
 *         or NULL if error occurred.
 *
 * \warning Buffer must be externally deallocated when no longer needed,
 *          using free().
 ****************************************************************************/
uint8_t *AllocAndRamRead(MemImage *f) {
	uint8_t *readBuf;
	Cmd cmd;
	CmdRep *rep = NULL;

	// Check address and length are OK
	if ((PROG_SRAM_BASE > f->addr) ||
			((PROG_SRAM_BASE + PROG_SRAM_LEN) < (f->addr + f->len))) {
		PrintErr("Wrong RAM read address:length combination!\n");
		return NULL;
	}

	readBuf = malloc(f->len);
	if (!readBuf) {
		perror("Allocating RAM read buffer");
		return NULL;
	}
	printf("Reading cart starting at 0x%06X... ", f->addr);
	fflush(stdout);

	// Prepare and send RAM read command	
	cmd.rdWr.cmd = CMD_RAM_READ;
	CMD_SET_ADDR(cmd.rdWr.addr, f->addr - PROG_SRAM_BASE);
	CMD_SET_LEN(cmd.rdWr.len, f->len);
	if ((CmdSendLongRep(&cmd, sizeof(CmdRdWrHdr), &rep, readBuf,
			f->len) != f->len) || (rep->command != CMD_OK)) {
		PrintErr("CMD response: %d. Couldn't read from cart!\n", rep->command);
		if (rep) CmdRepFree(rep);
		free(readBuf);
		return NULL;
	}
	CmdRepFree(rep);
	printf("OK!\n");
	return readBuf;
}

/************************************************************************//**
 * Send mapper configuration command.
 *
 * \param[in] mapper Mapper to set.
 *
 * \return 0 if OK, -1 if error.
 ****************************************************************************/
int CmdMapperCfg(CmdMapper mapper) {
	Cmd cmd;
	CmdRep *rep = NULL;

	cmd.command = CMD_MAPPER_SET;
	cmd.data[1] = mapper;
	if (CmdSend(&cmd, 2, &rep) < 0) return -1;

	return 0;
}

/************************************************************************//**
 * Evaluate a value. If less than 0, print the specified error. Else do 
 * nothing.
 *
 * \param[in] code  Code to evaluate.
 * \param[in] error Error message to print if code < 0.
 ****************************************************************************/
#define try(code, error)	\
	do{if ((code) < 0) {PrintErr(error);errCode = -1; \
		goto dealloc_exit;}}while(0)

/************************************************************************//**
 * Program entry point. Parses input parameters and perform requested
 * actions.
 *
 * \param[in] argc Number of input parameters.
 * \param[in] argv Array of input parameters strings to be parsed.
 *
 * \return 0 if OK, less than 0 if error.
 ****************************************************************************/
int main(int argc, char **argv){
	// Command-line flags
	Flags f;
	// Number of columns of the terminal
	unsigned int cols = 80;
	// Mapper to use
	int mapper = INT_MAX;
	// CHR sector erase address. Set to UINT32_MAX for none
	uint32_t chrSectErase = UINT32_MAX;
	// PRG sector erase address. Set to UINT32_MAX for none
	uint32_t prgSectErase = UINT32_MAX;
	// Rom file to write to CHR ROM
	MemImage fCWr = {NULL, 0, 0};
	// Rom file to read from CHR ROM (default read length: 256 KiB)
	MemImage fCRd = {NULL, 0, 256*1024};
	// Rom file to write to PRG ROM
	MemImage fPWr = {NULL, 0, 0};
	// Rom file to read from PRG ROM (default read length: 512 KiB)
	MemImage fPRd = {NULL, 0, 512*1024};
	// Binary blob to flash to the FPGA
	MemImage fFpga = {NULL, 0, 0};
	// Binary blob to flash to the AVR CIC microcontroller
	MemImage fCic = {NULL, 0, 0};
	// Binary blob to flash to the programmer microcontroller
	MemImage fFw = {NULL, 0, 0};
	// File to read from cartridge SRAM (8 KiB default).
	MemImage fRRd = {NULL, 0, 8 * 1024};
	// File to write to cartridge SRAM.
	MemImage fRWr = {NULL, 0, 0};
	// Error code for function calls
	int errCode = 0;
	// Buffer for writing data to CHR flash
    uint8_t *chrWrBuf = NULL;
	// Buffer for writing data to PRG flash
    uint8_t *prgWrBuf = NULL;
	// Buffer for reading CHR cart data
	uint8_t *chrRdBuf = NULL;
	// Buffer for reading PRG cart data
	uint8_t *prgRdBuf = NULL;
	// Buffer for RAM writes
	uint8_t *ramWrBuf = NULL;
	// Buffer for RAM reads
	uint8_t *ramRdBuf = NULL;
	// MPSSE interface to use (default: 1).
	long mpsseIf = 2;
	// Key file for the configuration
	GKeyFile *gkf = NULL;
	// Configuration file path
	const char cfgFile[] = "/etc/mk3-prog.cfg";
	// Temporal char pointer
	char *tmpChr = NULL;
	// Path for the Lattice Programmer software
	char *latPath = LATT_PROG_PATH;
	// Path of avrdude binary
	char *avrPath = AVR_PATH;
	// Path for the avrdude configuration
	char *avrDConf = AVR_PROG_CFG;
	// Programmer configuration for MCU
	char *progMcu  = AVR_PROG_MCU;
	// Programmer configuration for CIC
	char *progCic = AVR_PROG_CIC;
	// MCU chip
	char *chipMcu = AVR_CHIP_MCU;
	// CIC chip
	char *chipCic = AVR_CHIP_CIC;

	// Just for loop iteration
	int i;

	// Set all flags to FALSE
	f.all = 0;
    // Reads console arguments
    if(argc > 1)
    {
        /// Option index, for command line options parsing
        int opIdx = 0;
        /// Character returned by getopt_long()
        int c;

		// Open configuration file
		gkf = g_key_file_new();
		if (g_key_file_load_from_file(gkf, cfgFile, G_KEY_FILE_NONE, NULL)) {
			// Read config data
			if ((tmpChr = g_key_file_get_string(gkf, "LATTICE_PROGRAMMER",
							"path", NULL))) {
				latPath = tmpChr;
			} else puts("WARNING: Failed to load Lattice Programmer path "
					"from config file.");
			if ((tmpChr = g_key_file_get_string(gkf, "AVRDUDE",
							"path", NULL))) {
				avrPath = tmpChr;
			} else puts("WARNING: Failed to load avrdude configuration file.");
			if ((tmpChr = g_key_file_get_string(gkf, "AVRDUDE",
							"conf", NULL))) {
				avrDConf = tmpChr;
			} else puts("WARNING: Failed to load avrdude configuration file.");
			if ((tmpChr = g_key_file_get_string(gkf, "AVRDUDE",
							"prog_mcu", NULL))) {
				progMcu = tmpChr;
			} else
				puts("WARNING: Failed to load programmer chip configuration.");
			if ((tmpChr = g_key_file_get_string(gkf, "AVRDUDE",
							"prog_cic", NULL))) {
				progCic = tmpChr;
			} else puts("WARNING: Failed to load avrdude CIC chip "
					"configuration.");
			if ((tmpChr = g_key_file_get_string(gkf, "AVRDUDE",
							"chip_mcu", NULL))) {
				chipMcu = tmpChr;
			} else puts("WARNING: Failed to load programmer chip model");
			if ((tmpChr = g_key_file_get_string(gkf, "AVRDUDE",
							"chip_cic", NULL))) {
				chipCic = tmpChr;
			} else puts("WARNING: Failed to load CIC chip model");
			if ((mpsseIf = g_key_file_get_int64(gkf, "MPSSE", "ifnum", NULL))
					<=  0) {
				mpsseIf = 2;
				puts("WARNING: Failed to load MPSSE interface number.");
			}
		} else printf("WARNING: could not open configuration file \"%s\"\n", cfgFile);

		puts(latPath);
		puts(avrPath);
		puts(avrDConf);
		puts(progMcu);
		puts(progCic);
		puts(chipMcu);
		puts(chipCic);
		printf("%ld\n", mpsseIf);

        while ((c = getopt_long(argc, argv, "fc:p:C:P:eEs:S:ViR:W:b:a:F:m:M:drvh", opt, &opIdx)) != -1)
        {
			// Parse command-line options
            switch (c)
            {
				case 'f':
					f.fwVer = TRUE;
					break;

                case 'c': // Write CHR flash
					fCWr.file = optarg;
					if ((errCode = ParseMemArgument(&fCWr))) {
						PrintErr("Error: On CHR Flash file argument: ");
						PrintMemError(errCode);
						return 1;
					}
	                break;

                case 'p': // Write PRG flash
					fPWr.file = optarg;
					if ((errCode = ParseMemArgument(&fPWr))) {
						PrintErr("Error: On PRG Flash file argument: ");
						PrintMemError(errCode);
						return 1;
					}
	                break;

                case 'C': // Read CHR flash
					fCRd.file = optarg;
					if ((errCode = ParseMemArgument(&fCRd))) {
						PrintErr("Error: On CHR ROM read argument: ");
						PrintMemError(errCode);
						return 1;
					}
                	break;

                case 'P': // Read PRG flash
					fPRd.file = optarg;
					if ((errCode = ParseMemArgument(&fPRd))) {
						PrintErr("Error: On PRG ROM read argument: ");
						PrintMemError(errCode);
						return 1;
					}
	                break;

                case 'e': // Erase entire CHR flash
					f.chrErase = TRUE;
                	break;

                case 'E': // Erase entire PRG flash
					f.prgErase = TRUE;
                	break;

				case 's': // Erase CHR flash sector
	                chrSectErase = strtol( optarg, NULL, 16 );
					break;

				case 'S': // Erase CHR flash sector
	                prgSectErase = strtol( optarg, NULL, 16 );
					break;

                case 'V': // Verify flash write
					f.verify = TRUE;
	                break;

                case 'i': // Flash id
					f.flashId = TRUE;
	                break;

				case 'R': // RAM read
					fRRd.file = optarg;
					if ((errCode = ParseMemArgument(&fRRd))) {
						PrintErr("Error: On RAM read argument: ");
						PrintMemError(errCode);
						return 1;
					}
					break;


				case 'W': // RAM write
					fRWr.file = optarg;
					if ((errCode = ParseMemArgument(&fRWr))) {
						PrintErr("Error: On RAM write argument: ");
						PrintMemError(errCode);
						return 1;
					}
					break;

				case 'b': // Flash FPGA bitfile
					fFpga.file = optarg;
					if ((errCode = ParseMemArgument(&fFpga))) {
						PrintErr("Error: On FPGA bitfile argument. ");
						PrintMemError(errCode);
						return 1;
					}
					break;

                case 'a': // Flash AVR CIC firmware
					fCic.file = optarg;
					if ((errCode = ParseMemArgument(&fCic))) {
						PrintErr("Error: On AVR CIC firmware argument. ");
						PrintMemError(errCode);
						return 1;
					}
					break;

                case 'F': // Flash programmer firmware
					fFw.file = optarg;
					if ((errCode = ParseMemArgument(&fFw))) {
						PrintErr("Error: On programmer firmware argument. ");
						PrintMemError(errCode);
						return 1;
					}
					break;

				case 'm':
	                mpsseIf = strtol(optarg, NULL, 16);
					break;

				case 'M':
					mapper = strtol(optarg, NULL, 16);
					if (mapper <= 0 || mapper > 3) {
						PrintErr("Invalid mapper %s requested!\n", optarg);
						return 1;
					}
					// User interface mappers go from 1 to 3, but cart mappers
					// go from 0 to 2, so decrement number.
					mapper--;
					break;

				case 'd': // Dry run
					f.dry = TRUE;
				break;

                case 'r': // Version
					PrintVersion(argv[0]);
                return 0;

                case 'v': // Verbose
					f.verbose = TRUE;
                break;

                case 'h': // Help
					PrintHelp(argv[0]);
                return 0;

                case '?':       // Unknown switch
					putchar('\n');
                	PrintHelp(argv[0]);
                return 1;
            }
        }
    }
    else
    {
		printf("Nothing to do!\n");
		PrintHelp(argv[0]);
		return 0;
	}

	if (optind < argc) {
		PrintErr("Unsupported parameter:");
		for (i = optind; i < argc; i++) PrintErr(" %s", argv[i]);
		PrintErr("\n\n");
		PrintHelp(argv[0]);
		return -1;
	}

	if (f.verbose) {
		printf("\nUsing MPSSE interface: %ld\n", mpsseIf);
		printf("The following actions will%s be performed (in order):\n",
				f.dry?" NOT":"");
		printf("==================================================%s\n\n",
				f.dry?"====":"");
		if (fFpga.file) {
		   printf(" - Upload FPGA bitfile ");
		   PrintMemImage(&fFpga); putchar('\n');
		}
		if (fCic.file) {
		   printf(" - Upload AVR CIC firmware ");
		   PrintMemImage(&fFpga); putchar('\n');
		}
		if (fFw.file) {
		   printf(" - Upload programmer firmware ");
		   PrintMemImage(&fFw); putchar('\n');
		}
		if (mapper != INT_MAX) {
			printf(" - Set mapper to %d.\n", mapper);
		}
		CondPrintf(f.fwVer, " - Get programmer board firmware version.\n");
		CondPrintf(f.flashId, " - Show Flash chip identification.\n");
		if (fRWr.file) {
			printf(" - Write RAM %s", f.verify?"and verify ":"");
			PrintMemImage(&fRWr); putchar('\n');
		}
		if (fRRd.file) {
			printf(" - Read RAM to ");
			PrintMemImage(&fRRd); putchar('\n');
		}
		if (f.chrErase) printf(" - Erase CHR Flash.\n");
		else if (chrSectErase != UINT32_MAX) 
			printf(" - Erase CHR sector at 0x%X.\n", chrSectErase);
		if (f.prgErase) printf(" - Erase PRG Flash.\n");
		else if (prgSectErase != UINT32_MAX) 
			printf(" - Erase PRG sector at 0x%X.\n", prgSectErase);
		if (fCWr.file) {
		   printf(" - Flash CHR %s", f.verify?"and verify ":"");
		   PrintMemImage(&fCWr); putchar('\n');
		}
		if (fCRd.file) {
			printf(" - Read CHR ROM to ");
			PrintMemImage(&fCRd); putchar('\n');
		}
		if (fPWr.file) {
		   printf(" - Flash PRG %s", f.verify?"and verify ":"");
		   PrintMemImage(&fPWr); putchar('\n');
		}
		if (fPRd.file) {
			printf(" - Read PRG ROM to ");
			PrintMemImage(&fPRd); putchar('\n');
		}
		printf("\n");
	}

	if (f.dry) return 0;

	/*
	 * COMMAND LINE PARSING END,
	 * PROGRAM STARTS HERE.
	 */
	// Detect number of columns (for progress bar drawing).
#ifdef __OS_WIN
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    cols = csbi.srWindow.Right - csbi.srWindow.Left;
#else
    struct winsize max;
    ioctl(0, TIOCGWINSZ , &max);
	cols = max.ws_col;

	// Catch SIGTERM to restore cursor before exiting
	if ((signal(SIGTERM, Terminate) == SIG_ERR) ||
			(signal(SIGINT, Terminate) == SIG_ERR)){
		PrintErr("Could not catch signals.\n");
		return 1;
	}

	// Bonus: set transparent cursor
	printf("\e[?25l");
#endif

	/* First run commands related to MCU/FPGA flashing */
	// Flash FPGA bitfile
	if (fFpga.file) {
		if (LatticeFlash(latPath, fFpga.file)) {
			PrintErr("Programming bitfile failed!\n"
					 "Please verify the board is connected, jumpers are OK "
					 "and try again.\n");
			errCode = 1;
			goto dealloc_exit;
		}
	}
	// Flash CIC firmware blob
	if (fCic.file) {
		// File must be flashed using ADBUS interface. Prior to flashing,
		// BCBUS0 ping (FT_PSEL) must be set to '1'.
		if (AvrFlash(avrPath, avrDConf, chipCic, fCic.file, progCic)) {
			PrintErr("Flashing CIC failed!\n"
					 "Please verify the board is connected, jumpers are OK "
					 "and try again.\n");
			errCode = 1;
			goto dealloc_exit;
		}
	}
	// Configure programmer to use TK-ROM like mapper
	if (mapper != INT_MAX) {
		CmdMapperCfg(mapper);
	}
	// Flash programmer firmware blob
	if (fFw.file) {
		// File must be flashed using BDBUS interface. Prior to flashing,
		// user must short jumper JP3, or flashing will fail.
		if (AvrFlash(avrPath, avrDConf, chipMcu, fFw.file, progMcu)) {
			PrintErr("Flashing MCU failed!\n"
					 "Please verify the board is connected and JP3 is "
					 "shorted, and try again.\n");
			errCode = 1;
			goto dealloc_exit;
		}
	}

	/* Next come commands that communicate with the MCU */
	// Open MPSSE SPI interface with programmer board
	printf("Opening MPSSE interface... ");
	if (CmdInit(mpsseIf)) {
		errCode = 1;
		goto dealloc_exit;
	}
	printf("OK!\n");

	if (f.fwVer) {
		try(ProgFwGet(), "Couldn't get programmer firmware!\n");
	}

	if (f.flashId) {
		try(ProgFIdGet(), "Couldn't get flash ID\n");
	}
	// RAM write
	if (fRWr.file) {
		ramWrBuf = AllocAndRamWrite(&fRWr);
		if (!ramWrBuf) {
			errCode = 1;
			goto dealloc_exit;
		}
	}
	// RAM read/verify
	if (fRRd.file || (ramWrBuf && f.verify)) {
		// If verify is set, ignore addr and length set in command line.
		if (f.verify) {
			fRRd.addr = fRWr.addr;
			fRRd.len  = fRWr.len;
		}
		ramRdBuf = AllocAndRamRead(&fRRd);
		if (!ramRdBuf) {
			errCode = 1;
			goto dealloc_exit;
		}
		// Verify
		if (f.verify) {
			for (i = 0; i < fRWr.len; i++) {
				if (ramWrBuf[i] != ramRdBuf[i]) {
					break;
				}
			}
			if (i == fRWr.len)
				printf("RAM Verify OK!\n");
			else {
				printf("RAM Verify failed at addr 0x%07X!\n", i + fRWr.addr);
				printf("RAM Wrote: 0x%04X; Read: 0x%04X\n", ramWrBuf[i],
						ramRdBuf[i]);
				// Set error, but do not exit yet, because user might want
				// to write readed data to a file!
				errCode = 1;
			}
		}
		// Write output file
		if (fRRd.file) {
        	FILE *dump = fopen(fRRd.file, "wb");
			if (!dump) {
				perror(fRRd.file);
				errCode = 1;
				goto dealloc_exit;
			}
	        fwrite(ramRdBuf, fRRd.len, 1, dump);
	        fclose(dump);
			printf("Wrote RAM file %s.\n", fRRd.file);
		}
		// Exit if we had a previous error (e.g. on verify stage).
		if (errCode) goto dealloc_exit;
	}
	if (f.chrErase) {
		printf("Erasing CHR Flash... "); fflush(stdout);
		try(ProgFlashErase(PROG_CHIP_CHR, PROG_ERASE_FULL),
			"CHR chip erase ERROR!\n");
		printf("OK!\n");
	}
	if (f.prgErase) {
		printf("Erasing PRG Flash... "); fflush(stdout);
		try(ProgFlashErase(PROG_CHIP_PRG, PROG_ERASE_FULL),
			"PRG chip erase ERROR!\n");
		printf("OK!\n");
	}
	if (chrSectErase != UINT32_MAX) {
		printf("Erasing CHR sector at 0x%06X... ", chrSectErase);
		try(ProgFlashErase(PROG_CHIP_CHR,
				chrSectErase), "CHR sector erase ERROR!\n");
		printf("OK!\n");
	}
	if (prgSectErase != UINT32_MAX) {
		printf("Erasing PRG sector at 0x%06X... ", prgSectErase);
		try(ProgFlashErase(PROG_CHIP_PRG,
				prgSectErase), "PRG sector erase ERROR!\n");
		printf("OK!\n");
	}
	// CHR Flash program
	if (fCWr.file) {
		chrWrBuf = AllocAndFlash(PROG_CHIP_CHR, &fCWr, cols);
		if (!chrWrBuf) {
			errCode = 1;
			goto dealloc_exit;
		}
	}
	// CHR Flash read/verify
	if (fCRd.file || (chrWrBuf && f.verify)) {
		// If verify is set, ignore addr and length set in command line.
		if (f.verify) {
			fCRd.addr = fCWr.addr;
			fCRd.len  = fCWr.len;
		}
		chrRdBuf = AllocAndRead(PROG_CHIP_CHR, &fCRd, cols);
		if (!chrRdBuf) {
			errCode = 1;
			goto dealloc_exit;
		}
		// Verify
		if (f.verify) {
			for (i = 0; i < fCWr.len; i++) {
				if (chrWrBuf[i] != chrRdBuf[i]) {
					break;
				}
			}
			if (i == fCWr.len)
				printf("CHR Verify OK!\n");
			else {
				printf("CHR Verify failed at addr 0x%07X!\n", i + fCWr.addr);
				printf("CHR Wrote: 0x%04X; Read: 0x%04X\n", chrWrBuf[i],
						chrRdBuf[i]);
				// Set error, but do not exit yet, because user might want
				// to write readed data to a file!
				errCode = 1;
			}
		}
		// Write output file
		if (fCRd.file) {
        	FILE *dump = fopen(fCRd.file, "wb");
			if (!dump) {
				perror(fCRd.file);
				errCode = 1;
				goto dealloc_exit;
			}
	        fwrite(chrRdBuf, fCRd.len, 1, dump);
	        fclose(dump);
			printf("Wrote CHR file %s.\n", fCRd.file);
		}
		// Exit if we had a previous error (e.g. on verify stage).
		if (errCode) goto dealloc_exit;
	}
	// PRG Flash program
	if (fPWr.file) {
		prgWrBuf = AllocAndFlash(PROG_CHIP_PRG, &fPWr, cols);
		if (!prgWrBuf) {
			errCode = 1;
			goto dealloc_exit;
		}
	}
	// PRG Flash read/verify
	if (fPRd.file || (prgWrBuf && f.verify)) {
		// If verify is set, ignore addr and length set in command line.
		if (f.verify) {
			fPRd.addr = fPWr.addr;
			fPRd.len  = fPWr.len;
		}
		prgRdBuf = AllocAndRead(PROG_CHIP_PRG, &fPRd, cols);
		if (!prgRdBuf) {
			errCode = 1;
			goto dealloc_exit;
		}
		// Verify
		if (f.verify) {
			for (i = 0; i < fPWr.len; i++) {
				if (prgWrBuf[i] != prgRdBuf[i]) {
					break;
				}
			}
			if (i == fPWr.len)
				printf("PRG Verify OK!\n");
			else {
				printf("PRG Verify failed at addr 0x%07X!\n", i + fPWr.addr);
				printf("PRG Wrote: 0x%04X; Read: 0x%04X\n", prgWrBuf[i],
						prgRdBuf[i]);
				// Set error, but do not exit yet, because user might want
				// to write readed data to a file!
				errCode = 1;
			}
		}
		// Write output file
		if (fPRd.file) {
        	FILE *dump = fopen(fPRd.file, "wb");
			if (!dump) {
				perror(fPRd.file);
				errCode = 1;
				goto dealloc_exit;
			}
	        fwrite(prgRdBuf, fPRd.len, 1, dump);
	        fclose(dump);
			printf("Wrote PRG file %s.\n", fPRd.file);
		}
		// Exit if we had a previous error (e.g. on verify stage).
		if (errCode) goto dealloc_exit;
	}

dealloc_exit:
	if (gkf) g_key_file_free(gkf);
	if (ramWrBuf) free(ramWrBuf);
	if (ramRdBuf) free(ramRdBuf);
	if (chrWrBuf) free(chrWrBuf);
	if (prgWrBuf) free(prgWrBuf);
	if (chrRdBuf) free(chrRdBuf);
	if (prgRdBuf) free(prgRdBuf);
#ifndef __OS_WIN
	// Restore cursor
	printf("\e[?25h");
#endif
	return errCode;
}

/** \} */


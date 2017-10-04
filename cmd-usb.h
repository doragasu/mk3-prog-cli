/************************************************************************//**
 * \file
 * \brief Allows sending commands to the programmer, and receiving results.
 *
 * \defgroup cmd cmd
 * \{
 * \brief Allows sending commands to the programmer, and receiving results.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 ****************************************************************************/
#ifndef _CMD_H_
#define _CMD_H_

#include <stdint.h>


/// Vendor ID of the device to open
#define CMD_VID     				0x03EB
/// Peripheral ID of the device to open
#define CMD_PID     				0x206C

/// Bootloader Vendor ID
#define CMD_BOOT_VID    			0x03EB
/// Bootloader Peripheral ID
#define CMD_BOOT_PID    			0x2FF9

/// Device IN endpoint
#define CMD_ENDPOINT_IN        		0x83
/// Device OUT endpoint
#define CMD_ENDPOINT_OUT       		0x04

/// USB configuration command
#define CMD_CONFIG             		1
/// USB interface command
#define CMD_INTERF             		0

/// USB endpoint length
#define CMD_ENDPOINT_LEN			64

/// Maximum USB transfer length.
/// Can be up to 512 bytes, but it looks like 384 is the
/// optimum value to maximize speed
#define CMD_MAX_USB_TRANSFER_LEN	384

/** \addtogroup CmdRet
 *  \brief Return values for functions in this module and error codes.
 *  \{ */
#define CMD_OK		 0		///< Function completed successfully
#define CMD_ERROR	-1		///< Function completed with error
/** \} */

/// Maximum length of a command
#define CMD_MAXLEN			CMD_ENDPOINT_LEN

/// Maximum SRAM length is 8 KiB
#define CMD_SRAM_MAXLEN		8*1024

/** \addtogroup Cmds
 *  \brief Supported system commands. Also includes OK and ERROR codes.
 *  \{ */
#define CMD_REP_OK		  0	///< OK reply code
#define CMD_FW_VER        1 ///< Get programmer firmware version
#define CMD_CHR_WRITE	  2 ///< Write to CHR flash
#define CMD_PRG_WRITE	  3 ///< Write to PRG flash
#define CMD_CHR_READ	  4 ///< Read from CHR flash
#define CMD_PRG_READ	  5 ///< Read from PRG flash
#define CMD_CHR_ERASE	  6 ///< Erase CHR flash (entire or sectors)
#define CMD_PRG_ERASE	  7 ///< Erase PRG flash (entire or sectors)
#define CMD_FLASH_ID	  8 ///< Get flash chips identifiers
#define CMD_RAM_WRITE     9 ///< Write data to cartridge SRAM
#define CMD_RAM_READ	 10 ///< Read data from cartridge SRAM
#define CMD_MAPPER_SET	 11 ///< Configure cartridge mapper
#define CMD_REP_ERROR	255	///< Error reply code
/** \} */

/// Supported mappers.
typedef enum {
	CMD_MAPPER_MMC3X = 0,	///< MMC3X mapper
	CMD_MAPPER_TKROM		///< TKROM-like mapper
} CmdMapper;

/// Command header for memory read and write commands.
typedef struct {
	uint8_t cmd;		///< Command code
	uint8_t addr[3];	///< Address to read/write
	uint8_t len[2];		///< Length to read/write
} CmdRdWrHdr;

/// Command header for erase commands.
typedef struct {
	uint8_t cmd;			///< Command code
	uint8_t sectAddr[3];	///< Address to erase, Full chip if 0xFFFFFF
} CmdErase;

/// Generic command request.
typedef union {
	uint8_t data[CMD_MAXLEN];	///< Raw data (32 bytes max)
	uint8_t command;			///< Command code
	CmdRdWrHdr rdWr;			///< Read/write request
	CmdErase erase;				///< Erase request
} Cmd;

/// Flash chip identification information.
typedef struct {
	uint8_t manId;			///< Manufacturer ID
	uint8_t devId[3];		///< Device ID
} CmdFlashId;

/// Empty command response.
typedef struct {
	uint8_t code;			///< Response code (OK/ERROR)
} CmdRepEmpty;

/// Firmware version command response.
typedef struct {
	uint8_t code;			///< Response code (OK/ERROR)
	uint8_t ver_major;		///< Major version number
	uint8_t ver_minor;		///< Minor version number
} CmdRepFwVer;

/// Flash ID command response.
typedef struct {
	uint8_t code;			///< Command code
	uint8_t pad;			///< Padding
	CmdFlashId prg;			///< PRG Flash information
	CmdFlashId chr;			///< CHR Flash information
} CmdRepFlashId;

/// Generic reply to a command request.
typedef union {
	uint8_t data[CMD_MAXLEN];		///< Raw data (up to 32 bytes)
	uint8_t command;		///< Command code
	CmdRepEmpty eRep;		///< Empty command response
	CmdRepFlashId fId;		///< Flash ID command response
	CmdRepFwVer fwVer;		///< Firmware version command response
} CmdRep;

/************************************************************************//**
 * Module initialization. Call before using any other function.
 *
 * \param[in] channel Channel number of the FTX232H device to use for
 * 			  communications.
 *
 * \return CMD_OK if the command completed successfully. CMD_ERROR otherwise.
 ****************************************************************************/
int CmdInit(unsigned int channel);

/************************************************************************//**
 * Sends a command, and obtains the command response.
 *
 * \param[in]  cmd Command to send.
 * \param[in]  cmdLen Command length.
 * \param[out] rep Command reply
 * \param[in]  tout Timeout for the command send/receive operations (ms).
 *
 * \return Received bytes on success. CMD_ERROR otherwise.
 * \note This function does not allow sending or receiving commands with long
 * payloads. Use CmdSendLongCmd() or CmdSendLongRep() for long payloads.
 ****************************************************************************/
int CmdSend(const Cmd *cmd, uint8_t cmdLen, CmdRep **rep, unsigned int tout);

/************************************************************************//**
 * Sends a command with a long data payload, and obtains the command response.
 *
 * \param[in]  cmd Command to send.
 * \param[in]  cmdLen Command length.
 * \param[in]  data Long data payload to send.
 * \param[in]  dataLen Length of the data payload.
 * \param[out] rep Response to the sent command.
 * \param[in]  tout Timeout for the command send/receive operations (ms).
 *
 * \return CMD_OK if the command completed successfully. CMD_ERROR otherwise.
 * \note This function does not allow sending or receiving commands with long
 * payloads. Use CmdSendLongCmd() or CmdSendLongRep() for long payloads.
 ****************************************************************************/
int CmdSendLongCmd(const Cmd *cmd, uint8_t cmdLen, const uint8_t *data,
				   int dataLen, CmdRep **rep, unsigned int tout);

/************************************************************************//**
 * Sends a command requiring a long response payload.
 *
 * \param[in]  cmd Command to send.
 * \param[in]  cmdLen Command length.
 * \param[out] rep Response to the sent command.
 * \param[in]  data Long data payload to receive.
 * \param[in]  recvLen Length of payload to receive.
 * \param[in]  tout Timeout for the command send/receive operations (ms).
 *
 * \return Length of the received payload if OK, CMD_ERROR otherwise.
 ****************************************************************************/
int CmdSendLongRep(const Cmd *cmd, uint8_t cmdLen, CmdRep **rep,
				   uint8_t *data, int recvLen, unsigned int tout);

/// It looks like libmpsse does NOT free returned responses (it is at least
/// NOT documented), so we must free the memory on our own. As this can change
/// in the future, we must be extra careful with this "feature".
#define CmdRepFree(pRep)	free(pRep)

#endif /*_CMD_H_*/

/** \} */



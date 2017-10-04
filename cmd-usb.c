/************************************************************************//**
 * \file
 * \brief Allows sending commands to the programmer, and receiving results.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 ****************************************************************************/
#include "cmd-usb.h"
#include <string.h>
#include <stdio.h>		// Just for debugging
#include <stdlib.h>
#include "spi-com.h"
#include <libusb-1.0/libusb.h>
#include "util.h"

// Commands byte size
#define COMMAND_FRAME_BYTES     CMD_ENDPOINT_LEN

static libusb_device_handle *hCmd;

/************************************************************************//**
 * Module initialization. Call before using any other function.
 *
 * \param[in] channel Currently unused.
 *
 * \return CMD_OK if the command completed successfully. CMD_ERROR otherwise.
 ****************************************************************************/
int CmdInit(unsigned int channel) {
	int r;

	UNUSED_PARAM(channel);

	// Init libusb
    r = libusb_init(NULL);
	if (r < 0) {
        PrintErr( "Error: could not init libusb\n" );
        PrintErr( "   Code: %s\n", libusb_error_name(r) );
		return CMD_ERROR;
	}

	// Uncomment this to flood the screen with libusb debug information
	//libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_DEBUG);

    // Try opening device using VID/PID
	hCmd = libusb_open_device_with_vid_pid(NULL, CMD_VID, CMD_PID);

	if (hCmd == NULL) {
		PrintErr( "Error: could not open device %.4X : %.4X\n", CMD_VID,
				CMD_PID );
		return CMD_ERROR;
	}

    // Set configuration
	r = libusb_set_configuration(hCmd, CMD_CONFIG);
	if (r < 0) {
        PrintErr("Error: could not set configuration #%d\n", CMD_CONFIG);
        PrintErr("   Code: %s\n", libusb_error_name(r));
        return CMD_ERROR;
	}
//	else
//        PrintVerb("Completed: set configuration #%d\n", CMD_CONFIG);


    // Claim interface
	r = libusb_claim_interface(hCmd, CMD_INTERF);
    if(r != LIBUSB_SUCCESS) {
        PrintErr("Error: could not claim interface #%d\n", CMD_INTERF);
        PrintErr("   Code: %s\n", libusb_error_name(r));
        return CMD_ERROR;
    }
//    else
//        PrintVerb("Completed: claim interface #%d\n", CMD_INTERF);

	return CMD_OK;
}

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
int CmdSend(const Cmd *cmd, uint8_t cmdLen, CmdRep **rep, unsigned int tout) {
	int ret;
	int size;

    ret = libusb_bulk_transfer(hCmd, CMD_ENDPOINT_OUT,
        (unsigned char*)cmd->data, COMMAND_FRAME_BYTES, &size, tout);

    if ((ret != LIBUSB_SUCCESS) || (size != COMMAND_FRAME_BYTES)) {
		printf("Error: bulk transfer can not send %d command \n",
            cmd->command);

		printf("   Code: %s\n", libusb_error_name(ret));

		return CMD_ERROR;
	}

	// Receive the reply to the command
    ret = libusb_bulk_transfer(hCmd, CMD_ENDPOINT_IN,
			(*rep)->data, COMMAND_FRAME_BYTES, &size, tout);

    if ((ret != LIBUSB_SUCCESS) /*|| (size != COMMAND_FRAME_BYTES)*/) {
		printf("Error: bulk transfer reply failed \n");
		printf("   Code: %s\n", libusb_error_name(ret));
		return CMD_ERROR;
	}

	return size;
}

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
 ****************************************************************************/
int CmdSendLongCmd(const Cmd *cmd, uint8_t cmdLen, const uint8_t *data,
				   int dataLen, CmdRep **rep, unsigned int tout) {
	int sent;
	int ret;

	// Send command request and get response
	if (CmdSend(cmd, cmdLen, rep, tout) != CMD_OK) return CMD_ERROR;
	
	// Send big data chunck
	ret = libusb_bulk_transfer(hCmd, CMD_ENDPOINT_OUT, (unsigned char*)data,
			dataLen, &sent, tout);

	if ((ret != LIBUSB_SUCCESS) || (sent != dataLen)) {
		PrintErr("Error: couldn't write payload!\n");
		PrintErr("   Code: %s\n", libusb_error_name(ret));
		return CMD_ERROR;
	}

	return CMD_OK;
}

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
				   uint8_t *data, int recvLen, unsigned int tout) {
	int ret;
	int step;
	int last;
	int recvd = 0;

	// Send command request and get response
	if (CmdSend(cmd, cmdLen, rep, tout) != CMD_OK) return CMD_ERROR;
	
	// Receive long data payload
	if (data && recvLen) {
		// Now receive the big data payload
		while (recvd < recvLen) {
			step = MIN(CMD_MAX_USB_TRANSFER_LEN, recvLen - recvd);
			ret = libusb_bulk_transfer(hCmd, CMD_ENDPOINT_IN,
					(unsigned char*)(data+recvd), step, &last, tout);
		
			if ((ret != LIBUSB_SUCCESS) || (last != step)) {
				PrintErr("Error: couldn't get read payload!\n");
				PrintErr("   Code: %s\n", libusb_error_name(ret) );
			}
			recvd += step;
		}
	}

	return recvd;
}


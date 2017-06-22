/************************************************************************//**
 * \file
 * \brief Allows sending commands to the programmer, and receiving results.
 *
 * \author Jesus Alonso (doragasu)
 * \date   2016
 ****************************************************************************/
#include "cmd.h"
#include <string.h>
#include <stdio.h>		// Just for debugging
#include <stdlib.h>
#include "spi-com.h"
#include "util.h"

/// MPSSE SPI handler for communications with programmer
static struct mpsse_context *spi;

/************************************************************************//**
 * Module initialization. Call before using any other function.
 *
 * \param[in] channel Channel number of the FTX232H device to use for
 * 			  communications.
 *
 * \return CMD_OK if the command completed successfully. CMD_ERROR otherwise.
 ****************************************************************************/
int CmdInit(unsigned int channel) {
	if (!(spi = SCInit(channel))) return CMD_ERROR;

	return CMD_OK;
}

/************************************************************************//**
 * Sends a command, and obtains the command response.
 *
 * \param[in]  cmd Command to send.
 * \param[in]  cmdLen Command length.
 * \param[out] rep Response to the sent command.
 *
 * \return CMD_OK if the command completed successfully. CMD_ERROR otherwise.
 * \note This function does not allow sending or receiving commands with long
 * payloads. Use CmdSendLongCmd() or CmdSendLongRep() for long payloads.
 ****************************************************************************/
int CmdSend(const Cmd *cmd, uint8_t cmdLen, CmdRep **rep) {
	if (SCFrameSend(spi, (char*)cmd->data, cmdLen) != SC_OK)
		return CMD_ERROR;
	uint8_t maxLen = CMD_MAXLEN;

	if (!(*rep = (CmdRep*)SCFrameRecv(spi, &maxLen))) return CMD_ERROR;
	return maxLen;
}

/************************************************************************//**
 * Sends a command with a long data payload, and obtains the command response.
 *
 * \param[in]  cmd Command to send.
 * \param[in]  cmdLen Command length.
 * \param[in]  data Long data payload to send.
 * \param[in]  dataLen Length of the data payload.
 * \param[out] rep Response to the sent command.
 *
 * \return CMD_OK if the command completed successfully. CMD_ERROR otherwise.
 ****************************************************************************/
int CmdSendLongCmd(const Cmd *cmd, uint8_t cmdLen, const uint8_t *data,
				   int dataLen, CmdRep **rep) {
	int sent;
	int last;

	// Send command request
	if (SC_OK != SCFrameSend(spi, (char*)cmd->data, cmdLen))
		return CMD_ERROR;
	uint8_t maxLen = CMD_MAXLEN;

	// Get command response
	if (!(*rep = (CmdRep*)SCFrameRecv(spi, &maxLen))) return CMD_ERROR;
	
	// Send payload data
	for (sent = 0; sent < dataLen; sent += last) {
		last = MIN(dataLen - sent, CMD_MAXLEN);
		if (SC_OK != SCFrameSend(spi, (char*)data + sent, last))
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
 *
 * \return Length of the received payload if OK, CMD_ERROR otherwise.
 ****************************************************************************/
int CmdSendLongRep(const Cmd *cmd, uint8_t cmdLen, CmdRep **rep,
				   uint8_t *data, int recvLen) {
	char *tmp;
	int recv;
	uint8_t expected, last;

	// Send command request
	if (SC_OK != SCFrameSend(spi, (char*)cmd->data, cmdLen))
		return CMD_ERROR;
	uint8_t maxCmdLen = CMD_MAXLEN;

	// Get command response
	if (!(*rep = (CmdRep*)SCFrameRecv(spi, &maxCmdLen))) return CMD_ERROR;

	// Receive long data payload
	for (recv = 0; recv < recvLen; recv += last) {
		expected = last = MIN(recvLen - recv, CMD_MAXLEN);
		if (!(tmp = SCFrameRecv(spi, &last))) return CMD_ERROR;
		// Check we received requested data and copy to buffer.
		if (expected != last) return CMD_ERROR;
		memcpy(data + recv, tmp, last);
		CmdRepFree(tmp);
	}
	return recv;
}


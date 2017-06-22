/************************************************************************//**
 * \file
 * \brief Handles SPI and framing for communications.
 *
 * Frames are delimited by the SOF/EOF characters. Following SOF, payload
 * length in sent using 1 byte. Then data follows, and finally EOF character
 * ends transmission.
 * 
 * \author Jesus Alonso (doragasu)
 * \date   2016
 * \note   Uses open source mpsse library to interface FT2232 in MSPSSE mode.
 ****************************************************************************/
#include "spi-com.h"
#include "util.h"
#include <string.h>

#include <stdio.h>

/// Signals frame stop and returns with SC_ERROR
#define SCStopErr()		do{Stop(mpsse);return SC_ERROR;}while(0)

/// Signals frame stop and returns NULL
#define SCStopNull()	do{Stop(mpsse);return NULL;}while(0)

/************************************************************************//**
 * Module initialization. Call this function to obtain the handler needed
 * to call any other function in this module.
 *
 * \param[in] channel Channel number of the FT2232 device to open.
 *
 * \return The handler of the opened FT2232 MPSSE interface, or NULL if
 *         opening the interface failed.
 ****************************************************************************/
struct mpsse_context *SCInit(unsigned int channel) {
	struct mpsse_context *mpsse;
	mpsse = Open(SC_VID, SC_PID, SC_SPI_MODE, SC_SPI_CLK, MSB, SC_IFACE,
			NULL, NULL);
	if (mpsse) {
		// Turn ON PORTB LED (GPIOH1).
		PinLow(mpsse, GPIOH1);
	}
	return mpsse;
}

/************************************************************************//**
 * Sends data through the MPSSE interface, using a tiny framing protocol.
 *
 * \param[in] mpsse Handler of the previously opened MPSSE interface.
 * \param[in] data Data payload to send.
 * \param[in] datalen Length of the data payload to send in bytes.
 *
 * \return SC_OK on success, SC_ERROR on fail.
 ****************************************************************************/
// NOTE: Previous implementation sent each frame split in 3 steps to avoid
// having to copy input data: SOF + len, payload, eof. This caused huge delays
// due minimum 1 ms USB latency. Due to this implementation has been changed
// to build the complete frame (even if this means copying the data) and send
// it at once.
int SCFrameSend(struct mpsse_context *mpsse, char *data, uint16_t datalen) {
	// Frame buffer. Maximum length is the payload length + SOF + EOF + LEN.
	char frame[SC_MAX_DATALEN + 3];
	uint16_t rem;
	uint16_t sent;

	frame[0] = SC_SOF;
	// If payload greater than SC_MAX_DATALEN bytes, split data
	// transmission in SC_MAX_DATALEN byte chuncks.
	for (sent = 0, rem = datalen; 0 < rem; sent += frame[1], rem -= frame[1]) {
		// Build frame
		frame[1] = MIN(rem, SC_MAX_DATALEN);
		memcpy(frame + 2, data + sent, frame[1]);
		frame[frame[1] + 2] = SC_EOF;
		// Send frame
		if (Start(mpsse)) SCStopErr();
		if (Write(mpsse, frame, frame[1] + 3)) SCStopErr();
		if (Stop(mpsse)) SCStopErr();
	}

	return SC_OK;
}

/************************************************************************//**
 * Receives data through the MPSSE interface, using a tiny framing protocol.
 *
 * \param[in] mpsse Handler of the previously opened MPSSE interface.
 * \param[inout] maxlen Maximum length of the data payload to receive. On
 *               function return, holds the number of bytes received.
 *
 * \return Pointer to the received data, or NULL if reception failed.
 *
 * \warning It looks like libmpsse does NOT free the received data buffer,
 *  so we have to make sure to free it ourselves when not needed!
 *  Other option is maybe using Fast functions inside fast.c
 ****************************************************************************/
char *SCFrameRecv(struct mpsse_context *mpsse, uint8_t *maxlen) {
	char *data;
	uint8_t length;

	if (Start(mpsse)) SCStopNull();
	// Seek SOF
//	printf("Receiving SOF "); fflush(stdout);
	do {
		data = Read(mpsse, 1);
//		printf("%02X, ", (uint8_t)*data); fflush(stdout);
		if (data == NULL) SCStopNull();
	} while(*data != SC_SOF);
//	printf("OK!\n");
	// Read data length
//	printf("Reading length... "); fflush(stdout);
	if ((data = Read(mpsse, 1)) == NULL) SCStopNull();
	length = *data;
//	printf("OK!, length=%d\n", length);
	if (length > *maxlen) SCStopNull();
//	printf("Receiving data... "); fflush(stdout);
	if ((data = Read(mpsse, length + 1)) == NULL) SCStopNull();
	if (Stop(mpsse)) SCStopNull();
	if (data[length] != SC_EOF) return NULL;
	// Update number of received characters
	*maxlen = length;
//	printf("OK!\n");
	return data;
}




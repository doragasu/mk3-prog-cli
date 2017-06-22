/************************************************************************//**
 * \file
 * \brief Handles SPI and framing for communications.
 *
 * \defgroup spi-com spi-com
 * \{
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
#ifndef _SPI_COM_H_
#define _SPI_COM_H_

#include <stdint.h>
#include <mpsse.h>

/// SPI mode for using with MPSSE library
#define SC_SPI_MODE		SPI0
/// Maximum CLK for atmega8515 as SPI slave, is FOSC/4=12MHz/4
/// \todo Test if CLK can be increased by changing crystal oscillator
/// to a 16 MHz one.
//#define SC_SPI_CLK		400000
#define SC_SPI_CLK		100000

/// Start of frame marker
#define SC_SOF			0x7E
/// En of frame marker
#define SC_EOF			0x7D

/// USB Vendor ID of the programmer board
#define SC_VID			0x0403
/// USB Device ID of the programmer board
#define SC_PID			0x6010
/// FT2232 interface used to communicate with the microcontroller.
#define SC_IFACE		IFACE_B

/// Maximum data payload is 32 bytes long
#define SC_MAX_DATALEN	32

/** \addtogroup ScRetVals
 *  \brief Return values for this module.
 *  \{ */
/// OK status (0)
#define SC_OK			MPSSE_OK
/// Error status (-1)
#define SC_ERROR		MPSSE_FAIL
/** \} */

/************************************************************************//**
 * Module initialization. Call this function to obtain the handler needed
 * to call any other function in this module.
 *
 * \param[in] channel Channel number of the FT2232 device to open.
 *
 * \return The handler of the opened FT2232 MPSSE interface, or NULL if
 *         opening the interface failed.
 ****************************************************************************/
struct mpsse_context *SCInit(unsigned int channel);

/************************************************************************//**
 * Sends data through the MPSSE interface, using a tiny framing protocol.
 *
 * \param[in] mpsse Handler of the previously opened MPSSE interface.
 * \param[in] data Data payload to send.
 * \param[in] datalen Length of the data payload to send in bytes.
 *
 * \return SC_OK on success, SC_ERROR on fail.
 ****************************************************************************/
int SCFrameSend(struct mpsse_context *mpsse, char *data, uint16_t datalen);

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
char *SCFrameRecv(struct mpsse_context *mpsse, uint8_t *maxlen);

#endif /*_SPI_COM_H_*/

/** \} */


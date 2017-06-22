/************************************************************************//**
 * \file
 * \brief General purpose utiities.
 *
 * \defgroup util util
 * \{
 * \brief General purpose utilities.
 * 
 * \author Jesus Alonso (doragasu)
 * \date   2015
 ****************************************************************************/
#ifndef _UTIL_H_
#define _UTIL_H_

// Try to detect if we are building for Windows, and define __OS_WIN if true.
#if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
/// Windows build system detected.
#define __OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifndef TRUE
/// TRUE value definition for logic comparisons.
#define TRUE 1
#endif
#ifndef FALSE
/// FALSE value definition for logic comparisons.
#define FALSE 0
#endif

// Common macros to obtain maximum/minimum values.
#ifndef MAX
/// Return the maximum between two numbers.
#define MAX(a,b)	((a)>(b)?(a):(b))
#endif
#ifndef MIN
/// Return the minimum between two numbers.
#define MIN(a,b)	((a)<(b)?(a):(b))
#endif

/// printf-like macro that writes to stderr instead of stdout
#define PrintErr(...)	do{fprintf(stderr, __VA_ARGS__);}while(0)

// Delay ms function, compatible with both Windows and Unix
#ifdef __OS_WIN
/// Delay the specified amount of milliseconds.
#define DelayMs(ms) Sleep(ms)
#else
/// Delay the specified amount of milliseconds.
#define DelayMs(ms) usleep((ms)*1000)
#endif

#endif //_UTIL_H_

/** \} */


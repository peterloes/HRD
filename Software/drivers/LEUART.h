/***************************************************************************//**
 * @file
 * @brief	Header file of module LEUART.c
 * @author	Ralf Gerhauser
 * @version	2015-02-03
 ****************************************************************************//*
Revision History:
2015-02-03,rage	Initial version.
*/

#ifndef __INC_LEUART_h
#define __INC_LEUART_h

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include <stdbool.h>
#include "em_device.h"
#include "em_gpio.h"
#include "em_dma.h"
#include "config.h"		// include project configuration parameters

/*=============================== Definitions ================================*/

    /*! Switch to enable the receive part of the driver */
#define ENABLE_LEUART_RECEIVER	0

/*================================ Global Data ===============================*/

extern volatile bool	g_flgLEUART_LF2CRLF;
extern volatile bool	g_flgCmdLine;
extern uint8_t		g_CmdLine[];

/*================================ Prototypes ================================*/

/* Initialize Low Energy UART */
void	 drvLEUART_Init (uint32_t baud);

/* Put string into transmit FIFO */
void	 drvLEUART_puts (const char *pStr);

/* Put character into transmit FIFO */
void	 drvLEUART_putc (char c);


#endif /* __INC_LEUART_h */

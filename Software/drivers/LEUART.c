/**************************************************************************//**
 * @file
 * @brief	LEUART Driver
 * @author	Energy Micro AS
 * @author	Ralf Gerhauser
 * @version	2015-01-15
 *
 * This is the driver for the Low Energy UART.  It is used to write log and
 * debug information to a connected host system.  The LEUART device to use
 * can be set via the @ref LEUART define, for an assignment of the DMA channel,
 * see @ref DMA_CHAN_LEUART_RX and @ref DMA_CHAN_LEUART_TX.
 *
 * @note This driver only supports data transmission.
 *
 ******************************************************************************
 * @section License
 * <b>(C) Copyright 2013 Energy Micro AS, http://www.energymicro.com</b>
 *******************************************************************************
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 * 4. The source and compiled code may only be used on Energy Micro "EFM32"
 *    microcontrollers and "EFR4" radios.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: Energy Micro AS has no
 * obligation to support this Software. Energy Micro AS is providing the
 * Software "AS IS", with no express or implied warranties of any kind,
 * including, but not limited to, any implied warranties of merchantability
 * or fitness for any particular purpose or warranties against infringement
 * of any proprietary rights of a third party.
 *
 * Energy Micro AS will not be liable for any consequential, incidental, or
 * special damages, or any other relief, or for any claim by any third party,
 * arising from your use of this Software.
 *
 ****************************************************************************//*
Revision History:
2016-09-27,rage	Use INT_En/Disable() instead of __en/disable_irq().
*/

/*=============================== Header Files ===============================*/

#include "em_chip.h"
#include "em_device.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_int.h"
#include "em_leuart.h"
#include "LEUART.h"

/*=============================== Definitions ================================*/

    /*!@name Hardware Configuration: Serial Communication via LEUART. */
//@{
#define LEUART			LEUART0		//!< Device to use
#define cmuClock_LEUART		cmuClock_LEUART0    //!< CMU Clock for LEUART
#define LEUART_IRQn		LEUART0_IRQn	    //!< Interrupt for Rx
#define DMAREQ_LEUART_TXBL	DMAREQ_LEUART0_TXBL //!< DMA Request for Tx
#define DMAREQ_LEUART_RXDATAV	DMAREQ_LEUART0_RXDATAV //!< DMA Request for Rx
#define LEUART_IRQHandler	LEUART0_IRQHandler  //!< Interrupt Handler
#define LEUART_PORT		gpioPortD	//!< Port for TX pin
#define LEUART_PIN_TX		4		//!< Tx pin
#define LEUART_PIN_RX		5		//!< Rx pin
//@}

    /*! Size of the transmit FIFO in bytes */
#define TX_FIFO_SIZE		1024

#if ENABLE_LEUART_RECEIVER
    /*! Size of the command line buffer in bytes */
#define CMD_LINE_SIZE		40
#endif

/*======================== External Data and Routines ========================*/

extern DMA_DESCRIPTOR_TypeDef g_DMA_ControlBlock[];
extern DMA_CB_TypeDef g_DMA_Callback[];

/*========================= Global Data and Routines =========================*/

/*!@brief Global flag to convert \<LF> to \<CR>\<LF> */
volatile bool	g_flgLEUART_LF2CRLF = true;

#if ENABLE_LEUART_RECEIVER
/*!@brief Global flag to notify new command in g_CmdLine */
volatile bool	g_flgCmdLine;

/*!@brief Command line buffer */
uint8_t	 g_CmdLine[CMD_LINE_SIZE];
#endif

/*================================ Local Data ================================*/

/* Defining the LEUART initialization data */
static LEUART_Init_TypeDef leuartInit =
{
#if ENABLE_LEUART_RECEIVER
  .enable   = leuartEnable,	// Activate Tx and Rx Pin
#else
  .enable   = leuartEnableTx,	// Activate LEUn_TX pin only
#endif
  .refFreq  = 0,		// Get clock frequency from LEUART clock source
  .baudrate = 9600,		// Baudrate - overwritten by LEUART_Init()
  .databits = leuartDatabits8,	// Each LEUART frame containes 8 databits
  .parity   = leuartNoParity,	// No parity bits in use
  .stopbits = leuartStopbits2,	// Number of stop bits in a frame
};

/* DMA init structure */
static DMA_Init_TypeDef dmaInit =
{
    .hprot        = 0,			// No descriptor protection
    .controlBlock = g_DMA_ControlBlock,	// DMA control block alligned to 256
};

/* Setting up DMA channel for Tx */
static DMA_CfgChannel_TypeDef chnlCfgTx =
{
    .highPri   = false,			// Normal priority
    .enableInt = false,			// No interupt for callback function
    .select    = DMAREQ_LEUART_TXBL,	// DMA Req. is LEUARTx TX buffer empty
    .cb = &(g_DMA_Callback[DMA_CHAN_LEUART_TX]), // Callback for DMA TX done
};

/* Setting up channel descriptor for Tx  */
DMA_CfgDescr_TypeDef descrCfgTx =
{
    .dstInc  = dmaDataIncNone,		// Do not increment destination address
    .srcInc  = dmaDataInc1,		// Increment source address by one byte
    .size    = dmaDataSize1,		// Data size is one byte
    .arbRate = dmaArbitrate1,		// Rearbitrate for each byte recieved
    .hprot   = 0,			// No read/write source protection
};

#if ENABLE_LEUART_RECEIVER
/* Setting up DMA channel */
static DMA_CfgChannel_TypeDef chnlCfgRx =
{
    .highPri   = false,			// Normal priority
    .enableInt = false,			// No interupt for callback function
    .select    = DMAREQ_LEUART_RXDATAV,	// DMA Req. is LEUARTx RX data available
    .cb        = NULL,			// No callback function
};

/* Setting up channel descriptor */
DMA_CfgDescr_TypeDef descrCfgRx =
{
    .dstInc  = dmaDataInc1,		// Increment destination address by one
    .srcInc  = dmaDataIncNone,		// o not increment source address
    .size    = dmaDataSize1,		// Data size is one byte
    .arbRate = dmaArbitrate1,		// Rearbitrate for each byte recieved
    .hprot   = 0,			// No read/write source protection
};
#endif

/* Transmit FIFO and index variables */
static uint8_t	 txFIFO[TX_FIFO_SIZE];
static volatile uint16_t txIdxPut, txIdxGet, txIdxGetNext;

/* Flag if DMA transfer is in progress */
static volatile bool	flgDMArun;


/**************************************************************************//**
 * @brief  DMA Callback function
 *
 * When the DMA transfer is completed, disables the DMA wake-up on TX in the
 * LEUART to enable the DMA to sleep even when the LEUART buffer is empty.
 *
 ******************************************************************************/
void dmaTransferStart (void)
{
uint16_t	idxPut;		// local index variable
int16_t		cnt;		// number of bytes to send


    INT_Disable();

    if (flgDMArun)
    {
	INT_Enable();
	return;			// do not disturb a running DMA transfer
    }

    flgDMArun = true;		// set flag for DMA activity
    INT_Enable();

    /* Use local index that will not change */
    idxPut = txIdxPut;

    /* Calculate how many bytes to transfer */
    cnt = idxPut - txIdxGet;

    if (cnt == 0)
    {
	flgDMArun = false;	// clear flag again
	return;
    }

    if (cnt < 0)
    {
	/* Limit DMA transfer to end of FIFO buffer */
	idxPut = sizeof(txFIFO);
	cnt = idxPut - txIdxGet;
    }

    /* Calculate next value of the Get Index */
    txIdxGetNext = txIdxGet + cnt;
    if (txIdxGetNext >= sizeof(txFIFO))
	txIdxGetNext -= sizeof(txFIFO);

    /* Set new DMA source end address directly in the DMA descriptor */
    g_DMA_ControlBlock[DMA_CHAN_LEUART_TX].SRCEND = &txFIFO[idxPut-1];

    /* Enable DMA wake-up from LEUART TX */
    IO_Bit(LEUART->CTRL, _LEUART_CTRL_TXDMAWU_SHIFT) = 1;

    /* (Re)starting the transfer. Using Basic Mode */
    DMA_ActivateBasic(DMA_CHAN_LEUART_TX, // Activate channel selected
		      true,		// Use primary descriptor
		      false,		// No DMA burst
		      NULL,		// Keep destination address
		      NULL,		// Keep source address
		      cnt - 1);		// Size of buffer - 1
}


/**************************************************************************//**
 * @brief  DMA Callback function
 *
 * When the DMA transfer is completed, disables the DMA wake-up on TX in the
 * LEUART to enable the DMA to sleep even when the LEUART buffer is empty.
 *
 ******************************************************************************/
void dmaTransferDone(unsigned int channel, bool primary, void *user)
{
    (void) channel;
    (void) primary;
    (void) user;

    /* Disable DMA wake-up from LEUART TX */
    IO_Bit(LEUART->CTRL, _LEUART_CTRL_TXDMAWU_SHIFT) = 0;

    /* Update Get Index */
    txIdxGet = txIdxGetNext;

    /* Clear DMA running flag */
    flgDMArun = false;

    /* Check if still data to send */
    dmaTransferStart();
}


/**************************************************************************//**
 * @brief  Setup Low Energy UART with DMA operation
 *
 * The LEUART/DMA interaction is defined, and the DMA, channel and descriptor
 * is initialized. The destination for all the DMA transfers through this
 * channel is set to be the LEUART TXDATA register, and transfer complete
 * interrupt is enabled.
 *
 *****************************************************************************/
static void setupLeuartDma(void)
{
    /* Setting call-back function */
    g_DMA_Callback[DMA_CHAN_LEUART_TX].cbFunc  = dmaTransferDone;
    g_DMA_Callback[DMA_CHAN_LEUART_TX].userPtr = NULL;

    /* Initializing DMA, channel and descriptor for Tx */
    DMA_Init(&dmaInit);
    DMA_CfgChannel(DMA_CHAN_LEUART_TX, &chnlCfgTx);
    DMA_CfgDescr(DMA_CHAN_LEUART_TX, true, &descrCfgTx);

    /* Set new DMA destination end address directly in the DMA descriptor */
    g_DMA_ControlBlock[DMA_CHAN_LEUART_TX].DSTEND = &LEUART->TXDATA;

    /* Enable DMA Transfer Complete Interrupt */
    DMA->IEN = (DMA_IEN_CH0DONE << DMA_CHAN_LEUART_TX);

    /* Enable DMA interrupt vector */
    NVIC_EnableIRQ(DMA_IRQn);

#if ENABLE_LEUART_RECEIVER
    /* Initializing DMA, channel and desriptor */
    DMA_CfgChannel(DMA_CHAN_LEUART_RX, &chnlCfgRx);
    DMA_CfgDescr(DMA_CHAN_LEUART_RX, true, &descrCfgRx);

    /* Starting the transfer. Using Basic Mode */
    DMA_ActivateBasic(DMA_CHAN_LEUART_RX, // Activate channel selected
		      true,		// Use primary descriptor
		      false,		// No DMA burst
		      (void *) g_CmdLine,	// Destination address
		      (void *) &LEUART->RXDATA,	// Source address is register
		      CMD_LINE_SIZE - 1);	// Size of buffer - 1

    /* Set LEUART signal frame to <NL> (or <CR>) */
    LEUART->SIGFRAME = '\n';

    /* Enable LEUART Signal Frame Interrupt */
    LEUART_IntEnable(LEUART, LEUART_IEN_SIGF);

    /* Enable LEUART interrupt vector */
    NVIC_EnableIRQ(LEUART_IRQn);

    /* Make sure the LEUART wakes up the DMA on RX data */
    IO_Bit(LEUART->CTRL, _LEUART_CTRL_RXDMAWU_SHIFT) = 1;
#endif
}


/**************************************************************************//**
 * @brief  Initialize Low Energy UART
 *
 * Here the LEUART is initialized with the chosen settings. It is then routed
 * to location 0 to avoid conflict with the LCD pinout. Finally the GPIO mode
 * is set to push pull.
 *
 * @param[in] baud
 *	Baudrate to use (maximum 9600bd).
 *
 *****************************************************************************/
void	drvLEUART_Init (uint32_t baud)
{
    /* Enabling clocks, all other remain disabled */
    CMU_ClockEnable(cmuClock_DMA, true);	// Enable DMA clock
    CMU_ClockEnable(cmuClock_GPIO, true);	// Enable GPIO clock
    CMU_ClockEnable(cmuClock_LEUART, true);	// Enable LEUART clock

    /* Reseting and initializing LEUART */
    LEUART_Reset(LEUART);
    leuartInit.baudrate = baud;
    LEUART_Init(LEUART, &leuartInit);

    /* Route LEUART Tx (and Rx) Pin to location 0 (PD4/PD5) */
    LEUART->ROUTE = LEUART_ROUTE_TXPEN
#if ENABLE_LEUART_RECEIVER
		  | LEUART_ROUTE_RXPEN
#endif
		  | LEUART_ROUTE_LOCATION_LOC0;

    /* Enable GPIO for LEUART. Tx is on D4, Rx on D5 */
    GPIO_PinModeSet(LEUART_PORT,		// GPIO port
		    LEUART_PIN_TX,		// GPIO port number
		    gpioModePushPull,		// Pin mode is set to push pull
		    1);				// High idle state
#if ENABLE_LEUART_RECEIVER
    GPIO_PinModeSet(LEUART_PORT,		// GPIO port
		    LEUART_PIN_RX,		// GPIO port number
		    gpioModeInputPull,		// Pin mode is set to input with
		    1);				// Pull-Up
#endif

    /* Setup LEUART with DMA */
    setupLeuartDma();

    /* Set index variables to the beginning of the FIFO */
    txIdxPut = txIdxGet = 0;
}


#if ENABLE_LEUART_RECEIVER
/**************************************************************************//**
 * @brief LEUART IRQ handler
 *
 * When a character has been received by the LEUART, this interrupt routine will
 * store the character into the rxFIFO.
 *
 *****************************************************************************/
void LEUART_IRQHandler(void)
{
uint32_t leuartif, len;

    /* Store and reset pending interupts */
    leuartif = LEUART_IntGet(LEUART);
    LEUART_IntClear(LEUART, leuartif);

    /* Check for frame found */
    if (leuartif & LEUART_IF_SIGF)
    {
	/* Zero-terminate RX command line buffer */
	len = CMD_LINE_SIZE - 2
	    - ((g_DMA_ControlBlock[DMA_CHAN_LEUART_RX].CTRL >> 4) & 0x3FF);

	g_CmdLine[len] = EOS;

	/* set flag to notify new command is available */
	g_flgCmdLine = true;

	/* Re-start DMA */
	DMA_ActivateBasic(DMA_CHAN_LEUART_RX, // Activate channel selected
			  true,		// Use primary descriptor
			  false,	// No DMA burst
			  NULL,		// keep destination address
			  NULL,		// keep source address
			  CMD_LINE_SIZE - 1);	// Size of buffer - 1
    }
}
#endif


/***************************************************************************//**
 *
 * @brief  Put string into transmit FIFO
 *
 * This routine writes the specified string into the transmit FIFO, where it
 * is transferred to the LEUART via DMA.  If there is no more space in the
 * FIFO, characters will be discarded.
 *
 * @param[in] pStr
 *	Adress pointer of the string to write into the FIFO.
 *
 ******************************************************************************/
void	 drvLEUART_puts (const char *pStr)
{
int16_t	cnt;			// free buffer space in number of bytes
bool	sendCR = false;		// set true to write <CR> to buffer


    while (*pStr != EOS)
    {
	/* Non-blocking: discard string if FIFO is full */
	cnt  = txIdxPut;
	cnt -= txIdxGet;
	if (cnt < 0)
	    cnt += sizeof(txFIFO);

	if (cnt > (int16_t)(sizeof(txFIFO) - 2))
	    break;

	/* Check if to translate <LF> to <CR><LF> */
	if (g_flgLEUART_LF2CRLF  &&  (*pStr == '\n')  &&  ! sendCR)
	{
	    /* Write <CR> to FIFO */
	    txFIFO[txIdxPut] = '\r';
	    sendCR = true;	// special character <CR>, set flag
	}
	else
	{
	    /* Write Byte to FIFO */
	    txFIFO[txIdxPut] = *pStr++;
	    sendCR = false;	// regular character, be sure to clear flag
	}

	/* Increment FIFO index */
	if (++txIdxPut >= sizeof(txFIFO))
	    txIdxPut = 0;	// wrap around
    }

    /* Be sure to enable DMA for data transfer */
    dmaTransferStart();
}


/***************************************************************************//**
 *
 * @brief  Put character into the transmit FIFO
 *
 * This routine writes the specified character into the transmit FIFO, where
 * it will be transferred to the LEUART via DMA.  If there is no more space in
 * the FIFO, the character will be discarded.
  *
 * @param[in] c
 *	Character to write into the FIFO.
 *
 ******************************************************************************/
void	 drvLEUART_putc (char c)
{
char	 buffer[2];	// temporary character buffer


    /* Build local string with just one character */
    buffer[0] = c;
    buffer[1] = EOS;

    /* call puts() */
    drvLEUART_puts (buffer);
}


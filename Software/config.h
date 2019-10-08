/***************************************************************************//**
 * @file
 * @brief	Project configuration file
 * @author	Ralf Gerhauser
 * @version	2016-11-22
 *
 * This file allows to set miscellaneous configuration parameters.  It must be
 * included by all modules.
 *
 ****************************************************************************//*
Revision History:
2016-11-22,rage	Added DMA Channel Assignment for LEUART support.
2014-10-12,rage	Removed LED definitions.
2014-11-11,rage	Derived from project "AlarmClock".
*/

#ifndef __INC_config_h
#define __INC_config_h

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include <stdbool.h>
#include "em_device.h"

/*=============================== Definitions ================================*/

/*
 * Basic defines - should all be moved to Generic.h
 */
    /* terminators for lists and strings */

#define	EOL		NULL		/* EndOfList		*/
#define EOS		'\0'		/* EndOfString		*/
#define	NONE		(-1)

    /* macro to calculate the number of elements of an array */
#define ELEM_CNT(array)  (sizeof (array) / sizeof ((array)[0]))

/*!
 * @brief MPU Clock Configuration.
 *
 * Set to 0 to use the internal RC oscillator, if 1 the external 32MHz XTAL
 * is used.
 */
#define USE_EXT_32MHZ_CLOCK	0

/*
 * Configuration for module AlarmClock
 */
    /*!@brief RTC frequency in [Hz]. */
#define RTC_COUNTS_PER_SEC	32768

/*
 * Configuration for module "Keys.c"
 */
    /*!@brief Time in [ms] a key needs to be asserted to start autorepeat. */
#define AUTOREPEAT_THRESHOLD	750
    /*!@brief Rate in [ms] the autorepeat feature repeats the previous key. */
#define AUTOREPEAT_RATE		250

/* forward declaration */
void    drvLEUART_puts(const char *str);

/*!@name DMA Channel Assignment
 *
 * The following definitions assign the 8 DMA channels to the respective
 * devices or drivers.  These defines are used as index within the global
 * @ref DMA_DESCRIPTOR_TypeDef structure @ref g_DMA_ControlBlock.
 */
//@{
#define DMA_CHAN_LEUART_RX	0	//! LEUART Rx uses DMA channel 0
#define DMA_CHAN_LEUART_TX	1	//! LEUART Tx uses DMA channel 1
//@}

/*================================== Macros ==================================*/

#ifdef DEBUG
    /*
     * Debugging output via ITM or LEUART
     */
    #define DBG_PUTC(ch)	ITM_SendChar(ch)
    #define DBG_PUTS(str)	ITM_SendStr(str)
    uint32_t ITM_SendChar (uint32_t ch);
    void ITM_SendStr(const char *pStr);
    void dbgInit(void);
#else
    #define DBG_PUTC(ch)
    #define DBG_PUTS(str)
#endif

    /*! Macro to address a single bit in the I/O range (peripheral range) in
     *  an atomic manner.
     * @param address   I/O register address.
     * @param bitNum    Bit number within this register.
     */
#define IO_BIT_ADDR(address, bitNum)					\
	((__IO uint32_t *) (BITBAND_PER_BASE				\
			+ (((uint32_t)(address)) - PER_MEM_BASE) * 32	\
			+ (bitNum) * 4))

    /*! Shortcut to directly access an I/O-bit. */
#define IO_Bit(regName, bitNum)	*IO_BIT_ADDR(&regName, bitNum)

    /*! Macro to address a single bit in an SRAM variable in an atomic manner.
     * @param address   Address of the variable in SRAM.
     * @param bitNum    Bit number within this variable.
     */
#define SRAM_BIT_ADDR(address, bitNum)					\
	((__IO uint32_t *) (BITBAND_RAM_BASE				\
			+ (((uint32_t)(address)) - RAM_MEM_BASE) * 32	\
			+ (bitNum) * 4))

    /*! Shortcut to directly access a bit in a variable. */
#define Bit(varName, bitNum)	*SRAM_BIT_ADDR(&varName, bitNum)

/*=========================== Typedefs and Structs ===========================*/

/*!@brief Enumeration of Alarm Identifiers
 *
 * This is the list of Alarm IDs used by this application.  They are used to
 * identify a particular alarm time entry via the <b>alarmNum</b> parameter
 * when calling alarm functions, e.g. AlarmSet().
 */
typedef enum
{
    ALARM_XXX,		//!<  0: Example ALARM_ID, not used in this project
    END_ALARM_ID
} ALARM_ID;


/*!@brief Enumeration of the EM1 Modules (unused)
 *
 * This is the list of Software Modules that require EM1 to work, i.e. they
 * will not work in EM2 because clocks, etc. would be disabled.  These enums
 * are used to set/clear the appropriate bit in the @ref g_EM1_ModuleMask.
 */
typedef enum
{
    EM1_MOD_XXX,	//!<  0: Example enum, this project always used EM2
    END_EM1_MODULES
} EM1_MODULES;

/*======================== External Data and Routines ========================*/

extern volatile bool	 g_flgIRQ;		// Flag: Interrupt occured
extern volatile uint16_t g_EM1_ModuleMask;	// Modules that require EM1


#endif /* __INC_config_h */

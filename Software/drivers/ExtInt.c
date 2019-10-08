/***************************************************************************//**
 * @file
 * @brief	External Interrupt Handling
 * @author	Ralf Gerhauser
 * @version	2016-11-22
 *
 * The purpose of this module is to handle any kind of external interrupts
 * (EXTI).  In detail, this includes:
 * - Initialization and configuration of external interrupts.
 * - Routines that allow to enable or disable a dedicated external interrupt.
 * - An interrupt service routine (ISR), which detects rising and falling edge
 *   EXTIs, gets time stamp information, and calls the corresponding module
 *   handler.
 *
 * @note
 * This module <b>only</b> deals with the EXTI functionality, the affected
 * GPIO port pins must have been configured and connected to the EXTI by some
 * other code before.
 *
 ****************************************************************************//*
Revision History:
2016-11-22,rage	Clear interrupts early to be able to receive new interrupts.
2016-04-13,rage	BugFix: Always detect rising and falling edges, so no interrupts
		can get lost.  Determine real signal state from input port.
2016-04-05,rage	Made local variable <l_extiBitMask> of type "volatile".
2016-02-16,rage	Changed ExtIntInit(), so it does not automatically enable all
		configured interrupts because this may be too soon.
		Added ExtIntEnableAll() and ExtIntDisableAll() to enable or
		disable all configured external interrupts at once.
		BugFix in EXTI_Handler(): Only service interrupts which have
		been enabled (ignore disabled interrupts).
2014-05-12,rage	Added functions ExtIntEnable() and ExtIntDisable().
2014-04-06,rage	Initial version.
*/

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include "ExtInt.h"
#include "em_device.h"
#include "em_assert.h"
#include "em_bitband.h"


/*=============================== Definitions ================================*/


/*=========================== Typedefs and Structs ===========================*/


/*======================== External Data and Routines ========================*/


/*========================= Global Data and Routines =========================*/


/*================================ Local Data ================================*/

    /*! Local pointer to EXTI configuration */
static const EXTI_INIT *l_pExtIntCfg;

    /*! Bit mask of all EXTIs in use */
static volatile uint32_t l_extiBitMask;

/*=========================== Forward Declarations ===========================*/

void	EXTI_Handler (void);


/***************************************************************************//**
 *
 * @brief	External interrupt initialization
 *
 * This routine must be called once to initialize all external interrupts.
 * The affected GPIO pins must have been initialized separately!
 *
 * @param[in] pInitStruct
 *	Address of an initialization structure of type EXTI_INIT that defines
 *	all external interrupts to be used, their initial trigger state, and
 *	the corresponding function to be called.
 *
 * @note
 *	Parameter <b>pInitStruct</b> must point to a persistent data structure,
 *	i.e. this must be valid over the whole life time of the program.
 *
 * @note
 *	This routine does not enable the interrupts, call ExtIntEnableAll()
 *	for that purpose, or enable individual interrupt via ExtIntEnable().
 *
 ******************************************************************************/
void	ExtIntInit (const EXTI_INIT *pInitStruct)
{
    /* Parameter check */
    EFM_ASSERT(pInitStruct != NULL);

    /* Save configuration */
    l_pExtIntCfg = pInitStruct;

    /* Disable and clear all EXTIs */
    GPIO->IEN = 0;
    GPIO->EXTIRISE = 0;
    GPIO->EXTIFALL = 0;
    GPIO->IFC = 0xFFFF;

    /*
     * Initialize falling and rising edge triggers.
     * WARNING: This part of code is NOT multi-threading save!
     */
    l_extiBitMask = 0;

    while (pInitStruct->IntBitMask != 0)
    {
	l_extiBitMask |= pInitStruct->IntBitMask;	// add to bit mask

	pInitStruct++;
    }

    /* Always detect rising and falling edge */
    GPIO->EXTIRISE = l_extiBitMask;
    GPIO->EXTIFALL = l_extiBitMask;

    /* Clear and enable NVIC interrupts */
    NVIC_ClearPendingIRQ (GPIO_EVEN_IRQn);
    NVIC_EnableIRQ (GPIO_EVEN_IRQn);
    NVIC_ClearPendingIRQ (GPIO_ODD_IRQn);
    NVIC_EnableIRQ (GPIO_ODD_IRQn);
}

/***************************************************************************//**
 *
 * @brief	Disable all External Interrupts
 *
 * Disable all external interrupts.
 *
 ******************************************************************************/
void	ExtIntDisableAll (void)
{
    /* Disable all external interrupts */
    GPIO->IEN = 0x0000;
}

/***************************************************************************//**
 *
 * @brief	Enable all External Interrupts
 *
 * Enable the all previously configured external interrupts, clear possible
 * pending interrupts before doing this.
 *
 ******************************************************************************/
void	ExtIntEnableAll (void)
{
    /* Clear any pending interrupt */
    GPIO->IFC = 0xFFFF;

    /* Enable all configured interrupts */
    GPIO->IEN = l_extiBitMask;
}

/***************************************************************************//**
 *
 * @brief	Disable External Interrupt
 *
 * Disable the specified external interrupt.
 *
 * @param[in] extiNum
 *	Number of the external interrupt.
 *
 ******************************************************************************/
void	ExtIntDisable (int extiNum)
{
    /* Parameter check */
    EFM_ASSERT(0 <= extiNum  &&  extiNum <= 15);

    /* Disable this interrupt */
    BITBAND_Peripheral (&(GPIO->IEN), extiNum, 0);
}

/***************************************************************************//**
 *
 * @brief	Enable External Interrupt
 *
 * Enable the specified external interrupt, clear possible pending interrupts
 * before doing this.
 *
 * @param[in] extiNum
 *	Number of the external interrupt.
 *
 ******************************************************************************/
void	ExtIntEnable (int extiNum)
{
    /* Parameter check */
    EFM_ASSERT(0 <= extiNum  &&  extiNum <= 15);

    /* Clear any pending interrupt */
    GPIO->IFC = (1 << extiNum);

    /* Enable this interrupt */
    BITBAND_Peripheral (&(GPIO->IEN), extiNum, 1);
}

/***************************************************************************//**
 *
 * @brief	Interrupt Handler for even GPIOs
 *
 * This Interrupt Service Routine (ISR) gets called for even GPIO interrupts.
 * It simply leads to the generic EXTI_Handler.
 *
 ******************************************************************************/
void	GPIO_EVEN_IRQHandler (void)
{
    EXTI_Handler();
}

/***************************************************************************//**
 *
 * @brief	Interrupt Handler for odd GPIOs
 *
 * This Interrupt Service Routine (ISR) gets called for odd GPIO interrupts.
 * It simply leads to the generic EXTI_Handler.
 *
 ******************************************************************************/
void	GPIO_ODD_IRQHandler (void)
{
    EXTI_Handler();
}

/***************************************************************************//**
 *
 * @brief	Interrupt Handler for all EXTIs
 *
 * This is the handler for the EXTernal Interrupts (EXTI).  It is triggered for
 * all configured and enabled EXTIs, either for a rising, or a falling edge.
 * The sequence of operations in detail:
 * -# Receive EXTI on rising or falling edge.
 * -# Read current RTC value for time stamp.
 * -# For each interrupt perform the following actions:
 *    - Determine and call the corresponding handler.
 *    - Change the trigger type, i.e. rising to falling edge, and vice versa.
 * -# Clear all received interrupts at once.
 * -# Return from interrupt.
 *
 * @note
 * The time stamp is read from the Real Time Counter (RTC), so its resolution
 * depends on the RTC.
 *
 ******************************************************************************/
void	EXTI_Handler (void)
{
uint32_t  timeStamp;		// current time value from RTC
uint32_t  status;		// interrupt status flags
uint32_t  irqMask;		// bit mask of active external interrupts
int	  extiNum;		// EXTI number
uint32_t  extiBitMask;		// bit mask for EXTI number <extiNum>
int	  portNum;		// GPIO port number of an EXTI
bool	  extiLvl;		// current level of EXTI
const EXTI_INIT *pExtIntCfg;	// pointer to EXTI configuration data

    /* get time stamp from RTC */
    timeStamp = RTC->CNT;

    /* get EXTI status and mask out all disabled interrupts */
    status  = GPIO->IF;
    status &= GPIO->IEN;

    /* immediately return if there is nothing to do */
    if (status == 0)
	return;

    /* clear interrupt status bits early to be able to receive new interrupts */
    GPIO->IFC = status;

    /* get all EXTIs in use (rising and falling edge) */
    irqMask = status & l_extiBitMask;

    /* now process all asserted EXTIs */
    while (irqMask)
    {
	/* get bit number of the highest EXTI from the bit mask */
	extiNum = 31 - __CLZ (irqMask);
	extiBitMask = (0x1 << extiNum);

	/* remove this interrupt from the bit mask */
	irqMask &= ~extiBitMask;

	/* get associated port number */
	if (extiNum < 8)
	    portNum = (GPIO->EXTIPSELL >> (extiNum * 4)) & 0x7;
	else
	    portNum = (GPIO->EXTIPSELH >> ((extiNum-8) * 4)) & 0x7;

	/* determine whether rising or falling edge */
	extiLvl = GPIO->P[portNum].DIN & extiBitMask ? true : false;

	/* see which functions to be called for this EXTI */
	for (pExtIntCfg = l_pExtIntCfg;
	     pExtIntCfg->IntBitMask != 0;  pExtIntCfg++)
	{
	    if (pExtIntCfg->IntBitMask & extiBitMask)
	    {
		/* call all handlers that request this EXTI */
		pExtIntCfg->IntFct(extiNum, extiLvl, timeStamp);
	    }
	}
    }
}

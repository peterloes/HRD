/***************************************************************************//**
 * @file
 * @brief	Handling of Keys (push buttons)
 * @author	Ralf Gerhauser
 * @version	2016-11-22
 *
 * This module provides all the functionality to receive key events and
 * translate them into key codes.
 * In detail, this includes:
 * - Initialization of the hardware (GPIOs that are connected to keys).
 * - Receive key events and and translate them into key codes.
 * - Call an external function for each translated key code.
 *
 ****************************************************************************//*
Revision History:
2016-11-22,rage	Added support for Power-Key.
2015-06-22,rage	Derived from project "AlarmClock".
*/

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include "em_device.h"
#include "em_assert.h"
#include "em_cmu.h"
#include "Keys.h"
#include "AlarmClock.h"

/*=============================== Definitions ================================*/


/*=========================== Typedefs and Structs ===========================*/

/*! Internal states for key handling */
typedef enum
{
    KEY_IDLE,		//!< IDLE state - no key active
    KEY_POWER,		//!< the POWER Key is active
    KEY_NEXT,		//!< the NEXT Key is active
    KEY_PREV,		//!< the PREV Key is active
    KEY_CNT		//!< total number of key states.
} KEY_STATE;

/*======================== External Data and Routines ========================*/


/*========================= Global Data and Routines =========================*/


/*================================ Local Data ================================*/

    /*! Pointer to module configuration */
static const KEY_INIT *l_pKeyInit;

    /*! Variable to keep current key state */
static KEY_STATE     l_KeyState = KEY_IDLE;

    /*! Variable to keep autorepeat key code */
static KEYCODE	     l_KeyCode;

/*=========================== Forward Declarations ===========================*/

#if KEY_AUTOREPEAT
static void  KeyTimerFct (void);
#endif


/***************************************************************************//**
 *
 * @brief	Initialize key hardware
 *
 * This routine initializes the board-specific hardware for the key (push
 * button) functionality.  This is restricted to the GPIO set up, NVIC
 * interrupts will be configured later by calling function ExtIntInit().
 *
 * There are 2 keys and states supported:
 * - NEXT (S5) selects the next item to display
 * - PREV (S4) moves back to the previous item
 *
 * Each key generates an <b>asserted</b> code when the key is hit once,
 * one or more <b>repeat</b> codes as long as it is still pressed, and a
 * <b>released</b> code when it is released again.
 *
 * The GPIO ports and pins have been defined in the header file.
 *
 * @param[in] pInitStruct
 *	Address of an initialization structure of type KEY_INIT that defines
 *	the timings for the autorepeat threshold and rate, and a function to
 *	be called for each translated key.
 *
 * @note
 *	Parameter <b>pInitStruct</b> must point to a persistent data structure,
 *	i.e. this must be valid over the whole life time of the program.
 *
 ******************************************************************************/
void	KeyInit (const KEY_INIT *pInitStruct)
{
    /* Parameter check */
    EFM_ASSERT(pInitStruct != NULL);
    EFM_ASSERT(pInitStruct->KeyFct != NULL);

    /* Save configuration */
    l_pKeyInit = pInitStruct;

    /* Be sure to enable clock to GPIO (should already be done) */
    CMU_ClockEnable (cmuClock_GPIO, true);

    /*
     * Initialize GPIOs for all keys.  The port pins must be configured for
     * input, and connected to the external interrupt (EXTI) facility.  At
     * this stage, the interrupts are not enabled, this is done later by
     * calling ExtIntInit().
     */
    GPIO_PinModeSet (KEY_POWER_PORT, KEY_POWER_PIN, gpioModeInput, 0);
    GPIO_IntConfig  (KEY_POWER_PORT, KEY_POWER_PIN, false, false, false);

    GPIO_PinModeSet (KEY_NEXT_PORT, KEY_NEXT_PIN, gpioModeInput, 0);
    GPIO_IntConfig  (KEY_NEXT_PORT, KEY_NEXT_PIN, false, false, false);

    GPIO_PinModeSet (KEY_PREV_PORT, KEY_PREV_PIN, gpioModeInput, 0);
    GPIO_IntConfig  (KEY_PREV_PORT, KEY_PREV_PIN, false, false, false);

#if KEY_AUTOREPEAT
    /* Install high-resolution timer routine for autorepeat */
    msTimerAction (KeyTimerFct);
#endif
}

/***************************************************************************//**
 *
 * @brief	Key handler
 *
 * This handler is called by the EXTI interrupt service routine for each
 * key which is asserted or released.  Together with the autorepeat feature
 * via a high-resolution timer and KeyTimerFct(), it translates the interrupt
 * number into a @ref KEYCODE.  This is then passed to the @ref KEY_FCT
 * defined as part of the @ref KEY_INIT structure.
 *
 * @param[in] extiNum
 *	EXTernal Interrupt number of a key.  This is identical with the pin
 *	number, e.g. @ref KEY_NEXT_PIN.
 *
 * @param[in] extiLvl
 *	EXTernal Interrupt level: 0 means falling edge, logic level is now 0,
 *	1 means rising edge, logic level is now 1.  Since the keys are
 *	connected to ground, we have a negative logic, i.e. 0 means asserted!
 *
 * @param[in] timeStamp
 *	Time stamp when the key event has been received.  This may be used
 *	to distinguish between short and long time asserted keys.
 *
 * @note
 * The time stamp is read from the Real Time Counter (RTC), so its resolution
 * depends on the RTC.  Use the define @ref RTC_COUNTS_PER_SEC to convert the
 * RTC value into a duration.
 *
 ******************************************************************************/
void	KeyHandler (int extiNum, bool extiLvl, uint32_t timeStamp)
{
KEY_STATE  keyState;		// new key state
KEYCODE	   keyCode;		// translated key code


    (void) timeStamp;		// suppress compiler warning "unused parameter"

    /* map the EXTI (pin) number to a key ID */
    switch (extiNum)
    {
	case KEY_POWER_PIN:	// POWER is high-active button!
	    keyState = KEY_POWER;
	    keyCode  = KEYCODE_POWER_ASSERT;
	    extiLvl = !extiLvl;	// convert from high to low-active
	    break;

	case KEY_NEXT_PIN:	// NEXT
	    keyState = KEY_NEXT;
	    keyCode  = KEYCODE_NEXT_ASSERT;
	    break;

	case KEY_PREV_PIN:	// PREV
	    keyState = KEY_PREV;
	    keyCode  = KEYCODE_PREV_ASSERT;
	    break;

	default:		// unknown pin number - ignore
	    return;
    }

    /*
     * Check if a key has been asserted or released.  Since the keys are
     * connected to ground, we have a negative logic, i.e. 0 means asserted!
     * An exception is the power button which is high-active.  Therefore the
     * level has been inverted by the logic above.
     */
    if (extiLvl)
    {
	/* Level is 1, key has been RELEASED */

	if (keyState != l_KeyState)
	    return;	// only release active key - ignore all others

#if KEY_AUTOREPEAT
	/* be sure to cancel a running timer */
	msTimerCancel();
#endif

	/* pass a KEYCODE_XXX_RELEASE code to the KEY_FCT */
	keyCode += KEYOFFS_RELEASE;

	/* set key state to IDLE again */
	l_KeyState = KEY_IDLE;
    }
    else
    {
	/* Level is 0, key has been ASSERTED */

	if (l_KeyState != KEY_IDLE)
	    return;	// a key is already asserted - ignore all further keys

	/* set new key state and code */
	l_KeyState = keyState;
	l_KeyCode  = (KEYCODE)(keyCode + KEYOFFS_REPEAT);

#if KEY_AUTOREPEAT
	/* start timer with autorepeat threshold */
	msTimerStart (l_pKeyInit->AR_Threshold);
#endif
    }

    /* call the specified KEY_FCT */
    l_pKeyInit->KeyFct (keyCode);
}

#if KEY_AUTOREPEAT
/***************************************************************************//**
 *
 * @brief	Key Timer Function
 *
 * This callback function is called whenever the high-resolution millisecond
 * timer expires.  Depending on the state of the key handler, this means:
 *
 * - The autorepeat threshold time is over, i.e. a key is asserted long enough
 *   to start the autorepeat function.
 * - When the autorepeat function is already active, the timer generates the
 *   key rate, i.e. the currently asserted key has to be repeated.
 *
 ******************************************************************************/
static void  KeyTimerFct (void)
{
    /* re-start timer with autorepeat rate */
    msTimerStart (l_pKeyInit->AR_Rate);

    /* call the specified KEY_FCT with the REPEAT code */
    l_pKeyInit->KeyFct (l_KeyCode);
}
#endif

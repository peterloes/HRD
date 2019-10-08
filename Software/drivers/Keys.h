/***************************************************************************//**
 * @file
 * @brief	Header file of module Keys.c
 * @author	Ralf Gerhauser
 * @version	2016-11-22
 ****************************************************************************//*
Revision History:
2016-11-22,rage	Added support for Power-Key.
2014-11-11,rage	Derived from project "AlarmClock".
*/

#ifndef __INC_Keys_h
#define __INC_Keys_h

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include <stdbool.h>
#include "em_device.h"
#include "em_gpio.h"
#include "config.h"		// include project configuration parameters

/*=============================== Definitions ================================*/

/*!@brief Per default the key autorepeat function is enabled, however for
 * debugging purposes, it may be useful to disable it by setting the define
 * to 0.
 */
#define KEY_AUTOREPEAT	1

/*!@brief Here follows the definition of all keys (push buttons) and their
 * related hardware configurations.
 */
#define KEY_POWER_PORT	gpioPortA
#define KEY_POWER_PIN	5

#define KEY_NEXT_PORT	gpioPortA
#define KEY_NEXT_PIN	4

#define KEY_PREV_PORT	gpioPortA
#define KEY_PREV_PIN	3


/*!@brief Bit mask of all affected external interrupts (EXTIs). */
#define KEY_EXTI_MASK	( (1 << KEY_POWER_PIN) | (1 << KEY_NEXT_PIN)	\
			| (1 << KEY_PREV_PIN) )

/*=========================== Typedefs and Structs ===========================*/

/*!@brief Translated key codes. */
typedef enum
{
    KEYCODE_NONE,		//!< No key code active
    KEYCODE_POWER_ASSERT,	//!< Key code for POWER once asserted
    KEYCODE_POWER_REPEAT,	//!< Key code for POWER autorepeat, still active
    KEYCODE_POWER_RELEASE,	//!< Key code for POWER released again
    KEYCODE_NEXT_ASSERT,	//!< Key code for NEXT once asserted
    KEYCODE_NEXT_REPEAT,	//!< Key code for NEXT autorepeat, still active
    KEYCODE_NEXT_RELEASE,	//!< Key code for NEXT released again
    KEYCODE_PREV_ASSERT,	//!< Key code for PREV once asserted
    KEYCODE_PREV_REPEAT,	//!< Key code for PREV autorepeat, still active
    KEYCODE_PREV_RELEASE,	//!< Key code for PREV released again
    END_KEYCODE			//!< End of key code definitions
} KEYCODE;

/*!@brief Offsets to be added to the ASSERT key code */
#define KEYOFFS_REPEAT	(KEYCODE)1	// +1 for REPEAT code
#define KEYOFFS_RELEASE	(KEYCODE)2	// +2 for RELEASE code

/*!@brief Function to be called for each translated key code. */
typedef void	(* KEY_FCT)(KEYCODE keycode);

/*!@brief Key initialization structure.
 *
 * Initialization structure to define the timings for the autorepeat (AR)
 * threshold and rate (in milliseconds), and a function to be called for each
 * translated key.
 *
 * <b>Typical Example:</b>
 * @code
 * static const KEY_INIT l_KeyInit =
 * {
 *    250,		// Autorepeat threshold is 250ms
 *    100,		// Autorepeat rate is 100ms (10Hz)
 *    MenuKeyHandler	// Key handler of module "Menu.c"
 * };
 * @endcode
 */
typedef struct
{
    uint16_t  AR_Threshold;	//!< Threshold in [ms] after autorepeat starts
    uint16_t  AR_Rate;		//!< Key rate in [ms] when autorepeat is active
    KEY_FCT   KeyFct;		//!< Fct. to be called for each translated key
} KEY_INIT;

/*================================ Prototypes ================================*/

/* Initialize key hardware */
void	KeyInit (const KEY_INIT *pInitStruct);

/* Key handler, called from interrupt service routine */
void	KeyHandler	(int extiNum, bool extiLvl, uint32_t timeStamp);


#endif /* __INC_Keys_h */

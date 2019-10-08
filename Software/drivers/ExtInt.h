/***************************************************************************//**
 * @file
 * @brief	Header file of module ExtInt.c
 * @author	Ralf Gerhauser
 * @version	2016-04-13
 ****************************************************************************//*
Revision History:
2016-04-13,rage	Removed element <IntTrigMask> from structure EXTI_INIT.
2016-02-16,rage	Added prototypes for ExtIntEnableAll() and ExtIntDisableAll().
2014-05-12,rage	Added prototypes for ExtIntEnable() and ExtIntDisable().
2014-04-06,rage	Initial version.
*/

#ifndef __INC_ExtInt_h
#define __INC_ExtInt_h

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include <stdbool.h>
#include "em_device.h"

/*=============================== Definitions ================================*/


/*=========================== Typedefs and Structs ===========================*/

/*!@brief Function to be called for an asserted external interrupt. */
typedef void	(* EXTI_FCT)(int extiNum, bool extiLvl, uint32_t timeStamp);

/*!@brief External interrupt initialization structure.
 *
 * Initialization structure to define external interrupts to be used, and
 * their corresponding function to call when one of them is asserted.
 * This structure is used as 0-terminated array to connect certain EXTIs
 * with their corresponding handler.
 *
 * <b>Typical Example:</b>
 * @code
 * static const EXTI_INIT l_ExtIntCfg[] =
 * { //	IntBitMask,	IntFct
 *    {	KEY_EXTI_MASK,	KeyHandler	},
 *    {	0,		NULL		}
 * };
 * @endcode
 */
typedef struct
{
    uint16_t  IntBitMask;	//!< Bit mask defines affected EXTIs
    EXTI_FCT  IntFct;		//!< Function to be called for above EXTIs
} EXTI_INIT;

/*================================ Prototypes ================================*/

    /* External interrupt initialization */
void	ExtIntInit   (const EXTI_INIT *pInitStruct);
void	ExtIntEnableAll (void);
void	ExtIntDisableAll (void);
void	ExtIntEnable (int extiNum);
void	ExtIntDisable(int extiNum);


#endif /* __INC_ExtInt_h */

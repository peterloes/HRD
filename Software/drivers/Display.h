/***************************************************************************//**
 * @file
 * @brief	Header file of module Display.c
 * @author	Ralf Gerhauser
 * @version	2016-11-22
 ****************************************************************************//*
Revision History:
2016-11-22,rage	Defined separate format types for Overcurrent and Highcurrent
		Reaction Times.
2015-10-12,rage	Added defines for power-off logic.
2014-11-25,rage	Initial version.
*/

#ifndef __INC_Display_h
#define __INC_Display_h

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include <stdbool.h>
#include "em_device.h"
#include "config.h"		// include project configuration parameters
#include "Keys.h"
#include "BatteryMon.h"

/*=============================== Definitions ================================*/

    /*!@brief Time in [s] after which the LCD is powered-off. */
#ifndef LCD_POWER_OFF_TIMEOUT
    #define LCD_POWER_OFF_TIMEOUT	60
#endif

    /*!@brief Time in [s] after which the whole device is powered-off. */
#ifndef POWER_OFF_TIMEOUT
    #define POWER_OFF_TIMEOUT		120
#endif

/*
 * Power Pin Definitions (drives FET T1)
 */
    /*!@brief GPIO Port of the Power-Pin. */
#define HOLD_POWER_PORT		gpioPortD
    /*!@brief GPIO Pin of the Power-Pin. */
#define HOLD_POWER_PIN		0

/*=========================== Typedefs and Structs ===========================*/

    /*!@brief LCD field identifiers.
     *
     * These enumerations are used for two purposes:
     * -# As index within an array of type @ref LCD_FIELD, which specifies
     *    the position and width of a field on the LC-Display.
     * -# As identifier when such a field needs to be updated on the LCD,
     *    see @ref LCD_FIELD_ID_MASK_ITEM and DisplayUpdateCheck() for example.
     */
typedef enum
{
    LCD_LINE1_BLANK,	//!< 0: Leave line 1 of the LCD blank
    LCD_LINE2_BLANK,	//!< 1: Leave line 2 of the LCD blank
    LCD_LINE1_TEXT,	//!< 2: Display text in line 1
    LCD_LINE2_TEXT,	//!< 3: Display text in line 2
    LCD_ITEM_DESC,	//!< 4: Display item description (one line)
    LCD_ITEM_ADDR,	//!< 5: Display item register address
    LCD_ITEM_DATA,	//!< 6: Display item data
    LCD_CLOCK,		//!< 7: Display current uptime (for test purposes)
    LCD_FIELD_ID_CNT	//!< LCD Field ID count
} LCD_FIELD_ID;

    /*!@name Macros and defines to deal with field ID bit masks */
//@{
    /*!@brief Bit associated with <b>id</b> in bit mask */
#define LCD_FIELD_ID_BIT(id)		(1UL << (id))

    /*!@brief Address of bit associated with <b>id</b> in variable */
#define LCD_FIELD_ID_BIT_VAR(var, id)	Bit(var, (id))

    /*!@brief Bit mask that specifies the fields to display when a new item
     * is selected.
     */
#define LCD_FIELD_ID_MASK_ITEM	  LCD_FIELD_ID_BIT(LCD_ITEM_DESC)   \
				| LCD_FIELD_ID_BIT(LCD_ITEM_ADDR)   \
				| LCD_FIELD_ID_BIT(LCD_ITEM_DATA)
//@}

    /*!@brief LCD field definition.
     *
     * This structure defines the location of a field on the LC-Display, i.e.
     * the <b>X</b> and <b>Y</b> coordinates of the field, and its <b>Width</b>.
     */
typedef struct
{
    uint8_t	X;		//!< X position of the field , starting at 0
    uint8_t	Y;		//!< Y position of the field , starting at 0
    uint8_t	Width;		//!< Width of the field in characters
} LCD_FIELD;

    /*!@brief Function to be called when specified duration is over
     *
     * This type of function is used by DisplayNext() to install a callback
     * routine which is executed after the specified amount of time has elapsed.
     * The function argument <b>userParm</b> can be used to specify the next
     * item to be displayed.
     */
typedef void	(* DISP_NEXT_FCT)(int userParm);

    /*!@brief Format identifiers.
     *
     * These enumerations specify various data formats.  They are used as an
     * element in structure @ref ITEM to specify the data representation of
     * an item.  They are handled by a switch() statement in GetDataString().
     */
typedef enum
{
    FRMT_FW_VERSION,	//!<  0: Special ID to show firmware version and date
    FRMT_CR2032_BAT,	//!<  1: Voltage of local CR2032 supply battery
    FRMT_STRING,	//!<  2: 0-terminated string
    FRMT_HEX,		//!<  3: Hexadecimal data representation
    FRMT_INTEGER,	//!<  4: Integer value
    FRMT_SERNUM,	//!<  5: Serial Number
    FRMT_PERCENT,	//!<  6: Amount in percent
    FRMT_DURATION,	//!<  7: Duration in [min]
    FRMT_OC_REATIME,	//!<  8: Overcurrent Reaction Time in 1/2[ms] units
    FRMT_HC_REATIME,	//!<  9: Highcurrent Reaction Time in 2[ms] units
    FRMT_MILLIVOLT,	//!< 10: Voltage in [mV]
    FRMT_MILLIAMP,	//!< 11: Current in [mA]
    FRMT_MILLIAMPH,	//!< 12: Capacity in [mAh]
    FRMT_MICROOHM,	//!< 13: Resistance in [uOhm]
    FRMT_DATE,		//!< 14: Date (unknown format, TODO: get info)
    FRMT_TEMP,		//!< 15: Temperature (unknown format, TODO: get info)
    FRMT_TYPE_CNT	//!< Format Type Count
} FRMT_TYPE;

    /*!@brief Items to display
     *
     * This structure defines items that can be displayed on the LCD.
     */
typedef struct
{
    const char	*pDesc;		//!< Description to be displayed (max. 16 char)
    SBS_CMD	 Cmd;		//!< Register to read from Battery Controller
    FRMT_TYPE	 Frmt;		//!< Format to use for value representation
} ITEM;

/*================================ Prototypes ================================*/

void	DisplayInit(const LCD_FIELD *pField, const ITEM *pItemList, int itemCnt);
void	DisplayKeyHandler (KEYCODE keycode);
void	DisplayUpdateCheck (void);
void	DisplayUpdateTrigger (LCD_FIELD_ID fieldID);
void	DisplayText (int lineNum, const char *frmt, ...);
void	DisplayNext (unsigned int duration, DISP_NEXT_FCT fct, int userParm);


#endif /* __INC_Display_h */

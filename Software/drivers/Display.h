/***************************************************************************//**
 * @file
 * @brief	Header file of module Display.c
 * @author	Ralf Gerhauser
 * @version	2020-01-13
 ****************************************************************************//*
Revision History:
2020-01-13,rage	Added enums FRMT_BAT_CTRL and FRMT_HEXDUMP, and prototypes for
		PowerUp() and DisplaySelectItem().
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
    #define LCD_POWER_OFF_TIMEOUT	120
#endif

    /*!@brief Time in [s] after which the whole device is powered-off. */
#ifndef POWER_OFF_TIMEOUT
    #define POWER_OFF_TIMEOUT		240
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
    FRMT_BAT_CTRL,	//!<  1: Battery controller SMBus address and type
    FRMT_CR2032_BAT,	//!<  2: Voltage of local CR2032 supply battery
    FRMT_STRING,	//!<  3: 0-terminated string
    FRMT_HEX,		//!<  4: H1,2,3,4 Hexadecimal data representation
    FRMT_HEXDUMP,	//!<  5: Show Hexdump for more than 4 bytes
    FRMT_INTEGER,	//!<  6: Integer value
    FRMT_SERNUM,	//!<  7: Serial Number
    FRMT_PERCENT,	//!<  8: U1 Amount in percent [%]
    FRMT_DURATION,	//!<  9: U2 Duration in [min]
    FRMT_OC_REATIME,	//!< 10: Overcurrent Reaction Time in 1/2[ms] units
    FRMT_HC_REATIME,	//!< 11: Highcurrent Reaction Time in 2[ms] units
    FRMT_MILLIVOLT,	//!< 12: U2 Voltage in [mV]
    FRMT_MILLIAMP,	//!< 13: I2 Current in [±mA], +:charging, -:discharging
    FRMT_MILLIAMPH,	//!< 14: U2 Capacity in [mAh]
    FRMT_MICROOHM,	//!< 15: Resistance in [uOhm]
    FRMT_DATE,		//!< 16: Date [15:9=Year|8:5=Month|4:0=Day]
    FRMT_TEMP,		//!< 17: U2 Temperature [0.1°K]
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

void	PowerUp (void);
void	DisplayInit(const LCD_FIELD *pField, const ITEM *pItemList, int itemCnt);
void	DisplayKeyHandler (KEYCODE keycode);
void	DisplaySelectItem (int index);
void	DisplayUpdateCheck (void);
void	DisplayUpdateTrigger (LCD_FIELD_ID fieldID);
void	DisplayText (int lineNum, const char *frmt, ...);
void	DisplayNext (unsigned int duration, DISP_NEXT_FCT fct, int userParm);


#endif /* __INC_Display_h */

/***************************************************************************//**
 * @file
 * @brief	Display Manager
 * @author	Ralf Gerhauser
 * @version	2020-01-13
 *
 * This is the Display Manager module.  It controls all the information on
 * the LC-Display.  The keys <b>NEXT</b> and <b>PREV</b> are used to select the
 * next or previous item respectively:
 * - When <b>NEXT</b> is asserted, the display moves to the next
 *   information.
 * - When <b>PREV</b> is asserted, the display moves to the previous
 *   information.
 * - When none of the keys is asserted, the LCD is powered-off after 2 minutes.
 *
 * The low-level display routines can be found in LCD_DOGM162.c.
 *
 ****************************************************************************//*
Revision History:
2020-01-13,rage PowerUp() must be called to set Port Pin D0 and enable FET T1.
		Added flags to initiate power-off and probing of the battery
		controller type.
		DisplayKeyHandler: Check if next/previous item is applicable
		for the currently connected battery controller.
		DisplaySelectItem() directly selects the specified item to be
		displayed.  It is used to show the battery controller type.
		ItemDataString: Increased buffer sizes, implemented new format
		types FRMT_BAT_CTRL and FRMT_HEXDUMP.  FRMT_MILLIAMP outputs a
		signed value now.  FRMT_HEX supports 8, 16, 24 and 32bit values.
2016-11-22,rage	Asserting Power-Key returns to the first item to be displayed.
		Calculate temperature and manufacturing date from raw values.
2015-10-12,rage	Added functionality to automatically power-off the device
		when inactive for more than POWER_OFF_TIMEOUT seconds.
2015-06-22,rage	Initial version, derived from SNB_Heaven.
*/

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "em_device.h"
#include "em_assert.h"
#include "Keys.h"
#include "AlarmClock.h"
#include "Display.h"
#include "LCD_DOGM162.h"
#include "BatteryMon.h"

/*=============================== Definitions ================================*/

    //@brief Set level of the power enable pin.
#define SET_POWER_PIN(level)  IO_Bit(GPIO->P[HOLD_POWER_PORT].DOUT,	\
				     HOLD_POWER_PIN) = (level)

/*=============================== External Data ==============================*/

extern char const prjVersion[];
extern char const prjDate[];

/*================================ Local Data ================================*/

    /*!@brief Pointer to the display item list. */
static const ITEM	*l_pItemList;

    /*!@brief Number of elements in item list. */
static int		 l_ItemCnt;

    /*!@brief Index of the current item to be displayed. */
static int		 l_ItemIdx;

    /*!@brief Timer handle for switching the display off after a time. */
static TIM_HDL		 l_hdlLCD_Off = NONE;

    /*!@brief Timer handle for switching the who;e device off after a time. */
static TIM_HDL		 l_hdlPowerOff = NONE;

    /*!@brief Timer handle for Display Next callback routine. */
static TIM_HDL		 l_hdlDispNext = NONE;

    /*!@brief Flag if Display is currently powered on. */
static volatile bool	 l_flgDisplayIsOn;

    /*!@brief Flag to trigger Battery Controller Probing. */
static volatile bool	 l_flgBatteryCtrlProbe = true;

    /*!@brief Flag to trigger Power Off. */
static volatile bool	 l_flgPowerOff;


    /*!@brief Bit mask variable specifies which fields must be updated, each
     * bit refers to another field, see @ref LCD_FIELD_ID.
     */
uint16_t		 l_bitMaskFieldUpd;

    /*!@brief Bit mask variable, containing the currently active fields, i.e.
     * the fields should be displayed on the LCD, see @ref l_bitMaskFieldUpd.
     */
uint16_t		 l_bitMaskFieldActive;

    /*!@brief Flag is always set in this project due to missing DCF77 */
static volatile bool	 l_flgDisplayUpdEnabled = true;

    /*!@brief Flag triggers the execution of function @ref l_DispNextFct. */
static volatile bool	 l_DispNextFctTrigger;

    /*!@brief Function pointer for a callback routine which is executed after
     * the specified amount of time has elapsed, see @ref DisplayNext().
     */
static DISP_NEXT_FCT	 l_DispNextFct;

    /*!@brief User parameter for function @ref l_DispNextFct. */
static int		 l_DispNextUserParm;

/*=========================== Forward Declarations ===========================*/

static void  DisplayUpdate (void);
static char *ItemDataString (const ITEM *pItem);
static void  DisplayUpdateClock (void);
static void  SwitchLCD_Off(TIM_HDL hdl);
static void  SwitchDeviceOff(TIM_HDL hdl);
static void  DispNextTrigger(TIM_HDL hdl);


/***************************************************************************//**
 *
 * @brief	Power Up the Device
 *
 * This routine initializes and sets Port Pin D0 to enable FET T1.
 *
 ******************************************************************************/
void  PowerUp (void)
{
    /* Configure PD0 to hold power, set FET input to HIGH */
    GPIO_PinModeSet (HOLD_POWER_PORT, HOLD_POWER_PIN, gpioModePushPull, 1);
}


/***************************************************************************//**
 *
 * @brief	Initialize the Display
 *
 * This routine initializes the LC-Display and all the required functionality
 * around it, e.g. a timer to switch off the display when it is not in use.
 *
 ******************************************************************************/
void  DisplayInit (const LCD_FIELD *pField, const ITEM *pItemList, int itemCnt)
{
    /* Parameter check */
    EFM_ASSERT(pField != NULL);
    EFM_ASSERT(pItemList != NULL);

    /* Save configuration */
    l_pItemList = pItemList;
    l_ItemCnt   = itemCnt;

    /* Get a timer handle to switch the display off after a time */
    if (l_hdlLCD_Off == NONE)
	l_hdlLCD_Off = sTimerCreate (SwitchLCD_Off);

    /* Get a timer handle to switch the device off after a time */
    if (l_hdlPowerOff == NONE)
    {
	l_hdlPowerOff = sTimerCreate (SwitchDeviceOff);
	sTimerStart (l_hdlPowerOff, POWER_OFF_TIMEOUT);	// initial timer start
    }

    /* Create timer to trigger a callback routine after duration is over */
    if (l_hdlDispNext == NONE)
	l_hdlDispNext = sTimerCreate (DispNextTrigger);

    /* Connect the update function */
    DisplayUpdateFctInstall (DisplayUpdateClock);

    /* Set flags to active state */
    l_bitMaskFieldActive = LCD_FIELD_ID_BIT(LCD_LINE1_BLANK);	// pseudo field
    l_flgDisplayIsOn = true;

    /* Initialize the LCD module specific parts */
    LCD_Init (pField);
}


/***************************************************************************//**
 *
 * @brief	Display Key Handler
 *
 * This handler receives the translated key codes from the interrupt-driven
 * key handler, including autorepeat keys.  That is, whenever the user asserts
 * a key (push button), the resulting code is sent to this function.  The main
 * purpose of the handler is to navigate through the list of items which can
 * be displayed on the LCD.
 *
 * The following keys are recognized:
 * - <b>POWER</b> returns to the first item to be displayed and probes the
 *   battery controller type on the SMBus.  Keep asserted to switch off the
 *   device.
 * - <b>NEXT</b> moves to the next item to be displayed.
 * - <b>PREV</b> moves to the previous item.
 * If the keys are released, the LCD Power-Off timer is started.  The value
 * for this timer can be adjusted via the define @ref LCD_POWER_OFF_TIMEOUT.
 *
 * @warning
 * 	This function is called in interrupt context!
 *
 * @param[in] keycode
 *	Translated key code of type KEYCODE.
 *
 ******************************************************************************/
void	DisplayKeyHandler (KEYCODE keycode)
{
int	bitMaskCtrlType;

    /*
     * Build bit mask to detect items which are applicable for the current
     * controller type:
     * 0x08000 - BCT_UNKNOWN (0x00)
     * 0x10000 - BCT_ATMEL   (0x01)
     * 0x20000 - BCT_TI      (0x02)
     *
     * Note: Items with Cmd set to SBS_NONE (-1, i.e. all bits set!) will be
     *       displayed in any case.
     * See also BC_TYPE and SBS_CMD.
     */
    bitMaskCtrlType = (0x8000 << g_BatteryCtrlType);
    EFM_ASSERT(bitMaskCtrlType != 0);

    switch (keycode)
    {
	case KEYCODE_POWER_ASSERT:	// POWER was asserted
	    if (! l_flgDisplayIsOn)
		break;			// just use as wake-up if LCD is OFF

	    l_ItemIdx = 0;		// select item number 0, display version
	    l_flgBatteryCtrlProbe = true;	// first assertion, probe now
	    break;

	case KEYCODE_POWER_REPEAT:	// repeated POWER was asserted
	    /* Note: l_flgPowerOff is set below */
	    break;

	case KEYCODE_NEXT_ASSERT:	// NEXT was asserted
	    if (! l_flgDisplayIsOn)
		break;			// just use as wake-up if LCD is OFF

	    /* no break */

	case KEYCODE_NEXT_REPEAT:	// repeated NEXT was asserted
	    /* find next item to be displayed for the current controller */
	    do
	    {
		if (++l_ItemIdx >= l_ItemCnt)
		    l_ItemIdx = 0;	// wrap around
	    } while ((l_pItemList[l_ItemIdx].Cmd & bitMaskCtrlType) == 0);

	    break;

	case KEYCODE_PREV_ASSERT:	// PREV was asserted
	    if (! l_flgDisplayIsOn)
		break;			// just use as wake-up if LCD is OFF

	    /* no break */

	case KEYCODE_PREV_REPEAT:	// repeated PREV was asserted
	    /* find previous item to be displayed for the current controller */
	    do
	    {
		if (--l_ItemIdx < 0)
		    l_ItemIdx = l_ItemCnt-1; // wrap around
	    } while ((l_pItemList[l_ItemIdx].Cmd & bitMaskCtrlType) == 0);

	    break;

	case KEYCODE_POWER_RELEASE:	// POWER was released
	case KEYCODE_PREV_RELEASE:	// PREV was released
	case KEYCODE_NEXT_RELEASE:	// NEXT was released
	    /* (re-)start timers to switch display/device OFF after time */
	    if (l_hdlLCD_Off != NONE)
		sTimerStart (l_hdlLCD_Off, LCD_POWER_OFF_TIMEOUT);

	    if (l_hdlPowerOff != NONE)
		sTimerStart (l_hdlPowerOff, POWER_OFF_TIMEOUT);

	    return;

	default:	// ignore all other key codes
	    return;
    }

    /* POWER_REPEAT should switch the device off (when button is released) */
    l_flgPowerOff = (keycode == KEYCODE_POWER_REPEAT ? true : false);

    /* Limit the displayable range if no battery pack is connected */
    if (g_BatteryCtrlAddr == 0x00  &&  l_ItemIdx > 2)
	l_ItemIdx = 0;

    /* Common part of KEYCODE_Sx_ASSERT */
    l_bitMaskFieldActive = LCD_FIELD_ID_MASK_ITEM;  // activate item

    if (l_hdlLCD_Off != NONE)
	sTimerCancel (l_hdlLCD_Off);	// inhibit power-off of LCD

    if (l_hdlPowerOff != NONE)
	sTimerCancel (l_hdlPowerOff);	// inhibit device power-off

    /* Initiate first update for all selected fields */
    l_bitMaskFieldUpd = l_bitMaskFieldActive;
}


/***************************************************************************//**
 *
 * @brief	Select Item to be displayed
 *
 * This function selects a dedicated item from the @ref l_Item list by
 * specifying its index.
 *
 ******************************************************************************/
void	DisplaySelectItem (int index)
{
    if (index < 0  ||  index >= l_ItemCnt)
    {
	ConsolePrintf("DisplaySelectItem(%d): <index> out of range\n", index);
	return;
    }

    l_ItemIdx = index;

    l_bitMaskFieldUpd = l_bitMaskFieldActive = LCD_FIELD_ID_MASK_ITEM;
}


/***************************************************************************//**
 *
 * @brief	Display Update Check
 *
 * This function checks if the information on the LC-Display needs to be
 * updated, or if the LCD is currently not used and can be switched off.
 * It also handles power-off and battery controller probing requests.
 *
 * @note
 * 	This function may be called from standard program, usually the loop
 * 	in module "main.c" - it must not be called from interrupt routines!
 * 	In this application it is called every second, triggered from the
 * 	one second interrupt of the RTC.
 *
 ******************************************************************************/
void	DisplayUpdateCheck (void)
{
static int  prevSeconds;
static bool flgPowerOffActive;


    /*
     * Check if HRD should be powered off.
     */
    if (l_flgPowerOff)
    {
	if (! flgPowerOffActive)
	{
	    flgPowerOffActive = true;

	    DisplayText (1, "P O W E R  O F F");
	    DisplayText (2, "");

	    ConsolePrintf ("HRDevice is switched OFF now\n\n");
	    SET_POWER_PIN(0);		// set FET input to LOW
	}
	return;		// INHIBIT ALL OTHER ACTIONS
    }
#ifdef DEBUG	/* Only in debugging environment required: re-power device */
    else
    {
	if (flgPowerOffActive)
	{
	    flgPowerOffActive = false;
	    SET_POWER_PIN(1);	// set FET input to HIGH
	    l_flgDisplayIsOn = false;
	}
    }
#endif

    /*
     * Check if the Battery Controller Probe routine should be called (again).
     */
    if (l_flgBatteryCtrlProbe)
    {
	l_flgBatteryCtrlProbe = false;
	BatteryCtrlProbe();
    }

    /* If one second is over, we need to update measurements */
    if (prevSeconds != g_CurrDateTime.tm_sec)
    {
	prevSeconds = g_CurrDateTime.tm_sec;

	DisplayUpdateTrigger (LCD_ITEM_DATA);
    }

    /*
     * Check for callback trigger
     */
    if (l_DispNextFctTrigger)
    {
	DISP_NEXT_FCT fct = l_DispNextFct;

	/* Clear trigger flag */
	l_DispNextFctTrigger = false;

	/* See if a callback routine has been defined and call it */
	if (fct)
	{
	    l_DispNextFct = NULL;	// no NEW callback for default

	    fct (l_DispNextUserParm);	// call user routine
	}
	else
	{
	    /* No callback - switch LCD off */
	    SwitchLCD_Off((TIM_HDL)0);
	}

    }

    /*
     * Check if LC-Display should be powered-on or off.  This is executed
     * in this main loop since it must not happen in any interrupt service
     * routine (ISR) due to calling delay functions and other issues.
     * However, the reason when to do it is triggered via ISRs.
     */
    if (l_bitMaskFieldActive)
    {
	/* LCD should be powered ON */
	if (! l_flgDisplayIsOn)
	{
	    LCD_PowerOn();
	    l_flgDisplayIsOn = true;
	}

	/* LCD is ON - check if fields need to be updated */
	if (l_bitMaskFieldUpd & l_bitMaskFieldActive)
	    DisplayUpdate();
    }
    else
    {
	/* LCD should be powered OFF */
	if (l_flgDisplayIsOn)
	{
	    LCD_PowerOff();
	    l_flgDisplayIsOn = false;
	}
    }
}


/***************************************************************************//**
 *
 * @brief	Display Update
 *
 * This local function is called from DisplayUpdateCheck() whenever fields
 * need to be updated, i.e. when bits in @ref l_bitMaskFieldUpd are set.
 * It displays the respective data on the LCD and clears the associated bit.
 *
 ******************************************************************************/
static void DisplayUpdate (void)
{
LCD_FIELD_ID	 id;
const char	*pStr;


    for (id = LCD_LINE1_BLANK;  id < LCD_FIELD_ID_CNT;  id++)
    {
	if ((LCD_FIELD_ID_BIT_VAR(l_bitMaskFieldUpd,    id) == 0)
	||  (LCD_FIELD_ID_BIT_VAR(l_bitMaskFieldActive, id) == 0))
	    continue;		// nothing to be done for this field

	/* clear this bit */
	LCD_FIELD_ID_BIT_VAR(l_bitMaskFieldUpd, id) = 0;

	/* update the respective field */
	switch (id)
	{
	    case LCD_LINE1_BLANK:
	    case LCD_LINE2_BLANK:	// print empty line
		LCD_Printf (id, "");
		break;

	    case LCD_ITEM_DESC:		// display item description
		LCD_Printf (id, "%s", l_pItemList[l_ItemIdx].pDesc);
		break;

	    case LCD_ITEM_ADDR:		// display item register address
		if (l_pItemList[l_ItemIdx].Cmd != SBS_NONE)
		    LCD_Printf (id, "[%02X]", l_pItemList[l_ItemIdx].Cmd & 0xFF);
		break;

	    case LCD_ITEM_DATA:		// display item register data
		pStr = ItemDataString(&l_pItemList[l_ItemIdx]);
		if (pStr != NULL)
		{
		    if (l_pItemList[l_ItemIdx].Cmd != SBS_NONE)
			LCD_Printf (id, "%s", pStr);
		    else // use the whole line to display special information
			LCD_Printf (LCD_LINE2_TEXT, "%s", pStr);
		}
		else
		{
		    LCD_Printf (id, "READ ERROR");
		}
		break;

	    case LCD_CLOCK:		// current date and time
		LCD_Printf (id, "%02d%02d%02d %02d:%02d:%02d",
			    g_CurrDateTime.tm_year,
			    g_CurrDateTime.tm_mon + 1,
			    g_CurrDateTime.tm_mday,
			    g_CurrDateTime.tm_hour,
			    g_CurrDateTime.tm_min,
			    g_CurrDateTime.tm_sec);
		break;

	    default:		// LCD_LINE1_TEXT, LCD_LINE2_TEXT, or unknown ID
		break;		// nothing to be done
	}
    }
}


/***************************************************************************//**
 *
 * @brief	Item Data String
 *
 * This routine returns a formatted data string of the specified item data.
 * It uses BatteryRegReadWord() and BatteryRegReadBlock() to read the data
 * directly from the battery controller.
 *
 * @param[in] pItem
 *	Address of structure specifies the item that should be used.
 *
 * @return
 * 	Static buffer that contains the formatted data string of the item,
 * 	or NULL if there was an error, e.g. a read error from the battery
 * 	controller.
 *
 * @warning
 *	This routine is not MT-save (which should not be a problem for this
 *	application)!
 *
 ******************************************************************************/
static char	*ItemDataString (const ITEM *pItem)
{
static char	 strBuf[120];	// static buffer to return string into
uint8_t		 dataBuf[40];	// buffer for I2C data, read from the controller
uint32_t	 value;		// unsigned data variable
int		 data = 0;	// generic signed integer data variable
SBS_CMD		 cmd;		// command, i.e. the register address to read
int		 d, h, m;	// FRMT_DURATION: days, hours, minutes


    /* Parameter check */
    EFM_ASSERT(pItem != NULL);
    if (pItem == NULL)
	return NULL;		// error

    /* Prepare check for string buffer overflow */
    strBuf[sizeof(strBuf)-1] = 0x11;

    /* Check if item needs any data (that should be the standard) */
    cmd = pItem->Cmd;

    if (cmd != SBS_NONE)
    {
	/* See how many bytes we need to read */
	data = SBS_CMD_SIZE(cmd);	// get object size
	if (data > 4)
	{
	    /* More than 32 bits - must be a block, e.g. a string */
	    EFM_ASSERT(data < (int)sizeof(dataBuf));

	    if (BatteryRegReadBlock (cmd, dataBuf, data) < 0)
		return NULL;	// READ ERROR
	}
	else
	{
	    /* Read data word - may be 1, 2, 3, or 4 bytes long */
	    if (BatteryRegReadValue (cmd, &value) < 0)
		return NULL;	// READ ERROR

	    data = (int)value;
	}
    }

    /* Variable <data> contains 16bit raw value, build formatted string */
    switch (pItem->Frmt)
    {
	case FRMT_FW_VERSION:	// Firmware Version
	    sprintf (strBuf, "V%s %s", prjVersion, prjDate);
	    break;

	case FRMT_BAT_CTRL:	// Battery controller SMBus address and type
	    if (g_BatteryCtrlAddr == 0)
		strcpy (strBuf, "N O T  F O U N D");
	    else
		sprintf (strBuf, "0x%02X: %s", g_BatteryCtrlAddr,
			 g_BatteryCtrlName);
	    break;

	case FRMT_CR2032_BAT:	// Voltage of local CR2032 supply battery
	    data = ReadVdd();
	    sprintf (strBuf, "CR2032: %d.%03dV", data / 1000, data % 1000);
	    break;

	case FRMT_STRING:	// return string to be displayed
	    /*
	     * The first byte contains the number of ASCII characters WITHOUT
	     * a trailing 0 as EndOfString marker.  However, in some cases the
	     * specified byte count is larger than the string - then an EOS
	     * marker exists in the data read from the controller.
	     */
	    data = dataBuf[0];
	    EFM_ASSERT(data < (int)(sizeof(strBuf)-1));
	    strncpy (strBuf, (char *)dataBuf+1, data);
	    strBuf[data] = EOS;		// terminate string
	    break;

	case FRMT_HEXDUMP:	// prepare data as hexdump
	    data = dataBuf[0];
	    for (d = 0;  d < data;  d++)	// data = number of bytes
		sprintf (strBuf + 3*d, "%02X ", dataBuf[d+1]);
	    strBuf[3*d - 1] = EOS;
	    break;

	case FRMT_HEX:		// HEX Digits (8, 16, 24, or 32bit)
	    switch (SBS_CMD_SIZE(pItem->Cmd))
	    {
		case 1:
		    sprintf (strBuf, "0x%02X", data);
		    break;

		case 2:
		    sprintf (strBuf, "0x%04X", data);
		    break;

		case 3:
		    sprintf (strBuf, "0x%06X", data);
		    break;

		case 4:
		default:
		    sprintf (strBuf, "0x%08lX", value);
		    break;
	    }
	    break;

	case FRMT_INTEGER:	// Integer Value
	    sprintf (strBuf, "%5d", data);
	    break;

	case FRMT_SERNUM:	// 5-Digit Integer Value
	    sprintf (strBuf, "%05d", data);
	    break;

	case FRMT_PERCENT:	// Amount in percent
	    sprintf (strBuf, "%5d%%", data);
	    break;

	case FRMT_DURATION:	// Duration in [min]
	    if (data > 65534)		// > 45d
	    {
		strcpy (strBuf, "> 45 days");
	    }
	    else
	    {
		d = data / 60 / 24;
		data -= (d * 60 * 24);
		h = data / 60;
		data -= (h * 60);
		m = data;
		sprintf (strBuf, "%2dd %2dh %2dm", d, h, m);
	    }
	    break;

	case FRMT_OC_REATIME:	// Overcurrent Reaction Time in 1/2[ms] units
	    sprintf (strBuf, "%5dms", data/2);
	    break;

	case FRMT_HC_REATIME:	// Highcurrent Reaction Time in 2[ms] units
	    sprintf (strBuf, "%5dms", data*2);
	    break;

	case FRMT_MILLIVOLT:	// Voltage in [mV]
	    sprintf (strBuf, "%5dmV", data);
	    break;

	case FRMT_MILLIAMP:	// Current in [±mA], +:charging, -:discharging
	    sprintf (strBuf, "%5dmA", (int16_t)data);
	    break;

	case FRMT_MILLIAMPH:	// Capacity in [mAh]
	    sprintf (strBuf, "%5dmAh", data);
	    break;

	case FRMT_MICROOHM:	// Resistance in [uOhm]
	    sprintf (strBuf, "%5duOhm", data);
	    break;

	case FRMT_DATE:		// Date [15:9=Year|8:5=Month|4:0=Day]
	    sprintf (strBuf, "%04d-%02d-%02d", 1980 + (data >> 9),
		     (data >> 5) & 0xF, data & 0x1F);
	    break;

	case FRMT_TEMP:		// Temperature in 1/10[K], convert to [°C]
	    data -= 2732;	// subtract base of 273.16K
	    int degC = data / 10;
	    if (data < 0)
		data = -data;
	    sprintf (strBuf, "%d.%d C", degC, data % 10);
	    break;

	default:		// unsupported format
	    return NULL;

    }	// switch (pItem->Frmt)

    /* Perform check for string buffer overflow */
    if (strBuf[sizeof(strBuf)-1] != 0x11)
    {
	ConsolePrintf("ERROR in ItemDataString(%s): strBuf Overflow!\n",
		      pItem->pDesc);
	return NULL;		// handle similar as read error
    }

    return strBuf;
}


/***************************************************************************//**
 *
 * @brief	Display Text
 *
 * This routine allows you to display text on the LCD.  If the LCD is off,
 * it will be powered-on.  To automatically switch it off after a specified
 * duration, or to display another text after this time, DisplayNext() can
 * be used.
 *
 * @param[in] lineNum
 *	The line number where to display the text.  Must be 1 or 2.
 *
 * @param[in] frmt
 *	Format string of the text to print - same as for printf().
 *
 * @see
 * 	DisplayNext()
 *
 ******************************************************************************/
void	DisplayText (int lineNum, const char *frmt, ...)
{
LCD_FIELD_ID	id;
va_list		args;


    /* Parameter check */
    if (lineNum < 1  ||  lineNum > 2)
    {
	EFM_ASSERT(false);
	return;
    }

    /* Specify field to use */
    id = (lineNum == 1 ? LCD_LINE1_TEXT : LCD_LINE2_TEXT);

    /* Activate LCD */
    LCD_FIELD_ID_BIT_VAR(l_bitMaskFieldActive, id) = 1;
    LCD_FIELD_ID_BIT_VAR(l_bitMaskFieldUpd, id) = 1;
    DisplayUpdateCheck();

    /* Print Text */
    va_start (args, frmt);
    LCD_vPrintf (id, frmt, args);
    va_end (args);

    /* Cancel possible running power-off timer to ensure LCD remains ON */
    if (l_hdlLCD_Off != NONE)
	sTimerCancel (l_hdlLCD_Off);
}


/***************************************************************************//**
 *
 * @brief	Display Next
 *
 * This function defines what should happen next on the LC-Display.  It is
 * typically used after calling DisplayText() in one of the following ways:
 *
 * - @p duration is specified and @p fct is NULL: the LCD is switched off
 *   after the amount of time.
 *
 * - @p duration is specified and @p fct is not NULL: the callback function is
 *   executed after the amount of time.  This function may call DisplayText()
 *   and DisplayNext() again.  In this way it is possible to realize a
 *   "ticker" on the LCD.  For a generic approach a user parameter @p userParm
 *   is passed to the function which can be used to specify the item to be
 *   displayed.
 *
 * - @p duration is 0 and @p fct is NULL: the LCD is switched off immediately.
 *
 * - @p duration is 0 and @p fct is not NULL: the callback function will be
 *   executed as soon as possible.
 *
 * @param[in] duration
 * 	Duration in seconds, <b>after</b> which the specified action should
 * 	occur.  If a duration of 0 is specified, this happens immediately!
 *
 * @param[in] fct
 *	Callback function to be executed after @p duration.  If NULL is
 *	specified, no function will be called, instead the LC-Display is
 *	powered-off.
 *
 * @param[in] userParm
 *	User parameter for function @p fct.
 *
 * @note
 * 	Only one callback function can be installed at a dedicated time, i.e.
 * 	they cannot be stacked.  The function is called in standard context
 * 	by DisplayUpdateCheck() (not by an ISR), so there are no limitations.
 *
 ******************************************************************************/
void	DisplayNext (unsigned int duration, DISP_NEXT_FCT fct, int userParm)
{
    /* Be sure to reset the trigger flag */
    l_DispNextFctTrigger = false;

    /* Cancel possible running timer */
    if (l_hdlDispNext != NONE)
	sTimerCancel (l_hdlDispNext);

    /* Verify if LCD is active */
    if (! l_flgDisplayIsOn)
	return;			// LCD is already OFF, nothing to display

    /* Store function with argument */
    l_DispNextFct = fct;
    l_DispNextUserParm = userParm;

    /* Start timer for duration, or trigger function immediately */
    if (duration > 0)
    {
	if (l_hdlDispNext != NONE)
	    sTimerStart (l_hdlDispNext, duration);
    }
    else
    {
	l_DispNextFctTrigger = true;
	g_flgIRQ = true;	// keep on running
    }
}


/***************************************************************************//**
 *
 * @brief	Display Update for Clock
 *
 * This is a small helper routine to update the display whenever the system
 * time has changed, i.e. every second.  It is usually bound to the AlarmClock
 * module via DisplayUpdateFctInstall().
 *
 ******************************************************************************/
static void DisplayUpdateClock (void)
{
    if (l_flgDisplayIsOn)
    {
	LCD_FIELD_ID_BIT_VAR(l_bitMaskFieldUpd, LCD_CLOCK) = 1;
    }
}


/***************************************************************************//**
 *
 * @brief	Trigger a Display Update
 *
 * This routine must be called when a field needs to be updated, i.e. its
 * contents has changed, so the field must be re-displayed on the LCD.  If
 * the specified field ist currently not displayed on the LCD, the update
 * will be ignored.
 *
 * @param[in] fieldID
 *	The LCD field to be updated.
 *
 ******************************************************************************/
void DisplayUpdateTrigger (LCD_FIELD_ID fieldID)
{
    LCD_FIELD_ID_BIT_VAR(l_bitMaskFieldUpd, fieldID) = 1;

    g_flgIRQ = true;	// keep on running
}


/***************************************************************************//**
 *
 * @brief	Switch LCD Off
 *
 * This routine is called from the RTC interrupt handler to trigger the
 * power-off of the LC-Display, after @ref LCD_POWER_OFF_TIMEOUT seconds have
 * elapsed.
 *
 ******************************************************************************/
static void SwitchLCD_Off(TIM_HDL hdl)
{
    (void) hdl;		// suppress compiler warning "unused parameter"

    if (l_flgDisplayUpdEnabled)		// NOT in the very beginning
	l_bitMaskFieldActive = 0;

    g_flgIRQ = true;	// keep on running
}


/***************************************************************************//**
 *
 * @brief	Switch Device Off
 *
 * This routine is called from the RTC interrupt handler to trigger the
 * power-off of the whole device, after @ref POWER_OFF_TIMEOUT seconds are
 * over without any key assertion.
 *
 ******************************************************************************/
static void SwitchDeviceOff(TIM_HDL hdl)
{
    (void) hdl;		// suppress compiler warning "unused parameter"

    /* Set flag to initiate power-off */
    l_flgPowerOff = true;
}


/***************************************************************************//**
 *
 * @brief	Display Next Trigger
 *
 * This routine is called from the RTC interrupt handler to trigger a
 * @ref DISP_NEXT_FCT callback routine, after the specified amount of
 * time is over.  If no callback routine is installed, SwitchLCD_Off()
 * is called instead to switch the LCD off.
 *
 * @see
 * 	DisplayNext(), DisplayText()
 *
 ******************************************************************************/
static void DispNextTrigger(TIM_HDL hdl)
{
    (void) hdl;		// suppress compiler warning "unused parameter"

    /* See if callback routine has been specified */
    if (l_DispNextFct)
	l_DispNextFctTrigger = true;
    else
	SwitchLCD_Off((TIM_HDL)0);	// no further fct, switch LCD off

    g_flgIRQ = true;	// keep on running
}

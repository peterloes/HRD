/***************************************************************************//**
 * @file
 * @brief	Routines for LCD Module EA DOGM162
 * @author	Ralf Gerhauser
 * @version	2020-01-13
 *
 * This module contains the low-level, i.e. the DOGM162 specific part of the
 * display routine.  They are used by module Display.c, but should never be
 * called directly by user code.
 *
 ****************************************************************************//*
Revision History:
2020-02-13,rage	Use LCD_SetContrast() to set contrast before calling LCD_Init().
		LCD_vPrintf: Increased buffer sizes, wrap output on LCD if data
		string ist longer than the field width.
2016-11-22,rage	Implemented additional output to LEUART.
2016-04-05,rage	Made local variable <l_flgLCD_IsOn> of type "volatile".
2015-07-09,rage	IAR Compiler: Use vsprintf() instead vsiprintf().
2015-05-22,rage	Changed numeric defines to hexadecimal because the IAR compiler
		does not support binary constants.
2014-11-19,rage	Initial version.
*/

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "em_device.h"
#include "em_assert.h"
#include "em_gpio.h"
#include "AlarmClock.h"
#include "LCD_DOGM162.h"

/*=============================== Definitions ================================*/

    /*!@name Hardware Configuration: Power Enable for the LCD Module. */
//@{
#define LCD_POWER_PORT		gpioPortA	//!< Port for power control
#define LCD_POWER_PIN		8		//!< Power Pin: 0=OFF, 1=ON
//@}

    /*!@name Hardware Configuration: Control lines to the LCD Module. */
//@{
#define LCD_CTRL_PORT		gpioPortF	//!< Port for control lines
#define LCD_CTRL_PIN_E		3		//!< Data Enable signal
#define LCD_CTRL_PIN_RW		4		//!< Read/Write signal
#define LCD_CTRL_PIN_RS		5		//!< Register Select signal
//@}

    /*!@name Hardware Configuration: Data bus to the LCD Module. */
//@{
#define LCD_DATA_PORT		gpioPortE	//!< Port for the data bus
#define LCD_DATA_MODE_OUT	0x44444444	//!< 8x @ref gpioModePushPull
#define LCD_DATA_MODE_IN	0x11111111	//!< 8x @ref gpioModeInput
#define LCD_DATA_MASK		(0xFF << 8)	//!< Data bus uses bit 15:8
//@}

    /*!@brief Timeout for WaitCtrlReady() is 1ms */
#define LCD_WAIT_READY_TIMEOUT	(RTC_COUNTS_PER_SEC / 1000)

    /*!@name I/O Macros providing access to the LCD Module. */
//@{
    //! Set level of the LCD power enable pin.
#define SET_LCD_POWER_PIN(level)  IO_Bit(GPIO->P[LCD_POWER_PORT].DOUT,	\
					 LCD_POWER_PIN) = (level)
    //! Configure the data bus to the LCD module for input, i.e. reading data.
#define SET_LCD_DATA_MODE_IN  GPIO->P[LCD_DATA_PORT].MODEH = LCD_DATA_MODE_IN
    //! Configure the data bus to the LCD module for output, i.e. writing data.
#define SET_LCD_DATA_MODE_OUT GPIO->P[LCD_DATA_PORT].MODEH = LCD_DATA_MODE_OUT
    //! Read data from the data bus of the LCD module.
#define READ_LCD_DATA()       (GPIO->P[LCD_DATA_PORT].DIN >> 8)
    /*!@brief Write data to the LCD module.  Writing directly to the GPIO data
     * out register is possible since port <b>E</b> is exclusively used for the
     * data bus (bit 7 to 0 are not routed to any pin for QFP64).
     */
#define WRITE_LCD_DATA(data)  (GPIO->P[LCD_DATA_PORT].DOUT = (data) << 8)
    //! Set level of control line "E"
#define SET_LCD_CTRL_PIN_E(level)  IO_Bit(GPIO->P[LCD_CTRL_PORT].DOUT,	\
					  LCD_CTRL_PIN_E) = (level)
    //! Set level of control line "RW"
#define SET_LCD_CTRL_PIN_RW(level) IO_Bit(GPIO->P[LCD_CTRL_PORT].DOUT,	\
					  LCD_CTRL_PIN_RW) = (level)
    //! Set level of control line "RS"
#define SET_LCD_CTRL_PIN_RS(level) IO_Bit(GPIO->P[LCD_CTRL_PORT].DOUT,	\
					  LCD_CTRL_PIN_RS) = (level)
//@}

    /*!@anchor commands @name Commands for the LCD Controller. */
//@{
#define LCD_CMD_CLEAR_DISPLAY	0x01	//!< Clear display, addr=0
#define LCD_CMD_RETURN_HOME	0x02	//!< Set addr=0, cursor home
#define LCD_CMD_ENTRY_MODE	0x04	//!< Cursor move direction right
#define LCD_CMD_ENTRY_MODE_ID	0x06	//!< Cursor move direction left
#define LCD_CMD_ENTRY_MODE_S	0x05	//!< Shift display left
#define LCD_CMD_DISPLAY_OFF	0x08	//!< Switch display OFF
#define LCD_CMD_DISPLAY_ON_D	0x0C	//!< Entire display ON
#define LCD_CMD_DISPLAY_ON_C	0x0A	//!< Cursor ON
#define LCD_CMD_DISPLAY_ON_B	0X09	//!< Cursor blinking ON
#define LCD_CMD_FCT_SET		0x20	//!< 4bit, 1 line, Instr-Tab 00
#define LCD_CMD_FCT_SET_DL	0X30	//!< Interface data is 8 bit
#define LCD_CMD_FCT_SET_N	0x28	//!< Select 2 lines
#define LCD_CMD_FCT_SET_DH	0x24	//!< Double Height
#define LCD_CMD_FCT_SET_IS2	0x22	//!< Instruction-Table 10
#define LCD_CMD_FCT_SET_IS1	0x21	//!< Instruction-Table 01
#define LCD_CMD_FCT_SET_IS0	0x20	//!< Instruction-Table 00
#define LCD_CMD_SET_DDRAM_ADDR	0x80	//!< Set DDRAM address
#define LCD_CMD_IS0_CD_SHIFT	0x10	//!< Set Cursor Shift (left)
#define LCD_CMD_IS0_CD_SHIFT_SC	0x18	//!< Set Display Shift (left)
#define LCD_CMD_IS0_CD_SHIFT_RL	0x14	//!< Cursor/Display Shift right
#define LCD_CMD_IS0_SET_CGRAM	0x40	//!< Set CGRAM address AC5:0
#define LCD_CMD_IS1_BIAS_SET	0x14	//!< BS=0: 1/5 bias
#define LCD_CMD_IS1_BIAS_SET_BL	0x1C	//!< BS=1: 1/4 bias
#define LCD_CMD_IS1_ICON_ADDR	0x40	//!< Set ICON address AC3:0
#define LCD_CMD_IS1_IBC		0x50	//!< ICON+Boost OFF, Contrast 0
#define LCD_CMD_IS1_IBC_ION	0x58	//!< ICON display ON
#define LCD_CMD_IS1_IBC_BON	0x54	//!< Set booster curcuit ON
#define LCD_CMD_IS1_IBC_C5	0x52	//!< Contrast bit 5
#define LCD_CMD_IS1_IBC_C4	0x51	//!< Contrast bit 4
#define LCD_CMD_IS1_CONTR	0x70	//!< Contrast
#define LCD_CMD_IS1_CONTR_C3	0x78	//!< Contrast bit 3
#define LCD_CMD_IS1_CONTR_C2	0x74	//!< Contrast bit 2
#define LCD_CMD_IS1_CONTR_C1	0x72	//!< Contrast bit 1
#define LCD_CMD_IS1_CONTR_C0	0x71	//!< Contrast bit 0
#define LCD_CMD_IS1_FOLLOW	0x60	//!< Follower Control (all 0)
#define LCD_CMD_IS1_FOLLOW_FON	0x68	//!< Follower Ctrl: FON=1
#define LCD_CMD_IS1_FOLLOW_RAB2	0x64	//!< Follower Ampl. Ratio: RAB2
#define LCD_CMD_IS1_FOLLOW_RAB1	0x62	//!< Follower Ampl. Ratio: RAB1
#define LCD_CMD_IS1_FOLLOW_RAB0	0x61	//!< Follower Ampl. Ratio: RAB0
#define LCD_CMD_IS2_DBL_HP	0x10	//!< Double Height Position UD=0
#define LCD_CMD_IS2_DBL_HP_UD	0x18	//!< Double Height Position UD=1
//@}

/*================================ Local Data ================================*/

    /*!@brief Pointer to module configuration. */
static const LCD_FIELD *l_pField;

    /*!@brief LCD Contrast value (0 to 63), see LCD_SetContrast(). */
static volatile int l_Contrast = 30;

    /*!@brief Flag if LCD is on. */
static volatile bool l_flgLCD_IsOn;

/*=========================== Forward Declarations ===========================*/

static uint8_t BusyRead (void);
static bool WaitCtrlReady (void);
static void CmdWrite (uint8_t cmd);
// static uint8_t DataRead (void);
static void DataWrite (uint8_t data);


/***************************************************************************//**
 *
 * @brief	Initialize LCD
 *
 * This routine initializes the board-specific hardware (GPIOs) and the LCD
 * controller itself.
 *
 * @param[in] pField
 *	Address of an initialization structure of type @ref LCD_FIELD that
 *	points to a list of fields, referenced by an @ref LCD_FIELD_ID index.
 *
 * @note
 *	Parameter <b>pField</b> must point to a persistent data structure,
 *	i.e. this must be valid over the whole life time of the program.
 *
 ******************************************************************************/
void LCD_Init (const LCD_FIELD *pField)
{
    /* Parameter check */
    EFM_ASSERT(pField != NULL);

    /* Save configuration */
    l_pField = pField;

    /* Power the LCD Module On and initialize it */
    LCD_PowerOn();
}


/***************************************************************************//**
 *
 * @brief	Set LCD Contrast
 *
 * Store the contrast value in the local variable @ref l_Contrast.  This must
 * happen before initializing the LCD!
 *
 ******************************************************************************/
void LCD_SetContrast (int contrast)
{
    if (contrast < 20)
	contrast = 20;
    else if (contrast > 63)
	contrast = 63;

    l_Contrast = contrast;
}


/***************************************************************************//**
 *
 * @brief	Power LCD On
 *
 * This routine powers the LCD on and initializes the related hardware.
 *
 ******************************************************************************/
void LCD_PowerOn (void)
{
    /* Configure control lines E, RW, and RS as output */
    GPIO_PinModeSet (LCD_CTRL_PORT, LCD_CTRL_PIN_E, gpioModePushPull, 0);
    GPIO_PinModeSet (LCD_CTRL_PORT, LCD_CTRL_PIN_RW, gpioModePushPull, 0);
    GPIO_PinModeSet (LCD_CTRL_PORT, LCD_CTRL_PIN_RS, gpioModePushPull, 0);

    /* Set data bus to 0 for default */
    GPIO->P[LCD_DATA_PORT].DOUTCLR = LCD_DATA_MASK;

    /* Configure Power Enable Pin for LCD Module, switch it ON */
    GPIO_PinModeSet (LCD_POWER_PORT, LCD_POWER_PIN, gpioModePushPull, 1);

    /* Wait until LCD is powered up and ready */
    msDelay (100);

    /* Set 8bit data width, 2 lines, and instruction table 1 */
    CmdWrite (LCD_CMD_FCT_SET_DL|LCD_CMD_FCT_SET_N|LCD_CMD_FCT_SET_IS1);

    /* Instruction table 1: BIAS Set BS=0: 1/5 bias for a 2 line LCD */
    CmdWrite (LCD_CMD_IS1_BIAS_SET);

    /* Instruction table 1: booster ON, contrast bit C5:4 */
    CmdWrite (LCD_CMD_IS1_IBC_BON |(l_Contrast >> 4));

    /* Instruction table 1: Follower Ctrl FON=1, Amplifier Ratio = 5 */
    CmdWrite (LCD_CMD_IS1_FOLLOW_FON|LCD_CMD_IS1_FOLLOW_RAB2
				    |LCD_CMD_IS1_FOLLOW_RAB0);

    /* Set LCD Contrast bit C3:0 */
    CmdWrite (LCD_CMD_IS1_CONTR |(l_Contrast & 0x0F));

    /* Select instruction table 0 */
    CmdWrite (LCD_CMD_FCT_SET_DL|LCD_CMD_FCT_SET_N|LCD_CMD_FCT_SET_IS0);

    /* Switch display ON, cursor OFF and no blinking */
    CmdWrite (LCD_CMD_DISPLAY_ON_D);

    /* Clear display, set cursor home */
    CmdWrite (LCD_CMD_CLEAR_DISPLAY);

    /* Set cursor to autoincrement mode */
    CmdWrite (LCD_CMD_ENTRY_MODE_ID);

    /* LCD is now ON */
    l_flgLCD_IsOn = true;
}


/***************************************************************************//**
 *
 * @brief	Power LCD Off
 *
 * This routine powers the LCD off.
 *
 ******************************************************************************/
void LCD_PowerOff (void)
{
    /* LCD will be switched OFF */
    l_flgLCD_IsOn = false;

    /* Set LCD Power Enable Pin to OFF */
    SET_LCD_POWER_PIN(0);

    /*
     * Set all other signals also to GND, otherwise these will provide enough
     * power for the LCD to be still active!
     */
    SET_LCD_DATA_MODE_OUT;	// data bus output mode
    SET_LCD_CTRL_PIN_RW(0);	// set RW pin to 0
    SET_LCD_CTRL_PIN_RS(0);	// set RS pin to 0
    WRITE_LCD_DATA (0x00);	// set data bus to 0x00
    SET_LCD_CTRL_PIN_E (0);	// set E pin to 0
}


/***************************************************************************//**
 *
 * @brief	Print string to LCD and LEUART
 *
 * This routine is used to print text to the specified field on the LC-Display
 * and the LEUART.
 *
 * @note
 * LEUART support is realized in the following way: Since the LCD consists
 * of several independent fields on its two lines, which may be updated
 * separately at different times, it is not useful to write this information
 * directly to the LEUART.  Instead a copy of the complete LCD contents is held
 * in a memory buffer and updated synchronously to the LCD.  This buffer will
 * be written to the LEUART after dedicated fields have been updated (i.e.
 * LCD_ITEM_DATA and LCD_LINE2_TEXT), and, the content of the buffer has
 * changed.  As a special feature, if the length of LCD_LINE2_TEXT field is
 * longer than the LCD line, it will be output to the LEUART in complete length.
 *
 * @param[in] id
 *	Identifier of type @ref LCD_FIELD_ID to select a field on the LCD.
 *	The cursor is placed to the beginning of this field before text is
 *	written.
 *
 * @param[in] frmt
 *	Format string of the text to print - same as for printf().
 *
 ******************************************************************************/
void LCD_Printf (LCD_FIELD_ID id, const char *frmt, ...)
{
va_list	 args;


    va_start (args, frmt);
    LCD_vPrintf (id, frmt, args);
    va_end (args);
}


/***************************************************************************//**
 *
 * @brief	Print string with va_list to LCD and LEUART
 *
 * This routine is identical to LCD_Printf(), except parameter @p args is a
 * variable argument list.
 *
 * @param[in] id
 *	Identifier of type @ref LCD_FIELD_ID to select a field on the LCD.
 *	The cursor is placed to the beginning of this field before text is
 *	written.
 *
 * @param[in] frmt
 *	Format string of the text to print - same as for printf().
 *
 * @param[in] args
 *	Variable argument list.
 *
 ******************************************************************************/
void LCD_vPrintf (LCD_FIELD_ID id, const char *frmt, va_list args)
{
static int  strStart;
static char currSerBuf[130] = "                                ";
static char prevSerBuf[130];
char	 buffer[120];		// must be smaller than currSerBuf/prevSerBuf!
int	 len, fieldWidth;
char	*pField;


    /* Immediately return if LCD is OFF */
    if (! l_flgLCD_IsOn)
	return;

    /* Parameter check */
    if (id >= LCD_FIELD_ID_CNT)
    {
	ConsolePrintf("ERROR in LCD_vPrintf(%d): Invalid ID!\n", id);
	return;
    }
    if (strlen(frmt) > (sizeof(buffer) - 10))
    {
	ConsolePrintf("ERROR in LCD_vPrintf(%d): frmt is too long!\n", id);
	return;
    }

    vsprintf (buffer, frmt, args);

    /* Set LCD cursor to the beginning of the field */
    LCD_GotoXY (l_pField[id].X, l_pField[id].Y);

    /* Get field width and string length */
    fieldWidth = l_pField[id].Width;
    len = strlen (buffer);

    if (len > (int)(sizeof(buffer) - 2))
    {
	ConsolePrintf("ERROR in LCD_vPrintf(%d): buffer Overflow!\n", id);
	return;
    }

    /* If description is too long, truncate it and show error */
    if (l_pField[id].X == 0  &&  len > fieldWidth)
    {
	len = fieldWidth;		// limit to the maximum
	strcpy(buffer+fieldWidth-7, "!ERROR!");
    }

    /* If string is shorter than field width, add spaces */
    while (len < fieldWidth)
	buffer[len++] = ' ';

    /* First update the respective part of the serial output string */
    pField = currSerBuf+(l_pField[id].Y * (LCD_DIMENSION_X+1) + l_pField[id].X);
    strncpy(pField, buffer, len);

    /* Be sure to put a space between the two line fields */
    currSerBuf[LCD_DIMENSION_X] = ' ';

    /* At the end of the update sequence write string to LEUART */
    if (id == LCD_ITEM_DATA  ||  id == LCD_LINE2_TEXT)
    {
	/* Terminate data string for serial console output */
	pField[len] = EOS;

	/* Has output changed? */
	if (strcmp(currSerBuf, prevSerBuf) != 0)
	{
	    /* Yes, update serial buffer and write it to the LEUART */
	    strcpy(prevSerBuf, currSerBuf);

	    drvLEUART_puts(currSerBuf);
	    drvLEUART_puts("\n");

	    strStart = 0;
	}
	else
	{
	    /* No, data has NOT changed, wrap output on LCD if required */
	    if (len > fieldWidth)
	    {
		len -= strStart;
		strncpy(buffer, buffer+strStart, len);

		/* Update string start offset for the next call */
		if (len < fieldWidth - 3)
		    strStart = 0;	// re-start at beginning of the string
		else
		    strStart++;		// move offset one character further

		/* If string is shorter than field width, add spaces */
		while (len < fieldWidth)
		    buffer[len++] = ' ';
	    }
	}
    }

    /* If string is too long, truncate it */
    if (len > fieldWidth)
	len = fieldWidth;		// limit to the maximum

    /* Terminate string according to the field width on the LCD */
    buffer[len] = EOS;

    /* Write string to LCD */
    LCD_Puts (buffer);
}


/***************************************************************************//**
 *
 * @brief	Put string to LCD
 *
 * This routine puts the specified string to the LC-Display.
 *
 * @param[in] pStr
 *	String to output on the LCD at the actual cursor position.
 *
 ******************************************************************************/
void LCD_Puts (char *pStr)
{
    while (*pStr != EOS)
	LCD_Putc (*pStr++);
}


/***************************************************************************//**
 *
 * @brief	Put character to LCD
 *
 * This routine puts the specified character to the LC-Display.
 *
 * @param[in] c
 *	Character to output on the LCD at the actual cursor position.
 *
 ******************************************************************************/
void LCD_Putc (char c)
{
    /* Write character to LCD data bus */
    if (l_flgLCD_IsOn)
	DataWrite (c);
}


/***************************************************************************//**
 *
 * @brief	Move cursor on X-/Y-Position
 *
 * This routine moves the cursor the the specified position on the LC-Display.
 * Coordinates 0,0 represent the upper left corner of the display.
 *
 * @param[in] x
 *	X-Position to move cursor to.
 *
 * @param[in] y
 *	Y-Position to move cursor to.
 *
 ******************************************************************************/
void LCD_GotoXY (uint8_t x, uint8_t y)
{
uint8_t addr;


    /* Immediately return if LCD is OFF */
    if (! l_flgLCD_IsOn)
	return;

    EFM_ASSERT (x < LCD_DIMENSION_X  &&  y < LCD_DIMENSION_Y);

    addr = (y * 0x40) + x;

    CmdWrite (LCD_CMD_SET_DDRAM_ADDR | addr);
}


/***************************************************************************//**
 *
 * @brief	Read Busy Flag and Address
 *
 * This routine reads the busy flag and current value of the internal address
 * counter of the LC-Display.
 *
 * @return
 *	Current status: busy flag in bit [7] and address counter in bits [6:0].
 *
 ******************************************************************************/
static uint8_t BusyRead (void)
{
uint8_t status;

    SET_LCD_DATA_MODE_IN;	// input
    SET_LCD_CTRL_PIN_RW(1);	// read
    SET_LCD_CTRL_PIN_RS(0);	// register
    SET_LCD_CTRL_PIN_E (1);	// enable LCD output

    DelayTick();
    status = READ_LCD_DATA();	// read busy flag

    SET_LCD_CTRL_PIN_E (0);	// disable LCD output

    return status;
}


/***************************************************************************//**
 *
 * @brief	Wait until the LCD Controller is ready
 *
 * This routine reads the current status of the LCD controller and checks its
 * busy flag.  It waits as long as the busy flag is 1, i.e. the controller
 * is not ready to receive new commands or data due to internal activity, or
 * a timeout is reached.
 *
 * @return
 *	Status: false if LCD is ready now, true in case of timeout
 *
 ******************************************************************************/
static bool WaitCtrlReady (void)
{
int	i;

    for (i = 0;  i < LCD_WAIT_READY_TIMEOUT;  i++)
    {
	if ((BusyRead() & (1 << 7)) == false)
	    return false;

	DelayTick();	// delay for 30us
    }

    return true;	// timeout
}


/***************************************************************************//**
 *
 * @brief	Write Command to LCD Controller
 *
 * This routine waits until the LCD controller is ready and then writes
 * the specified command to it.  For a list of available commands, see
 * @ref commands "Commands for the LCD Controller".
 *
 * @param[in] cmd
 *	Command to write to the LCD controller.
 *
 ******************************************************************************/
static void CmdWrite (uint8_t cmd)
{
    /* Check if LCD controller is ready to receive a new command */
    if (WaitCtrlReady())
	return;			// timeout - abort

    SET_LCD_DATA_MODE_OUT;	// output
    SET_LCD_CTRL_PIN_RW(0);	// write
    SET_LCD_CTRL_PIN_RS(0);	// register

    WRITE_LCD_DATA (cmd);

    SET_LCD_CTRL_PIN_E (1);	// enable data valid
    DelayTick();
    SET_LCD_CTRL_PIN_E (0);	// disable data valid
}


#if 0
/***************************************************************************//**
 *
 * @brief	Read Data from LCD Memory
 *
 * This routine reads the data byte, i.e. character code, from the current
 * address of the internal memory of the LCD controller.  Use the
 * command @ref LCD_CMD_SET_DDRAM_ADDR to change the value of the internal
 * address pointer.
 *
 * @return
 *	The byte (character) as read from the internal memory.
 *
 ******************************************************************************/
static uint8_t DataRead (void)
{
uint8_t data;

    SET_LCD_DATA_MODE_IN;	// input
    SET_LCD_CTRL_PIN_RW(1);	// read
    SET_LCD_CTRL_PIN_RS(1);	// data bus
    SET_LCD_CTRL_PIN_E (1);	// enable LCD output

    DelayTick();
    data = READ_LCD_DATA();	// read data bus

    SET_LCD_CTRL_PIN_E (0);	// disable LCD output

    return data;
}
#endif


/***************************************************************************//**
 *
 * @brief	Write Data to LCD Memory
 *
 * This routine writes the specified data byte, i.e. character code, to the
 * current address of the internal memory of the LCD controller.  Use the
 * command @ref LCD_CMD_SET_DDRAM_ADDR to change the value of the internal
 * address pointer.
 *
 * @param[in] data
 *	Data to write to the internal memory of the LCD controller.
 *
 ******************************************************************************/
static void DataWrite (uint8_t data)
{
    /* Check if LCD controller is ready to receive new data */
    if (WaitCtrlReady())
	return;			// timeout - abort

    SET_LCD_DATA_MODE_OUT;	// output
    SET_LCD_CTRL_PIN_RW(0);	// write
    SET_LCD_CTRL_PIN_RS(1);	// data bus

    WRITE_LCD_DATA (data);

    SET_LCD_CTRL_PIN_E (1);	// enable data valid
    DelayTick();
    SET_LCD_CTRL_PIN_E (0);	// disable data valid
}

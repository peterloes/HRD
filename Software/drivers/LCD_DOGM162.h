/***************************************************************************//**
 * @file
 * @brief	Header file for LCD_DOGM162.c
 * @author	Ralf Gerhauser
 * @version	2014-11-19
 *
 * This is the header file of module "LCD_DOGM162.c"
 *
 ****************************************************************************//*
Revision History:
2014-11-19,rage	Initial version.
*/
#ifndef __INC_LCD_DOGM162_h
#define __INC_LCD_DOGM162_h

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "Display.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================== Definitions ================================*/

    /*!@name LCD Dimensions. */
//@{
#define LCD_DIMENSION_X	16	//!< X dimension is 16 characters.
#define LCD_DIMENSION_Y  2	//!< Y dimension is 2 lines.
//@}

/*================================ Prototypes ================================*/

    /* Regular functions */
void LCD_Init (const LCD_FIELD *pField);
void LCD_PowerOn(void);
void LCD_PowerOff(void);
void LCD_Printf (LCD_FIELD_ID id, const char *frmt, ...);
void LCD_vPrintf(LCD_FIELD_ID id, const char *frmt, va_list args);
void LCD_Puts (char *pStr);
void LCD_Putc (char c);
void LCD_GotoXY (uint8_t x, uint8_t y);

#ifdef __cplusplus
}
#endif


#endif /* __INC_LCD_DOGM162_h */

/***************************************************************************//**
 * @file
 * @brief System clock for tickless calendar application note
 * @author Energy Micro AS
 * @version 2.05
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2013 Energy Micro AS, http://www.energymicro.com</b>
 *******************************************************************************
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 * 4. The source and compiled code may only be used on Energy Micro "EFM32"
 *    microcontrollers and "EFR4" radios.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: Energy Micro AS has no
 * obligation to support this Software. Energy Micro AS is providing the
 * Software "AS IS", with no express or implied warranties of any kind,
 * including, but not limited to, any implied warranties of merchantability
 * or fitness for any particular purpose or warranties against infringement
 * of any proprietary rights of a third party.
 *
 * Energy Micro AS will not be liable for any consequential, incidental, or
 * special damages, or any other relief, or for any claim by any third party,
 * arising from your use of this Software.
 *
 ****************************************************************************//*
Revision History:
2015-06-10,rage	Be sure to set tm_isdst to 0 before calling mktime().
		Added __getzone() for the IAR version of mktime().
2014-04-10,rage	Made rtcStartTime global, renamed to g_rtcStartTime.
2014-04-06,rage	BUGFIX in time(): If an overflow interrupt occurs between
		reading <rtcOverflowCounter> and RTC->CNT, the 24bit counter
		has wrapped around and the calculated time value is 512s in
		the past!  To prevent this wrong behaviour, the OF bit in the
		IEN is cleared while calculating the current time and is
		enabled again after.
*/

/* Include standard libraries */
#include <time.h>
#include <stddef.h>

/* Include emlib */
#include "em_device.h"
#include "em_bitband.h"
#include "em_rtc.h"

/* Include system clock*/
#include "clock.h"

/* RTC variables. Used for converting RTC counter to system time */
static uint16_t   rtcCountsPerSec       = 0;
time_t	          g_rtcStartTime        = 0;
static uint32_t   rtcOverflowCounter    = 0;
static uint32_t   rtcOverflowInterval   = 0;
static uint32_t   rtcOverflowIntervalR  = 0;



/******************************************************************************
 * @brief Returns the current system time
 *
 * @param timer
 *   If not a null pointer, time is copied to this
 *
 * @return
 *   Current system time. Should, but does not, return -1 if system time is not available
 *
 *****************************************************************************/
#if defined (__ICCARM__)
time_t __time32( time_t * timer )
#elif defined (__CC_ARM)
time_t time( time_t * timer )
#elif defined (__GNUC__)
time_t time( time_t * timer )
#else
#error Undefined toolkit, need to define alignment
#endif
{
  time_t t;

  /* Add the time offset */
  t = g_rtcStartTime;

  /* RAGE: Disable overflow interrupt */
  BITBAND_Peripheral (&(RTC->IEN), _RTC_IEN_OF_SHIFT, 0);

  /* Add time based on number of counter overflows*/
  t += rtcOverflowCounter * rtcOverflowInterval;

  /* Add remainder if the overflow interval is not an integer */
  if ( rtcOverflowIntervalR != 0 )
  {
    t += (rtcOverflowCounter * rtcOverflowIntervalR) / rtcCountsPerSec;
  }

  /* Add the number of seconds for RTC */
  t += ( RTC->CNT / rtcCountsPerSec );

  /* RAGE: Enable overflow interrupt again */
  BITBAND_Peripheral (&(RTC->IEN), _RTC_IEN_OF_SHIFT, 1);

  /* Copy system time to timer if not NULL*/
  if ( timer != NULL )
  {
    *timer = t;
  }

  return t;
}



/***************************************************************************//**
 * @brief Initialize system CLOCK
 *
 * @param[in] init
 *   CLOCK initialization structure with start date and RTC frequency.
 *
 ******************************************************************************/
void clockInit(Clock_Init_TypeDef *init)
{
  /* Store configuration variables */
  rtcCountsPerSec = init->rtcCountsPerSec;

  /* Reset overflow counter */
  rtcOverflowCounter = 0;

  /* Calculate overflow interval based on RTC counter width and frequency */
  rtcOverflowInterval   = ((0x00FFFFFF+1) / rtcCountsPerSec);
  rtcOverflowIntervalR  = ((0x00FFFFFF+1) % rtcCountsPerSec); /* remainder */

  /* Set epoch offset */
  clockSetStartCalendar(&init->startDate);
}



/***************************************************************************//**
 * @brief Set the epoch offset
 *
 * @param[in] timeptr
 *   Calendar struct which is converted to unix time and used as new epoch
 *   offset
 *
 ******************************************************************************/
void clockSetStartCalendar(struct tm * timeptr)
{
  timeptr->tm_isdst = 0;		// always 0 for mktime()
  g_rtcStartTime = mktime(timeptr);
}



/***************************************************************************//**
 * @brief Set the epoch offset
 *
 * @param[in] offset
 *   Unix time when the counter was started
 *
 ******************************************************************************/
void clockSetStartTime(time_t offset)
{
  g_rtcStartTime = offset;
}



/***************************************************************************//**
 * @brief Get the epoch offset
 *
 * @return
 *   Unix time when the counter was started
 *
 ******************************************************************************/
time_t clockGetStartTime(void)
{
  return g_rtcStartTime;
}






/***************************************************************************//**
 * @brief Call this function on counter overflow to let CLOCK know how many
 *        overflows has occurred since start time
 *
 ******************************************************************************/
uint32_t clockOverflow(void)
{
  rtcOverflowCounter++;
  return rtcOverflowCounter;
}



/***************************************************************************//**
 * @brief Initialize the overflow counter.
 *
 ******************************************************************************/
void clockSetOverflowCounter(uint32_t of)
{
  rtcOverflowCounter = of;
}



/***************************************************************************//**
 * @brief Get current value of the overflow counter.
 *
 ******************************************************************************/
uint32_t clockGetOverflowCounter(void)
{
  return rtcOverflowCounter;
}



#if defined (__ICCARM__)
// IAR version of mktime() calls __getzone() which should always return UTC
const char *__getzone(void)
{
    return ":GMT+0:GMT+0:0000:0";	// UTC
}
#endif

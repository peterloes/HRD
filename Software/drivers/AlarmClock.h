/***************************************************************************//**
 * @file
 * @brief	Header file of module AlarmClock.c
 * @author	Ralf Gerhauser
 * @version	2016-09-14
 ****************************************************************************//*
Revision History:
2016-09-14,rage	Added prototype for ClockGetMilliSec().
2016-04-05,rage	Made variable <g_isdst> of type "volatile".
		Added variable <g_PowerUpTime>.
2014-11-11,rage	Added prototype for msDelay() and DelayTick().
2014-05-10,rage	Added macro MS2TICS() to convert milliseconds to tics.
2014-04-25,rage	Implemented workaround for Year 2038 problem.
2014-04-07,rage	Initial version.
*/

#ifndef __INC_AlarmClock_h
#define __INC_AlarmClock_h

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include <stdbool.h>
#include "em_device.h"
#include "em_rtc.h"
#include "config.h"		// include project configuration parameters
#include "clock.h"

/*=============================== Definitions ================================*/

#ifndef MAX_SEC_TIMERS
    /*!@brief Maximum number of sTimer entries */
    #define MAX_SEC_TIMERS	10
#endif

#ifndef MAX_ALARMS
    /*!@brief Maximum number of alarms */
    #define MAX_ALARMS		10
#endif

#ifndef RTC_COUNTS_PER_SEC
    /*!@brief RTC frequency in [Hz] */
    #define RTC_COUNTS_PER_SEC	32768
#endif

    /*!@brief Macro to convert milliseconds to RTC tics. */
#define MS2TICS(ms)	((ms) * RTC_COUNTS_PER_SEC / 1000)

    /*!@brief Workaround for Y2K38 problem
     *
     * If the define Y2K38_WORKAROUND is 1, a workaround for the Year 2038
     * problem will be activated.  The original UNIX time_t variable uses a
     * <b>signed</b> 32bit type for counting the seconds since 1970-01-01.
     * On 19. January 2038 this variable will become negative and the resulting
     * date is 1901-12-13.
     * @see https://de.wikipedia.org/wiki/Jahr-2038-Problem.
     *
     * This workaround shifts the whole century from 2014 to 1914.  Leap years
     * are handled correctly till 2100, however the week days are calculated
     * incorrectly.  Therefore the macro Y2K38_WDAY() is provided, which puts
     * an offset of 6 days modulo 7 to the week day value.  Always use this
     * macro when reading element <b>tm_wday</b> from a <i>tm</i> structure!
     * It is also recommended to use a modulo 100 operation when accessing
     * element <b>tm_year</b> to hide the internal value in use.
     */
#define Y2K38_WORKAROUND	1

#if Y2K38_WORKAROUND
  #define Y2K38_WDAY(wday)	(((wday) + 6) % 7)
#else
  #define Y2K38_WDAY(wday)	(wday)
#endif

/*=========================== Typedefs and Structs ===========================*/

/*!@brief Data type for timer handle */
typedef int	TIM_HDL;

/*!@brief Function to be called when a timer has been counted down to 0.
 *
 * The <b>hdl</b> argument can be used to identify the timer, if the same
 * function should be used for more than one timer.
 */
typedef void	(* TIMER_FCT)(TIM_HDL hdl);

/*!@brief Function to be called when alarm time has been reached.
 *
 * The function argument <b>alarmNum</b> can be used to identify the alarm,
 * if the same function is used for several alarms.
 */
typedef void	(* ALARM_FCT)(int alarmNum);

/*================================ Global Data ===============================*/

extern struct tm  	g_CurrDateTime;	//!< Current date and time structure
extern volatile bool	g_isdst;	//!< Flag for "daylight saving time"
extern volatile time_t	g_PowerUpTime;	//!< Power-Up Time as UNIX time

/*================================ Prototypes ================================*/

    /* Initialization of the Alarm Clock module */
void	AlarmClockInit (void);

    /* Alarm handling functions */
void	AlarmAction (int alarmNum, ALARM_FCT function);
void	AlarmSet    (int alarmNum, int8_t hour, int8_t min);
void	AlarmGet    (int alarmNum, int8_t *pHourVar, int8_t *pMinVar);
bool	AlarmIsEnabled (int alarmNum);
void	AlarmEnable (int alarmNum);
void	AlarmDisable(int alarmNum);

    /* sTimer handling functions (1 second granularity) */
TIM_HDL	sTimerCreate(TIMER_FCT function);
void	sTimerDelete(TIM_HDL hdl);
void	sTimerStart (TIM_HDL hdl, uint32_t seconds);
void	sTimerCancel(TIM_HDL hdl);

    /* msTimer handling functions (high-resolution timer) */
void	msTimerAction(void (*function)(void));
void	msTimerStart (uint32_t ms);
void	msTimerCancel(void);
void	msDelay (uint32_t ms);
void	DelayTick (void);

    /* System Clock functions */
void	DisplayUpdateFctInstall	(void (*function)(void));
void	ClockUpdate (bool readTime);
void	ClockGet (struct tm *pTimeDateVar);
void	ClockGetMilliSec (struct tm *pTimeDateVar, unsigned int *pMsVar);
void	ClockSet (struct tm *pNewTimeDate, bool sync);


#endif /* __INC_AlarmClock_h */

/***************************************************************************//**
 * @file
 * @brief	Alarm Clock Module
 * @author	Ralf Gerhauser
 * @version	2016-09-27
 *
 * This module implements an Alarm Clock.  It uses the Real Time Counter (RTC)
 * for this purpose.  The main features are:
 * - Base clock (1 second) for counting date and time.
 * - Up to 10 software timers with callback functionality and a granularity
 *   of one second.
 * - One high-resolution timer for short time measurements, e.g. timeout or
 *   autorepeat features for keys (push buttons).
 * - Up to 10 alarm times with callback functionality and a granularity of
 *   one minute (repeated after 24h).
 *
 * @note
 * The index for specifying a dedicated alarm time (i.e. the <b>alarmNum</b>
 * parameter) is an integer value between 0 and (MAX_ALARMS - 1).  It is
 * recommended to define <b>enums</b> for easier referencing of alarms.
 * Since this depends on the usage, it is outside of the scope of this module.
 *
 * <b>Example:</b>
   @code
   typedef enum
   {
       ALARM_PWR1_ON,	    // enable Power 1 output
       ALARM_PWR1_OFF,	    // disable Power 1 output
       ...
   } ALARM_ID;
   @endcode
 *
 ****************************************************************************//*
Revision History:
2016-09-27,rage	Use INT_En/Disable() instead of __en/disable_irq().
2016-09-27,rage	Added ClockGetMilliSec().
2016-04-05,rage	Made all local and global variables of type "volatile".
2015-06-10,rage	ClockSet: Be sure to set tm_isdst to 0 before calling mktime().
2015-03-30,rage	Added <g_PowerUpTime>, write log on initial time sync.
2014-11-11,rage	Added msDelay() and DelayTick().
2014-05-15,rage	Added parameter <sync> to function ClockSet().
		Added parameter <readTime> to function ClockUpdate().
		Added flag <g_isdst> for "daylight saving time" (MESZ) because
		element <tm_isdst> of <g_CurrDateTime> gets overwritten!
2014-05-12,rage	Extended alarm time logic to be able to define alarms that
		occur every hour.  See AlarmSet() and
2014-05-08,rage	Use localtime() instead of localtime_r() because this does not
		exist in the IAR library!
2014-04-07,rage	Initial version.
*/

/*=============================== Header Files ===============================*/

#include <stdio.h>
#include "em_device.h"
#include "em_assert.h"
#include "em_bitband.h"
#include "em_int.h"
#include "AlarmClock.h"

/*=============================== Definitions ================================*/

/*!@brief Calculate maximum value to prevent overflow of a 32bit register. */
#define MAX_VALUE_FOR_32BIT	(0xFFFFFFFFUL / RTC_COUNTS_PER_SEC)

/*=========================== Typedefs and Structs ===========================*/

/*!@brief Alarm entry.
 *
 * Structure to keep an alarm time and the function to call when the alarm
 * time is reached.  The <b>Enable</b> flag allows you to disable an alarm
 * without changing other parameters.
 */
typedef struct
{
    bool	Enabled;	//!< TRUE: enabled, FALSE: disabled
    int8_t	Hour;		//!< Alarm time: Hour (NONE for every hour)
    int8_t	Minute;		//!< Alarm time: Minute
    ALARM_FCT	Function;	//!< Function to be called for this alarm
} ALARM;

/*!
 * @brief Structure for a one-second timer.
 */
typedef struct
{
    uint32_t  Counter;		//!< Time counter (number of remaining seconds).
    TIMER_FCT Function;		//!< Function to be called when timer expires
} SEC_TIMER;

/*================================ Global Data ===============================*/

/*!@brief Current date and time structure. */
struct tm	 g_CurrDateTime;

/*!
 * @brief Separate flag for "<i>daylight saving time</i>" (MESZ) because
 * element <b>tm_isdst</b> of @ref g_CurrDateTime gets overwritten!
 */
volatile bool	 g_isdst;

/*!@brief Power-Up Time as UNIX time.
 * This global variable contains the power-up time.  It can be referred
 * to calculate the "up-time" of the system, or report the number of lost
 * log messages since power-up.  The (initial) value 0 means, there has
 * been no DCF77 synchronization yet.
 */
volatile time_t	 g_PowerUpTime;

/*================================ Local Data ================================*/

/*!@brief List of alarm times. */
static volatile ALARM l_Alarm[MAX_ALARMS];

/*!@brief List of one-second timers. */
static volatile SEC_TIMER l_sTimer[MAX_SEC_TIMERS];

/*!@brief Maximum handle, currently in use. */
static volatile int   l_MaxHdl;

/*!@brief Function to call when high-resolution timer expires. */
static void  (*l_msTimerFunction) (void);

/*!@brief Function to call for a display update. */
static void  (*l_DisplayUpdateFct) (void);


/***************************************************************************//**
 *
 * @brief	Initialization of the Alarm Clock
 *
 * This routine must be called once to initialize this module.
 *
 * @note
 * The Low Frequency (LF) clock and RTC prescaler must be configured before
 * calling this function.
 *
 ******************************************************************************/
void	AlarmClockInit (void)
{
    /* Initialize the POSIX time() module (see clock.c) */
    Clock_Init_TypeDef initialCalendar = CLOCK_INIT_DEFAULT;
#if Y2K38_WORKAROUND
    /* Year 2038 Workaround: shift 2014 to 1914 */
    initialCalendar.startDate.tm_year = 14;
#endif
    initialCalendar.rtcCountsPerSec = RTC_COUNTS_PER_SEC;
    clockInit (&initialCalendar);

    /* Configure the RTC */
    RTC_Init_TypeDef rtcInit;
    rtcInit.debugRun = false;
    rtcInit.comp0Top = false;
    rtcInit.enable = false;

    /* Initialize the RTC */
    RTC_Init (&rtcInit);

    /*
     * We use all 3 interrupts:
     *   Overflow - to count above 24bit
     *   COMP0    - for the 1s base clock and the software timers
     *   COMP1    - for the high-resolution timer (will be enabled on request)
     */
    RTC_IntEnable (RTC_IEN_COMP0 | RTC_IEN_OF);

    /* Enable RTC */
    RTC_Enable (true);

    /* Enable RTC interrupts */
    NVIC_ClearPendingIRQ(RTC_IRQn);
    NVIC_EnableIRQ(RTC_IRQn);
}

/***************************************************************************//**
 *
 * @brief	Handler for RTC Interrupt
 *
 * This is the handler for the Real Time Counter (RTC) interrupt.  There are 3
 * sources for this interrupt:
 * - The <b>Overflow Interrupt</b> occurs when the 24bit counter wraps around,
 *   so a variable needs to be incremented that holds the higher bits.
 *   With a clock frequency of 32.768Hz this happens every 512s (8.5min).
 * - <b>COMP0</b> is used for the 1s base clock and the software timers, and
 *   every minute all alarm times are compared to the current time.
 * - <b>COMP1</b> is used as a high-resolution timer, see @ref msTimerStart().
 *   It may be used as autorepeat timer for the keys, for example.
 *
 ******************************************************************************/
void	RTC_IRQHandler (void)
{
static int8_t	processed_min = (-1);	// already processed minute
uint32_t	status;			// interrupt status flags
int		i;			// index variable

    /*
     * Measured execution times:
     * - 130us for COMP0 interrupt (1s) without sTimer and alarms.
     * - 150us for COMP0 interrupt (1s) without sTimer, but checking
     *   all MAX_ALARMS (no execution of any alarm functions).
     */

    /* get interrupt status and mask out disabled IRQs */
    status  = RTC->IF;
    status &= RTC->IEN;

    /* First of all check for OverFlow interrupt and increase high counter */
    if (status & RTC_IF_OF)
    {
	clockOverflow();		// see clock.c
	RTC->IFC = RTC_IFC_OF;
    }

    /* Check for COMP0 interrupt which occurs every second */
    if (status & RTC_IF_COMP0)
    {
	/* Generate next COMP0 interrupt after another second */
	RTC_CompareSet (0, (RTC->COMP0 + RTC_COUNTS_PER_SEC) & 0xFFFFFF);
	RTC->IFC = RTC_IFC_COMP0;

	/*
	 * Get current UNIX time, convert to <tm>, and store in global struct
	 * <g_CurrDateTime>.  This requires about 100us which is quite long
	 * for an interrupt routine.  Nevertheless we need this information
	 * for comparing the alarm times below.
	 */
	ClockUpdate (true);

	/* decrement software timers and call functions if reaching 0 */
	for (i = 0;  i <= l_MaxHdl;  i++)
	{
	    if (l_sTimer[i].Counter)
	    {
		/* only decrement if not already 0 */
		if (--(l_sTimer[i].Counter) == 0)
		{
		    /* if reaching 0, call the specified function */
		    if (l_sTimer[i].Function)
			l_sTimer[i].Function (i);
		}
	    }
	}

	/* compare all alarm times for every minute */
	if (processed_min != g_CurrDateTime.tm_min)
	{
	    processed_min = g_CurrDateTime.tm_min;

	    for (i = 0;  i < MAX_ALARMS;  i++)
	    {
		/* we compare hours and minutes only */
		if (l_Alarm[i].Enabled
		&&  l_Alarm[i].Minute == g_CurrDateTime.tm_min
		&&  (l_Alarm[i].Hour  == NONE	// repeat every hour
		  || l_Alarm[i].Hour  == g_CurrDateTime.tm_hour))
		{
		    /* reached alarm time, call the specified function */
		    if (l_Alarm[i].Function)
			l_Alarm[i].Function (i);
		}
	    }
	}
    }	// if (status & RTC_IF_COMP0)

    /* Check for COMP1 interrupt (high-resolution timer) */
    if (status & RTC_IF_COMP1)
    {
	/* disable and clear COMP1 interrupt */
	BITBAND_Peripheral (&(RTC->IEN), _RTC_IEN_COMP1_SHIFT, 0);
	RTC->IFC = RTC_IFC_COMP1;

	/* call the specified function */
	if (l_msTimerFunction)
	    l_msTimerFunction();
    }
}

/***************************************************************************//**
 *
 * @brief	Specify an Alarm Action
 *
 * Specify a function that should be called when the alarm time is reached.
 *
 * @param[in] alarmNum
 *	Number of the alarm to specify the action for.  This parameter can be
 *	any integer value between 0 and (MAX_ALARMS - 1).
 *
 * @param[in] function
 *	Function to be called when the alarm happens.
 *
 * @warning
 *	The specified function will be called in interrupt context, therefore
 *	care has to be taken not to execute non-interrupt save routines, or
 *	functions that need too much execution time.
 *
 ******************************************************************************/
void	AlarmAction (int alarmNum, ALARM_FCT function)
{
    /* Parameter check */
    EFM_ASSERT(0 <= alarmNum  &&  alarmNum < MAX_ALARMS  &&  function != NULL);

    /* Set function pointer */
    l_Alarm[alarmNum].Function = function;
}

/***************************************************************************//**
 *
 * @brief	Set Alarm Time
 *
 * Set alarm hour and minute.  The alarm action is called when the specified
 * hour and minute has been reached.  The alarm is repeated after 24 hours,
 * except if hour is set to @ref NONE, it is repeated every hour.
 *
 * @note
 * When an alarm is set for the first time, it is <b>disabled</b> per default,
 * so function AlarmEnable() must be called once.  This is not required when
 * changing the time of an already enabled alarm.
 *
 * @param[in] alarmNum
 *	Number of the alarm to specify the time for.  This parameter can be
 *	any integer value between 0 and (MAX_ALARMS - 1).
 *
 * @param[in] hour
 *	Alarm hour to be set, or @ref NONE if it should be repeated every hour.
 *
 * @param[in] min
 *	Alarm minute to be set.
 *
 ******************************************************************************/
void	AlarmSet    (int alarmNum, int8_t hour, int8_t min)
{
bool	orgState;	// original state of the alarm


    /* Parameter check */
    EFM_ASSERT (0 <= alarmNum  &&  alarmNum < MAX_ALARMS  &&
		NONE <= hour  &&  hour <= 23  &&  0 <= min  &&  min <= 59);

    /* Save current state, then disable this alarm */
    orgState = l_Alarm[alarmNum].Enabled;
    l_Alarm[alarmNum].Enabled = false;

    /* Set new alarm time */
    l_Alarm[alarmNum].Hour   = hour;
    l_Alarm[alarmNum].Minute = min;

    /* Restore original state */
    l_Alarm[alarmNum].Enabled = orgState;
}

/***************************************************************************//**
 *
 * @brief	Get Alarm Time
 *
 * Get current alarm hour and minute.
 *
 * @param[in] alarmNum
 *	Number of the alarm to get the values from.  This parameter can be
 *	any integer value between 0 and (MAX_ALARMS - 1).
 *
 * @param[in] pHourVar
 *	Pointer to a variable where to store the hour value.
 *
 * @param[in] pMinVar
 *	Pointer to a variable where to store the minute value.
 *
 ******************************************************************************/
void	AlarmGet    (int alarmNum, int8_t *pHourVar, int8_t *pMinVar)
{
    /* Parameter check */
    EFM_ASSERT (0 <= alarmNum  &&  alarmNum < MAX_ALARMS  &&
		pHourVar != NULL  &&  pMinVar != NULL);

    /* Get current alarm time */
    *pHourVar = l_Alarm[alarmNum].Hour;
    *pMinVar  = l_Alarm[alarmNum].Minute;
}

/***************************************************************************//**
 *
 * @brief	Test if Alarm is Enabled
 *
 * This routine returns <b>true</b> if the specified alarm is enabled.
 *
 * @param[in] alarmNum
 *	Number of the alarm to get the state from.  This parameter can be
 *	any integer value between 0 and (MAX_ALARMS - 1).
 *
 * @return
 *	State: true if alarm is enabled, false if disabled.
 *
 ******************************************************************************/
bool	AlarmIsEnabled (int alarmNum)
{
    /* Parameter check */
    EFM_ASSERT (0 <= alarmNum  &&  alarmNum < MAX_ALARMS);

    /* Return current Enabled state of this alarm */
    return l_Alarm[alarmNum].Enabled;
}

/***************************************************************************//**
 *
 * @brief	Enable Alarm
 *
 * Enable the specified alarm.
 *
 * @param[in] alarmNum
 *	Number of the alarm to enable.  This parameter can be
 *	any integer value between 0 and (MAX_ALARMS - 1).
 *
 * @see AlarmDisable().
 *
 ******************************************************************************/
void	AlarmEnable (int alarmNum)
{
    /* Parameter check */
    EFM_ASSERT (0 <= alarmNum  &&  alarmNum < MAX_ALARMS);

    /* Only enable alarms where an action was defined */
    EFM_ASSERT (l_Alarm[alarmNum].Function != NULL);

    /* Set enable flag */
    l_Alarm[alarmNum].Enabled = true;
}

/***************************************************************************//**
 *
 * @brief	Disable Alarm
 *
 * Disable the specified alarm.
 *
 * @param[in] alarmNum
 *	Number of the alarm to disable.  This parameter can be
 *	any integer value between 0 and (MAX_ALARMS - 1).
 *
 * @see AlarmEnable().
 *
 ******************************************************************************/
void	AlarmDisable (int alarmNum)
{
    /* Parameter check */
    EFM_ASSERT (0 <= alarmNum  &&  alarmNum < MAX_ALARMS);

    /* Clear enable flag */
    l_Alarm[alarmNum].Enabled = false;
}

/***************************************************************************//**
 *
 * @brief	Create a new 1-s Timer
 *
 * Create a new one-second timer, i.e. a timer with a granularity of 1s.  The
 * routine returns a reference handle for the new timer.  After the timer has
 * been created, sTimerStart() must be used to specify the number of seconds
 * the timer should run until <b>function</b> is called.
 *
 * @param[in] function
 *	Function to be called when the timer expires.
 *
 * @return
 *	Handle for the newly created timer.
 *
 * @see sTimerDelete().
 *
 ******************************************************************************/
TIM_HDL	sTimerCreate   (TIMER_FCT function)
{
int	i;	// index variable


    /* Parameter check */
    EFM_ASSERT (function != NULL);

    /* Search the next available entry in the list */
    for (i = 0;  i <= l_MaxHdl;  i++)
    {
	/* see if this entry is free */
	if (l_sTimer[i].Function == NULL)
	{
	    /* yes, allocate it and return handle */
	    l_sTimer[i].Counter  = 0;
	    l_sTimer[i].Function = function;
	    return i;	// return handle for the newly created timer
	}
    }

    /* no free entry found - try to extend the current handle count */
    EFM_ASSERT (l_MaxHdl < (MAX_SEC_TIMERS - 1));

    /* increase the current handle count and allocate the new entry */
    i = l_MaxHdl + 1;
    l_sTimer[i].Counter  = 0;
    l_sTimer[i].Function = function;

    l_MaxHdl = i;

    return i;
}

/***************************************************************************//**
 *
 * @brief	Delete 1-s Timer
 *
 * This routine deletes the specified one-second timer.
 *
 * @param[in] hdl
 *	Handle to specify the timer.
 *
 * @see sTimerCreate().
 *
 ******************************************************************************/
void	sTimerDelete (TIM_HDL hdl)
{
    /* Parameter check */
    EFM_ASSERT (0 <= hdl  &&  hdl <= l_MaxHdl);

    /* De-allocate the specified entry */
    l_sTimer[hdl].Counter  = 0;
    l_sTimer[hdl].Function = NULL;
}

/***************************************************************************//**
 *
 * @brief	Start 1-s Timer
 *
 * This routine allows you to specify the number of seconds the timer should
 * run, and starts it.  When the timer expires, the function that was introduced
 * by sTimerCreate(), will be called.  Use sTimerCancel() to abort this duration.
 *
 * @param[in] hdl
 *	Handle to specify the timer.
 *
 * @param[in] seconds
 *	Duration in seconds how long the timer should run.
 *
 * @see sTimerCancel().
 *
 ******************************************************************************/
void	sTimerStart (TIM_HDL hdl, uint32_t seconds)
{
    /* Parameter check */
    EFM_ASSERT (0 <= hdl  &&  hdl <= l_MaxHdl  &&  0 < seconds);

    /* Check specified entry */
    EFM_ASSERT (l_sTimer[hdl].Function != NULL);

    /* Load counter */
    l_sTimer[hdl].Counter = seconds;
}

/***************************************************************************//**
 *
 * @brief	Cancel a running 1-s Timer
 *
 * Call this routine to cancel a running one-second timer, i.e. no action is
 * performed when the timer expires.
 *
 * @param[in] hdl
 *	Handle to specify the timer.
 *
 * @see sTimerStart().
 *
 ******************************************************************************/
void	sTimerCancel (TIM_HDL hdl)
{
    /* Parameter check */
    EFM_ASSERT (0 <= hdl  &&  hdl <= l_MaxHdl);

    /* Set the counter to 0 to disable further decrements */
    l_sTimer[hdl].Counter = 0;
}

/***************************************************************************//**
 *
 * @brief	Define an Action for the millisecond Timer
 *
 * Defines the function that should be called, when the high-resolution timer
 * expires.
 *
 * @param[in] function
 *	Function to be called when the timer expires.
 *
 ******************************************************************************/
void	msTimerAction	(void (*function)(void))
{
    /* Parameter check */
    EFM_ASSERT (function != NULL);

    /* Set function pointer */
    l_msTimerFunction = function;
}

/***************************************************************************//**
 *
 * @brief	Start the millisecond Timer
 *
 * This routine allows you to specify the number of milliseconds the
 * high-resolution timer should run, and starts it.  When the timer expires,
 * the function that was introduced by msTimerAction(), will be called.
 * Use msTimerCancel() to abort this duration.
 *
 * @param[in] ms
 *	Duration in milliseconds how long the timer should run.
 *
 * @see msTimerCancel().
 *
 ******************************************************************************/
void	msTimerStart (uint32_t ms)
{
    /* Parameter check */
    EFM_ASSERT (0 < ms  &&  ms <= MAX_VALUE_FOR_32BIT);

    /* Verify that a function has been defined for the timer */
    EFM_ASSERT (l_msTimerFunction != NULL);

    /* Convert the [ms] value in number of ticks and set COMP1 */
    RTC_CompareSet (1, (RTC->CNT + (ms * RTC_COUNTS_PER_SEC) / 1000) & 0xFFFFFF);

    /* Be sure to clear IRQ flag, then enable the COMP1 interrupt */
    RTC_IntClear (RTC_IFC_COMP1);
    BITBAND_Peripheral (&(RTC->IEN), _RTC_IEN_COMP1_SHIFT, 1);
}

/***************************************************************************//**
 *
 * @brief	Cancel the running millisecond Timer
 *
 * Call this routine to cancel the running millisecond timer, i.e. no action is
 * performed when the timer expires.
 *
 * @see msTimerStart().
 *
 ******************************************************************************/
void	msTimerCancel (void)
{
    /* Disable COMP1 interrupt */
    BITBAND_Peripheral (&(RTC->IEN), _RTC_IEN_COMP1_SHIFT, 0);
    RTC_IntClear (RTC_IFC_COMP1);
}

/***************************************************************************//**
 *
 * @brief	Delay for milliseconds
 *
 * This is a delay routine, it returns to the caller after the specified amount
 * of milliseconds has elapsed.
 *
 * @note
 * This routine permanently reads the Real Time Counter until the calculated
 * value has been reached.  No interrupts are used, i.e. the CPU is kept busy
 * all the time.  Usually delay routines are only used for hardware-related
 * timing constraints and must not be called from interrupt routines.
 *
 * @param[in] ms
 *	Duration in milliseconds to wait before returning to the caller.
 *
 ******************************************************************************/
void	msDelay (uint32_t ms)
{
uint32_t ovflCnt, currCnt, destCnt;

    /* Parameter check */
    EFM_ASSERT (0 < ms  &&  ms <= MAX_VALUE_FOR_32BIT);

    /* Disable interrupts */
    INT_Disable();

    /* Get current time counter values */
    currCnt = RTC->CNT;
    ovflCnt = clockGetOverflowCounter();

    /* Enable interrupts again */
    INT_Enable();

    /* Convert the [ms] value in number of ticks */
    destCnt = (currCnt + (ms * RTC_COUNTS_PER_SEC) / 1000) & 0xFFFFFF;

    /* Check if RTC will overflow before reaching destination time */
    if (destCnt > currCnt)
    {
	/* Value remains within current 24bit range */
	while ((RTC->CNT < destCnt)  &&  (clockGetOverflowCounter() == ovflCnt))
	    ;
    }
    else
    {
	/* RTC must wrap-around before reaching destination time */
	while (clockGetOverflowCounter() == ovflCnt)
	    ;

	while (RTC->CNT < destCnt)
	    ;
    }
}

/***************************************************************************//**
 *
 * @brief	Delay for one tick
 *
 * This is a delay routine for very short durations, it returns to the caller
 * after one tick, i.e. with a 32kHz XTAL about 30 microseconds (may be up to
 * 59 microseconds).
 *
 ******************************************************************************/
void	DelayTick (void)
{
uint32_t currCnt;

    /* Get current time counter value */
    currCnt = RTC->CNT;

    /* First synchronize with next counter change */
    while (RTC->CNT == currCnt)
	;

    /* Wait another tick change */
    currCnt = RTC->CNT;
    while (RTC->CNT == currCnt)
	;
}

/***************************************************************************//**
 *
 * @brief	Install a Display Update function
 *
 * Installs a function that will be called from ClockUpdate() to update the
 * display with an updated time or date value.
 *
 * @param[in] function
 *	Function to be called when the timer expires.
 *
 * @see ClockUpdate().
 *
 ******************************************************************************/
void	DisplayUpdateFctInstall	(void (*function)(void))
{
    /* Parameter check */
    EFM_ASSERT (function != NULL);

    /* Set function pointer */
    l_DisplayUpdateFct = function;
}

/***************************************************************************//**
 *
 * @brief	Update Clock with current date and time
 *
 * This routine updates the global system clock structure @ref g_CurrDateTime
 * with the current date and time.  Usually it is called from the RTC interrupt
 * handler only.  If a display update routine has been installed via
 * DisplayUpdateFctInstall(), this will be called after the clock has been
 * updated.
 *
 * @param[in] readTime
 *	Set <b>true</b> to read the current system time, convert it into a
 *	<i>tm</i> structure, and store it into the @ref g_CurrDateTime
 *	variable.  This is the normal case.<br>
 *	If @ref g_CurrDateTime is already up to date this can be omitted,
 *	set the flag to <b>false</b> for this case.
 *
 * @see DisplayUpdateFctInstall().
 *
 ******************************************************************************/
void	ClockUpdate (bool readTime)
{
time_t	now;

    /*
     * Measured execution time: 110us without calling l_DisplayUpdateFct().
     */

    /*
     * Get current UNIX time, convert to <tm>, and store in global struct.
     * We have to use function localtime() here instead of the reentrant
     * version localtime_r() because this does not exist in the IAR libraries.
     *
     * THIS IS NOT MULTI-THREADING SAVE AND ONLY WORKS BECAUSE localtime() IS
     * CALLED FROM INTERRUPT SERVICE ROUTINES WHICH HAVE THE SAME PRIORITY
     * LEVEL, SO THEY DO NOT INTERFERE.
     */
    if (readTime)
    {
	/* Disable interrupts */
	INT_Disable();

	now = time (NULL);
	g_CurrDateTime = *localtime(&now);

	/* Enable interrupts again */
	INT_Enable();
    }

    /* Also update display, if a function has been defined for that purpose */
    if (l_DisplayUpdateFct)
	l_DisplayUpdateFct();
}

/***************************************************************************//**
 *
 * @brief	Get System Clock
 *
 * This routine copies the current date and time into the local <i>tm</i>
 * structure pointed to by the <b>pTimeDateVar</b> parameter.
 *
 * @param[in] pTimeDateVar
 *	Pointer to a variable where to store the current time and date.
 *
 ******************************************************************************/
void	ClockGet (struct tm *pTimeDateVar)
{
    EFM_ASSERT (pTimeDateVar != NULL);

    /* Disable interrupts */
    INT_Disable();

    /* Get current date and time */
    *pTimeDateVar = g_CurrDateTime;

    /* Enable interrupts again */
    INT_Enable();
}

/***************************************************************************//**
 *
 * @brief	Get System Clock with Milliseconds
 *
 * Like ClockGet(), this routine copies the current date and time into the
 * local <i>tm</i> structure pointed to by the <b>pTimeDateVar</b> parameter.
 * Additionally the number of milliseconds is stored in <b>pMsVar</b>.
 *
 * @param[in] pTimeDateVar
 *	Pointer to a variable where to store the current time and date.
 *
 * @param[in] pMsVar
 *	Pointer to a variable where to store the [ms] portion.
 *
 ******************************************************************************/
void	ClockGetMilliSec (struct tm *pTimeDateVar, unsigned int *pMsVar)
{
uint32_t	currSubSec;

    EFM_ASSERT (pTimeDateVar != NULL  &&  pMsVar != NULL);

    /* Disable interrupts */
    INT_Disable();

    /* Read current RTC value - only sub-seconds are of interest */
    currSubSec = (RTC->CNT - RTC->COMP0) % RTC_COUNTS_PER_SEC;

    /* Get current date and time */
    *pTimeDateVar = g_CurrDateTime;

    /* NOW check if RTC overflow happened (interrupt is still pending!) */
    if (RTC->IF & RTC_IF_COMP0)
	*pMsVar = 999;			// use maximum [ms] value
    else				// Calculate remaining [ms]
	*pMsVar = currSubSec * 1000 / RTC_COUNTS_PER_SEC;

    /* Enable interrupts again */
    INT_Enable();
}

/***************************************************************************//**
 *
 * @brief	Set System Clock
 *
 * This routine sets the System Clock to the new date and time as specified by
 * the <b>pNewTimeDate</b> parameter.
 *
 * @param[in] pNewTimeDate
 *	New time and date value.
 *
 * @param[in] sync
 *	If <b>true</b>, seconds are synchronized, i.e. the millisecond portion
 *	of the counter is set to zero.  If <b>false</b>, the sub-seconds time
 *	offset remains untouched, this is useful for changing date, while
 *	not modifying time.
 *
 ******************************************************************************/
void	ClockSet (struct tm *pNewTimeDate, bool sync)
{
time_t    newRtcStartTime;
uint32_t  rtcIEN;	// save state of the RTC Interrupt Enable register
uint32_t  rtcCNT;	// save state of the RTC Interrupt Enable register


    EFM_ASSERT (pNewTimeDate != NULL);

    /* Convert <tm> structure to <time_t> */
    pNewTimeDate->tm_isdst = 0;		// always 0 for mktime()
    newRtcStartTime = mktime (pNewTimeDate);

    /*
     * If this is the very first time the clock ist set after power-up, then
     * this time shall be stored in <g_PowerUpTime>, so it can be referred
     * to calculate the "up-time" of the system, or report the number of lost
     * log messages since power-up.
     */
    if (g_PowerUpTime == 0)
    {
	g_PowerUpTime  = newRtcStartTime;
	g_CurrDateTime = *localtime(&newRtcStartTime);
#ifdef LOGGING
	Log ("Initial Time Synchronisation");
#endif
    }

    /* Be sure to disable RTC interrupts while manipulating registers */
    rtcIEN = RTC->IEN;
    RTC->IEN = 0;		// disable all RTC interrupts

    /* Get current counter value and stop the clock */
    rtcCNT = RTC->CNT;
    RTC_Enable (false);

    /* Wait for any pending previous write operation to have been completed */
    /* in low frequency domain. This is only required for the Gecko Family */
    while (RTC->SYNCBUSY & (RTC_SYNCBUSY_COMP0 | RTC_SYNCBUSY_COMP1))
	;

    /*
     * Calculate the respective COMP values if counter is zero.  If <sync>
     * flag is true, the milliseconds portion of the counter is also reset by
     * setting COMP0 to RTC_COUNTS_PER_SEC (i.e. the next full second).
     * The high-resolution timer COMP1 is always changed in a way that the
     * remaining time will be correct.
     */
    RTC->COMP0 = (sync ? RTC_COUNTS_PER_SEC : RTC->COMP0 - rtcCNT);
    RTC->COMP1 -= rtcCNT;

    /* Set new start time and reset overflow counter */
    clockSetStartTime (newRtcStartTime);
    clockSetOverflowCounter (0);

    /* Start the clock */
    RTC_Enable (true);

    /* Finally restore the original state of the IEN register */
    RTC->IEN = rtcIEN;
}

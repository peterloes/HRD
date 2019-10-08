#ifdef DEBUG	/*##################### ONLY FOR DEBUGGING ###################*/
#include "config.h"
#include "em_gpio.h"

void setupSWOForPrint(void);

/*================RAGE-DEBUG===============*/
void dbgInit(void)
{
    /* Set up Serial Wire Output */
    setupSWOForPrint();
}

void setupSWOForPrint(void)
{
    /* Enable GPIO clock. */
    CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_GPIO;

    /* Enable Serial wire output pin */
    GPIO->ROUTE |= GPIO_ROUTE_SWOPEN;

    /* Set location 0 */
    GPIO->ROUTE = (GPIO->ROUTE & ~(_GPIO_ROUTE_SWLOCATION_MASK)) | GPIO_ROUTE_SWLOCATION_LOC0;

    /* Enable output on pin - GPIO Port F, Pin 2 */
    GPIO->P[5].MODEL &= ~(_GPIO_P_MODEL_MODE2_MASK);
    GPIO->P[5].MODEL |= GPIO_P_MODEL_MODE2_PUSHPULL;

    /* Enable debug clock AUXHFRCO */
    CMU->OSCENCMD = CMU_OSCENCMD_AUXHFRCOEN;

    /* Wait until clock is ready */
    while (!(CMU->STATUS & CMU_STATUS_AUXHFRCORDY));

    /* Enable trace in core debug */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    ITM->LAR  = 0xC5ACCE55;
    ITM->TER  = 0x0;
    ITM->TCR  = 0x0;
    TPI->SPPR = 2;
    TPI->ACPR = 0xf;
    ITM->TPR  = 0x0;
    DWT->CTRL = 0x400003FE;
    ITM->TCR  = 0x0001000D;
    TPI->FFCR = 0x00000100;
    ITM->TER  = 0x1;
}

void ITM_SendStr(const char *pStr)
{
    while (*pStr != EOS)
    {
	ITM_SendChar(*pStr);
	pStr++;
    }
}

    /* Debug pointers to all peripherial addresses */
AES_TypeDef	* const _pAES		= AES;
DMA_TypeDef	* const _pDMA		= DMA;
MSC_TypeDef	* const _pMSC		= MSC;
EMU_TypeDef	* const _pEMU		= EMU;
RMU_TypeDef	* const _pRMU		= RMU;
CMU_TypeDef	* const _pCMU		= CMU;
TIMER_TypeDef	* const _pTIMER0	= TIMER0;
TIMER_TypeDef	* const _pTIMER1	= TIMER1;
TIMER_TypeDef	* const _pTIMER2	= TIMER2;
USART_TypeDef	* const _pUSART0	= USART0;
USART_TypeDef	* const _pUSART1	= USART1;
USART_TypeDef	* const _pUSART2	= USART2;
LEUART_TypeDef	* const _pLEUART0	= LEUART0;
LEUART_TypeDef	* const _pLEUART1	= LEUART1;
RTC_TypeDef	* const _pRTC		= RTC;
LETIMER_TypeDef	* const _pLETIMER0	= LETIMER0;
PCNT_TypeDef	* const _pPCNT0		= PCNT0;
PCNT_TypeDef	* const _pPCNT1		= PCNT1;
PCNT_TypeDef	* const _pPCNT2		= PCNT2;
ACMP_TypeDef	* const _pACMP0		= ACMP0;
ACMP_TypeDef	* const _pACMP1		= ACMP1;
PRS_TypeDef	* const _pPRS		= PRS;
DAC_TypeDef	* const _pDAC0		= DAC0;
GPIO_TypeDef	* const _pGPIO		= GPIO;
VCMP_TypeDef	* const _pVCMP		= VCMP;
ADC_TypeDef	* const _pADC0		= ADC0;
I2C_TypeDef	* const _pI2C0		= I2C0;
WDOG_TypeDef	* const _pWDOG		= WDOG;
CALIBRATE_TypeDef * const _pCALIBRATE	= CALIBRATE;
DEVINFO_TypeDef * const _pDEVINFO	= DEVINFO;
NVIC_Type	* const _pNVIC		= NVIC;
ROMTABLE_TypeDef * const _pROMTABLE	= ROMTABLE;

#endif		/*##################### ONLY FOR DEBUGGING ###################*/

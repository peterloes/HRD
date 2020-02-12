/***************************************************************************//**
 * @file
 * @brief	Header file of module BatteryMon.c
 * @author	Peter Loës
 * @version	2020-01-13
 ****************************************************************************//*
Revision History:
2020-01-13,rage	Merged with version from Peter Loës, added BC_TYPE, variables
		g_BatteryCtrlAddr, g_BatteryCtrlType, and g_BatteryCtrlName.
		Added prototype for BatteryCtrlProbe().
2019-03-12,Loës	Li-Ion Battery Pack Manager change (bq40z50-R11; Texas Instr.).
2015-03-29,rage	Completed documentation.
2014-12-20,rage	Initial version.
*/

#ifndef __INC_BatteryMon_h
#define __INC_BatteryMon_h

/*=============================== Header Files ===============================*/

#include "em_device.h"
#include "em_gpio.h"
#include "config.h"		// include project configuration parameters

/*=============================== Definitions ================================*/

    /*!@brief Battery Controller Types (bit mask) */
typedef enum
{
    BCT_UNKNOWN = 0x00,		//!< Battery Controller Type not known yet
    BCT_ATMEL   = 0x01,		//!< ATMEL Controller (2014)
    BCT_TI      = 0x02,		//!< Texas Instruments Controller (2019)
} BC_TYPE;

/*!@brief SBS Commands
 *
 * These are the defines for the registers of the battery controller.  Each
 * define contains the following bit fields:
 * - Bit 17:15 specify the controller type where the define belongs to.  Bit 16
 *   (0x10000) represents the Atmel controller, while bit 17 (0x20000) specifies
 *   the TI controller. If both bits are set (0x30000), the register exists in
 *   both controller types.  Bit 15 is used if no controller is connected to
 *   identify the remaining valid entries (where SBS_NONE is set).
 * - Bit 14:8 contains the number of bytes to read.  For 8, 16, or 32 bit values
 *   BatteryRegReadValue() is called, while for more than 4 bytes function
 *   BatteryRegReadBlock() will be used (block command).
 * - Bit 7:0 of the enum contains the address as used for the SBS commands sent
 *   via I2C.
 *
 * All command sequences beginn with I²C device address to access the battery
 * controller.  The device address depends on the type of battery controller:
 * - 0x0A in case of Atmel, and
 * - 0x16 for the TI bq40z50.
 * The address can be probed via BatteryCtrlProbe().
 *
 * @see
 * Functions BatteryRegReadValue() and BatteryRegReadBlock() use these enums.
 * A list of all registers can be found in document
 * <a href="../SBS_Commands.pdf">SBS Commands</a> and in the
 * <a href="../sluubc1b_Technical_Reference_bq40z50-R1.pdf">Technical Reference
 * Manual</a> of the Texas Instruments bq40z50 battery controller.
 *
 * @note
 * Registers 0x60~0x78 and those marked by brackets, e.g. [0x50], cannot be
 * accessed when the battery device in "sealed" mode, which is the default
 * state.
 */
typedef enum
{
    SBS_NONE = (-1),			//!< (-1) No Command / Address
    SBS_ManufacturerAccess = 0x30200,	//!< 0x00 ManufacturerAccess (legacy)
    SBS_RemainingCapacityAlarm,		//!< 0x01 Word: in [mAh / 10mWh]
    SBS_RemainingTimeAlarm,		//!< 0x02 Word: in [min]
    SBS_BatteryMode,			//!< 0x03 Word: in Hex
    SBS_AtRate		 = 0x20204,	//!< 0x04 TI Word: in [mA / 10mW]
    SBS_AtRateTimeToFull,		//!< 0x05 TI Word in [min]
    SBS_AtRateTimeToEmpty,		//!< 0x06 TI Word in [min]
    SBS_AtRateOK,			//!< 0x07 TI Word >0 if energy for min. 10s
    SBS_Temperature	 = 0x30208,	//!< 0x08 Word: Temperature in [0.1°K]
    SBS_Voltage,			//!< 0x09 Word: in [mV]
    SBS_Current,			//!< 0x0A Word: Actual Current in [mA]
    SBS_AverageCurrent,			//!< 0x0B Word: in [mA]
    SBS_MaxError,			//!< 0x0C Word  in [%]
    SBS_RelativeStateOfCharge,		//!< 0x0D Word: in [%]
    SBS_AbsoluteStateOfCharge,		//!< 0x0E Word: in [%]
    SBS_RemainingCapacity,		//!< 0x0F Word: in [mAh / 10mWh]
    SBS_FullChargeCapacity,		//!< 0x10 Word: in [mAh / 10mWh]
    SBS_RunTimeToEmpty,			//!< 0x11 Word: in [min]
    SBS_AverageTimeToEmpty,		//!< 0x12 Word: in [min]
    SBS_AverageTimeToFull,		//!< 0x13 Word: in [min]
    SBS_ChargingCurrent,		//!< 0x14 Word: in [mA]
    SBS_ChargingVoltage,		//!< 0x15 Word: in [mV]
    SBS_BatteryStatus,			//!< 0x16 Word: Hex @see SBS_16_BITS
    SBS_CycleCount,			//!< 0x17 Word: in [cycles]
    SBS_DesignCapacity,			//!< 0x18 Word: in [mAh / 10mWh]
    SBS_DesignVoltage,			//!< 0x19 Word: in [mV]
    SBS_SpecificationInfo,		//!< 0x1A Word: Version and Revision
    SBS_ManufactureDate,		//!< 0x1B Word: [Year-1980:7|Month:4|Day:5]
    SBS_SerialNumber,			//!< 0x1C Word: Binary coded S/N
    SBS_ManufacturerName = 0x31220,	//!< 0x20 Block: ASCII string
    SBS_DeviceName	 = 0x31021,	//!< 0x21 Block: ASCII string
    SBS_DeviceChemistry  = 0x31022,	//!< 0x22 Block: ASCII string
    SBS_ManufacturerData = 0x12123,	//!< 0x23 ATMEL Block: ASCII string
    SBS_ManufacturerDataTI = 0x21023,	//!< 0x23 TI Block: Hex Bytes
	/* 0x24 ~ 0x29 reserved */
    SBS_ShuntResistance  = 0x1022A,	//!< 0x2A ATMEL Word: in [µOhm]
	/* 0x2B ~ 0x2E reserved */
    SBS_Authenticate     = 0x2212F,	//!< 0x2F TI Block: MAC Authentication
	/* 0x30 ~ 0x3B reserved */
    SBS_CellsInSeries	 = 0x1023C,	//!< 0x3C ATMEL Word: Number of Cells
    SBS_OverCurrentReactionTime,	//!< 0x3D ATMEL Word: Level 1 Time in [??]
    SBS_OverCurrentCharge,		//!< 0x3E ATMEL Word: Level 1 Charge in [mA]
    SBS_OverCurrentDischarge,		//!< 0x3F ATMEL Word: Level 1 Discharge in [mA]
    SBS_HighCurrentReactionTime,	//!< 0x40 ATMEL Word: Level 2 Time in [??]
    SBS_HighCurrentCharge,		//!< 0x41 ATMEL Word: Level 2 Charge in [mA]
    SBS_HighCurrentDischarge,		//!< 0x42 ATMEL Word: Level 2 Discharge in [mA]
	/* ATMEL: 0x43 reserved */
    SBS_VoltageCell4	 = 0x10244,	//!< 0x44 ATMEL Word: in [mV]
    SBS_VoltageCell3,			//!< 0x45 ATMEL Word: in [mV]
    SBS_VoltageCell2,			//!< 0x46 ATMEL Word: in [mV]
    SBS_VoltageCell1,			//!< 0x47 ATMEL Word: in [mV]

    SBS_CellVoltage4	 = 0x2023C,	//!< 0x3C TI Word: in [mV]
    SBS_CellVoltage3,			//!< 0x3D TI Word: in [mV]
    SBS_CellVoltage2,			//!< 0x3E TI Word: in [mV]
    SBS_CellVoltage1,			//!< 0x3F TI Word: in [mV]
	/* TI: 0x40 ~ 0x43 reserved */
    SBS_ManufacturerBlockAccess = 0x20444, //!< 0x44 TI Read/Write MAC Data
	/* TI: 0x45 ~ 0x49 reserved */
    SBS_BTPDischargeSet  = 0x2024A,	//!< 0x4A TI Signed Int: in [mAh]
    SBS_BTPChargeSet,			//!< 0x4B TI Signed Int: in [mAh]
	/* 0x4C ~ 0x4E reserved */
    SBS_StateOfHealth	 = 0x2024F,	//!< 0x4F TI Word: in % of design cap.
    SBS_SafetyAlert,			//!<[0x50]TI Long: Hex @see SBS_50_BITS
    SBS_SafetyStatus,			//!<[0x51]TI Long: Hex @see SBS_51_BITS
    SBS_PFAlert,			//!<[0x52]TI Long: Hex
    SBS_PFStatus,			//!<[0x53]TI Long: Hex

    SBS_CellMinVoltage	 = 0x10254,	//!< 0x54 ATMEL Word: in [mV]
    SBS_CellMaxVoltage,			//!< 0x55 ATMEL Word: in [mV]
    SBS_CellPowerOffVoltage,		//!< 0x56 ATMEL Word: in [mV]

    SBS_OperationStatus  = 0x20454,	//!<[0x54]TI Long: Hex @see SBS_54_BITS
    SBS_ChargingStatus	 = 0x20355,	//!<[0x55]TI 24bit: Hex
    SBS_GaugingStatus,			//!<[0x56]TI 24bit: Hex
    SBS_ManufacturingStatus = 0x20257,	//!<[0x57]TI Word: Hex
    SBS_AFERegister	 = 0x22158,	//!<[0x58]TI Block: (internal debug data)
    SBS_TurboPower	 = 0x20259,	//!< 0x59 TI Word: in [cW]
    SBS_TurboFinal,			//!< 0x5A TI Word: in [cW]
    SBS_TurboPackR,			//!< 0x5B TI Word: in [mOhm]
    SBS_TurboSysR,			//!< 0x5C TI Word: in [mOhm]
    SBS_TurboEdv,			//!< 0x5D TI Word: in [mV]
    SBS_TurboCurrent,			//!< 0x5E TI Word: in [mAh]
    SBS_NoLoadRemCap,			//!< 0x5F TI Word: in [mAh]
    /*
     * All other registers and those in brackets cannot be read when the battery
     * device in "sealed" mode, which is the default state.
     */
    END_SBS_CMD,			//!< End of SBS Command Definitions
} SBS_CMD;

    /*!@brief Macro to extract address from @ref SBS_CMD enum. */
#define SBS_CMD_ADDR(cmd)	((cmd) & 0xFF)

    /*!@brief Macro to extract size from @ref SBS_CMD enum. */
#define SBS_CMD_SIZE(cmd)	((cmd >> 8) & 0xFF)

    /*!@name SBS_03_BITS - Bits of Battery Controller Register 0x03 */
//@{
#define SBS_03_BIT_BALANCING_CELL3	 12	//!< Balancing Cell 3
#define SBS_03_BIT_BALANCING_CELL2	 11	//!< Balancing Cell 2
#define SBS_03_BIT_BALANCING_CELL1	 10	//!< Balancing Cell 1
#define SBS_03_BIT_BALANCING_CELL4	  6	//!< Balancing Cell 4
#define SBS_03_BIT_DUVRD		  5	//!< Deep Under Voltage Recovery
#define SBS_03_BIT_CPS			  4	//!< Current Protection Status
#define SBS_03_BIT_DFE			  3	//!< Discharge FET status
#define SBS_03_BIT_CFE			  2	//!< Charge FET status
//@}

    /*!@name SBS_16_BITS - Bits of Battery Controller Register 0x16 */
//@{
#define SBS_16_BIT_OVER_CHARGE_ALARM	 15	//!< Over Charge Alarm
#define SBS_16_BIT_TERM_CHARGE_ALARM	 14	//!< Terminate Charge Alarm
#define SBS_16_BIT_OVER_TEMP_ALARM	 12	//!< Over Temperature Alarm
#define SBS_16_BIT_TERM_DISCHARGE_ALARM	 11	//!< Terminate Discharge Alarm
#define SBS_16_BIT_BATTERY_PROTECTION	 10	//!< FETs have been switched off
#define SBS_16_BIT_REMAIN_CAP_ALARM	  9	//!< Remaining Capacity Alarm
#define SBS_16_BIT_REMAIN_TIME_ALARM	  8	//!< Remaining Time Alarm
#define SBS_16_BIT_INITIALIZED		  7	//!< Controller is initialized
#define SBS_16_BIT_DISCHARGING		  6	//!< Battery is discharged
#define SBS_16_BIT_FULLY_CHARGED	  5	//!< Battery is fully charged
#define SBS_16_BIT_FULLY_DISCHARGED	  4	//!< Battery is fully discharged
#define SBS_16_BIT_ERROR_CODE_MASK	0xF	//!< Bit Mask 3~0 for Error Code
#define SBS_16_BIT_ERROR_OK		0x0	//!< OK, no error
#define SBS_16_BIT_ERROR_BUSY		0x1	//!< Busy
#define SBS_16_BIT_ERROR_RSVD_CMD	0x2	//!< Reserved Command
#define SBS_16_BIT_ERROR_UNSUPP_CMD	0x3	//!< Unsupported Command
#define SBS_16_BIT_ERROR_ACC_DENIED	0x4	//!< Access Denied
#define SBS_16_BIT_ERROR_OVERFLOW	0x5	//!< Overflow or Underflow
#define SBS_16_BIT_ERROR_BAD_SIZE	0x6	//!< Bad Size
#define SBS_16_BIT_ERROR_UNKN_ERROR	0x7	//!< Unknown Error
//@}

    /*!@name SBS_50_BITS - Bits of Register 0x50 Safety Alert (TI) */
//@{
#define SBS_50_BIT_UTD			 27	//!< Undertemp. During Discharge
#define SBS_50_BIT_UTC			 26	//!< Undertemp. During Charge
#define SBS_50_BIT_PCHGC		 25	//!< Over-Precharge Current
#define SBS_50_BIT_CHGV			 24	//!< Overcharging Voltage
#define SBS_50_BIT_CHGC			 23	//!< Overcharging Current
#define SBS_50_BIT_OC			 22	//!< Overcharge (status)
#define SBS_50_BIT_CTOS			 21	//!< Charge Timeout Suspend
#define SBS_50_BIT_CTO			 20	//!< Charge Timeout
#define SBS_50_BIT_PTOS			 19	//!< Precharge Timeout Suspend
#define SBS_50_BIT_PTO			 18	//!< Precharge Timeout
#define SBS_50_BIT_OTF			 16	//!< Overtemperature FET
#define SBS_50_BIT_CUVC			 14	//!< Cell Undervolt. Compensated
#define SBS_50_BIT_OTD			 13	//!< Overtemp. During Discharge
#define SBS_50_BIT_OTC			 12	//!< Overtemp. During Charge
#define SBS_50_BIT_ASCDL		 11	//!< Short-Circuit During Disch.
#define SBS_50_BIT_ASCCL		  9	//!< Short-Circuit During Charge
#define SBS_50_BIT_AOLDL		  7	//!< Overload During Discharge
#define SBS_50_BIT_OCD2			  5	//!< Overcurrent During Dis. 2
#define SBS_50_BIT_OCD1			  4	//!< Overcurrent During Dis. 1
#define SBS_50_BIT_OCC2			  3	//!< Overcurrent During Charge 2
#define SBS_50_BIT_OCC1			  2	//!< Overcurrent During Charge 1
#define SBS_50_BIT_COV			  1	//!< Cell Overvoltage
#define SBS_50_BIT_CUV			  0	//!< Cell Undervoltage
//@}

    /*!@name SBS_51_BITS - Bits of Register 0x51 Safety Status (TI) */
//@{
#define SBS_51_BIT_UTD			 27	//!< Undertemp. During Discharge
#define SBS_51_BIT_UTC			 26	//!< Undertemp. During Charge
#define SBS_51_BIT_PCHGC		 25	//!< Over-Precharge Current
#define SBS_51_BIT_CHGV			 24	//!< Overcharging Voltage
#define SBS_51_BIT_CHGC			 23	//!< Overcharging Current
#define SBS_51_BIT_OC			 22	//!< Overcharge (status)
#define SBS_51_BIT_CTO			 20	//!< Charge Timeout
#define SBS_51_BIT_PTO			 18	//!< Precharge Timeout
#define SBS_51_BIT_OTF			 16	//!< Overtemperature FET
#define SBS_51_BIT_CUVC			 14	//!< Cell Undervolt. Compensated
#define SBS_51_BIT_OTD			 13	//!< Overtemp. During Discharge
#define SBS_51_BIT_OTC			 12	//!< Overtemp. During Charge
#define SBS_51_BIT_ASCDL		 11	//!< Short-Circ. dur. Disch. Lt
#define SBS_51_BIT_ASCD			 10	//!< Short-Circuit During Disch.
#define SBS_51_BIT_ASCCL		  9	//!< Short-Circ. dur. Ch. Latch
#define SBS_51_BIT_ASCC			  8	//!< Short-Circuit During Charge
#define SBS_51_BIT_AOLDL		  7	//!< Overload During Dis. Latch
#define SBS_51_BIT_AOLD			  6	//!< Overload During Discharge
#define SBS_51_BIT_OCD2			  5	//!< Overcurrent During Dis. 2
#define SBS_51_BIT_OCD1			  4	//!< Overcurrent During Dis. 1
#define SBS_51_BIT_OCC2			  3	//!< Overcurrent During Charge 2
#define SBS_51_BIT_OCC1			  2	//!< Overcurrent During Charge 1
#define SBS_51_BIT_COV			  1	//!< Cell Overvoltage
#define SBS_51_BIT_CUV			  0	//!< Cell Undervoltage
//@}

    /*!@name SBS_54_BITS - Bits of Register 0x54 Operation Status (TI) */
//@{
#define SBS_54_BIT_EMSHUT		 29	//!< Emergency Shutdown
#define SBS_54_BIT_CB			 28	//!< Cell balancing status
#define SBS_54_BIT_SLPCC		 27	//!< CC Measurem. in Sleep Mode
#define SBS_54_BIT_SLPAD		 26	//!< ADC Measurem. in Sleep Mode
#define SBS_54_BIT_SMBLCAL		 25	//!< Auto Calib. when bus is low
#define SBS_54_BIT_INIT			 24	//!< Initialization after Reset
#define SBS_54_BIT_SLEEPM		 23	//!< Sleep Mode trig.by command
#define SBS_54_BIT_XL			 22	//!< 400kHz SMBus Mode
#define SBS_54_BIT_CAL_OFFSET		 21	//!< Calibration Output Offset
#define SBS_54_BIT_CAL			 20	//!< Calibration Output
#define SBS_54_BIT_AUTOCALM		 19	//!< Auto CC Offset Calib.by MAC
#define SBS_54_BIT_AUTH			 18	//!< Authentication in Progress
#define SBS_54_BIT_LED			 17	//!< LED Display
#define SBS_54_BIT_SDM			 16	//!< Shutdown trig. by command
#define SBS_54_BIT_SLEEP		 15	//!< Sleep Mode Contitions met
#define SBS_54_BIT_XCHG			 14	//!< Charging disabled
#define SBS_54_BIT_XDSG			 13	//!< Discharging disabled
#define SBS_54_BIT_PF			 12	//!< Permanent Failure Status
#define SBS_54_BIT_SS			 11	//!< Safety Mode Status
#define SBS_54_BIT_SDV			 10	//!< Shutdown trig. by low volt.
#define SBS_54_BIT_SEC_MASK		0x300	//!< SECURITY Mode
#define SBS_16_BIT_SEC_RESERVED		0x000	//!< Reserved
#define SBS_16_BIT_SEC_FULL_ACCESS	0x100	//!< Full Accesss
#define SBS_16_BIT_SEC_UNSEALED		0x200	//!< Unsealed
#define SBS_16_BIT_SEC_SEALED		0x300	//!< Sealed
#define SBS_54_BIT_BTP_INT		  7	//!< Battery Trip Point IRQ
#define SBS_54_BIT_FUSE			  5	//!< Fuse Status
#define SBS_54_BIT_PCHG			  3	//!< Precharge FET Status
#define SBS_54_BIT_CHG			  2	//!< Charge FET Status
#define SBS_54_BIT_DSG			  1	//!< Discharge FET Status
#define SBS_54_BIT_PRES			  0	//!< System Present Low
//@}


    /*!@brief Error code for I2C timeout, additionally to @ref
     * I2C_TransferReturn_TypeDef
     */
#define i2cTransferTimeout		-10

    /*!@brief Error code for invalid parameter, additionally to @ref
     * I2C_TransferReturn_TypeDef
     */
#define i2cInvalidParameter		-11

/*================================ Global Data ===============================*/

    /* I2C Device Address of the Battery Controller */
extern uint8_t g_BatteryCtrlAddr;

    /* Battery Controller Type */
extern BC_TYPE g_BatteryCtrlType;

    /* ASCII Name of the Battery Controller, or "" if no one found */
extern const char *g_BatteryCtrlName;

/*================================ Prototypes ================================*/

    /* Initialize Battery Monitor module */
void	 BatteryMonInit (void);

    /* Probe for Controller Type */
void	 BatteryCtrlProbe (void);

    /* Register read functions */
int	 BatteryRegReadValue (SBS_CMD cmd, uint32_t *pValue);
int	 BatteryRegReadBlock (SBS_CMD cmd, uint8_t  *pBuf, size_t bufSize);

    /* Read local Vdd value */
uint32_t ReadVdd (void);

#endif /* __INC_BatteryMon_h */

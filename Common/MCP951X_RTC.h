//------------------------------------------------------------------------------------------------
//---- MCP951X_RTC.h (C) 2026 Dave Gaunt                                                      ----
//------------------------------------------------------------------------------------------------
#ifndef __MCP951X_RTC_h_included
#define __MCP951X_RTC_h_included

#include "types.h"
#include "hardware/spi.h"

#define RTC_SRAM_BASE 	(0x20)
#define RTC_SRAM_SIZE 	(0x40)
#define RTC_EEPROM_BASE	(0x00)
#define RTC_EEPROM_SIZE	(0x80)		
#define RTC_PAGE_SHIFT	(3)

typedef struct
{
	u8	m_uOnes : 4;
	u8	m_uTens : 4;
} rtc_bcd_pair;
static_assert(sizeof(rtc_bcd_pair) == 1);

typedef struct
{
	u8	m_eDayOfWeek : 3;
	u8	m_bLeapYear  : 1;
	u8				 : 4;
	rtc_bcd_pair m_Date;
	rtc_bcd_pair m_Month;
	rtc_bcd_pair m_Year;
} rtc_date;
static_assert(sizeof(rtc_date) == 4);

typedef struct
{
	rtc_bcd_pair m_Hundredths;
	rtc_bcd_pair m_Seconds;
	rtc_bcd_pair m_Minutes;
	rtc_bcd_pair m_Hours;
} rtc_time;
static_assert(sizeof(rtc_time) == 4);

enum rtc_output_frequency
{
	RCT_OUTPUT_SQUARE_WAVE_1Hz = 0,
	RCT_OUTPUT_SQUARE_WAVE_4096Hz,
	RCT_OUTPUT_SQUARE_WAVE_8192Hz,
	RCT_OUTPUT_SQUARE_WAVE_32768Hz
};

enum rtc_day_of_week
{
	RTC_WEEKDAY_SUNDAY = 0,
	RTC_WEEKDAY_MONDAY,
	RTC_WEEKDAY_TUESDAY,
	RTC_WEEKDAY_WEDNESDAY,
	RTC_WEEKDAY_THURSDAY,
	RTC_WEEKDAY_FRIDAY,
	RTC_WEEKDAY_SATURDAY
};

bool RTC_Initialise(spi_inst_t* pSpi, const u32 uBaudRate, const u32 uClkPin, const u32 uTxPin, const u32 uRxPin, const u32 uCsPin);

bool RTC_Start(void);
bool RTC_Stop(void);
bool RTC_IsRunning(void);

rtc_date RTC_GetDate(void);
bool RTC_SetDate(const rtc_date uDate);

rtc_time RTC_GetTime(void);
bool RTC_SetTime(const rtc_time uTime, const bool bStart);

bool RTC_ReadSRAM(void* pDestination, const u8 uRTCAddress, const u8 uLength);
bool RTC_WriteSRAM(void* pSource, const u8 uRTCAddress, const u8 uLength);

bool RTC_ReadEEPROM(void* pDestination, const u8 uPageIndex);

bool RTC_ReadID(void* pDestination, const u8 uPageIndex);

#endif /* __MCP951X_RTC_h_included */

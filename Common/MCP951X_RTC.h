//------------------------------------------------------------------------------------------------
//---- MCP951X_RTC.h (C) 2026 Dave Gaunt                                                      ----
//------------------------------------------------------------------------------------------------
#ifndef __MCP951X_RTC_h_included
#define __MCP951X_RTC_h_included

#include "types.h"
#include "hardware/spi.h"

typedef struct
{
	u8	m_uOnes : 4;
	u8	m_uTens : 4;
} rtc_bcd_pair;

typedef struct
{
	rtc_bcd_pair m_Hundredths;
	rtc_bcd_pair m_Seconds;
	rtc_bcd_pair m_Minutes;
	rtc_bcd_pair m_Hours;
} rtc_time;

enum rtc_output_frequency
{
	RCT_OUTPUT_SQUARE_WAVE_1Hz = 0,
	RCT_OUTPUT_SQUARE_WAVE_4096Hz,
	RCT_OUTPUT_SQUARE_WAVE_8192Hz,
	RCT_OUTPUT_SQUARE_WAVE_32768Hz
};

bool RTC_Initialise(spi_inst_t* pSpi, const u32 uBaudRate, const u32 uClkPin, const u32 uTxPin, const u32 uRxPin, const u32 uCsPin);

bool RTC_Start(void);
bool RTC_Stop(void);
bool RTC_IsRunning(void);

rtc_time RTC_GetTime(void);
bool RTC_SetTime(const rtc_time uTime, const bool bStart);

bool RTC_ReadSRAM(void* pDestination, const u8 uRTCAddress, const u8 uLength);
bool RTC_WriteSRAM(void* pSource, const u8 uRTCAddress, const u8 uLength);

#endif /* __MCP951X_RTC_h_included */

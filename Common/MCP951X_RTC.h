//------------------------------------------------------------------------------------------------
//---- MCP951X_RTC.h (C) 2026 Dave Gaunt                                                      ----
//------------------------------------------------------------------------------------------------
#ifndef __MCP951X_RTC_h_included
#define __MCP951X_RTC_h_included

#include "types.h"
#include "hardware/spi.h"

enum rtc_output_frequency
{
	RCT_OUTPUT_SQUARE_WAVE_1Hz = 0,
	RCT_OUTPUT_SQUARE_WAVE_4096Hz,
	RCT_OUTPUT_SQUARE_WAVE_8192Hz,
	RCT_OUTPUT_SQUARE_WAVE_32768Hz
};

bool RTC_Initialise(spi_inst_t* pSpi, const u32 uBaudRate, const u32 uClkPin, const u32 uTxPin, const u32 uRxPin, const u32 uCsPin);

bool RTC_ReadSRAM(void* pDestination, const u8 uRTCAddress, const u8 uLength);
bool RTC_WriteSRAM(void* pSource, const u8 uRTCAddress, const u8 uLength);

#endif /* __MCP951X_RTC_h_included */

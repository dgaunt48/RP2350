//------------------------------------------------------------------------------------------------
//---- RealTimeClock.c (C) 2026 Dave Gaunt                                                    ----
//------------------------------------------------------------------------------------------------
//---- v1.0 - Connect a MCP951X	Real Time Clock To The RP2350                                 ----
//------------------------------------------------------------------------------------------------

#include <stdio.h>
#include "types.h"
#include "pico/stdlib.h"

#include "MCP951X_RTC.h"
#include "vga111.h"

#define SPI_BAUD_RATE	(10 * 1000 * 1000)	/* 10Mhz */

//32.7695 with 10pf load caps

// 32769.5 * 60  = 1,966,170
// 32768 * 60    = 1,966,080
// ..170 - ..080  = 90 / 2 = 45;

enum board_pins
{
	PIN_RED = 0, PIN_GREEN, PIN_BLUE, PIN_HSYNC, PIN_VSYNC,
	PIN_SPI_MOSI = 16, PIN_SPI_CS, PIN_SPI_CLOCK, PIN_SPI_MISO
};

const char aWeekDays[7][4] =
{
	"SUN",
	"MON",
	"TUE",
	"WED",
	"THU",
	"FRI",
	"SAT"
};

static u8 s_aIOBuffer[16];

//------------------------------------------------------------------------------------------------
//---- ASCII to PetSCII                                                                       ----
//------------------------------------------------------------------------------------------------
u8 ascii_to_petscii(const u8 c)
{
    // Handle ASCII Lowercase (97-122) Maps 'a'-'z' to ROM Lowwercase (1-26)
    if (c >= 97 && c <= 122)
		return c - 96;

    // Special Case For '@'
    if (c == 64)
		return 0;

    // Handle Space (32) through 'Z' (90)
	// This includes numbers and maps ASCII Uppercase (65-90) to ROM Uppercase (65-90)
    if (c >= 32 && c <= 90)
		return c;

    // Default to Space
    return 32; 
}

//------------------------------------------------------------------------------------------------
//---- FormatHexDumpLine	                                                                  ----
//------------------------------------------------------------------------------------------------
void FormatHexDumpLine(u32 uCharX, u32 uCharY, const u32 uAddress, const u8* pLineBuffer, const u8 uColour, const bool bASCII)
{
	// Write Address Offset in 6 byte hex.
	for(int i=5; i>=0; --i)
		vga_DrawPetsciiChar((uCharX + 5 - i) << 3, uCharY << 3, g_aHexTable[(uAddress >> (i << 2)) & 15], uColour);

	// Write 16 bytes worth of hex values.
 	for(u32 uIndex=0; uIndex<16; ++uIndex)
	{
    	const u16 uHexPair = byteToHex(pLineBuffer[uIndex]);
		vga_DrawPetsciiChar((uCharX + 8 + (uIndex * 3)) << 3, uCharY << 3, uHexPair >> 8, uColour);
		vga_DrawPetsciiChar((uCharX + 9 + (uIndex * 3)) << 3, uCharY << 3, uHexPair & 255, uColour);

		// Write ASCII or PETSCII version of byte.
		const u8 uCurrentChar = (bASCII) ? ascii_to_petscii(pLineBuffer[uIndex]) : pLineBuffer[uIndex];
		vga_DrawPetsciiChar((uCharX + 57 + uIndex) << 3, uCharY << 3, uCurrentChar, uColour);
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void DisplayDateTime(const u32 uCharX, const u32 uCharY, const char* pszString, const rtc_date rtcDate, const rtc_time rtcTime, const u8 uColour)
{
	char szString[128];
	u32 uOffset = sprintf(szString, "%s    ", pszString);
	szString[uOffset + 0 ] = aWeekDays[rtcDate.m_eDayOfWeek][0];
	szString[uOffset + 1 ] = aWeekDays[rtcDate.m_eDayOfWeek][1];
	szString[uOffset + 2 ] = aWeekDays[rtcDate.m_eDayOfWeek][2];
	szString[uOffset + 3 ] = ' ';
	szString[uOffset + 4 ] = '0' + rtcDate.m_Date.m_uTens;
	szString[uOffset + 5 ] = '0' + rtcDate.m_Date.m_uOnes;
	szString[uOffset + 6 ] = '/';
	szString[uOffset + 7 ] = '0' + rtcDate.m_Month.m_uTens;
	szString[uOffset + 8 ] = '0' + rtcDate.m_Month.m_uOnes;
	szString[uOffset + 9 ] = '/';
	szString[uOffset + 10] = '2';
	szString[uOffset + 11] = '0';
	szString[uOffset + 12] = '0' + rtcDate.m_Year.m_uTens;
	szString[uOffset + 13] = '0' + rtcDate.m_Year.m_uOnes;
	szString[uOffset + 14] = ' ';
	szString[uOffset + 15] = ' ';
	szString[uOffset + 16] = ' ';
	szString[uOffset + 17] = '0' + rtcTime.m_Hours.m_uTens;
	szString[uOffset + 18] = '0' + rtcTime.m_Hours.m_uOnes;
	szString[uOffset + 19] = ':';
	szString[uOffset + 20] = '0' + rtcTime.m_Minutes.m_uTens;
	szString[uOffset + 21] = '0' + rtcTime.m_Minutes.m_uOnes;
	szString[uOffset + 22] = ':';
	szString[uOffset + 23] = '0' + rtcTime.m_Seconds.m_uTens;
	szString[uOffset + 24] = '0' + rtcTime.m_Seconds.m_uOnes;
	szString[uOffset + 25] = '.';
	szString[uOffset + 26] = '0' + rtcTime.m_Hundredths.m_uTens;
	szString[uOffset + 27] = '0' + rtcTime.m_Hundredths.m_uOnes;
	szString[uOffset + 28] = 0;
	vga_DrawString(uCharX, uCharY, szString, uColour);
}

//------------------------------------------------------------------------------------------------
//---- Yep It's Main!                                                                         ----
//------------------------------------------------------------------------------------------------
int main(void)
{
	stdio_init_all();

	vga_Init(PIN_RED, PIN_HSYNC, PIN_VSYNC);
	vga_FilledRect(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB111_GREEN);
	vga_FilledRect(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB111_BLACK);

	if (RTC_Initialise(spi0, SPI_BAUD_RATE, PIN_SPI_CLOCK, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_CS))
	{
		rtc_flags flags = RTC_GetFlags();

		if (!flags.m_bOscillatorRunning)
		{
//			RTC_WriteID("- Hello ", 0);
//			RTC_WriteID("World! -", 1);

//			RTC_WriteEEPROM("This Is ", 1);
//			RTC_WriteEEPROM("A Test..", 2);

			rtc_date rtcCurrentDate;

			// Sunday Janurary 1st 2006 (Non Leap Year)
//			rtcCurrentDate.m_uDate = 0x01;
//			rtcCurrentDate.m_uMonth = 0x01;
//			rtcCurrentDate.m_uYear = 0x06;

			// Tuesday July 4th 2023 (Non Leap Year)
//			rtcCurrentDate.m_uDate = 0x04;
//			rtcCurrentDate.m_uMonth = 0x07;
//			rtcCurrentDate.m_uYear = 0x23;

			// Thursday Feburary 29th 2024 (Leap Year)
			rtcCurrentDate.m_uDate = 0x29;
			rtcCurrentDate.m_uMonth = 0x02;
			rtcCurrentDate.m_uYear = 0x24;
			RTC_SetDate(rtcCurrentDate);

			rtc_time rtcCurrentTime;
			rtcCurrentTime.m_uHours = 0x14;
			rtcCurrentTime.m_uMinutes = 0x15;
			rtcCurrentTime.m_uSeconds = 0x30;
			rtcCurrentTime.m_uHundredths = 0x00;
			RTC_SetTime(rtcCurrentTime, true);
		}
	}

	vga_DrawString(11, 3, "Protected EEPROM", RGB111_YELLOW);
	RTC_ReadID(&s_aIOBuffer[0], 0);
	RTC_ReadID(&s_aIOBuffer[8], 1);
	FormatHexDumpLine(3, 5, 0, s_aIOBuffer, RGB111_WHITE, true);

	u32 uLineIndex = 33;
	vga_DrawString(11, uLineIndex, "SRAM", RGB111_YELLOW);
	uLineIndex += 2;

	u32 uSRAMOffset = 0;
	while (uSRAMOffset < RTC_SRAM_SIZE)
	{
		RTC_ReadSRAM(s_aIOBuffer, uSRAMOffset + RTC_SRAM_BASE, 16);
		FormatHexDumpLine(3, uLineIndex, uSRAMOffset, s_aIOBuffer, RGB111_WHITE, true);
		uSRAMOffset += 16;
		++uLineIndex;
	}

	uLineIndex += 4;
	vga_DrawString(11, uLineIndex, "EEPROM", RGB111_YELLOW);
	uLineIndex += 2;

	u32 uPageIndex = 0;
	while (uPageIndex < (RTC_EEPROM_SIZE >> RTC_PAGE_SHIFT))
	{
		RTC_ReadEEPROM(&s_aIOBuffer[0], uPageIndex);
		RTC_ReadEEPROM(&s_aIOBuffer[8], uPageIndex + 1);
		FormatHexDumpLine(3, uLineIndex, uPageIndex << RTC_PAGE_SHIFT, s_aIOBuffer, RGB111_WHITE, true);
		uPageIndex += 2;
		++uLineIndex;
	}

	char szString[128];

	while(true)
	{
		rtc_flags flags = RTC_GetFlags();

		sprintf(
			szString,
			"Oscillator Running = %s  Trim Enabled = %s",
			flags.m_bOscillatorRunning ? "True " : "False",
			flags.m_bTrimEnabled ? "True " : "False"
		);
		vga_DrawString(11, 9, szString, RGB111_MAGENTA);

		sprintf(
			szString,
			"Output Square Wave = %s",
			flags.m_bOutputSquareWave ? "True " : "False"
		);
		vga_DrawString(11, 11, szString, RGB111_MAGENTA);

		sprintf(
			szString, 
			"Battery Enable = %s      Power Fail = %s",
			flags.m_bBatteryEnabled ? "True " : "False",
			flags.m_bPowerFail ? "True " : "False"
		);
		vga_DrawString(11, 13, szString, RGB111_MAGENTA);

		sprintf(
			szString, 
			"Leap Year = %s",
			flags.m_bLeapYear ? "True " : "False"
		);
		vga_DrawString(11, 15, szString, RGB111_MAGENTA);

		rtc_date rtcCurrentDate = RTC_GetDate();
		rtc_time rtcCurrentTime = RTC_GetTime();
		DisplayDateTime(11, 20, "Current Time", rtcCurrentDate, rtcCurrentTime, RGB111_CYAN);

		if (flags.m_bAlarm0)
		{
		}
		else
		{
			vga_DrawString(11, 22, "Alarm 0         Disabled", RGB111_CYAN);
		}

		if (flags.m_bAlarm1)
		{
		}
		else
		{
			vga_DrawString(11, 24, "Alarm 1         Disabled", RGB111_CYAN);
		}

		vga_DrawString(11, 26, "Power Down Time ----", RGB111_CYAN);
		vga_DrawString(11, 28, "Power  Up  Time ----", RGB111_CYAN);

		sleep_ms(16);
	}
}

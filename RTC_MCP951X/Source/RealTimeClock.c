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
int main()
{
	stdio_init_all();

	vga_Init(PIN_RED, PIN_HSYNC, PIN_VSYNC);
	vga_FilledRect(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB111_GREEN);
	vga_FilledRect(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB111_BLACK);

	if (RTC_Initialise(spi0, SPI_BAUD_RATE, PIN_SPI_CLOCK, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_CS))
	{
		if (!RTC_IsRunning())
		{
//			RTC_WriteID("- Hello ", 0);
//			RTC_WriteID("World! -", 1);

//			RTC_WriteEEPROM("This Is ", 1);
//			RTC_WriteEEPROM("A Test..", 2);

			rtc_date rtcCurrentDate;

			// Sunday Janurary 1st 2006 (Non Leap Year)
//			*(u8*)&rtcCurrentDate.m_Date = 0x01;
//			*(u8*)&rtcCurrentDate.m_Month = 0x01;
//			*(u8*)&rtcCurrentDate.m_Year = 0x06;

			// Tuesday July 4th 2023 (Non Leap Year)
//			*(u8*)&rtcCurrentDate.m_Date = 0x04;
//			*(u8*)&rtcCurrentDate.m_Month = 0x07;
//			*(u8*)&rtcCurrentDate.m_Year = 0x23;

			// Thursday Feburary 29th 2024 (Leap Year)
			*(u8*)&rtcCurrentDate.m_Date = 0x29;
			*(u8*)&rtcCurrentDate.m_Month = 0x02;
			*(u8*)&rtcCurrentDate.m_Year = 0x24;
			RTC_SetDate(rtcCurrentDate);

			rtc_time rtcCurrentTime;
			*(u8*)&rtcCurrentTime.m_Hours = 0x14;
			*(u8*)&rtcCurrentTime.m_Minutes = 0x15;
			*(u8*)&rtcCurrentTime.m_Seconds = 0x30;
			*(u8*)&rtcCurrentTime.m_Hundredths = 0x00;
			RTC_SetTime(rtcCurrentTime, true);
		}
	}

	vga_DrawString(11, 4, "Protected EEPROM", RGB111_YELLOW);
	RTC_ReadID(&s_aIOBuffer[0], 0);
	RTC_ReadID(&s_aIOBuffer[8], 1);
	FormatHexDumpLine(3, 6, 0, s_aIOBuffer, RGB111_WHITE, true);

	u32 uLineIndex = 22;
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

	char szString[64];

	while(true)
	{
		rtc_date rtcCurrentDate = RTC_GetDate();
		szString[0] = aWeekDays[rtcCurrentDate.m_eDayOfWeek][0];
		szString[1] = aWeekDays[rtcCurrentDate.m_eDayOfWeek][1];
		szString[2] = aWeekDays[rtcCurrentDate.m_eDayOfWeek][2];
		szString[3] = ' ';
		szString[4] = '0' + rtcCurrentDate.m_Date.m_uTens;
		szString[5] = '0' + rtcCurrentDate.m_Date.m_uOnes;
		szString[6] = '/';
		szString[7] = '0' + rtcCurrentDate.m_Month.m_uTens;
		szString[8] = '0' + rtcCurrentDate.m_Month.m_uOnes;
		szString[9] = '/';
		szString[10] = '2';
		szString[11] = '0';
		szString[12] = '0' + rtcCurrentDate.m_Year.m_uTens;
		szString[13] = '0' + rtcCurrentDate.m_Year.m_uOnes;
		szString[14] = 0;
		vga_DrawString(11, 12, szString, RGB111_CYAN);

		rtc_time rtcCurrentTime = RTC_GetTime();
		szString[0] = '0' + rtcCurrentTime.m_Hours.m_uTens;
		szString[1] = '0' + rtcCurrentTime.m_Hours.m_uOnes;
		szString[2] = ':';
		szString[3] = '0' + rtcCurrentTime.m_Minutes.m_uTens;
		szString[4] = '0' + rtcCurrentTime.m_Minutes.m_uOnes;
		szString[5] = ':';
		szString[6] = '0' + rtcCurrentTime.m_Seconds.m_uTens;
		szString[7] = '0' + rtcCurrentTime.m_Seconds.m_uOnes;
		szString[8] = '.';
		szString[9] = '0' + rtcCurrentTime.m_Hundredths.m_uTens;
		szString[10] = '0' + rtcCurrentTime.m_Hundredths.m_uOnes;
		szString[11] = 0;
		vga_DrawString(11, 14, szString, RGB111_CYAN);

		if (rtcCurrentDate.m_bLeapYear)
		{
			vga_DrawString(11, 16, "Leap Year = YES", RGB111_CYAN);
		}
		else
		{
			vga_DrawString(11, 16, "Leap Year = NO", RGB111_CYAN);
		}

		sleep_ms(16);
	}
}

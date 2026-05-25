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
		if (RTC_IsRunning())
		{
			vga_FilledRect(10, 10, 20, 20, RGB111_GREEN);
		}
		else
		{
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
			
			vga_FilledRect(10, 10, 20, 20, RGB111_RED);
		}
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
		vga_DrawString(10, 10, szString, RGB111_YELLOW);

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
		vga_DrawString(10, 12, szString, RGB111_YELLOW);

		if (rtcCurrentDate.m_bLeapYear)
		{
			vga_DrawString(10, 14, "Leap Year = YES", RGB111_YELLOW);
		}
		else
		{
			vga_DrawString(10, 14, "Leap Year = NO", RGB111_YELLOW);
		}

		sleep_ms(16);
	}
}

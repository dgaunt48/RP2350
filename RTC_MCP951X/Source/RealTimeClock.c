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

//Zeller's Congruence To Get Day Of Week From Date

enum board_pins
{
	PIN_RED = 0, PIN_GREEN, PIN_BLUE, PIN_HSYNC, PIN_VSYNC,
	PIN_SPI_MOSI = 16, PIN_SPI_CS, PIN_SPI_CLOCK, PIN_SPI_MISO
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
		vga_FilledRect(10, 10, 20, 20, RGB111_CYAN);
	}

	// s_rtcTime.m_bOutputSquareWave = true;
	// s_rtcTime.m_uSquareWaveOutputFrequency = RCT_OUTPUT_SQUARE_WAVE_32768Hz;
	// s_rtcTime.m_bStartOscillator = true;

	// RTC_WriteSRAM((void*)&s_rtcTime.m_uRegControl, RTC_REG_CONTROL, 1);
	// if (RTC_WriteSRAM((void*)&s_rtcTime.m_uRegSeconds, RTC_REG_SEC, 1))

	while(true)
	{
		sleep_ms(16);
	}
}

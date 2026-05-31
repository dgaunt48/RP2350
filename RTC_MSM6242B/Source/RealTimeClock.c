//------------------------------------------------------------------------------------------------
//---- RealTimeClock.c (C) 2026 Dave Gaunt                                                    ----
//------------------------------------------------------------------------------------------------
//---- v1.0 - Connect A MSM6242B Real Time Clock To The RP2350                                ----
//------------------------------------------------------------------------------------------------

#include <stdio.h>
#include "types.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"

#include "vga111.h"

#define RTC_CLOCK_SPEED	(32767)		/* Gives Me 32768.1 On The Prototype Board */

enum board_pins
{
	PIN_RED = 0, PIN_GREEN, PIN_BLUE,
	PIN_CS, PIN_CS_HIGH = 5,
	PIN_HSYNC = 8, PIN_VSYNC,
	PIN_READ, PIN_WRITE,
	PIN_A0, PIN_A1, PIN_A2, PIN_A3,
	PIN_D0, PIN_D1, PIN_D2, PIN_D3,
	PIN_ALE,
	PIN_32768HZ_CLOCK
};

static_assert(21 == PIN_32768HZ_CLOCK, "Pico only exposes a few pins for external clocks!");

enum registers_msm6242b
{
	MSM6242B_SECONDS_ONES = 0,
	MSM6242B_SECONDS_TENS,
	MSM6242B_MINUTES_ONES,
	MSM6242B_MINUTES_TENS,
	MSM6242B_HOURS_ONES,
	MSM6242B_HOURS_TENS,
	MSM6242B_DAY_ONES,
	MSM6242B_DAY_TENS,
	MSM6242B_MONTH_ONES,
	MSM6242B_MONTH_TENS,
	MSM6242B_YEAR_ONES,
	MSM6242B_YEAR_TENS,
	MSM6242B_WEEK_DAY,
	MSM6242B_CTRL_D,
	MSM6242B_CTRL_E,
	MSM6242B_CTRL_F
};

typedef struct
{
	union
	{
		u8	m_Seconds_Reg;
		struct
		{
			u8	m_uOnes : 4;
			u8	m_uTens : 3;
			u8			: 1;
		} m_Seconds;
	};
	union
	{
		u8	m_Minutes_Reg;
		struct
		{
			u8	m_uOnes : 4;
			u8	m_uTens : 3;
			u8			: 1;
		} m_Minutes;
	};
	union
	{
		u8	m_Hours_Reg;
		struct
		{
			u8	m_uOnes : 4;
			u8	m_uTens : 2;
			u8	m_bPM	: 1;
			u8			: 1;
		} m_Hours;
	};

	u32	m_2;
} rtc_msm6242b;
static_assert(sizeof(rtc_msm6242b) == 8);

static volatile rtc_msm6242b s_rtcRegisters;

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
//---- 				                                                                          ----
//------------------------------------------------------------------------------------------------
u8 register_read(const enum registers_msm6242b eRegister)
{
	gpio_put_masked(0xF << PIN_A0, eRegister << PIN_A0);
    gpio_put(PIN_CS, false);
	gpio_set_dir_in_masked(0xF << PIN_D0);
	delay_40ns();					// Address Stable For Min 20ns Before Read
    gpio_put(PIN_READ, false);
	delay_120ns();					// Data Ready In Maximum 120ns
	const u32 uValue = gpio_get_all();
    gpio_put(PIN_CS, true);
	return (uValue >> PIN_D0) & 0xf;
}

//------------------------------------------------------------------------------------------------
//---- 				                                                                          ----
//------------------------------------------------------------------------------------------------
void register_write(const enum registers_msm6242b eRegister, const u32 uValue)
{
	gpio_put_masked(0xF << PIN_A0, eRegister << PIN_A0);
    gpio_put(PIN_CS, false);
	delay_40ns();					// Address Stable For Min 20ns Before Write
    gpio_put(PIN_WRITE, false);
	gpio_set_dir_in_masked(0xF << PIN_D0);
	gpio_put_masked(0xF << PIN_D0, uValue << PIN_D0);
	delay_120ns();					// Write Pulse Width Minimum 120ns
    gpio_put(PIN_WRITE, true);
    gpio_put(PIN_CS, true);
}

//------------------------------------------------------------------------------------------------
//---- Yep It's Main!                                                                         ----
//------------------------------------------------------------------------------------------------
int main(void)
{
	stdio_init_all();

	gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, true);

	gpio_init(PIN_CS_HIGH);
    gpio_set_dir(PIN_CS_HIGH, GPIO_OUT);
    gpio_put(PIN_CS_HIGH, false);

	gpio_init(PIN_READ);
    gpio_set_dir(PIN_READ, GPIO_OUT);
    gpio_put(PIN_READ, true);

	gpio_init(PIN_WRITE);
    gpio_set_dir(PIN_WRITE, GPIO_OUT);
    gpio_put(PIN_WRITE, true);

	gpio_init(PIN_ALE);
    gpio_set_dir(PIN_ALE, GPIO_OUT);
    gpio_put(PIN_ALE, true);

	for(u32 i=0; i<4; ++i)
	{
		gpio_init(PIN_A0 + i);
		gpio_set_dir(PIN_A0 + i, GPIO_OUT);
	    gpio_put(PIN_A0 + i, false);

		gpio_init(PIN_D0 + i);
		gpio_set_dir(PIN_D0 + i, GPIO_IN);
	    gpio_put(PIN_D0 + i, false);
	}
	
	// Start Clock
	clock_gpio_init(PIN_32768HZ_CLOCK, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, ((float)SYS_CLK_HZ / (float)RTC_CLOCK_SPEED));
    gpio_put(PIN_CS_HIGH, true);
	
	vga_Init(PIN_RED, PIN_HSYNC, PIN_VSYNC);
	vga_FilledRect(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB111_GREEN);
	vga_FilledRect(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB111_BLACK);

	char szString[128];

	register_write(MSM6242B_CTRL_D, 0x0);
	register_write(MSM6242B_CTRL_E, 0x0);
	register_write(MSM6242B_CTRL_F, 0x0);

	while(true)
	{
		s_rtcRegisters.m_Minutes_Reg = (register_read(MSM6242B_MINUTES_TENS) << 4) | register_read(MSM6242B_MINUTES_ONES);
		s_rtcRegisters.m_Seconds_Reg = (register_read(MSM6242B_SECONDS_TENS) << 4) | register_read(MSM6242B_SECONDS_ONES);
		s_rtcRegisters.m_Hours_Reg = (register_read(MSM6242B_HOURS_TENS) << 4) | register_read(MSM6242B_HOURS_ONES);

		u32 uOffset = sprintf(szString, "Time ");
		szString[uOffset + 0] = '0' + s_rtcRegisters.m_Hours.m_uTens;
		szString[uOffset + 1] = '0' + s_rtcRegisters.m_Hours.m_uOnes;
		szString[uOffset + 2] = ':';
		szString[uOffset + 3] = '0' + s_rtcRegisters.m_Minutes.m_uTens;
		szString[uOffset + 4] = '0' + s_rtcRegisters.m_Minutes.m_uOnes;
		szString[uOffset + 5] = ':';
		szString[uOffset + 6] = '0' + s_rtcRegisters.m_Seconds.m_uTens;
		szString[uOffset + 7] = '0' + s_rtcRegisters.m_Seconds.m_uOnes;
		szString[uOffset + 8] = 0;
		vga_DrawString(10, 10, szString, RGB111_CYAN);
		sleep_ms(16);
	}
}

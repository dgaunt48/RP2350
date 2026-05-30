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

void write(const u32 uRegister, const u32 uValue)
{
	gpio_put_masked(0xF << PIN_A0, uRegister << PIN_A0);
    gpio_put(PIN_CS, false);
	busy_wait_at_least_cycles(100);
    gpio_put(PIN_WRITE, false);
	busy_wait_at_least_cycles(100);
	gpio_set_dir_in_masked(0xF << PIN_D0);
	gpio_put_masked(0xF << PIN_D0, uValue << PIN_D0);
	busy_wait_at_least_cycles(100);
    gpio_put(PIN_WRITE, true);
	busy_wait_at_least_cycles(100);
    gpio_put(PIN_CS, true);
	busy_wait_at_least_cycles(100);
	gpio_set_dir_out_masked(0xF << PIN_D0);
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

	write(0xd, 0x0);
	write(0xe, 0x0);
	write(0xf, 0x0);

	gpio_put_masked(0xF << PIN_A0, 0 << PIN_A0);
	gpio_put(PIN_CS, false);

	while(true)
	{
		busy_wait_at_least_cycles(1000);
		gpio_put(PIN_READ, false);
		busy_wait_at_least_cycles(1000);
		gpio_put(PIN_READ, true);
	}
}

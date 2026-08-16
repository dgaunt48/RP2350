//------------------------------------------------------------------------------------------------
//---- SelfTest_40GPIO.c (C) 2026 Dave Gaunt                                                  ----
//------------------------------------------------------------------------------------------------
//---- v1.0 - 										                                          ----
//------------------------------------------------------------------------------------------------

#include <stdio.h>
#include "types.h"
#include "pico/stdlib.h"

#include "vga111.h"

enum board_pins
{
	PIN_RED = 0, PIN_GREEN, PIN_BLUE, PIN_HSYNC = 8, PIN_VSYNC
};

// Pins are the centre 20, both ends wrap so i don't have to do mod 20 in throughout the tests.
static const u8 s_aLeftPins[22] =  {27,03,05,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,03};
static const u8 s_aRightPins[22] = {28,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,47};
static bool s_aPassFail[48];

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
int main()
{
	stdio_init_all();

	// Left 20 Pins Output 0 / Right 20 Pins Input
	for(u32 i=0; i<20; ++i)
	{
		gpio_init(s_aLeftPins[i+1]);
		gpio_set_dir(s_aLeftPins[i+1], GPIO_OUT);
		gpio_put(s_aLeftPins[i+1], false);
		gpio_init(s_aRightPins[i+1]);
		gpio_set_dir(s_aRightPins[i+1], GPIO_IN);
	}

	vga_Init(PIN_RED, PIN_HSYNC, PIN_VSYNC);
	vga_FilledRect(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB111_GREEN);
	vga_FilledRect(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB111_BLACK);

	// Default all pass fail flags to true
	for(u32 i=0; i<48; ++i)
		s_aPassFail[i] = true;
	
	for(u32 i=0; i<20; ++i)
	{
		// Set Left Pin To Assert High
		gpio_put(s_aLeftPins[i+1], true);
		sleep_us(1);

		if (false != gpio_get(s_aRightPins[i]))
			s_aPassFail[s_aRightPins[i]] = false;

		// Check The Right Pin and the ones to both sides of it
		if (true != gpio_get(s_aRightPins[i+1]))
			s_aPassFail[s_aRightPins[i+1]] = false;

		if (false != gpio_get(s_aRightPins[i+2]))
			s_aPassFail[s_aRightPins[i+2]] = false;

		// Return The Left Pin To Low
		gpio_put(s_aLeftPins[i+1], false);
	}

	// All Pins Back To Input
	for(u32 i=0; i<20; ++i)
		gpio_set_dir(s_aLeftPins[i+1], GPIO_IN);

	// Right 20 Pins Output 0
	for(u32 i=0; i<20; ++i)
	{
		gpio_set_dir(s_aRightPins[i+1], GPIO_OUT);
		gpio_put(s_aRightPins[i+1], false);
	}

	// Repeat Test From Right To Left
	for(u32 i=0; i<20; ++i)
	{
		// Set Right Pin To Assert High
		gpio_put(s_aRightPins[i+1], true);
		sleep_us(1);

		if (false != gpio_get(s_aLeftPins[i]))
			s_aPassFail[s_aLeftPins[i]] = false;

		// Check The Left Pin and the ones to both sides of it
		if (true != gpio_get(s_aLeftPins[i+1]))
			s_aPassFail[s_aLeftPins[i+1]] = false;

		if (false != gpio_get(s_aLeftPins[i+2]))
			s_aPassFail[s_aLeftPins[i+2]] = false;

		// Return The Left Pin To Low
		gpio_put(s_aRightPins[i+1], false);
	}

	vga_DrawString(21, 4, "Self Test rp2350B 40 GPIO Board", RGB111_CYAN);

	for(u32 i=0; i<20; ++i)
	{
		if (true == s_aPassFail[s_aLeftPins[i+1]])
			vga_DrawString(20, i + 10, "Pass", RGB111_GREEN);
		else
			vga_DrawString(20, i + 10, "Fail", RGB111_RED);

		if (true == s_aPassFail[s_aRightPins[i+1]])
			vga_DrawString(50, i + 10, "Pass", RGB111_GREEN);
		else
			vga_DrawString(50, i + 10, "Fail", RGB111_RED);
	}

	while(true)
	{
		sleep_ms(16);
	}
}

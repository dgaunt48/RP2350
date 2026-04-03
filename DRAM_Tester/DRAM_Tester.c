//------------------------------------------------------------------------------------------------
//---- DRAM Tester 						                                                      ----
//------------------------------------------------------------------------------------------------
//---- Version 0.1                                                                            ----
//---- Code Based On https://github.com/schlae/pico-dram-tester.git							  ----
//---- Vastly Cut Down, Just Learning How The Pico PIO Works								  ----
//------------------------------------------------------------------------------------------------
#include <stdio.h>
#include "types.h"

#include "pico/stdlib.h"
#include "lcd7789.h"

// #include "hardware/pio.h"
// #include "hardware/dma.h"

enum device_pins {
	PIN_RED = 0,
	PIN_GREEN,
	PIN_BLUE,
	PIN_HSYNC = 8,
	PIN_VSYNC,

	PIN_CLK = 23
};

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
int main()
{
	stdio_init_all();
	lcd7789_Init();

	// gpio_init(PIN_RESET);						// Put The VIA Into Reset
	// gpio_set_dir(PIN_RESET, GPIO_OUT);
	// gpio_put(PIN_RESET, false);

	while(true)
	{
		sleep_ms(16);
	}
}

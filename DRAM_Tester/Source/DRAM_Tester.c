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
#include "hardware/pio.h"

#include "mem_chip.h"
#include "pio_patcher.h"

PIO pio;
uint sm = 0;
uint offset; // Returns offset of starting instruction

#include "ram4164.pio.h"
#include "ram41256.pio.h"

enum device_pins {
	PIN_CLK = 23
};

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
int main()
{
	stdio_init_all();
	lcd7789_Init();

	lcd7789_Fill(0, 0, 240, 135, 0x0540);

	ram41256_setup_pio(5, 0);
    sleep_ms(10);

	u32 j=0;

    for (u32 i=0; i < 131072; ++i)
	{
		if (j != ((i >> 14) & 1))
		{
			j = (i >> 14) & 1;
			lcd7789_Fill(0, 0, 240, 135, (j) ? 0x57ea : 0x0540);
		}

        ram41256_ram_write(i, 1);
        if (ram41256_ram_read(i) != 1)
		{
			lcd7789_Fill(0, 0, 240, 135, 0x52bf);
			break;
		}

		ram41256_ram_write(i, 0);

        if (ram41256_ram_read(i) != 0)
		{
			lcd7789_Fill(0, 0, 240, 135, 0x52bf);
			break;
		}
    }

	while(true)
	{
		sleep_ms(16);
	}
}

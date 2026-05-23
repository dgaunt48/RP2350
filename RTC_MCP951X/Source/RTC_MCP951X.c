//------------------------------------------------------------------------------------------------
//---- RTC_MCP951X.c (C) 2026 Dave Gaunt                                                      ----
//------------------------------------------------------------------------------------------------
//---- v1.0 - Connect a MCP951X	Real Time Clock To The RP2350                                 ----
//------------------------------------------------------------------------------------------------

#include <stdio.h>
#include "types.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "vga111.h"

#define SPI_BAUD_RATE	(5 * 1000 * 1000)	/* 5Mhz */

//32.7695 with 10pf load caps

enum board_pins
{
	PIN_RED = 0, PIN_GREEN, PIN_BLUE, PIN_HSYNC, PIN_VSYNC,
	PIN_SPI_MOSI = 16, PIN_SPI_CS, PIN_SPI_CLOCK, PIN_SPI_MISO
};

enum rtc_commands
{
	RTC_SRWRITE = 1,
	RTC_EEWRITE,
	RTC_EEREAD,
	RTC_EEWRDI,
	RTC_SRREAD,
	RTC_EEWREN,
	RTC_WRITE = 18,
	RTC_READ,
	RTC_UNLOCK,
	RTC_IDWRITE = 50,
	RTC_IDREAD,
	RTC_CLRRAM = 84
};

enum rtc_registers
{
	RTC_REG_HSEC = 0,
	RTC_REG_SEC,
	RTC_REG_MIN,
	RTC_REG_HOUR,
	RTC_REG_WKDAY,
	RTC_REG_DATE,
	RTC_REG_MTH,
	RTC_REG_YEAR,
	RTC_REG_CONTROL,
	RTC_REG_OSCTRIM,
	RTC_ALM0_SEC = 12,
	RTC_ALM0_MIN,
	RTC_ALM0_HOUR,
	RTC_ALM0_WKDAY,
	RTC_ALM0_DATE,
	RTC_ALM0_MTH,
	RTC_ALM1_HSEC,
	RTC_ALM1_SEC,
	RTC_ALM1_MIN,
	RTC_ALM1_HOUR,
	RTC_ALM1_WKDAY,
	RTC_ALM1_DATE,
	RTC_PWRDN_MIN,
	RTC_PWRDN_HOUR,
	RTC_PWRDN_DATE,
	RTC_PWRDN_MTH,
	RTC_PWRUP_MIN,
	RTC_PWRUP_HOUR,
	RTC_PWRUP_DATE,
	RTC_PWRUP_MTH
};

static spi_inst_t* s_pSpi = 0;
static u32 s_uCsMask = 0;

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
const u8 RTC_ReadRegister(const u8 uRegister)
{
    delay_40ns();
	gpio_clr_mask(s_uCsMask);
	u32 uRecieveBuffer;
	const u32 uSendBuffer = (u32)uRegister << 8 | RTC_READ;
	const u32 uBytesWritten = spi_write_read_blocking(s_pSpi, (u8*)&uSendBuffer, (u8*)&uRecieveBuffer, 3);
	gpio_set_mask(s_uCsMask);
	return(uRecieveBuffer >> 16);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_WriteRegister(const u8 uRegister, const u8 uValue)
{
    delay_40ns();
	gpio_clr_mask(s_uCsMask);
	const u32 uSendBuffer = (u32)uValue << 16 | (u32)uRegister << 8 | RTC_WRITE;
	const u32 uBytesWritten = spi_write_blocking(s_pSpi, (u8*)&uSendBuffer, 3);
	gpio_set_mask(s_uCsMask);
	return(3 == uBytesWritten);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_Initialise(spi_inst_t* pSpi, const u32 uBaudRate, const u32 uClkPin, const u32 uTxPin, const u32 uRxPin, const u32 uCsPin)
{
    assert(0 == s_pSpi);
    s_pSpi = pSpi;
    s_uCsMask = (1 << uCsPin);

    assert(uBaudRate <= 10000000);			// Max Baud Rate For RTC Is 10MHz
    spi_init(pSpi, uBaudRate);

	gpio_set_dir(uCsPin, GPIO_OUT);
    gpio_clr_mask(s_uCsMask);

    gpio_set_function(uCsPin, GPIO_FUNC_SIO);
    gpio_set_function(uClkPin, GPIO_FUNC_SPI);
    gpio_set_function(uTxPin, GPIO_FUNC_SPI);
    gpio_set_function(uRxPin, GPIO_FUNC_SPI);

	delay_40ns();							// Pulse The CS Line Once
	gpio_clr_mask(s_uCsMask);
	delay_40ns();
	gpio_set_mask(s_uCsMask);


/*	
	u32 uSendBuffer = 0;
	u32 uRecieveBuffer = 0;

	gpio_clr_mask(s_uCsMask);
	uSendBuffer = 0xDA7E2000 | RTC_WRITE;
	spi_write_blocking(s_pSpi, (u8*)&uSendBuffer, 4);
	gpio_set_mask(s_uCsMask);
    delay_40ns();

	gpio_clr_mask(s_uCsMask);
	uSendBuffer = 0x2000 | RTC_READ;
	spi_write_read_blocking(s_pSpi, (u8*)&uSendBuffer, (u8*)&uRecieveBuffer, 4);
	gpio_set_mask(s_uCsMask);
    delay_40ns();
*/
	return true;
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

	RTC_Initialise(spi0, SPI_BAUD_RATE, PIN_SPI_CLOCK, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_CS);

	RTC_WriteRegister(0x08, 0b01000011);
	if (RTC_WriteRegister(0x01, 0b10000000))
	{
		vga_FilledRect(10, 10, 20, 20, RGB111_YELLOW);
	}



	// if(SpiNorFlash_Initialise(spi0, SPI_BAUD_RATE, PIN_SPI_CLOCK, PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_CS))
	// {
	// 	vga_FilledRect(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB111_GREEN);

	// 	if (SpiNorFlash_Verify(0, iCE40_BitStream_size, iCE40_BitStream))
	// 	{
	// 		// Flash Data Verified And Correct - Nothing To Do !!!
	// 		vga_FilledRect(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB111_BLACK);
	// 		vga_DrawString(4, 4, "FPGA Binary Valid.", RGB111_GREEN);
	// 	}
	// 	else
	// 	{
	// 		// Erase Flash Memory And Attempt To Rewrite Data.
	// 		SpiNorFlash_Erase64kBlock(0);
	// 		SpiNorFlash_Erase64kBlock(1);
	// 		SpiNorFlash_Erase64kBlock(2);

	// 		if (SpiNorFlash_Write(0, iCE40_BitStream_size, iCE40_BitStream, true))
	// 		{
	// 			// Rewrite Success - Flash Data Is Valid And Up To Date.
	// 			vga_FilledRect(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB111_BLACK);
	// 			vga_DrawString(4, 4, "FPGA Binary Updated And Valid.", RGB111_GREEN);
	// 		}
	// 		else
	// 		{
	// 			// Rewrite Failed - Flash Data Is Corrupt!!!
	// 			vga_FilledRect(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB111_RED);
	// 			vga_FilledRect(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB111_BLACK);
	// 			vga_DrawString(4, 4, "FPGA Binary Corrupt!!!", RGB111_RED);
	// 		}
	// 	}
	// }
	// else
	// {
	// 	vga_DrawString(4, 4, "Can't Communicate With Flash ROM!!!", RGB111_RED);
	// }

	while(true)
	{
		sleep_ms(16);
	}
}

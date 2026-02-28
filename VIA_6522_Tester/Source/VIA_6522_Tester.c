//------------------------------------------------------------------------------------------------
//---- VIA 6522 Tester ... 2026 Dave Gaunt                                                    ----
//------------------------------------------------------------------------------------------------
//---- Version 0.1                                                                            ----
//------------------------------------------------------------------------------------------------
#include <stdio.h>
#include "types.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

#include "hsync.pio.h"
#include "vsync.pio.h"
#include "rgb.pio.h"

#include "VicChars.h"

#define VGA_RESOLUTION_X    	(640)
#define VGA_RESOLUTION_Y  		(480)
#define TERMINAL_CHARS_WIDE		(VGA_RESOLUTION_X >> 3)
#define TERMINAL_CHARS_HIGH		(VGA_RESOLUTION_Y >> 3)

#define VIC_PAL_CLOCK       	(4433618)
#define VIC_CPU_CLOCK			(VIC_PAL_CLOCK >> 2)

enum device_pins {
	PIN_RED = 0,
	PIN_GREEN,
	PIN_BLUE,
	PIN_S02_READ,
	PIN_HSYNC = 8,
	PIN_VSYNC,

	PIN_RESET,
	PIN_ADDRESS_CS1,		/* ADDRESS BIT 4 OR 5 On VIC */
	PIN_IO0,				/* CS2 ACTIVE LOW */
	PIN_READ_WRITE,
	PIN_IRQ,

	PIN_DATA_BIT0,
	PIN_DATA_BIT1,
	PIN_DATA_BIT2,
	PIN_DATA_BIT3,
	PIN_DATA_BIT4,
	PIN_DATA_BIT5,
	PIN_DATA_BIT6,
	PIN_DATA_BIT7,

	PIN_CLK,				/* S02 - Data Transfer Occurs Only When Phase 2 Clock Is High */
	PIN_ADDRESS_BIT0,
	PIN_ADDRESS_BIT1,
	PIN_ADDRESS_BIT2,
	PIN_ADDRESS_BIT3,

	PIN_PORT_A = 32,
	PIN_PORT_B = 40
};

static_assert(23 == PIN_CLK, "Clock must be on PIN 23!");

typedef struct
{
	union
	{
		u8 m_aReg[16];
		struct
		{
			u8 m_u8PortB;					/* 0 */
			u8 m_u8PortA;					/* 1 */
			u8 m_uDataDirB;					/* 2 */
			u8 m_uDataDirA;					/* 3 */
			union
			{
				u16	m_uTimer1;
				struct
				{
					u8 m_uTimer1_L;			/* 4 */
					u8 m_uTimer1_H;			/* 5 */
				};
			};
			union
			{
				u16	m_uTimer1_Latch;
				struct
				{
					u8 m_uTimer1_Latch_L;	/* 6 */
					u8 m_uTimer1_Latch_H;	/* 7 */
				};
			};
			union
			{
				u16	m_uTimer2;
				struct
				{
					u8 m_uTimer2_L;			/* 8 */
					u8 m_uTimer2_H;			/* 9 */
				};
			};
			u8 m_uShiftReg;					/* A */
			u8 m_uAuxiliaryCtrl;			/* B */
			u8 m_uPeripheralCtrl;			/* C */
			u8 m_uInterruptFlags;			/* D */
			u8 m_uInterruptEnable;			/* E */
			u8 m_u8PortA_NoHandshake;		/* F */
		};
	};
} ViaRegisters;
static_assert(sizeof(ViaRegisters) == 16);

static volatile ViaRegisters s_viaRegs = {0};

typedef struct
{
	u8	m_uOffset;
	u8	m_uData;
} RegisterBuffer;

#define VIA_RING_BUFFER_SIZE	(64)			/* Must Be A Power Of 2! */
static volatile RegisterBuffer s_aRegBuffer[VIA_RING_BUFFER_SIZE];
static volatile u8 s_uRegHead = VIA_RING_BUFFER_SIZE - 1;
static volatile u8 s_uRegTail = VIA_RING_BUFFER_SIZE - 1;

enum rgbColours {RGB_BLACK, RGB_RED, RGB_GREEN, RGB_YELLOW, RGB_BLUE, RGB_MAGENTA, RGB_CYAN, RGB_WHITE};

u8 volatile aVGAScreenBuffer[(VGA_RESOLUTION_X * VGA_RESOLUTION_Y) >> 1];
volatile u8* address_pointer = aVGAScreenBuffer;

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
enum via_register_names
{
	VIA_REG_PORTB = 0,
	VIA_REG_PORTA,
	VIA_REG_DATA_DIRB,
	VIA_REG_DATA_DIRA,
	VIA_REG_TIMER1_L,
	VIA_REG_TIMER1_H,
	VIA_REG_TIMER1_LATCH_L,
	VIA_REG_TIMER1_LATCH_H,
	VIA_REG_TIMER2_L,
	VIA_REG_TIMER2_H,
	VIA_REG_SHIFT,
	VIA_REG_AUXILIARY_CONTROL,
	VIA_REG_PERIPHERAL_CONTROL,
	VIA_REG_INTERRUPT_FLAGS,
	VIA_REG_INTERRUPT_ENABLE,
	VIA_REG_PORTA_NO_HANDSHAKE
};

// Register Name Strings For Debug View.
static const char s_aszRegisterNames[16][16] =
{
/*  "123456789ABCDEF"	*/
	"Port B",
	"Port A",
	"Dir B",
	"Dir A",
	"Timer 1 L",
	"Timer 1 H",
	"T1 Latch L",
	"T1 Latch H",
	"Timer 2 L",
	"Timer 2 H",
	"Shift Reg",
	"Aux Ctrl",
	"Periph Ctrl",
	"Int Flags",
	"Int Enable",
	"PA No HShake"
/*  "123456789ABCDEF"	*/
};

enum via_irq_flags
{
	VIA_IRQ_CA2 = 0,
	VIA_IRQ_CA1,
	VIA_IRQ_SHIFT,
	VIA_IRQ_CB2,
	VIA_IRQ_CB1,
	VIA_IRQ_TIMER2,
	VIA_IRQ_TIMER1,
	VIA_IRQ_SET_CLR
};

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void initVGA()
{
	// Choose which PIO instance to use (there are two instances, each with 4 state machines)
	PIO pio = pio0;
	const uint hsync_offset = pio_add_program(pio, &hsync_program);
	const uint vsync_offset = pio_add_program(pio, &vsync_program);
	const uint rgb_offset = pio_add_program(pio, &rgb_program);

	// Manually select a few state machines from pio instance pio0.
	uint hsync_sm = 0;
	uint vsync_sm = 1;
	uint rgb_sm = 2;
	hsync_program_init(pio, hsync_sm, hsync_offset, PIN_HSYNC);
	vsync_program_init(pio, vsync_sm, vsync_offset, PIN_VSYNC);
	rgb_program_init(pio, rgb_sm, rgb_offset, PIN_RED);

	/////////////////////////////////////////////////////////////////////////////////////////////////////
	// ============================== PIO DMA Channels =================================================
	/////////////////////////////////////////////////////////////////////////////////////////////////////

	// DMA channels - 0 sends color data, 1 reconfigures and restarts 0
	int rgb_chan_0 = 0;
	int rgb_chan_1 = 1;

	// Channel Zero (sends color data to PIO VGA machine)
	dma_channel_config c0 = dma_channel_get_default_config(rgb_chan_0);  	// default configs
	channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);              	// 8-bit txfers
	channel_config_set_read_increment(&c0, true);                        	// yes read incrementing
	channel_config_set_write_increment(&c0, false);                      	// no write incrementing
	channel_config_set_dreq(&c0, DREQ_PIO0_TX2) ;                        	// DREQ_PIO0_TX2 pacing (FIFO)
	channel_config_set_chain_to(&c0, rgb_chan_1);                        	// chain to other channel

	dma_channel_configure
	(
		rgb_chan_0,                                                        	// Channel to be configured
		&c0,                                                               	// The configuration we just created
		&pio->txf[rgb_sm],                                                 	// write address (RGB PIO TX FIFO)
		&aVGAScreenBuffer,                                                 	// The initial read address (pixel color array)
		(VGA_RESOLUTION_X * VGA_RESOLUTION_Y) >> 1,                        	// Number of transfers; in this case each is 1 byte.
		false                                                              	// Don't start immediately.
	);

	// Channel One (reconfigures the first channel)
	dma_channel_config c1 = dma_channel_get_default_config(rgb_chan_1);  	// default configs
	channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);             	// 32-bit txfers
	channel_config_set_read_increment(&c1, false);                       	// no read incrementing
	channel_config_set_write_increment(&c1, false);                      	// no write incrementing
	channel_config_set_chain_to(&c1, rgb_chan_0);                        	// chain to other channel

	dma_channel_configure
	(
		rgb_chan_1,                         	// Channel to be configured
		&c1,                                	// The configuration we just created
		&dma_hw->ch[rgb_chan_0].read_addr,  	// Write address (channel 0 read address)
		&address_pointer,                   	// Read address (POINTER TO AN ADDRESS)
		1,                                 	 	// Number of transfers, in this case each is 4 byte
		false                               	// Don't start immediately.
	);

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////////////////////////

	// Initialize PIO state machine counters. This passes the information to the state machines
	// that they retrieve in the first 'pull' instructions, before the .wrap_target directive
	// in the assembly. Each uses these values to initialize some counting registers.
	#define H_ACTIVE   655    // (active + frontporch - 1) - one cycle delay for mov
	#define V_ACTIVE   479    // (active - 1)
	#define RGB_ACTIVE 319    // (horizontal active)/2 - 1
	// #define RGB_ACTIVE 639 // change to this if 1 pixel/byte
	pio_sm_put_blocking(pio, hsync_sm, H_ACTIVE);
	pio_sm_put_blocking(pio, vsync_sm, V_ACTIVE);
	pio_sm_put_blocking(pio, rgb_sm, RGB_ACTIVE);

	// Start the two pio machine IN SYNC
	// Note that the RGB state machine is running at full speed,
	// so synchronization doesn't matter for that one. But, we'll
	// start them all simultaneously anyway.
	pio_enable_sm_mask_in_sync(pio, ((1u << hsync_sm) | (1u << vsync_sm) | (1u << rgb_sm)));

	// Start DMA channel 0. Once started, the contents of the pixel color array
	// will be continously DMA's to the PIO machines that are driving the screen.
	// To change the contents of the screen, we need only change the contents
	// of that array.
	dma_start_channel_mask((1u << rgb_chan_0)) ;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void FilledRectangle(u32 uPositionX, u32 uPositionY, u32 uWidth, u32 uHeight, u32 uColour)
{
	if (uPositionX + uWidth >= VGA_RESOLUTION_X)
		uWidth = VGA_RESOLUTION_X - uPositionX;

	if (uPositionY + uHeight >= VGA_RESOLUTION_Y)
		uHeight = VGA_RESOLUTION_Y - uPositionY;

	if ((uWidth > 0) && (uHeight > 0))
	{
		u32 uPixelOffset = ((uPositionY * VGA_RESOLUTION_X) + uPositionX) >> 1;

		if (uPositionX & 1)
		{
			u32 uOffset = uPixelOffset++;
			--uWidth;

			for(u32 y=0; y<uHeight; ++y)
			{
				aVGAScreenBuffer[uOffset] = (aVGAScreenBuffer[uOffset] & 0b11000111) | (uColour << 3);
				uOffset += VGA_RESOLUTION_X >> 1;
			}
		}

		while (uWidth > 1)
		{
		u32 uOffset = uPixelOffset++;
		uWidth -= 2;

		for(u32 y=0; y<uHeight; ++y)
		{
			aVGAScreenBuffer[uOffset] = (uColour << 3) | uColour;
			uOffset += VGA_RESOLUTION_X >> 1;
		}
		}

		if (1 == uWidth)
		{
			for(u32 y=0; y<uHeight; ++y)
			{
				aVGAScreenBuffer[uPixelOffset] = (aVGAScreenBuffer[uPixelOffset] & 0b11111000) | uColour;
				uPixelOffset += VGA_RESOLUTION_X >> 1;
			}
		}
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void DrawPetsciiChar(const u32 uXPos, const u32 uYPos, const u8 uChar, const u8 uColour)
{
	for (u32 uLine=0; uLine<8; ++uLine)
	{
		u32 uPixelOffset = ((((uYPos + uLine) * VGA_RESOLUTION_X ) + uXPos) >> 1) + 3;
		u32 uCharLine = VicChars901460_03[2048 + (uChar << 3) + uLine];

		for (u32 x=0; x<4; ++x)
		{
			u8 uPixelPair = 0;

			if (uCharLine & 2)
				uPixelPair = uColour;

			if (uCharLine & 1)
				uPixelPair |= (uColour << 3);

			aVGAScreenBuffer[uPixelOffset--] = uPixelPair;
			uCharLine >>= 2;
		}
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void DrawString(uint32_t uCharX, uint32_t uCharY, const char* pszString, const uint8_t uColour)
{
	while (*pszString)
	{
		if (uCharX >= (TERMINAL_CHARS_WIDE-1))
		{
			uCharX = 1;
			++uCharY;
		}

		if (uCharY >= (TERMINAL_CHARS_HIGH-1))
			return;

		uint8_t c = *pszString++;

		if (c >= '`')
			c -= '`';

		DrawPetsciiChar(uCharX << 3, uCharY << 3, c, uColour);
		++uCharX;
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static const uint8_t aHexTable[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

static inline uint16_t byteToHex(const uint8_t uByte)
{
	return (aHexTable[(uByte >> 4) & 15] << 8) | aHexTable[uByte & 15];
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static void PushVIARegister(const u8 uRegister, const u8 uValue)
{
	// Push Register Set Onto Ring Buffer.
	const u8 uRegTail = (s_uRegTail + 1) & (VIA_RING_BUFFER_SIZE - 1);
	assert(uRegTail != s_uRegHead);		// Ring Buffer Is Full !!!
	s_aRegBuffer[uRegTail].m_uOffset = uRegister;
	s_aRegBuffer[uRegTail].m_uData = uValue;
	s_uRegTail = uRegTail;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
#define __not_in_flash_func(func_name)   __not_in_flash(__STRING(func_name)) func_name
static void __not_in_flash_func(function_core1)(void)
{
	save_and_disable_interrupts();
 	u32 uLow32Pins = gpioc_lo_in_get();
	u32 uS02 = (uLow32Pins >> PIN_CLK) & 1;

	// Wait for IO0 To Return Hi OR S02 To Assert Low
	while ( (0 == ((uLow32Pins >> PIN_IO0) & 1)) || (1 == ((uLow32Pins >> PIN_CLK) & 1)) )
		uLow32Pins = gpioc_lo_in_get();

	u8 uAddress = 0;

	while(true)
 	{
	 	u32 uLow32Pins = gpioc_lo_in_get();

		// Wait for S02 To Assert Low
		while (1 == ((uLow32Pins >> PIN_S02_READ) & 1) )
			uLow32Pins = gpioc_lo_in_get();
			
		// Put Address On BUS
		gpio_put(PIN_ADDRESS_BIT0, uAddress);
		uAddress = ~uAddress;

		// Enable VIA
		gpio_put(PIN_ADDRESS_CS1, true);

		// Wait for S02 To Assert High
		while (0 == ((uLow32Pins >> PIN_S02_READ) & 1) )
			uLow32Pins = gpioc_lo_in_get();

		u32 uHiPins = gpioc_hi_in_get();

		// Disable VIA
		gpio_put(PIN_ADDRESS_CS1, false);

		if(uAddress)
		{
			PushVIARegister(VIA_REG_PORTB, (uHiPins >> (PIN_PORT_B - 32)) & 0xFF);
		}
		else
		{
			PushVIARegister(VIA_REG_PORTA, (uHiPins >> (PIN_PORT_A - 32)) & 0xFF);
		}
	}
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
int main()
{
	stdio_init_all();

	gpio_init(PIN_RESET);						// Put The VIA Into Reset
	gpio_set_dir(PIN_RESET, GPIO_OUT);
	gpio_put(PIN_RESET, false);

	// CS1 Is Address Line 4 Or 5 On The VIC.
	gpio_init(PIN_ADDRESS_CS1);					// Active High
	gpio_set_dir(PIN_ADDRESS_CS1, GPIO_OUT);
	gpio_put(PIN_ADDRESS_CS1, false);

	gpio_init(PIN_IO0);							// Active Low
	gpio_set_dir(PIN_IO0, GPIO_OUT);
	gpio_put(PIN_IO0, true);

	gpio_init(PIN_READ_WRITE);
	gpio_set_dir(PIN_READ_WRITE, GPIO_OUT);
	gpio_put(PIN_READ_WRITE, true);				// Read Mode

	// IRQ Active Low
	gpio_init(PIN_IRQ);
	gpio_set_dir(PIN_IRQ, GPIO_IN);

	// Set All Data Pins To Input
	for(u32 uPin=PIN_DATA_BIT0; uPin<=PIN_DATA_BIT7; ++uPin)
	{
		gpio_init(uPin);
		gpio_set_dir(uPin, GPIO_IN);
	}

	// Set All Address Pins To Output
	for(u32 uPin=PIN_ADDRESS_BIT0; uPin<=PIN_ADDRESS_BIT3; ++uPin)
	{
		gpio_init(uPin);
		gpio_set_dir(uPin, GPIO_OUT);
		gpio_put(uPin, false);
	}

	for(u32 uPinIndex=0; uPinIndex<8; ++uPinIndex)
	{
		gpio_init(PIN_PORT_A + uPinIndex);
		gpio_set_dir(PIN_PORT_A + uPinIndex, GPIO_OUT);
		gpio_put(PIN_PORT_A + uPinIndex, uPinIndex & 1);

		gpio_init(PIN_PORT_B + uPinIndex);
		gpio_set_dir(PIN_PORT_B + uPinIndex, GPIO_OUT);
		gpio_put(PIN_PORT_B + uPinIndex, ~uPinIndex & 1);
	}

	multicore_launch_core1(function_core1);

	// Create The Phase 2 Clock
	gpio_init(PIN_S02_READ);
	gpio_set_dir(PIN_S02_READ, GPIO_IN);
	clock_gpio_init(PIN_CLK, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, ((float)SYS_CLK_HZ / (float)VIC_CPU_CLOCK));

	initVGA();
	FilledRectangle(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB_GREEN);
	FilledRectangle(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB_BLACK);

	gpio_put(PIN_RESET, true);			// Release VIA From RESET
	gpio_put(PIN_IO0, false);			// Leave CS2 Enabled And We Will Control Through CS1

	// Draw All The Constant Text To The Screen
	char szTempString[128];
	DrawString(20, 18, "VIA 6522", RGB_CYAN);

	for (u32 uRegisterIndex=0; uRegisterIndex<16; ++uRegisterIndex)
	{ 
		sprintf(szTempString, "0x%04X", 0x9110 + uRegisterIndex);
		DrawString(13, 20 + uRegisterIndex, szTempString, RGB_BLUE);
		DrawString(20, 20 + uRegisterIndex, "0x", RGB_YELLOW);
		DrawString(25, 20 + uRegisterIndex, s_aszRegisterNames[uRegisterIndex], RGB_CYAN);
	}

	// TODO - REMOVE - HACK TO RUN WITHOUT RESET !!!!
	// s_aViaRegs[1].m_uTimer1 = 0x4826;
	// s_aViaRegs[1].m_uTimer1_Latch = 0x4826;
	// s_aViaRegs[1].m_uInterruptEnable = 0x40;
	// TODO - REMOVE - HACK TO RUN WITHOUT RESET !!!!

	while(true)
	{
		// Update The Register List From The Ring Buffer.
		if (s_uRegHead != s_uRegTail)
		{
			const u8 uRegister = s_aRegBuffer[s_uRegHead].m_uOffset & (VIA_RING_BUFFER_SIZE - 1);
			const u8 uData = s_aRegBuffer[s_uRegHead].m_uData;
			s_viaRegs.m_aReg[uRegister] = uData;
			s_uRegHead = (s_uRegHead + 1) & (VIA_RING_BUFFER_SIZE - 1);
		}

		for (u32 uRegisterIndex=0; uRegisterIndex<16; ++uRegisterIndex)
		{ 
			// Write The Register Values To The Appropriate Screen Position
			const uint16_t uHexPair = byteToHex(s_viaRegs.m_aReg[uRegisterIndex]);
			DrawPetsciiChar(22 << 3, (20 + uRegisterIndex) << 3, uHexPair >> 8, RGB_YELLOW);
			DrawPetsciiChar(23 << 3, (20 + uRegisterIndex) << 3, uHexPair & 255, RGB_YELLOW);
		}
		// sleep_ms(16);
	}
}

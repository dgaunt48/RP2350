//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
#ifndef __vga111_h_included
#define __vga111_h_included

#include "types.h"
#include "hsync.pio.h"
#include "vsync.pio.h"
#include "rgb.pio.h"

#define VGA_RESOLUTION_X    	(640)
#define VGA_RESOLUTION_Y  		(480)
#define TERMINAL_CHARS_WIDE		(VGA_RESOLUTION_X >> 3)
#define TERMINAL_CHARS_HIGH		(VGA_RESOLUTION_Y >> 3)

extern const u8 g_aHexTable[];

void vga_Init(const u32 uPinRGB, const u32 uPinHSync, const u32 uPinVSync);
void vga_FilledRect(u32 uPositionX, u32 uPositionY, u32 uWidth, u32 uHeight, u32 uColour);
void vga_CopyRect(const u8* pSource, const u32 uXPos, const u32 uYPos, const u32 uWidth, const u32 uHeight, const u8* pColourTable);
void vga_DrawPetsciiChar(const u32 uXPos, const u32 uYPos, const u8 uChar, const u8 uColour);
void vga_DrawString(u32 uCharX, u32 uCharY, const char* pszString, const u8 uColour);

static inline u16 byteToHex(const u8 uByte)
{
	return (g_aHexTable[(uByte >> 4) & 15] << 8) | g_aHexTable[uByte & 15];
}

#endif // __vga111_h_included

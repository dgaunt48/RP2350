//------------------------------------------------------------------------------------------------
//---- SpiNorFlash.h (C) 2023 Dave Gaunt                                                         ----
//------------------------------------------------------------------------------------------------
//---- v1.0 -                                                                                 ----
//------------------------------------------------------------------------------------------------

#ifndef __SpiNorFlash_h_included
#define __SpiNorFlash_h_included

#include "types.h"
#include "hardware/spi.h"

bool SpiNorFlash_Initialise(spi_inst_t* pSpi, const u32 uBaudRate, const u32 uClkPin, const u32 uTxPin, const u32 uRxPin, const u32 uCsPin);
void SpiNorFlash_Finalise(void);

void SpiNorFlash_DeepPowerDown_Enter(void);
u32 SpiNorFlash_DeepPowerDown_Release(void);

bool SpiNorFlash_IsErased(const u32 uOffset, const u32 uLength);
bool SpiNorFlash_Erase4kSector(const u32 uSectorIndex);
bool SpiNorFlash_Erase64kBlock(const u32 uBlockIndex);

bool SpiNorFlash_Read(const u32 uOffset, const u32 uLength, u8* pReadBuffer);
bool SpiNorFlash_Write(const u32 uOffset, const u32 uLength, const u8* pData, const bool bVerify);
bool SpiNorFlash_Verify(const u32 uOffset, const u32 uLength, const u8* pData);

#endif // __SpiNorFlash_h_included

//------------------------------------------------------------------------------------------------
//---- SpiNorFlash.c (C) 2023 Dave Gaunt                                                      ----
//------------------------------------------------------------------------------------------------
//---- v1.0 -                                                                                 ----
//------------------------------------------------------------------------------------------------

#include "SpiNorFlash.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define SPI_NORFLASH_PAGE_SHIFT        (8)
#define SPI_NORFLASH_PAGE_SIZE         (1 << SPI_NORFLASH_PAGE_SHIFT)         /* 256 Bytes */
#define SPI_NORFLASH_SECTOR_SHIFT      (12)
#define SPI_NORFLASH_SECTOR_SIZE       (1 << SPI_NORFLASH_SECTOR_SHIFT)       /* 4k Bytes */
#define SPI_NORFLASH_BLOCK_SHIFT       (16)
#define SPI_NORFLASH_BLOCK_SIZE        (1 << SPI_NORFLASH_BLOCK_SHIFT)        /* 64k Bytes */

#define SPI_NORFLASH_CMD_WRITE_PAGE                        (0x02)
#define SPI_NORFLASH_CMD_WRITE_DISABLE                     (0x04)
#define SPI_NORFLASH_CMD_READ_STATUS_REGISTER              (0x05)
#define SPI_NORFLASH_CMD_WRITE_ENABLE                      (0x06)
#define SPI_NORFLASH_CMD_FAST_READ                         (0x0B)
#define SPI_NORFLASH_CMD_ERASE_SECTOR                      (0x20)
#define SPI_NORFLASH_CMD_READ_IDENTIFICATION               (0x9F)
#define SPI_NORFLASH_CMD_RELEASE_FROM_DEEP_POWER_DOWN      (0xAB)
#define SPI_NORFLASH_CMD_ENTER_DEEP_POWER_DOWN             (0xB9)
#define SPI_NORFLASH_CMD_ERASE_BLOCK                       (0xD8)

enum SPINORFLASH_STATUS
{
    SPINORFLASH_STATUS_BUSY = 1,
    SPINORFLASH_STATUS_WRITE_ENABLE = 2
};

enum SPINORFLASH_MANUFACTURER
{
    SPINORFLASH_MANUFACTURER_UNKNOWN = 0,
    SPINORFLASH_MANUFACTURER_EON = 0x1C,
	SPINORFLASH_MANUFACTURER_WINBOND = 0xEF
};

enum SPINORFLASH_EON
{
    SPINORFLASH_EON_EN25X16 = 0x14,
    SPINORFLASH_EON_TYPENOR = 0x70
};

enum SPINORFLASH_WINBOND
{
	SPINORFLASH_WINBOND_W25Q16JV = 0x14,
	SPINORFLASH_WINBOND_IQ_JQ = 0x40,
	SPINORFLASH_WINBOND_IM_JM = 0x70
};

static spi_inst_t* s_pSpi = 0;
static u32 s_uCsMask = 0;
static u32 s_uDeviceID = 0;
static enum SPINORFLASH_MANUFACTURER s_eManufacturer = SPINORFLASH_MANUFACTURER_UNKNOWN;
static u32 s_u32FlashSize = 0;

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static enum SPINORFLASH_STATUS ReadStatusRegister(void)
{
	gpio_clr_mask(s_uCsMask);
    u32 uSendBuffer = SPI_NORFLASH_CMD_READ_STATUS_REGISTER;
    u32 uRecieveBuffer = 0;
	spi_write_read_blocking(s_pSpi, (u8*)&uSendBuffer, (u8*)&uRecieveBuffer, 2);
	gpio_set_mask(s_uCsMask);
    delay_40ns();

    return (uRecieveBuffer >> 8);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static void WriteEnable(const bool bWriteEnable)
{
	gpio_clr_mask(s_uCsMask);
    u8 u8Send = (bWriteEnable) ? SPI_NORFLASH_CMD_WRITE_ENABLE : SPI_NORFLASH_CMD_WRITE_DISABLE;
	spi_write_blocking(s_pSpi, &u8Send, 1);
	gpio_set_mask(s_uCsMask);
    delay_40ns();
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static u32 WaitUntilReady(void)
{
	u32 uWaitDelay = 0;

    while(true)
    {
        enum SPINORFLASH_STATUS eStatus = ReadStatusRegister();

        if (0 == (eStatus & SPINORFLASH_STATUS_BUSY))
            return uWaitDelay;

        sleep_ms(1);
        ++uWaitDelay;
    }
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
static bool EraseChunk(const u32 uOffset, const u8 uEraseCommand)
{
    assert(s_pSpi);
    if (uOffset < s_u32FlashSize)
    {
        u8 aSendBuffer[4];
        WaitUntilReady();

        enum SPINORFLASH_STATUS eStatus = ReadStatusRegister();
        if (! (eStatus & SPINORFLASH_STATUS_WRITE_ENABLE))
        	WriteEnable(true);

    	gpio_clr_mask(s_uCsMask);
        aSendBuffer[0] = uEraseCommand;
        aSendBuffer[1] = (uOffset >> 16);
        aSendBuffer[2] = (uOffset >> 8);
        aSendBuffer[3] = uOffset;
        spi_write_blocking(s_pSpi, aSendBuffer, 4);
        gpio_set_mask(s_uCsMask);
        delay_40ns();
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool SpiNorFlash_Initialise(spi_inst_t* pSpi, const u32 uBaudRate, const u32 uClkPin, const u32 uTxPin, const u32 uRxPin, const u32 uCsPin)
{
    assert(0 == s_pSpi);

    s_pSpi = pSpi;
    s_uCsMask = (1 << uCsPin);

    // TODO: Check Max Baud Rate For Flash NOR Device.
    assert(uBaudRate <= 10000000);
    spi_init(pSpi, uBaudRate);

    gpio_set_dir(uCsPin, GPIO_OUT);
    gpio_clr_mask(s_uCsMask);

    gpio_set_function(uCsPin, GPIO_FUNC_SIO);
    gpio_set_function(uClkPin, GPIO_FUNC_SPI);
    gpio_set_function(uTxPin, GPIO_FUNC_SPI);
    gpio_set_function(uRxPin, GPIO_FUNC_SPI);

    gpio_set_mask(s_uCsMask);
    sleep_ms(20);
    s_uDeviceID = SpiNorFlash_DeepPowerDown_Release();

    u32 uRecieveBuffer = 0;
	gpio_clr_mask(s_uCsMask);
    u32 uSendBuffer = SPI_NORFLASH_CMD_READ_IDENTIFICATION;
	spi_write_read_blocking(s_pSpi, (u8*)&uSendBuffer, (u8*)&uRecieveBuffer, 4);
	gpio_set_mask(s_uCsMask);

    s_eManufacturer = uRecieveBuffer >> 8;

    switch (s_eManufacturer)
    {
        case SPINORFLASH_MANUFACTURER_EON:
        {
            if ((SPINORFLASH_EON_EN25X16 == s_uDeviceID) && (SPINORFLASH_EON_TYPENOR == ((uRecieveBuffer >> 16) & 255)))
            {
                s_u32FlashSize = 1 << (uRecieveBuffer >> 24);
                return true;
            }
        }
        break;

        case SPINORFLASH_MANUFACTURER_WINBOND:
        {
        	if (SPINORFLASH_WINBOND_W25Q16JV == s_uDeviceID)
        	{
        		const u8 uType = (uRecieveBuffer >> 16) & 255;

        		if ((SPINORFLASH_WINBOND_IQ_JQ == uType) || (SPINORFLASH_WINBOND_IM_JM == uType))
        		{
					s_u32FlashSize = 1 << (uRecieveBuffer >> 24);
					assert(2097152 == s_u32FlashSize);
					return true;
        		}
        	}
        }
        break;

        case SPINORFLASH_MANUFACTURER_UNKNOWN:
        	s_u32FlashSize = 0;
        break;
    }
  
    s_eManufacturer = SPINORFLASH_MANUFACTURER_UNKNOWN;
    s_u32FlashSize = 0;
    s_pSpi = 0;

    return false;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void SpiNorFlash_Finalise(void)
{
    assert(s_pSpi);
	SpiNorFlash_DeepPowerDown_Enter();
    s_eManufacturer = SPINORFLASH_MANUFACTURER_UNKNOWN;
    s_pSpi = 0;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
void SpiNorFlash_DeepPowerDown_Enter(void)
{
    assert(s_pSpi);
    WaitUntilReady();
	gpio_clr_mask(s_uCsMask);
    u8 uSendBuffer = SPI_NORFLASH_CMD_ENTER_DEEP_POWER_DOWN;
	spi_write_blocking(s_pSpi, &uSendBuffer, 1);
	gpio_set_mask(s_uCsMask);
    delay_40ns();
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
u32 SpiNorFlash_DeepPowerDown_Release(void)
{
    assert(s_pSpi);
    u8 aSendBuffer[8];
    u8 aRecieveBuffer[8];

	gpio_clr_mask(s_uCsMask);
    aSendBuffer[0] = SPI_NORFLASH_CMD_RELEASE_FROM_DEEP_POWER_DOWN;
	spi_write_read_blocking(s_pSpi, aSendBuffer, aRecieveBuffer, 5);
	gpio_set_mask(s_uCsMask);

    // Wait here until flash has enough time to enable.
    sleep_ms(3);

    return aRecieveBuffer[4];
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool SpiNorFlash_IsErased(const u32 uOffset, const u32 uLength)
{
    if ((uOffset + uLength) > s_u32FlashSize)
        return false;

    u32 uReadOffset = 0;
    u8 aReadBuffer[16];

    while(uReadOffset < uLength)
    {
        const u32 uReadLength = ((uLength - uReadOffset) >= 16) ? 16 : (uLength - uReadOffset);

        if(!SpiNorFlash_Read(uOffset + uReadOffset, uReadLength, aReadBuffer))
        	return false;

        for(u32 i=0; i<uReadLength; ++i)
        {
            if (0xFF != aReadBuffer[i])
                return false;
        }

        uReadOffset += uReadLength;
    }

    return true;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool SpiNorFlash_Erase4kSector(const u32 uSectorIndex)
{
    const u32 uOffset = uSectorIndex << SPI_NORFLASH_SECTOR_SHIFT;
    return EraseChunk(uOffset, SPI_NORFLASH_CMD_ERASE_SECTOR);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool SpiNorFlash_Erase64kBlock(const u32 uBlockIndex)
{
    const u32 uOffset = uBlockIndex << SPI_NORFLASH_BLOCK_SHIFT;
    return EraseChunk(uOffset, SPI_NORFLASH_CMD_ERASE_BLOCK);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool SpiNorFlash_Read(const u32 uOffset, const u32 uLength, u8* pReadBuffer)
{
    WaitUntilReady();

    u8 aSendBuffer[8];
    aSendBuffer[0] = SPI_NORFLASH_CMD_FAST_READ;
    aSendBuffer[1] = (uOffset >> 16);
    aSendBuffer[2] = (uOffset >> 8);
    aSendBuffer[3] = uOffset;

	gpio_clr_mask(s_uCsMask);
	spi_write_blocking(s_pSpi, aSendBuffer, 5);
	spi_read_blocking(s_pSpi, 0, pReadBuffer, uLength);
	gpio_set_mask(s_uCsMask);
    delay_40ns();

    return true;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool SpiNorFlash_Write(const u32 uOffset, const u32 uLength, const u8* pData, const bool bVerify)
{
    if ((uOffset + uLength) > s_u32FlashSize)
        return false;

    u32 uWriteOffset = 0;
    u8 aSendBuffer[4];

    while(uWriteOffset < uLength)
    {
        const u32 uMaxWrite = SPI_NORFLASH_PAGE_SIZE - ((uOffset + uWriteOffset) & (SPI_NORFLASH_PAGE_SIZE - 1));
        const u32 uWriteLength = (uMaxWrite <= (uLength - uWriteOffset)) ? uMaxWrite : (uLength - uWriteOffset);

        WaitUntilReady();
        WriteEnable(true);

        aSendBuffer[0] = SPI_NORFLASH_CMD_WRITE_PAGE;
        aSendBuffer[1] = ((uOffset + uWriteOffset) >> 16);
        aSendBuffer[2] = ((uOffset + uWriteOffset) >> 8);
        aSendBuffer[3] = (uOffset + uWriteOffset);

        gpio_clr_mask(s_uCsMask);
        spi_write_blocking(s_pSpi, aSendBuffer, 4);
        spi_write_blocking(s_pSpi, (u8*)&pData[uWriteOffset], uWriteLength);
        gpio_set_mask(s_uCsMask);
        delay_40ns();

        uWriteOffset += uWriteLength;
    }

    if (bVerify)
    	return SpiNorFlash_Verify(uOffset, uLength, pData);

    return true;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool SpiNorFlash_Verify(const u32 uOffset, const u32 uLength, const u8* pData)
{
    if ((uOffset + uLength) > s_u32FlashSize)
        return false;

    u32 uReadsFailed = 0;
    u32 uReadOffset = 0;
    u8 aReadBuffer[16];

    while(uReadOffset < uLength)
    {
        const u32 uReadLength = ((uLength - uReadOffset) >= 16) ? 16 : (uLength - uReadOffset);
        if (SpiNorFlash_Read(uOffset + uReadOffset, uReadLength, aReadBuffer))
        {
            for(u32 i=0; i<uReadLength; ++i)
            {
                if (pData[uReadOffset + i] != aReadBuffer[i])
                {
                	++uReadsFailed;
                	i = uReadLength;
                }
            }
        }
        else
        {
        	++uReadsFailed;
        }

        uReadOffset += uReadLength;
    }

    return(0 == uReadsFailed);
}

//------------------------------------------------------------------------------------------------
//---- MCP951X_RTC.c (C) 2026 Dave Gaunt                                                      ----
//------------------------------------------------------------------------------------------------
//---- v1.0 - Connect a MCP951X	Real Time Clock To The RP2350                                 ----
//------------------------------------------------------------------------------------------------

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "MCP951X_RTC.h"

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

#define RTC_TIME_MASK (0x3F7F7FFF)
#define RTC_DATE_MASK (0xFF1F3F07)

typedef struct
{
	u8	m_bWriteInProgress : 1;
	u8	m_bWriteEnable : 1;
	u8	m_uBlockProtect : 2;
} RTC_Status;
static_assert(sizeof(RTC_Status) == 1);

typedef struct
{
	union
	{
		u8	m_uRegHundredths;
		struct
		{
			u8	m_uHundredthsOnes : 4;
			u8	m_uHundredthsTens : 4;
		};
	};
	union
	{
		u8	m_uRegSeconds;
		struct
		{
			u8	m_uSecondsOnes : 4;
			u8	m_uSecondsTens : 3;
			u8	m_bStartOscillator : 1;
		};
	};
	union
	{
		u8	m_uRegMinutes;
		struct
		{
			u8	m_uMinutesOnes : 4;
			u8	m_uMinutesTens : 3;
		};
	};
	union
	{
		u8	m_uRegHours;
		struct
		{
			u8	m_uTime12_HoursOnes : 4;
			u8	m_uTime12_HoursTens : 1;
			u8	m_bTime12_AM_PM : 1;
			u8	m_bTime12_Hours24 : 1;
			u8	m_bTime12_bTrimSign : 1;
		};
		struct
		{
			u8	m_uTime24_HoursOnes : 4;
			u8	m_uTime24_HoursTens : 2;
			u8	m_bTime24_Hours24 : 1;
			u8	m_bTime24_bTrimSign : 1;
		};
	};
	union
	{
		u8	m_uRegWeekDay;
		struct
		{
			u8	m_uDayOfWeek : 3;
			u8	m_bBatteryEnable : 1;
			u8	m_bPowerFail : 1;
			u8	m_bFlagOscillatorRunning : 1;
		};
	};
	union
	{
		u8	m_uRegDate;
		struct
		{
			u8	m_uDateOnes : 4;
			u8	m_uDateTens : 2;
		};
	};
	union
	{
		u8	m_uRegMonth;
		struct
		{
			u8	m_uMonthOnes : 4;
			u8	m_uMonthTens : 1;
			u8	m_bFlagLeapYear : 1;
		};
	};
	union
	{
		u8	m_uRegYear;
		struct
		{
			u8	m_uYearOnes : 4;
			u8	m_uYearTens : 4;
		};
	};
	union
	{
		u8	m_uRegControl;
		struct
		{
			u8	m_uSquareWaveOutputFrequency : 2;
			u8  m_bCoarseTrim : 1;
			u8	m_bExternalClock : 1;		// Signal On X1 - Not Using Crystal.
			u8	m_bEnableAlarm0 : 1;
			u8	m_bEnableAlarm1 : 1;
			u8	m_bOutputSquareWave : 1;
		};
	};
	u8	m_uOscillatorTrim;
} RTC_Time;

static_assert(sizeof(RTC_Time) == 10);
static volatile RTC_Time s_rtcTime;

static spi_inst_t* s_pSpi = 0;
static u32 s_uCsMask = 0;

//------------------------------------------------------------------------------------------------
//---- rtc Read                                                                               ----
//------------------------------------------------------------------------------------------------
bool rtcRead(void* pDestination, const u16 uCommand, const u8 uLength)
{
    assert(0 != s_pSpi);

	delay_40ns();
	gpio_clr_mask(s_uCsMask);
	const u32 uHeaderLength = spi_write_blocking(s_pSpi, (u8*)&uCommand, 2);
	const u32 uBytesRead = spi_read_blocking(s_pSpi, 0, (u8*)pDestination, uLength);
	gpio_set_mask(s_uCsMask);

	return ((uHeaderLength + uBytesRead) == (uLength + 2));
}

//------------------------------------------------------------------------------------------------
//---- rtc Write                                                                              ----
//------------------------------------------------------------------------------------------------
bool rtcWrite(void* pSource, const u16 uCommand, const u8 uLength)
{
    assert(0 != s_pSpi);

	delay_40ns();
	gpio_clr_mask(s_uCsMask);
	const u32 uHeaderLength = spi_write_blocking(s_pSpi, (u8*)&uCommand, 2);
	const u32 uBytesWritten = spi_write_blocking(s_pSpi, (u8*)pSource, uLength);
	gpio_set_mask(s_uCsMask);

	return ((uHeaderLength + uBytesWritten) == (uLength + 2));

}

//------------------------------------------------------------------------------------------------
//---- rtc Send Command		                                                                  ----
//------------------------------------------------------------------------------------------------
bool rtcSendCommand(const u16 uCommand, const u32 uLength)
{
    assert(0 != s_pSpi);

	delay_40ns();
	gpio_clr_mask(s_uCsMask);
	const u32 uCommandLength = spi_write_blocking(s_pSpi, (u8*)&uCommand, uLength);
	gpio_set_mask(s_uCsMask);

	return (1 == uCommandLength);
}

//------------------------------------------------------------------------------------------------
//---- rtc Get Status Register                                                                ----
//------------------------------------------------------------------------------------------------
RTC_Status rtcGetStatusRegister(void)
{
    assert(0 != s_pSpi);

	delay_40ns();
	gpio_clr_mask(s_uCsMask);
	const u16 uCommand = RTC_SRREAD;
	u8 aRecieveBuffer[2];
	spi_write_read_blocking(s_pSpi, (u8*)&uCommand, &aRecieveBuffer[0], 2);
	gpio_set_mask(s_uCsMask);

	return *(RTC_Status*)&aRecieveBuffer[1];
}

//------------------------------------------------------------------------------------------------
//---- rtc Wait EEPROM Write                                                                  ----
//------------------------------------------------------------------------------------------------
void rtcWaitEEPROMWrite(void)
{
	RTC_Status rtcStatus;

	do {
		// Takes About 5 Loops To Complete 1 Page Write.
		rtcStatus = rtcGetStatusRegister();
		sleep_ms(1);
		
	} while (rtcStatus.m_bWriteInProgress);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_WriteSRAM(void* pSource, const u8 uRTCAddress, const u8 uLength)
{
	return rtcWrite(pSource, (u16)uRTCAddress << 8 | RTC_WRITE, uLength);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_WriteEEPROM(void* pSource, const u8 uPageIndex)
{
	rtcSendCommand(RTC_EEWREN, 1);
	if (rtcWrite(pSource, (u16)uPageIndex << (RTC_PAGE_SHIFT + 8) | RTC_EEWRITE, 1 << RTC_PAGE_SHIFT))
	{
		rtcWaitEEPROMWrite();
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_WriteID(void* pSource, const u8 uPageIndex)
{
	rtcSendCommand(RTC_EEWREN, 1);
	rtcSendCommand(0x5500 | RTC_UNLOCK, 2);
	rtcSendCommand(0xAA00 | RTC_UNLOCK, 2);
	if (rtcWrite(pSource, (u16)uPageIndex << (RTC_PAGE_SHIFT + 8) | RTC_IDWRITE, 1 << RTC_PAGE_SHIFT))
	{
		rtcWaitEEPROMWrite();
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_ReadSRAM(void* pDestination, const u8 uRTCAddress, const u8 uLength)
{
	return rtcRead(pDestination, (u16)uRTCAddress << 8 | RTC_READ, uLength);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_ReadEEPROM(void* pDestination, const u8 uPageIndex)
{
	return rtcRead(pDestination, (u16)uPageIndex << (RTC_PAGE_SHIFT + 8) | RTC_EEREAD, 1 << RTC_PAGE_SHIFT);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_ReadID(void* pDestination, const u8 uPageIndex)
{
	return rtcRead(pDestination, (u16)uPageIndex << (RTC_PAGE_SHIFT + 8) | RTC_IDREAD, 1 << RTC_PAGE_SHIFT);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
rtc_date RTC_GetDate(void)
{
	RTC_ReadSRAM((void*)&s_rtcTime.m_uRegWeekDay, RTC_REG_WKDAY, 4);
	const u32 uCurrentDate = (*(u32*)&s_rtcTime.m_uRegWeekDay & RTC_DATE_MASK) | (((u32)s_rtcTime.m_bFlagLeapYear & 1) << 3);
	return *(rtc_date*)&uCurrentDate;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_SetDate(const rtc_date uDate)
{
	const bool bWasRunning = RTC_IsRunning();

	if (bWasRunning)
	{
		// Stop The Clock And Wait For m_bFlagOscillatorRunning to Clear
		while (RTC_IsRunning())
			RTC_Stop();
	}

	// Set The Year First
	RTC_WriteSRAM((void*)&uDate.m_Year, RTC_REG_YEAR, 1);

	// Read The Current Settings As Battery Enable And Other Flags Are Stored Here.
	RTC_ReadSRAM((void*)&s_rtcTime.m_uRegWeekDay, RTC_REG_WKDAY, 3);

	// Extreme Type Safety ;)
	*(u32*)&s_rtcTime.m_uRegWeekDay = (*(u32*)&s_rtcTime.m_uRegWeekDay & ~RTC_DATE_MASK) | (*(u32*)&uDate & RTC_DATE_MASK);

	// Date Will Be Rejected By MCP951X As We Are Not Currently In A Leap Year
	if ((*(u8*)&uDate.m_Month == 0x02) && (*(u8*)&uDate.m_Date == 0x29) && (!s_rtcTime.m_bFlagLeapYear))
		s_rtcTime.m_uDateOnes = 8;

	const int q = (s_rtcTime.m_uDateTens * 10) + s_rtcTime.m_uDateOnes;
	int m = (s_rtcTime.m_uMonthTens * 10) + s_rtcTime.m_uMonthOnes;
	int y = 2000 + (s_rtcTime.m_uYearTens * 10) + s_rtcTime.m_uYearOnes;		// Y2K1 Bug !!!

	if (m < 3)
	{
		// Move Janurary And Feburary The The End Of The Previous Year.
		m += 12;
		--y;
	}

	// Claus Tøndering adaption of Zeller's congruence to calculate the day of the week
	const int c = y / 100;
	const int h = (q + (31 * (m - 2)) / 12 + y + (y >> 2) - c + (c >> 2)) % 7;
	s_rtcTime.m_uDayOfWeek = (h + 7) % 7;

	// Write The New Date While Keeping Other Flags.
	const bool bReturnValue = RTC_WriteSRAM((void*)&s_rtcTime.m_uRegWeekDay, RTC_REG_WKDAY, 4);

	if (bWasRunning)
		RTC_Start();

	return bReturnValue;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
rtc_time RTC_GetTime(void)
{
	RTC_ReadSRAM((void*)&s_rtcTime.m_uRegHundredths, RTC_REG_HSEC, 4);
	const u32 uCurrentTime = *(u32*)&s_rtcTime.m_uRegHundredths & RTC_TIME_MASK;
	return *(rtc_time*)&uCurrentTime;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_SetTime(const rtc_time uTime, const bool bStart)
{
	// Stop The Clock And Wait For m_bFlagOscillatorRunning to Clear
	while (RTC_IsRunning())
		RTC_Stop();

	// Extreme Type Safety ;)
	*(u32*)&s_rtcTime.m_uRegHundredths = (*(u32*)&s_rtcTime.m_uRegHundredths & ~RTC_TIME_MASK) | (*(u32*)&uTime & RTC_TIME_MASK);

	// I Only Support 24 Hour Time At The Moment.
	s_rtcTime.m_bTime24_Hours24 = true;
	s_rtcTime.m_bStartOscillator = bStart;

	return RTC_WriteSRAM((void*)&s_rtcTime.m_uRegHundredths, RTC_REG_HSEC, 4);
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
	sleep_ms(16);

	return RTC_ReadSRAM((void*)&s_rtcTime, 0, sizeof(s_rtcTime));
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_Start(void)
{
	if (RTC_IsRunning())
		return false;

	RTC_ReadSRAM((void*)&s_rtcTime.m_uRegSeconds, RTC_REG_SEC, 1);
	s_rtcTime.m_bStartOscillator = true;

	return RTC_WriteSRAM((void*)&s_rtcTime.m_uRegSeconds, RTC_REG_SEC, 1);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_Stop(void)
{
	if (!RTC_IsRunning())
		return false;

	RTC_ReadSRAM((void*)&s_rtcTime.m_uRegSeconds, RTC_REG_SEC, 1);
	s_rtcTime.m_bStartOscillator = false;

	return RTC_WriteSRAM((void*)&s_rtcTime.m_uRegSeconds, RTC_REG_SEC, 1);
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_IsRunning(void)
{
	RTC_ReadSRAM((void*)&s_rtcTime.m_uRegWeekDay, RTC_REG_WKDAY, 1);
	return s_rtcTime.m_bFlagOscillatorRunning;
}

	// s_rtcTime.m_bOutputSquareWave = true;
	// s_rtcTime.m_uSquareWaveOutputFrequency = RCT_OUTPUT_SQUARE_WAVE_32768Hz;
	// RTC_WriteSRAM((void*)&s_rtcTime.m_uRegControl, RTC_REG_CONTROL, 1);

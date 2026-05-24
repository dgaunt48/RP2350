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
			u8	m_bFlagPowerFail : 1;
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
			u8	m_bLeapYear : 1;
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
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_ReadSRAM(void* pDestination, const u8 uRTCAddress, const u8 uLength)
{
    assert(0 != s_pSpi);

	delay_40ns();
	gpio_clr_mask(s_uCsMask);

	const u16 uSendBuffer = (u16)uRTCAddress << 8 | RTC_READ;
	const u32 uHeaderLength = spi_write_blocking(s_pSpi, (u8*)&uSendBuffer, 2);
	const u32 uBytesRead = spi_read_blocking(s_pSpi, 0, (u8*)pDestination, uLength);
	gpio_set_mask(s_uCsMask);

	return ((uHeaderLength + uBytesRead) == (uLength + 2));
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_WriteSRAM(void* pSource, const u8 uRTCAddress, const u8 uLength)
{
    assert(0 != s_pSpi);

	delay_40ns();
	gpio_clr_mask(s_uCsMask);

	const u16 uSendBuffer = (u16)uRTCAddress << 8 | RTC_WRITE;
	const u32 uHeaderLength = spi_write_blocking(s_pSpi, (u8*)&uSendBuffer, 2);
	const u32 uBytesWritten = spi_write_blocking(s_pSpi, (u8*)pSource, uLength);
	gpio_set_mask(s_uCsMask);

	return ((uHeaderLength + uBytesWritten) == (uLength + 2));
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
rtc_time RTC_GetTime(void)
{
	RTC_ReadSRAM((void*)&s_rtcTime.m_uRegHundredths, RTC_REG_HSEC, 4);
	u32 uCurrentTime = *(u32*)&s_rtcTime.m_uRegHundredths & RTC_TIME_MASK;
	return *(rtc_time*)&uCurrentTime;
}

//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
bool RTC_SetTime(const rtc_time uTime, const bool bStart)
{
	RTC_Stop();			// Stop The Clock If It Is Running

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

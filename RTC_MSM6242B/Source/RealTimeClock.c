//------------------------------------------------------------------------------------------------
//---- RealTimeClock.c (C) 2026 Dave Gaunt                                                    ----
//------------------------------------------------------------------------------------------------
//---- v1.0 - Connect A MSM6242B Real Time Clock To The RP2350                                ----
//------------------------------------------------------------------------------------------------

#include <stdio.h>
#include "types.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"

#include "vga111.h"

#define RTC_CLOCK_SPEED	(32767)		/* Gives Me 32768.1 On The Prototype Board */

enum board_pins
{
	PIN_RED = 0, PIN_GREEN, PIN_BLUE,
	PIN_CS, PIN_CS_HIGH = 5,
	PIN_HSYNC = 8, PIN_VSYNC,
	PIN_READ, PIN_WRITE,
	PIN_A0, PIN_A1, PIN_A2, PIN_A3,
	PIN_D0, PIN_D1, PIN_D2, PIN_D3,
	PIN_ALE,
	PIN_32768HZ_CLOCK
};

static_assert(21 == PIN_32768HZ_CLOCK, "Pico only exposes a few pins for external clocks!");

enum registers_msm6242b
{
	MSM6242B_SECONDS_ONES = 0,
	MSM6242B_SECONDS_TENS,
	MSM6242B_MINUTES_ONES,
	MSM6242B_MINUTES_TENS,
	MSM6242B_HOURS_ONES,
	MSM6242B_HOURS_TENS,
	MSM6242B_DAY_ONES,
	MSM6242B_DAY_TENS,
	MSM6242B_MONTH_ONES,
	MSM6242B_MONTH_TENS,
	MSM6242B_YEAR_ONES,
	MSM6242B_YEAR_TENS,
	MSM6242B_WEEK_DAY,
	MSM6242B_CTRL_D,
	MSM6242B_CTRL_E,
	MSM6242B_CTRL_F
};

#define MSM6242B_CTRL_D_HOLD		(1)
#define MSM6242B_CTRL_D_BUSY		(2)
#define MSM6242B_CTRL_D_IRQ_FLAG	(4)
#define MSM6242B_CTRL_D_30_SEC_ADJ	(8)

#define MSM6242B_CTRL_E_MASK		(1)
#define MSM6242B_CTRL_E_INTERRUPT	(2)
#define MSM6242B_CTRL_E_PERIOD0		(4)
#define MSM6242B_CTRL_E_PERIOD1		(8)

#define MSM6242B_CTRL_F_RESET		(1)
#define MSM6242B_CTRL_F_STOP		(2)
#define MSM6242B_CTRL_F_24_HOUR		(4)
#define MSM6242B_CTRL_F_TEST		(8)

typedef struct
{
	union
	{
		u8	m_Seconds_Reg;
		struct
		{
			u8	m_uOnes : 4;
			u8	m_uTens : 3;
		} m_Seconds;
	};
	union
	{
		u8	m_Minutes_Reg;
		struct
		{
			u8	m_uOnes : 4;
			u8	m_uTens : 3;
		} m_Minutes;
	};
	union
	{
		u8	m_Hours_Reg;
		struct
		{
			u8	m_uOnes : 4;
			u8	m_uTens : 2;
			u8	m_bPM	: 1;
		} m_Hours;
	};
	union
	{
		u8	m_Day_Reg;
		struct
		{
			u8	m_uOnes : 4;
			u8	m_uTens : 2;
		} m_Day;
	};
	union
	{
		u8	m_Month_Reg;
		struct
		{
			u8	m_uOnes : 4;
			u8	m_uTens : 1;
		} m_Month;
	};
	union
	{
		u8	m_Year_Reg;
		struct
		{
			u8	m_uOnes : 4;
			u8	m_uTens : 4;
		} m_Year;
	};
	union
	{
		u8	m_Week_Reg;
		struct
		{
			u8	m_uDayOfWeek 	: 3;
		} m_Week;
		struct
		{
			u8 				: 4;
			u8 m_uControl_D	: 4;
		};
	};
	u8	m_uControl_E	: 4;
	u8	m_uControl_F	: 4;
} rtc_msm6242b;
static_assert(sizeof(rtc_msm6242b) == 8);

static volatile rtc_msm6242b s_rtcRegisters;

const char aWeekDays[7][4] =
{
	"SUN",
	"MON",
	"TUE",
	"WED",
	"THU",
	"FRI",
	"SAT"
};

//------------------------------------------------------------------------------------------------
//---- register read				                                                          ----
//------------------------------------------------------------------------------------------------
u8 register_read(const enum registers_msm6242b eRegister)
{
    gpio_put_masked(0xF << PIN_A0, eRegister << PIN_A0);
    gpio_set_dir_in_masked(0xF << PIN_D0);
    delay_40ns();
    gpio_put(PIN_READ, false);
    delay_120ns(); 
    const u32 uValue = gpio_get_all();
    gpio_put(PIN_READ, true);
    busy_wait_at_least_cycles(40);
    return (uValue >> PIN_D0) & 0xF;
}

//------------------------------------------------------------------------------------------------
//---- register write				                                                          ----
//------------------------------------------------------------------------------------------------
void register_write(const enum registers_msm6242b eRegister, const u32 uValue)
{
    gpio_put_masked(0xF << PIN_A0, eRegister << PIN_A0);
    gpio_set_dir_out_masked(0xF << PIN_D0);
    gpio_put_masked(0xF << PIN_D0, (uValue & 0x0F) << PIN_D0);
    delay_40ns(); 
    gpio_put(PIN_WRITE, false);
    delay_120ns(); 
    gpio_put(PIN_WRITE, true);
    busy_wait_at_least_cycles(40);
	gpio_set_dir_in_masked(0xF << PIN_D0);
}

//------------------------------------------------------------------------------------------------
//---- Yep It's Main!                                                                         ----
//------------------------------------------------------------------------------------------------
int main(void)
{
	stdio_init_all();

	// Set CS High While We Initialise
	gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, true);

	// The A501 Only Changes the Read / Write Lines
	// It Never Uses The Select Or ALE Lines.
	gpio_init(PIN_CS_HIGH);
    gpio_set_dir(PIN_CS_HIGH, GPIO_OUT);
	gpio_put(PIN_CS_HIGH, true);

	gpio_init(PIN_ALE);
    gpio_set_dir(PIN_ALE, GPIO_OUT);
    gpio_put(PIN_ALE, true);

	gpio_init(PIN_READ);
    gpio_set_dir(PIN_READ, GPIO_OUT);
    gpio_put(PIN_READ, true);

	gpio_init(PIN_WRITE);
    gpio_set_dir(PIN_WRITE, GPIO_OUT);
    gpio_put(PIN_WRITE, true);

	for(u32 i=0; i<4; ++i)
	{
		gpio_init(PIN_A0 + i);
		gpio_set_dir(PIN_A0 + i, GPIO_OUT);
	    gpio_put(PIN_A0 + i, false);

		gpio_init(PIN_D0 + i);
		gpio_set_dir(PIN_D0 + i, GPIO_IN);
	    gpio_put(PIN_D0 + i, false);
	}
	
	// Start Clock
	clock_gpio_init(PIN_32768HZ_CLOCK, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, ((float)SYS_CLK_HZ / (float)RTC_CLOCK_SPEED));
	
	vga_Init(PIN_RED, PIN_HSYNC, PIN_VSYNC);
	vga_FilledRect(0, 0, VGA_RESOLUTION_X, VGA_RESOLUTION_Y, RGB111_GREEN);
	vga_FilledRect(1, 1, VGA_RESOLUTION_X-2, VGA_RESOLUTION_Y-2, RGB111_BLACK);

	sleep_ms(100);

	// Ready To Go
	gpio_put(PIN_CS, false);

	register_write(MSM6242B_CTRL_F, MSM6242B_CTRL_F_STOP);

	register_write(MSM6242B_YEAR_TENS, 0x2);
	register_write(MSM6242B_YEAR_ONES, 0x6);
	register_write(MSM6242B_MONTH_TENS, 0x0);
	register_write(MSM6242B_MONTH_ONES, 0x5);
	register_write(MSM6242B_DAY_TENS, 0x3);
	register_write(MSM6242B_DAY_ONES, 0x1);
	register_write(MSM6242B_WEEK_DAY, 0x0);

	register_write(MSM6242B_HOURS_TENS, 0x0);
	register_write(MSM6242B_HOURS_ONES, 0x5);
	register_write(MSM6242B_MINUTES_TENS, 0x1);
	register_write(MSM6242B_MINUTES_ONES, 0x0);
	register_write(MSM6242B_SECONDS_TENS, 0x1);
	register_write(MSM6242B_SECONDS_ONES, 0x0);

	register_write(MSM6242B_CTRL_D, 0x0);
	register_write(MSM6242B_CTRL_E, 0x0);
	register_write(MSM6242B_CTRL_F, 0x0);

	char szString[128];

	while(true)
	{
		bool bRegisterRead = false;
		u32 uTimeout = 0;

		while (!bRegisterRead)
		{
			register_write(MSM6242B_CTRL_D, MSM6242B_CTRL_D_IRQ_FLAG | MSM6242B_CTRL_D_HOLD);
			busy_wait_us(150); 
			s_rtcRegisters.m_uControl_D = register_read(MSM6242B_CTRL_D);

			if (s_rtcRegisters.m_uControl_D & MSM6242B_CTRL_D_BUSY)
			{
				register_write(MSM6242B_CTRL_D, MSM6242B_CTRL_D_IRQ_FLAG);
				busy_wait_us(120); 
				
				if (uTimeout++ > 1000)
				{
					vga_DrawString(10, 14, "Loop Break!!!", RGB111_RED);
					break; 
				}
			}
			else
			{
				s_rtcRegisters.m_Hours_Reg = (register_read(MSM6242B_HOURS_TENS) << 4) | register_read(MSM6242B_HOURS_ONES);
				s_rtcRegisters.m_Minutes_Reg = (register_read(MSM6242B_MINUTES_TENS) << 4) | register_read(MSM6242B_MINUTES_ONES);
				s_rtcRegisters.m_Seconds_Reg = (register_read(MSM6242B_SECONDS_TENS) << 4) | register_read(MSM6242B_SECONDS_ONES);

				s_rtcRegisters.m_Year_Reg = (register_read(MSM6242B_YEAR_TENS) << 4) | register_read(MSM6242B_YEAR_ONES);
				s_rtcRegisters.m_Month_Reg = (register_read(MSM6242B_MONTH_TENS) << 4) | register_read(MSM6242B_MONTH_ONES);
				s_rtcRegisters.m_Day_Reg = (register_read(MSM6242B_DAY_TENS) << 4) | register_read(MSM6242B_DAY_ONES);
				s_rtcRegisters.m_Week_Reg = register_read(MSM6242B_WEEK_DAY);
				bRegisterRead = true;
			}
		}

		register_write(MSM6242B_CTRL_D, MSM6242B_CTRL_D_IRQ_FLAG);

		s_rtcRegisters.m_uControl_D = register_read(MSM6242B_CTRL_D);
		s_rtcRegisters.m_uControl_E = register_read(MSM6242B_CTRL_E);
		s_rtcRegisters.m_uControl_F = register_read(MSM6242B_CTRL_F);
		sprintf(
			szString, 
			"Control Registers  D 0x%01x  E 0x%01x  F 0x%01x", 
			s_rtcRegisters.m_uControl_D,
			s_rtcRegisters.m_uControl_E,
			s_rtcRegisters.m_uControl_F
		);
		vga_DrawString(7, 8, szString, RGB111_YELLOW);

		u32 uOffset = sprintf(szString, "Date  ");
		szString[uOffset + 0 ] = aWeekDays[s_rtcRegisters.m_Week.m_uDayOfWeek][0];
		szString[uOffset + 1 ] = aWeekDays[s_rtcRegisters.m_Week.m_uDayOfWeek][1];
		szString[uOffset + 2 ] = aWeekDays[s_rtcRegisters.m_Week.m_uDayOfWeek][2];
		szString[uOffset + 3 ] = ' ';
		szString[uOffset + 4 ] = '0' + s_rtcRegisters.m_Day.m_uTens;
		szString[uOffset + 5 ] = '0' + s_rtcRegisters.m_Day.m_uOnes;
		szString[uOffset + 6 ] = '/';
		szString[uOffset + 7 ] = '0' + s_rtcRegisters.m_Month.m_uTens;
		szString[uOffset + 8 ] = '0' + s_rtcRegisters.m_Month.m_uOnes;
		szString[uOffset + 9 ] = '/';
		szString[uOffset + 10] = '2';
		szString[uOffset + 11] = '0';
		szString[uOffset + 12] = '0' + s_rtcRegisters.m_Year.m_uTens;
		szString[uOffset + 13] = '0' + s_rtcRegisters.m_Year.m_uOnes;
		szString[uOffset + 14] = ' ';
		szString[uOffset + 15] = ' ';
		szString[uOffset + 16] = ' ';
		szString[uOffset + 17] = '0' + s_rtcRegisters.m_Hours.m_uTens;
		szString[uOffset + 18] = '0' + s_rtcRegisters.m_Hours.m_uOnes;
		szString[uOffset + 19] = ':';
		szString[uOffset + 20] = '0' + s_rtcRegisters.m_Minutes.m_uTens;
		szString[uOffset + 21] = '0' + s_rtcRegisters.m_Minutes.m_uOnes;
		szString[uOffset + 22] = ':';
		szString[uOffset + 23] = '0' + s_rtcRegisters.m_Seconds.m_uTens;
		szString[uOffset + 24] = '0' + s_rtcRegisters.m_Seconds.m_uOnes;
		szString[uOffset + 25] = 0;
		vga_DrawString(10, 10, szString, RGB111_CYAN);

		if ((s_rtcRegisters.m_Week.m_uDayOfWeek != 0) ||
			(s_rtcRegisters.m_Year.m_uTens != 2) ||
			(s_rtcRegisters.m_Year.m_uOnes != 6) ||
			(s_rtcRegisters.m_Month.m_uTens != 0) ||
			(s_rtcRegisters.m_Month.m_uOnes != 5) ||
			(s_rtcRegisters.m_Day.m_uTens != 3) ||
			(s_rtcRegisters.m_Day.m_uOnes != 1))
		{
			vga_DrawString(10, 12, szString, RGB111_RED);
		}

		if (s_rtcRegisters.m_Hours.m_uTens != 0)
			vga_DrawString(10, 16, szString, RGB111_RED);

		sleep_ms(16);
	}
}

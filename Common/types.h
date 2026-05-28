//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
//----                                                                                        ----
//------------------------------------------------------------------------------------------------
#ifndef __types_h_included
#define __types_h_included

#include <pico.h>
#include <stdbool.h>
#include <assert.h>

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;

typedef struct
{
	u8	m_uOnes : 4;
	u8	m_uTens : 4;
} bcd_pair;
static_assert(sizeof(bcd_pair) == 1);

typedef struct
{
    union
    {
        u32 m_uFixed;
        struct
        {
            u32 m_uFraction : 8;
            int m_uInteger : 24;
        };
    };
} fixed_24_8;
static_assert(sizeof(fixed_24_8) == 4);

#define __not_in_flash_func(func_name)   __not_in_flash(__STRING(func_name)) func_name

//------------------------------------------------------------------------------------------------
//----  nop = 1,000,000,000 / 125,000,000 = 8 ns       RP2040                                 ----
//----  minumum write pulse width = 40 ns ... = 5 cycles                                       ----
//----                                                                                        ----
//----  nop = 1,000,000,000 / 150,000,000 = 6 ns       RP2350                                 ----
//----  minumum write pulse width = 40 ns ... = 7 cycles                                      ----
//------------------------------------------------------------------------------------------------
static inline void delay_40ns(void)
{
    busy_wait_at_least_cycles(9);
}

//------------------------------------------------------------------------------------------------
//----  nop = 1,000,000,000 / 125,000,000 = 8 ns       RP2040                                 ----
//----  minumum write pulse width = 120 ns ... = 15 nop's                                     ----
//----                                                                                        ----
//----  nop = 1,000,000,000 / 150,000,000 = 6 ns       RP2350                                 ----
//----  minumum write pulse width = 120 ns ... = 20 nop's                                     ----
//------------------------------------------------------------------------------------------------
static inline void delay_120ns(void)
{
    busy_wait_at_least_cycles(20);
}

static inline u16 swap_u16(const u16 value)
{
    return (value << 8) | (value >> 8);
}

#endif /* __types_h_included */

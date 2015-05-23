/*
 *
 * Copyright 2008-2010 Gary Chen, All Rights Reserved
 *
 * Written by Lisen (wzlyhcn@gmail.com)
 *
 */
#ifndef ECG_SYSUTILS_H
#define ECG_SYSUTILS_H

#include "ecg_Core.h"

#define NOP() __no_operation()
#define NOP2() NOP(); NOP()
#define NOP3() NOP2(); NOP()
#define NOP4() NOP3(); NOP()
#define NOP5() NOP4(); NOP()
#define NOP6() NOP5(); NOP()
#define NOP7() NOP6(); NOP()
#define NOP8() NOP7(); NOP()
#define NOP9() NOP8(); NOP()
#define NOP10() NOP9(); NOP()
#define NOP20() NOP10(); NOP10()
#define NOP30() NOP20(); NOP10()
#define NOP40() NOP30(); NOP10()
#define NOP50() NOP40(); NOP10()
#define NOP60() NOP50(); NOP10()
#define NOP70() NOP60(); NOP10()
#define NOP80() NOP70(); NOP10()
#define NOP90() NOP80(); NOP10()
#define NOP100() NOP90(); NOP10()

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时1us
 */
inline void delay1us()
{
    NOP10();
    NOP10();
    NOP10();
    NOP10();
    NOP10();
    NOP10();
    NOP10();
    NOP2();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时2us
 */
inline void delay2us()
{
    NOP20();
    NOP20();
    NOP20();
    NOP20();
    NOP20();
    NOP20();
    NOP20();
    NOP4();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时3us
 */
inline void delay3us()
{
    NOP30();
    NOP30();
    NOP30();
    NOP30();
    NOP30();
    NOP30();
    NOP30();
    NOP6();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时4us
 */
inline void delay4us()
{
    NOP40();
    NOP40();
    NOP40();
    NOP40();
    NOP40();
    NOP40();
    NOP40();
    NOP8();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时5us
 */
inline void delay5us()
{
    NOP50();
    NOP50();
    NOP50();
    NOP50();
    NOP50();
    NOP50();
    NOP50();
    NOP10();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时6us
 */
inline void delay6us()
{
    NOP60();
    NOP60();
    NOP60();
    NOP60();
    NOP60();
    NOP60();
    NOP60();
    NOP10();
    NOP2();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时7us
 */
inline void delay7us()
{
    NOP70();
    NOP70();
    NOP70();
    NOP70();
    NOP70();
    NOP70();
    NOP70();
    NOP10();
    NOP4();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时8us
 */
inline void delay8us()
{
    NOP80();
    NOP80();
    NOP80();
    NOP80();
    NOP80();
    NOP80();
    NOP80();
    NOP10();
    NOP6();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时9us
 */
inline void delay9us()
{
    NOP90();
    NOP90();
    NOP90();
    NOP90();
    NOP90();
    NOP90();
    NOP90();
    NOP10();
    NOP8();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时10us
 */
inline void delay10us()
{
    NOP100();
    NOP100();
    NOP100();
    NOP100();
    NOP100();
    NOP100();
    NOP100();
    NOP20();
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 延时N个10us
 */
inline void delayN10us(dword n)
{
    while (n--) {
        NOP100();
        NOP100();
        NOP100();
        NOP100();
        NOP100();
        NOP100();
        NOP100();
        NOP20();
    }
}

} // namespace

#endif // ECG_SYSUTILS_H
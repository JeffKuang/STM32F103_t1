/*
 *
 * Copyright 2008-2015 Gary Chen, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_CORE_H
#define ECG_CORE_H

#include <stddef.h>
#include <stm32f1xx.h>
#include "ecg_PcbVersion.h"

// 定义空指针常量
#ifndef NULL
    #define NULL 0
#endif

// 禁用全局中断
#define DISABLE_INTERRUPT() __disable_interrupt()

// 使能全局中断
#define ENABLE_INTERRUPT() __enable_interrupt()

namespace ecg {

// 定义常用基本数据类型
typedef unsigned char uchar;
typedef uchar byte;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef ushort word;
typedef ulong dword;
typedef long long int64;
typedef unsigned long long uint64;

} // namespace

#endif // ECG_CORE_H

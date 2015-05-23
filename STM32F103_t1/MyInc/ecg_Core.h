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

// �����ָ�볣��
#ifndef NULL
    #define NULL 0
#endif

// ����ȫ���ж�
#define DISABLE_INTERRUPT() __disable_interrupt()

// ʹ��ȫ���ж�
#define ENABLE_INTERRUPT() __enable_interrupt()

namespace ecg {

// ���峣�û�����������
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

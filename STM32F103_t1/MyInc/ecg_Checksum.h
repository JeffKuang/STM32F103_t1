/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_CHECKSUM_H
#define ECG_CHECKSUM_H

#include "ecg_Core.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
typedef dword CRC32;

////////////////////////////////////////////////////////////////////////////////
/**
 * 计算CRC32
 *
 * 多项式：x32 + x26 + x23 + x22 + x16 + x12 + x11 + x10 + x8 + x7 + x5 + x4 + x2 + x + 1
 */
CRC32 calcCRC32(const dword* buffer, size_t size);

} // namespace

#endif // ECG_CHECKSUM_H
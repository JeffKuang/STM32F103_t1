/*
 *
 * Copyright 2008-2015 Gary Chen, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#include "ecg_Checksum.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
CRC32 calcCRC32(const dword* buffer, size_t size)
{
    CRC->CR = 1;
    while (size--) {
        CRC->DR = __REV(*buffer++);
    }
    return CRC->DR;
}

} // namespace

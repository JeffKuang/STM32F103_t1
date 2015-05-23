/*
 *
 * Copyright 2008-2010 Gary Chen, All Rights Reserved
 *
 * Written by Lisen (wzlyhcn@gmail.com)
 *
 */
#ifndef ECG_NONCOPYABLE_H
#define ECG_NONCOPYABLE_H

#include "ecg_Core.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
/**
 * ·Ç¿É¿½±´
 */
class NonCopyable
{
protected:
    NonCopyable() {}
    ~NonCopyable() {}
private:
    NonCopyable(const NonCopyable&) {}
    const NonCopyable& operator=(const NonCopyable&) { return *this; }
};

} // namespace

#endif // ECG_NONCOPYABLE_H

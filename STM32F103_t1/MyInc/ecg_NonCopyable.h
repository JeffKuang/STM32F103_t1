/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_NONCOPYABLE_H
#define ECG_NONCOPYABLE_H

#include "ecg_Core.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
/**
 * �ǿɿ���
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

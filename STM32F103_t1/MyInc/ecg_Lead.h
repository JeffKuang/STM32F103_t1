/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_LEAD_H
#define ECG_LEAD_H

#include "ecg_Core.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
typedef short LeadPoint; /**< 导联点的数据类型 */

////////////////////////////////////////////////////////////////////////////////
/**
 * 导联类型
 *
 * 设备的实现可以只支持部分导联类型，参考具体设备的能力。比如：12导联设备
 * 会支持所有ltI到ltV6的导联。某些设备支持其中少部分导联。对于没有在这里
 * 列出的导联，可以在最后追加，勿在现有序列中插入。
 * @warning 不要随意更改其常量值
 */
enum LeadType {
    ltI = 0, /**< I导联 */
    ltII, /**< II导联 */
    ltV1, /**< V1导联 */
    ltV2, /**< V2导联 */
    ltV3, /**< V3导联 */
    ltV4, /**< V4导联 */
    ltV5, /**< V5导联 */
    ltV6, /**< V6导联 */
};

enum {
    LeadTypeCount = 8,
};

////////////////////////////////////////////////////////////////////////////////
/**
 * 判断是否是合法的导联类型
 */
inline bool isLeadType(int leadType)
{
    return leadType >=ltI && leadType <= ltV6;
}

////////////////////////////////////////////////////////////////////////////////
/**
 * 导联增益类型
 *
 * 大多数设备都可以为每个导联单独设置其增益。通常，对真实硬件而言，可以
 * 实现在保持好的信噪比的基础上，改变信号的增益。设备的实现可以只支持
 * 部分导联增益类型，通常，至少要支持lgtX1增益类型。
 * @warning 不要随意更改其常量值
 */
enum LeadGainType {
    lgtX1 = 0x00, /**< 原信号 */
    lgtX2 = 0x01, /**< 将原信号放大2倍 */
    lgtX0p5 = 0x02, /**< 将原信号缩小0.5倍 */
    lgtX4 = 0x03, /**< 将原信号放大4倍 */
    lgtX0p25 = 0x04, /**< 将原信号缩小0.25倍 */
};

enum {
    LeadGainTypeCount = 5,
};

////////////////////////////////////////////////////////////////////////////////
/**
 * 判断是否是合法的导联增益类型
 */
inline bool isLeadGainType(int leadGainType)
{
    return leadGainType >= lgtX1 && leadGainType <= lgtX0p25;
}

} // namespace

#endif // ECG_LEAD_H
/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_ECGDEVICE_H
#define ECG_ECGDEVICE_H

#include "ecg_Core.h"
#include "ecg_NonCopyable.h"
#include "ecg_Lead.h"
#include "ecg_TLC3541.h"
#include "ecg_Checksum.h"
#include <string.h>

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
class ECGDevice;
class ECGDeviceCallback;

////////////////////////////////////////////////////////////////////////////////
/**
 * ECG导联
 *
 * 表示单独一个ECG导联。
 */
struct ECGLead
{
    friend class ECGDevice;
public:
    /**
     * 构造器
     */
    ECGLead()
    {
        initialize();
    }

    /**
     * 初始化
     */
    void initialize()
    {
        type = ltI;
        point = 0;
        lastPoint = 0;
        gainType = lgtX1;
        lastGainType = lgtX1;
        alarm = false;
        lastAlarm = false;
        isValidPoint = false;
        lastIsValidPoint = false;
        prior = NULL;
        next = NULL;
        gainTypeBackup = lgtX1;
        resetPointOffsets();
    }

    /**
     * 复位所有增益对应的采样点偏移量，及其之和
     */
    void resetPointOffsets()
    {
        for (byte leadGainType = lgtX1; leadGainType < LeadGainTypeCount; ++leadGainType) {
            pointOffsets[leadGainType] = 0;
            pointOffsetSums[leadGainType] = 0;
        }
    }

    /**
     * 获得无符号的采样点
     */
    unsigned short getUnsignedPoint()
    {
        return BaseLineLeadPoint + point; // 由基线电压AD量加上有符号采样点得到
    }

    /**
     * 获得无符号的最后的采样点
     */
    unsigned short getUnsignedLastPoint()
    {
        return BaseLineLeadPoint + lastPoint; // 类似于getUnsignedPoint()
    }


    LeadType type; // 导联类型
    LeadPoint point; // 采样点，用于实时采样
    LeadPoint lastPoint; // 最后的采样点，用于数据安全访问，它总是表示对point的最后一次复制
    LeadGainType gainType; // 增益类型
    LeadGainType lastGainType; // 最后的增益类型，用于数据安全访问，它总是表示对gainType的最后一次复制
    bool alarm; // 警告状态，用于指示是否存在导联脱离或者极化电压超标
    bool lastAlarm; // 最后的警告状态，用于数据安全访问，它总是表示对alarm的最后一次复制
    bool isValidPoint; // 导联数据是否可用
    bool lastIsValidPoint; // 最后的导联数据是否可用，用于数据安全访问，它总是表示对isValidPoint的最后一次复制
private:
    enum {
        BaseLineLeadPoint = 8192, // 基线电压的AD量，相当于2.5V
    };
private:
    /**
     * 拷贝到最后的采样数据
     */
    void copyToLast()
    {
        lastPoint = point;
        lastAlarm = alarm;
        lastGainType = gainType;
        lastIsValidPoint = isValidPoint;
    }
private:
    ECGLead* prior; // 指向双向链表的前一个元素
    ECGLead* next; //  指向双向链表的后一个元素
    LeadPoint pointOffsets[LeadGainTypeCount]; // 采样点偏移量的数组，每个元素对应一种增益，用于信号校准
    int pointOffsetSums[LeadGainTypeCount]; // 采样点偏移量之和的数组，每个元素对应一种增益，用于信号校准
    LeadGainType gainTypeBackup;
};

////////////////////////////////////////////////////////////////////////////////
/**
 * 内置滤波器类型
 */
enum BuiltInFilterType {
    bftFilter0 = 0x00,
    bftFilter1 = 0x01,
};

enum {
    BuiltInFilterTypeCount = 2,
};

inline bool isBuiltInFilterType(byte filterType)
{
    switch (filterType) {
    case bftFilter0:
    case bftFilter1:
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
/**
 * ECG设备指的是用于采集人体心电信号的电子设备或者模块。
 */
class ECGDevice : public NonCopyable
{
public:
    /**
     * 构造器
     */
    ECGDevice();

    /**
     * 析构器
     */
    ~ECGDevice();

    /**
     * 初始化
     */
    void initialize();

    /**
     * 开始采样
     */
    void startSampling();

    /**
     * 停止采样
     */
    void stopSampling();

    /**
     * 获得是否正在采样
     * @return 该值
     */
    bool isSampling();

    /**
     * 获得采样是否完成
     * @return 该值
     */
    bool isSamplingPeriodCompleted();

    /**
     * 清除采样标记
     */
    void clearSamplingPeriodCompletedFlag();

    /**
     * 获得导联
     * @param[in] leadType 导联类型
     * @return 该值
     */
    ECGLead& getLead(LeadType leadType);

    /**
     * 设置信号是否复位
     * @param[in] signalReset 该值
     */
    void setSignalReset(bool signalReset);

    /**
     * 开始校准信号
     */
    void startCalibrateSignal();

    /**
     * 停止校准信号
     */
    void stopCalibrateSignal();

    /**
     * 是否正在校准信号
     * @return 该值
     */
    bool isCalibratingSignal();

    /**
     * 是否完成校准信号
     * @return 该值
     */
    bool isCalibrateSignalCompleted();

    /**
     * 清除校准信号
     */
    void clearCalibratingSignal();

    /**
     * 设置内置滤波器使能
     * @param[in] filterType 滤波器类型
     * @param[in] enabled 该值
     */
    void setupBuiltInFilter(BuiltInFilterType filterType, bool enabled);
private:
    enum {
        MaxCalibrateLeadPointCount = 1000, // 最大的校准信号采样点数量，通常用1000点即可
        CalibrateResultAddressInFlash = 0x0800F800, // 倒数第2页首地址，Flash Size = 64K
        CalibrateResultPaddingSize = 21, // 校准结果的填充字节数，用于满足CRC32算法的4字节对齐的要求

        SamplingTimeSliceCount = 4, // 把每导联的采样时间125us分成4份，每份为一个采样时间片31.25us = (1/4)*125us
    };

    /**
     * 采样状态
     */
    enum SamplingState {
        ssInReady, // 处于就绪状态
        ssInSampling, // 处于采样状态
    };
// 设置内存为非对齐
#pragma pack(push, 1)
    // 校准结果数据结构，用于直接存储于Flash当中
    union CalibrateResult {
        CRC32 calcCRC() const
        {
            return calcCRC32(reinterpret_cast<const dword*>(this), sizeof(pointOffsets) / sizeof(dword));
        }

        struct {
            short pointOffsets[LeadTypeCount][LeadGainTypeCount];
            dword crc;
        };
        dword padding[CalibrateResultPaddingSize];
    };
#pragma pack(pop)
private:
    /**
     * SysTick中断处理程序
     */
    static void handleSysTick();

    /**
     * 选择当前导联，直接写相应的GPIO
     */
    void selectLeadType(LeadType leadType);

    /**
     * 选择当前增益，直接写相应的GPIO
     */
    void selectLeadGainType(LeadGainType leadGainType);

    /**
     * 检测当前导联警报状态，直接读相应的GPIO
     */
    bool checkLeadAlarm();

    /**
     * 应用额外增益
     */
    void applyExtraLeadGain(LeadGainType leadGainType, LeadPoint& leadPoint);

    /**
     * 限制采样点范围
     */
    void limitLeadPointRange(LeadPoint& leadPoint);

    /**
     * 获得ADC采样点
     */
    LeadPoint getAdcSamplingPoint();

    /**
     * 解析处于就绪状态
     */
    void parseInReady();

    /**
     * 解析处于采样状态
     */
    void parseInSampling();

    /**
     * 采样任务0
     */
    void samplingTask0();

    /**
     * 采样任务1
     */
    void samplingTask1();

    /**
     * 采样任务2
     */
    void samplingTask2();

    /**
     * 采样任务3
     */
    void samplingTask3();

    /**
     * 采样过程，每个采样时间片被调用一次，周期为31.25us
     */
    void sampling();

    /**
     * 复位采样状态
     */
    void resetSampling();

    /**
     * 从Flash中装载校准结果
     */
    void loadCalibrateResult();

    /**
     * 保存校准结果到Flash中
     */
    void saveCalibrateResult();

    /**
     * 检查校准是否完成
     */
    void checkCalibrateCompleted();
private:
    static ECGDevice* m_instance;
    TLC3541 m_adc; // 表示TLC3541 ADC
    ECGLead m_leads[LeadTypeCount]; // 导联的数组，每一导联对应一种导联类型，便于访问
    ECGLead* m_lead; // 指向m_leads中的一个元素，表示当前正在操作的导联
    volatile byte m_samplingTimeSlice; // 采样时间片，只有4种取值：0,1,2,3
    volatile bool m_isSampling; // 是否正在采样过程中
    volatile SamplingState m_samplingState; // 当前的采样状态
    volatile bool m_needAdcStartSimpling; // 用于在采样时间片1中，控制是否需要开始ADC的异步采样
    volatile bool m_isFirstSamplingPeriod; // 用于判断是否属于第一个采样周期（每次调用startSampling()后，认为是第一个采样周期），一个采样周期为1ms
    volatile bool m_isSamplingPeriodCompleted; // 用于判断一个采样周期是否已经完成，以便读取所有导联的采样数据
    volatile bool m_isCalibratingSignal; // 用于判断是否在校准信号中
    volatile bool m_isCalibrateSignalCompleted; // 用于判断校准信号是否已经完成，以便有机会通知主机
    volatile byte m_calibratedLeadGainType; // 用于指示校准信号过程中的当前增益
    volatile size_t m_calibratedLeadPointCount; // 用于计数已校准信号的点个数
};

} // namespace

#endif // ECG_ECGDEVICE_H
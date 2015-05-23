/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_TLC3541_H
#define ECG_TLC3541_H

#include "ecg_Core.h"
#include "ecg_NonCopyable.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
/**
 * 模拟/数字转换器TLC3541
 * TLC3541的通信接口是SPI从模式，根据原理图的设计，用STM32的SPI2主模式与之通信，
 * 由于整个系统对实时性和性能的要求都较高，因此，TLC3541的实现采用SPI的中断模式，
 * 并通过自动机来精确控制每一个工作阶段。
 */
class TLC3541 : public NonCopyable
{
public:
    enum {
        MinSamplingValue = 0, // ADC的最小采样值
        MaxSamplingValue = 16383, // ADC的最大采样值
    };
public:
    /**
     * 构造器
     */
    TLC3541();

    /**
     * 析构器
     */
    ~TLC3541();

    /**
     * 初始化
     */
    void initialize();

    /**
     * 复位
     */
    void reset();

    /**
     * 开始采样
     */
    void startSampling();

    /**
     * 获得是否正在采样
     * @return 该值
     */
    bool isSampling();

    /**
     * 同步等待采样结果完成
     */
    void waitForSamplingResult();

    /**
     * 获得采样结果
     * @return 该值
     */
    word getSamplingResult();
private:
    enum {
        ResetSamplingValue = 0x3FC0, // 用于验证ADC复位是否成功的采样值（参考TLC3541规格书）
    };

    // 采样状态
    enum SamplingState {
        ssInSendMSBClock, // 处于发送高字节时钟的状态
        ssInReceiveMSB, // 处于接收高字节数据的状态
        ssInSendLSBClock, // 处于发送低字节时钟的状态
        ssInReceiveLSB, // 处于接收低字节数据的状态
        ssInSendExtraClock, // 处于发送额外时钟的状态
        ssInReceiveExtra, // 处于接收额外数据的状态
        ssInEnd, // 处于结束的状态
    };
private:
    /**
     * SPI2中断处理程序
     */
    static void handleSPI2();

    /**
     * ADC片选
     */
    void select();

    /**
     * ADC取消片选
     */
    void deselect();

    /**
     * 同步双工传输数据，先发送后接收数据
     */
    byte transmit(byte data);

    /**
     * 采样自动机
     */
    void sampling();

    /**
     * 解析处于发送高字节时钟的状态
     */
    void parseInSendMSBClock();

    /**
     * 解析处于接收高字节数据的状态
     */
    void parseInReceiveMSB();

    /**
     * 解析处于发送低字节时钟的状态
     */
    void parseInSendLSBClock();

    /**
     * 解析处于接收低字节数据的状态
     */
    void parseInReceiveLSB();

    /**
     * 解析处于发送额外时钟的状态
     */
    void parseInSendExtraClock();

    /**
     * 解析处于接收额外数据的状态
     */
    void parseInReceiveExtra();

    /**
     * 解析处于结束的状态
     */
    void parseInEnd();
private:
    static TLC3541* m_instance;
    volatile word m_samplingResult; // ADC的采样结果，16位数值，只有低14位有效
    volatile SamplingState m_state; // ADC的采样状态，用于采样自动机
    volatile bool m_isSampling; // 表示ADC是否正在采样状态
};

} // namespace

#endif // ECG_TLC3541_H
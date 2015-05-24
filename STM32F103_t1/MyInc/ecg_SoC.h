/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_SOC_H
#define ECG_SOC_H

#include "ecg_Core.h"
#include "ecg_NonCopyable.h"
#include "spi.h"
#include "usart.h"
#include "dma.h"
#include "stm32f1xx_hal_usart.h"

// 如果编译为Debug版本，则禁止看门狗；Release版本，则使用看门狗
#ifdef NDEBUG
    #define USE_WATCH_DOG 1
#else
    #define USE_WATCH_DOG 0
#endif

// 定义是否使用DMA方式的UART
#define USE_USART1_TX_DMA 1

// 定义是否使用TIM2
#define USE_TIM2 1

// 定义是否使用BootLoader
#define USE_BOOT_LOADER 0

// 定义是否使用实现喂狗调用
#if USE_WATCH_DOG
    #define FEED_WATCH_DOG() SoC::getInstance()->feedWatchDog()
#else
    #define FEED_WATCH_DOG()
#endif

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
/**
 * 片上系统
 */
class SoC : public NonCopyable
{
public:
    /**
     * 中断服务例程类型
     */
    enum IsrType {
        itNMI = 0,
        itHardFault,
        itMemManage,
        itBusFault,
        itUsageFault,
        itSVC,
        itDebugMon,
        itPendSV,
        itSysTick,
        itUSART1,
        itUSART2,
        itSPI1,
        itSPI2,
        itTIM1,
        itTIM2,
        IsrTypeCount, // 中断类型个数
    };

    typedef void (*Isr)();
public:
    /**
     * 获得实例
     * @return 该值
     */
    static SoC* getInstance();

    /**
     * 初始化
     */
    void initialize();

    /**
     * 设置ISR
     */
    void setIsr(IsrType isrType, Isr isr);

    /**
     * 调用ISR
     */
    void callIsr(IsrType isrType);

    /**
     * 喂看门狗
     */
#if USE_WATCH_DOG
    void feedWatchDog();
#endif


    /**
     * 通过USART1发送数据
     * @param[in] data 要发送的数据缓冲区
     * @param[in] size 要发送的大小，单位：字节
     */
    void sendByUSART1(const void* data, size_t size);
protected:
    SoC();
    ~SoC();
private:
    enum {
        USART1_BaudRate = 921600, // UART的波特率
#if USE_USART1_TX_DMA
        USART1_DR_Address = 0x40013804, // UART的DMA地址
        USART1_MaxDMABufferSize = 512, // UART的DMA缓冲区大小，单位是字节
#endif
   };
private:
    /**
     * 配置中断例程
     */
    void configureIsr();

    /**
     * 配置RCC
     */
    void configureRCC();

    /**
     * 配置NVIC
     */
    void configureNVIC();

#if USE_WATCH_DOG
    /**
     * 配置IWDG
     */
    void configureIWDG();
#endif

    /**
     * 配置DMA
     */
    void configureDMA();

    /**
     * 配置GPIO
     */
    void configureGPIO();

    /**
     * 配置USART1
     */
    void configureUSART1();

    /**
     * 配置SPI2
     */
    //void configureSPI2();

    /**
     * 配置SysTick
     */
    void configureSysTick();

#if USE_TIM2
    /**
     * 配置TIM2
     */
    void configureTIM2();
#endif

    /**
     * 空中断例程
     */
    static void nullIsr();

    /**
     * 死循环中断例程
     */
    static void foreverIsr();
private:
    static SoC m_instance;
    volatile Isr m_isrTable[IsrTypeCount];  // 中断向量表
#if USE_USART1_TX_DMA
    volatile byte m_bufferOfUSART1DMA[USART1_MaxDMABufferSize]; // UART的DMA缓冲区
#endif
    volatile bool m_isFirstSendByUART1; // 标记是否是UART的第一次发送，用于解决DMA的调用流程不一致的问题
};

} // namespace

#endif // ECG_SOC_H
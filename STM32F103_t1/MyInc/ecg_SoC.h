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

// �������ΪDebug�汾�����ֹ���Ź���Release�汾����ʹ�ÿ��Ź�
#ifdef NDEBUG
    #define USE_WATCH_DOG 1
#else
    #define USE_WATCH_DOG 0
#endif

// �����Ƿ�ʹ��DMA��ʽ��UART
#define USE_USART1_TX_DMA 1

// �����Ƿ�ʹ��TIM2
#define USE_TIM2 1

// �����Ƿ�ʹ��BootLoader
#define USE_BOOT_LOADER 0

// �����Ƿ�ʹ��ʵ��ι������
#if USE_WATCH_DOG
    #define FEED_WATCH_DOG() SoC::getInstance()->feedWatchDog()
#else
    #define FEED_WATCH_DOG()
#endif

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
/**
 * Ƭ��ϵͳ
 */
class SoC : public NonCopyable
{
public:
    /**
     * �жϷ�����������
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
        IsrTypeCount, // �ж����͸���
    };

    typedef void (*Isr)();
public:
    /**
     * ���ʵ��
     * @return ��ֵ
     */
    static SoC* getInstance();

    /**
     * ��ʼ��
     */
    void initialize();

    /**
     * ����ISR
     */
    void setIsr(IsrType isrType, Isr isr);

    /**
     * ����ISR
     */
    void callIsr(IsrType isrType);

    /**
     * ι���Ź�
     */
#if USE_WATCH_DOG
    void feedWatchDog();
#endif


    /**
     * ͨ��USART1��������
     * @param[in] data Ҫ���͵����ݻ�����
     * @param[in] size Ҫ���͵Ĵ�С����λ���ֽ�
     */
    void sendByUSART1(const void* data, size_t size);
protected:
    SoC();
    ~SoC();
private:
    enum {
        USART1_BaudRate = 921600, // UART�Ĳ�����
#if USE_USART1_TX_DMA
        USART1_DR_Address = 0x40013804, // UART��DMA��ַ
        USART1_MaxDMABufferSize = 512, // UART��DMA��������С����λ���ֽ�
#endif
   };
private:
    /**
     * �����ж�����
     */
    void configureIsr();

    /**
     * ����RCC
     */
    void configureRCC();

    /**
     * ����NVIC
     */
    void configureNVIC();

#if USE_WATCH_DOG
    /**
     * ����IWDG
     */
    void configureIWDG();
#endif

    /**
     * ����DMA
     */
    void configureDMA();

    /**
     * ����GPIO
     */
    void configureGPIO();

    /**
     * ����USART1
     */
    void configureUSART1();

    /**
     * ����SPI2
     */
    //void configureSPI2();

    /**
     * ����SysTick
     */
    void configureSysTick();

#if USE_TIM2
    /**
     * ����TIM2
     */
    void configureTIM2();
#endif

    /**
     * ���ж�����
     */
    static void nullIsr();

    /**
     * ��ѭ���ж�����
     */
    static void foreverIsr();
private:
    static SoC m_instance;
    volatile Isr m_isrTable[IsrTypeCount];  // �ж�������
#if USE_USART1_TX_DMA
    volatile byte m_bufferOfUSART1DMA[USART1_MaxDMABufferSize]; // UART��DMA������
#endif
    volatile bool m_isFirstSendByUART1; // ����Ƿ���UART�ĵ�һ�η��ͣ����ڽ��DMA�ĵ������̲�һ�µ�����
};

} // namespace

#endif // ECG_SOC_H
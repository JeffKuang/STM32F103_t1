/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#include "ecg_SoC.h"
#include <string.h>

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
SoC SoC::m_instance;

////////////////////////////////////////////////////////////////////////////////
SoC::SoC()
    : m_isFirstSendByUART1(true)
{
    memset((void*)m_isrTable, 0, sizeof(m_isrTable));
}

SoC::~SoC()
{
}

SoC* SoC::getInstance()
{
    return &m_instance;
}

void SoC::initialize()
{
    // 配置所需的片上外设
    configureIsr();
    configureRCC();
    configureNVIC();
#if USE_WATCH_DOG
    configureIWDG();
#endif
    //configureDMA();
    MX_DMA_Init();
    configureGPIO();
    //configureUSART1();
    MX_USART1_UART_Init();
    //configureSPI2();
    MX_SPI2_Init();
    configureSysTick();
#if USE_TIM2
    configureTIM2();
#endif
}

void SoC::setIsr(IsrType isrType, Isr isr)
{
    if (isr == NULL) {
        // 对于isr被指定为NULL，则表示用户要解除自定义的中断例程，并用系统默认的中断例程代之
        switch (isrType) {
        case itHardFault:
        case itMemManage:
        case itBusFault:
        case itUsageFault:
            // 对于以上几种中断例程，通常的默认处理是死循环
            isr = foreverIsr;
            break;
        default:
            // 其余的中断例程则可以简单的空处理即可
            isr = nullIsr;
        }
    }
    assert_param(isr != NULL);
    m_isrTable[isrType]= isr;
}

void SoC::callIsr(IsrType isrType)
{
    // 直接调用相应的中断例程，任何时候，都能保证每一个中断例程都可用（即，不等于NULL）
    assert_param(m_isrTable[isrType] != NULL);
    m_isrTable[isrType]();
}

#if USE_WATCH_DOG
void SoC::feedWatchDog()
{
    IWDG_ReloadCounter();
}
#endif


void SoC::sendByUSART1(const void* data, size_t size)
{
#if USE_USART1_TX_DMA
    assert_param(data != NULL);
    assert_param(size > 0);
    assert_param(size <= USART1_MaxDMABufferSize);
    if (m_isFirstSendByUART1) {
        m_isFirstSendByUART1 = false;
    }
    else {
        while (DMA_GetFlagStatus(DMA1_FLAG_TC4) == RESET);
        DMA_Cmd(DMA1_Channel4, DISABLE);
        DMA_ClearFlag(DMA1_FLAG_TC4);
    }
    memcpy((void*)m_bufferOfUSART1DMA, data, size);
    DMA1_Channel4->CNDTR = size;
    DMA_Cmd(DMA1_Channel4, ENABLE);
#else
    assert_param(data != NULL);
    assert_param(size > 0);
    const byte* p = reinterpret_cast<const byte*>(data);
    while (size--) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *p++);
    }
#endif
}

void SoC::configureIsr()
{
    for (byte isrType = 0; isrType < IsrTypeCount; ++isrType) {
        setIsr(static_cast<IsrType>(isrType), NULL);
    }
}

void SoC::configureRCC()
{
    SystemInit();
#if USE_WATCH_DOG
    if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET) {
        RCC_ClearFlag();
    }
#endif
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2
#if USE_TIM2
        | RCC_APB1Periph_TIM2
#endif
        | RCC_APB1Periph_SPI2, ENABLE);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC
		| RCC_APB2Periph_USART1 | RCC_APB2Periph_ADC1 | RCC_APB2Periph_AFIO, ENABLE);

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1 | RCC_AHBPeriph_CRC, ENABLE);
}

void SoC::configureNVIC()
{
#if USE_BOOT_LOADER
    NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x5000);
#endif

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

    NVIC_InitTypeDef initTypeDef;

    initTypeDef.NVIC_IRQChannel = USART1_IRQn;
    initTypeDef.NVIC_IRQChannelPreemptionPriority = 1;
    initTypeDef.NVIC_IRQChannelSubPriority = 0;
    initTypeDef.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&initTypeDef);

    initTypeDef.NVIC_IRQChannel = SPI2_IRQn;
    initTypeDef.NVIC_IRQChannelPreemptionPriority = 0;
    initTypeDef.NVIC_IRQChannelSubPriority = 0;
    initTypeDef.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&initTypeDef);

    initTypeDef.NVIC_IRQChannel = static_cast<uint8_t>(SysTick_IRQn);
    initTypeDef.NVIC_IRQChannelPreemptionPriority = 2;
    initTypeDef.NVIC_IRQChannelSubPriority = 0;
    initTypeDef.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&initTypeDef);

#if USE_TIM2
    initTypeDef.NVIC_IRQChannel = TIM2_IRQn;
    initTypeDef.NVIC_IRQChannelPreemptionPriority = 3;
    initTypeDef.NVIC_IRQChannelSubPriority = 0;
    initTypeDef.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&initTypeDef);
#endif
}

#if USE_WATCH_DOG
void SoC::configureIWDG()
{
    // 设置IWDG超时为1004.8ms，IWDG计数器时钟: 40KHz(LSI) / 256 = 156.25 Hz
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_256);
    IWDG_SetReload(157);
    IWDG_ReloadCounter();
    IWDG_Enable();
}
#endif

/*
void SoC::configureDMA()
{
#if USE_USART1_TX_DMA
    DMA_InitTypeDef initTypeDef;

    DMA_DeInit(DMA1_Channel4);
    initTypeDef.DMA_PeripheralBaseAddr = USART1_DR_Address;
    initTypeDef.DMA_MemoryBaseAddr = (uint32_t)m_bufferOfUSART1DMA;
    initTypeDef.DMA_DIR = DMA_DIR_PeripheralDST;
    initTypeDef.DMA_BufferSize = USART1_MaxDMABufferSize;
    initTypeDef.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    initTypeDef.DMA_MemoryInc = DMA_MemoryInc_Enable;
    initTypeDef.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    initTypeDef.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    initTypeDef.DMA_Mode = DMA_Mode_Normal;
    initTypeDef.DMA_Priority = DMA_Priority_High;
    initTypeDef.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &initTypeDef);
#endif

// 	DMA_DeInit(DMA1_Channel1);
// 	initTypeDef.DMA_PeripheralBaseAddr = ADC1_DR_Address;
// 	initTypeDef.DMA_MemoryBaseAddr = (uint32_t)m_bufferOfADC1DMA1;
// 	initTypeDef.DMA_DIR = DMA_DIR_PeripheralSRC;
// 	initTypeDef.DMA_BufferSize = ADC1_MaxDMABufferSize;
// 	initTypeDef.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
// 	initTypeDef.DMA_MemoryInc = DMA_MemoryInc_Enable;
// 	initTypeDef.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
// 	initTypeDef.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
// 	initTypeDef.DMA_Mode = DMA_Mode_Circular;
// 	initTypeDef.DMA_Priority = DMA_Priority_High;
// 	initTypeDef.DMA_M2M = DMA_M2M_Disable;
// 	DMA_Init(DMA1_Channel1, &initTypeDef);
// 	DMA_Cmd(DMA1_Channel1, ENABLE);
}
*/

void SoC::configureGPIO()
{
    GPIO_InitTypeDef initTypeDef;

    // 配置USART1的RX
    initTypeDef.Pin = GPIO_PIN_10;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_AF_INPUT;
    HAL_GPIO_Init(GPIOA, &initTypeDef);

    // 配置USART1的TX
    initTypeDef.Pin = GPIO_PIN_9;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_AF_PP;
    HAL_GPIO_Init(GPIOA, &initTypeDef);

    // 配置导联选择器F0，F1和F2
    initTypeDef.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &initTypeDef);

    // 配置增益选择器F4和F5
    initTypeDef.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOB, &initTypeDef);

    // 配置信号复位F3
    initTypeDef.Pin = GPIO_PIN_7;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &initTypeDef);

    // 配置信号滤波器之选择器FS0和FS1
    initTypeDef.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOB, &initTypeDef);

    // 配置导联警告F6
    initTypeDef.Pin = GPIO_PIN_10;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(GPIOB, &initTypeDef);

    // 配置SPI2的SCK和MOSI
    initTypeDef.Pin = GPIO_PIN_13 | GPIO_PIN_15;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_AF_PP;
    HAL_GPIO_Init(GPIOB, &initTypeDef);

    // 配置SPI2的NSS
    initTypeDef.Pin = GPIO_PIN_12;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOB, &initTypeDef);

    // 配置SPI2的MISO
    initTypeDef.Pin = GPIO_PIN_14;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_AF_INPUT;
    HAL_GPIO_Init(GPIOB, &initTypeDef);

// 	initTypeDef.Pin = GPIO_PIN_0;
// 	initTypeDef.Mode = GPIO_MODE_INPUT;
// 	GPIO_Init(GPIOA, &initTypeDef);

    // 调试用
    initTypeDef.Pin = GPIO_PIN_0;
    initTypeDef.Speed = GPIO_SPEED_HIGH;
    initTypeDef.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOA, &initTypeDef);
}
/*
void SoC::configureUSART1()
{
    UART_InitTypeDef initTypeDef;

    initTypeDef.BaudRate = USART1_BaudRate;
    initTypeDef.WordLength = UART_WORDLENGTH_9B;
    initTypeDef.StopBits = UART_STOPBITS_1;
    initTypeDef.Parity = UART_PARITY_ODD;
    initTypeDef.HwFlowCtl = UART_HWCONTROL_NONE;
    initTypeDef.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(USART1, &initTypeDef);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
#if USE_USART1_TX_DMA
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
#endif
    USART_Cmd(USART1, ENABLE);
}
*/

/*
void SoC::configureSPI2()
{
    SPI_InitTypeDef initTypeDef;

    initTypeDef.Direction = SPI_DIRECTION_2LINES;
    initTypeDef.Mode = SPI_MODE_MASTER;
    initTypeDef.DataSize = SPI_DATASIZE_8BIT;
    initTypeDef.CLKPolarity = SPI_POLARITY_HIGH;
    initTypeDef.CLKPhase = SPI_PHASE_1EDGE;
    initTypeDef.NSS = SPI_NSS_SOFT;
    initTypeDef.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    initTypeDef.FirstBit = SPI_FIRSTBIT_MSB;
    initTypeDef.CRCPolynomial = 7;
    HAL_SPI_Init(SPI2, &initTypeDef);
    SPI_SSOutputCmd(SPI2, ENABLE);
    SPI_Cmd(SPI2, ENABLE);
}
*/

void SoC::configureSysTick()
{
    SysTick_Config(SystemFrequency / 32000); // 周期：32000=31.25us, 16000=62.5us, 8000=125us
}

#if USE_TIM2
void SoC::configureTIM2()
{
    TIM_TimeBaseInitTypeDef timeBaseInitTypeDef;

    timeBaseInitTypeDef.TIM_Period = 36000; // 周期：1ms
    timeBaseInitTypeDef.TIM_Prescaler = 1;
    timeBaseInitTypeDef.TIM_ClockDivision = 0;
    timeBaseInitTypeDef.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &timeBaseInitTypeDef);

    TIM_OCInitTypeDef ocInitTypeDef;

    ocInitTypeDef.TIM_OCMode = TIM_OCMode_Timing;
    ocInitTypeDef.TIM_OutputState = TIM_OutputState_Enable;
    ocInitTypeDef.TIM_Pulse = 10;
    ocInitTypeDef.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &ocInitTypeDef);

    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Disable);
    TIM_ITConfig(TIM2, TIM_IT_CC1, ENABLE);
}
#endif


void SoC::nullIsr()
{
    // 空函数
}

void SoC::foreverIsr()
{
    // 死循环函数
    for (;;) {
    }
}

} // namespace


#if __cplusplus
extern "C" {
#endif

using namespace ecg;

////////////////////////////////////////////////////////////////////////////////
void NMI_Handler()
{
    SoC::getInstance()->callIsr(SoC::itNMI);
}

////////////////////////////////////////////////////////////////////////////////
void HardFault_Handler()
{
    SoC::getInstance()->callIsr(SoC::itHardFault);
}

////////////////////////////////////////////////////////////////////////////////
void MemManage_Handler()
{
    SoC::getInstance()->callIsr(SoC::itMemManage);
}

////////////////////////////////////////////////////////////////////////////////
void BusFault_Handler()
{
    SoC::getInstance()->callIsr(SoC::itBusFault);
}

////////////////////////////////////////////////////////////////////////////////
void UsageFault_Handler()
{
    SoC::getInstance()->callIsr(SoC::itUsageFault);
}

////////////////////////////////////////////////////////////////////////////////
void SVC_Handler()
{
    SoC::getInstance()->callIsr(SoC::itSVC);
}

////////////////////////////////////////////////////////////////////////////////
void DebugMon_Handler()
{
    SoC::getInstance()->callIsr(SoC::itDebugMon);
}

////////////////////////////////////////////////////////////////////////////////
void PendSV_Handler()
{
    SoC::getInstance()->callIsr(SoC::itPendSV);
}

////////////////////////////////////////////////////////////////////////////////
void SysTick_Handler()
{
    SoC::getInstance()->callIsr(SoC::itSysTick);
}

////////////////////////////////////////////////////////////////////////////////
void USART1_IRQHandler()
{
    SoC::getInstance()->callIsr(SoC::itUSART1);
}

////////////////////////////////////////////////////////////////////////////////
void USART2_IRQHandler()
{
    SoC::getInstance()->callIsr(SoC::itUSART2);
}

////////////////////////////////////////////////////////////////////////////////
void SPI1_IRQHandler()
{
    SoC::getInstance()->callIsr(SoC::itSPI1);
}

////////////////////////////////////////////////////////////////////////////////
void SPI2_IRQHandler()
{
    SoC::getInstance()->callIsr(SoC::itSPI2);
}

////////////////////////////////////////////////////////////////////////////////
void TIM1_IRQHandler()
{
    SoC::getInstance()->callIsr(SoC::itTIM1);
}

////////////////////////////////////////////////////////////////////////////////
void TIM2_IRQHandler()
{
    SoC::getInstance()->callIsr(SoC::itTIM2);
}

#if __cplusplus
}
#endif

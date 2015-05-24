/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#include "ecg_TLC3541.h"
#include "ecg_SoC.h"
#include "ecg_SysUtils.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
TLC3541* TLC3541::m_instance = NULL;

////////////////////////////////////////////////////////////////////////////////
TLC3541::TLC3541()
    : m_samplingResult(0),
    m_state(ssInEnd),
    m_isSampling(false)
{
    m_instance = this;
}

TLC3541::~TLC3541()
{
}

void TLC3541::initialize()
{
    SoC::getInstance()->setIsr(SoC::itSPI2, &handleSPI2);
}

void TLC3541::reset()
{
    // ADC的复位是非常关键的过程，在进入采样过程之前，一定要成功复位，
    // 以ADC返回一个特殊的复位采样值来确认是否成功，如果失败，则一直重复
    // 此复位过程，直到看门狗超时，便导致STM32复位。
    for (;;) {
        deselect();
        delayN10us(10);
        select();
        // 按照TLC3541的时序要求，发送8个下降时钟沿，以便其内部完成初始化工作
        transmit(0); 
        deselect();
        delayN10us(10);
        // 开始第一次采样，正常情况下，采样结果应该等于ResetSamplingValue
        startSampling();
        waitForSamplingResult();
        if (getSamplingResult() == ResetSamplingValue) {
            delay10us();
            break; // 只有ADC复位成功，才能结束重复复位过程
        }
    }
}

void TLC3541::startSampling()
{
    if (!m_isSampling) {
        // 为采样前做初始化工作
        m_isSampling = true;
        m_samplingResult = 0;
        m_state = ssInSendMSBClock;
        // 按照TLC3541的时序要求做片选动作
        select();
        // 使能SPI的发送中断，以便开启异步通信
        SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, ENABLE);
    }
}

bool TLC3541::isSampling()
{
    return m_isSampling;
}

void TLC3541::waitForSamplingResult()
{
    while (m_isSampling);
}

word TLC3541::getSamplingResult()
{
    return m_samplingResult;
}

void TLC3541::handleSPI2()
{
    m_instance->sampling();
}

void TLC3541::select()
{
#if PCB_VERSION == 1
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
#else
    //GPIO_ResetBits(GPIOB, GPIO_PIN_12);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET); // 暂时修补
#endif
}

void TLC3541::deselect()
{
#if PCB_VERSION == 1
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
#else
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); // 暂时修补
#endif
}

byte TLC3541::transmit(byte data)
{
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI2, data);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);
    return (~SPI_I2S_ReceiveData(SPI2)) & 0x00FF;
}

void TLC3541::sampling()
{
    // 采样自动机的执行流程
    switch (m_state) {
    case ssInSendMSBClock:
        parseInSendMSBClock();
        break;
    case ssInReceiveMSB:
        parseInReceiveMSB();
        break;
    case ssInSendLSBClock:
        parseInSendLSBClock();
        break;
    case ssInReceiveLSB:
        parseInReceiveLSB();
        break;
    case ssInSendExtraClock:
        parseInSendExtraClock();
        break;
    case ssInReceiveExtra:
        parseInReceiveExtra();
        break;
    case ssInEnd:
        parseInEnd();
        break;
    } // switch
}

void TLC3541::parseInSendMSBClock()
{
    if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_TXE) != RESET) {
        SPI_I2S_SendData(SPI2, 0);
        SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, ENABLE);
        m_state = ssInReceiveMSB;
    }
}

void TLC3541::parseInReceiveMSB()
{
    if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_RXNE) != RESET) {
        //m_instance->m_samplingResult = SPI_I2S_ReceiveData(SPI2) << 8;
        m_samplingResult = (~SPI_I2S_ReceiveData(SPI2)) << 8; // 暂时修补
        m_state = ssInSendLSBClock;
    }
}

void TLC3541::parseInSendLSBClock()
{
    if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_TXE) != RESET) {
        SPI_I2S_SendData(SPI2, 0);
        m_state = ssInReceiveLSB;
    }
}

void TLC3541::parseInReceiveLSB()
{
    if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_RXNE) != RESET) {
        //m_instance->m_samplingResult |= SPI_I2S_ReceiveData(SPI2);
        m_samplingResult |= (~SPI_I2S_ReceiveData(SPI2)) & 0x00FF; // 暂时修补
        m_samplingResult >>= 2;
        m_state = ssInSendExtraClock;
    }
}

void TLC3541::parseInSendExtraClock()
{
    if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_TXE) != RESET) {
        SPI_I2S_SendData(SPI2, 0);
        m_state = ssInReceiveExtra;
    }
}

void TLC3541::parseInReceiveExtra()
{
    if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_RXNE) != RESET) {
        SPI_I2S_ReceiveData(SPI2);
        SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, DISABLE);
        SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, DISABLE);
        deselect();
        m_isSampling = false;
        m_state = ssInEnd;
    }
}

void TLC3541::parseInEnd()
{
    // 空任务
}


} // namespace
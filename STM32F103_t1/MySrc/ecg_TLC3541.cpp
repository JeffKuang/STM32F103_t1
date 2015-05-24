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
    // ADC�ĸ�λ�Ƿǳ��ؼ��Ĺ��̣��ڽ����������֮ǰ��һ��Ҫ�ɹ���λ��
    // ��ADC����һ������ĸ�λ����ֵ��ȷ���Ƿ�ɹ������ʧ�ܣ���һֱ�ظ�
    // �˸�λ���̣�ֱ�����Ź���ʱ���㵼��STM32��λ��
    for (;;) {
        deselect();
        delayN10us(10);
        select();
        // ����TLC3541��ʱ��Ҫ�󣬷���8���½�ʱ���أ��Ա����ڲ���ɳ�ʼ������
        transmit(0); 
        deselect();
        delayN10us(10);
        // ��ʼ��һ�β�������������£��������Ӧ�õ���ResetSamplingValue
        startSampling();
        waitForSamplingResult();
        if (getSamplingResult() == ResetSamplingValue) {
            delay10us();
            break; // ֻ��ADC��λ�ɹ������ܽ����ظ���λ����
        }
    }
}

void TLC3541::startSampling()
{
    if (!m_isSampling) {
        // Ϊ����ǰ����ʼ������
        m_isSampling = true;
        m_samplingResult = 0;
        m_state = ssInSendMSBClock;
        // ����TLC3541��ʱ��Ҫ����Ƭѡ����
        select();
        // ʹ��SPI�ķ����жϣ��Ա㿪���첽ͨ��
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
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET); // ��ʱ�޲�
#endif
}

void TLC3541::deselect()
{
#if PCB_VERSION == 1
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
#else
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); // ��ʱ�޲�
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
    // �����Զ�����ִ������
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
        m_samplingResult = (~SPI_I2S_ReceiveData(SPI2)) << 8; // ��ʱ�޲�
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
        m_samplingResult |= (~SPI_I2S_ReceiveData(SPI2)) & 0x00FF; // ��ʱ�޲�
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
    // ������
}


} // namespace
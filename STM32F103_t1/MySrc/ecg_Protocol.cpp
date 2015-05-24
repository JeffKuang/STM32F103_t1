/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#include "ecg_Protocol.h"
#include "ecg_SoC.h"
#include "ecg_Checksum.h"
#include <string.h>

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
Protocol* Protocol::m_instance = NULL;

////////////////////////////////////////////////////////////////////////////////
Protocol::Protocol()
    : m_sendingFrameSize(0),
    m_isReliableSending(false),
    m_isReceivedAck(false),
    m_retry(ReliableSendRetry),
    m_timeoutCounter(0),
    m_state(psInStartToken),
    m_token(0),
    m_hasToken(false),
    m_dataIndex(0),
    m_extDataSize(0),
    m_totalDataSize(0),
    m_crc(0)
{
    m_sendingFrame.header.startToken = StartToken;
    m_sendingFrame.header.id = Frame::InitialId;
    m_ackFrame.startToken = StartToken;
    m_ackFrame.type = Frame::tyAck;
    m_instance = this;
}

Protocol::~Protocol()
{
    m_instance = NULL;
}

void Protocol::initialize()
{
    SoC::getInstance()->setIsr(SoC::itUSART1, &handleUSART1);
    SoC::getInstance()->setIsr(SoC::itTIM2, &handleTIM2);
}

bool Protocol::send(const void* data, size_t size, bool reliable)
{
    // ��������������ȷ��
    assert_param(data != NULL);
    assert_param(size > 0);
    assert_param(size <= Frame::MaxDataSize);
    if (!m_isReliableSending) {
        // ֻ�в��ڷ��Ϳɿ�����֡ʱ�����ܽ���˴������
        // ����reliable��ֵ������m_isReliableSending
        m_isReliableSending = reliable; 
        if (reliable) {
            // ��ʼ���ط���������processResend()�лᱻ�Լ�
            m_retry = ReliableSendRetry; 
        }
        // ʹ����Ӧ�Ĳ����Է���֡sendingFrame���г�ʼ��
        m_sendingFrame.header.type = reliable ? 
            Frame::tyReliableData : Frame::tyUnreliableData;
        ++m_sendingFrame.header.id;
        m_sendingFrame.header.dataSize = size;
        m_sendingFrame.header.dataSizeAnti = ~m_sendingFrame.header.dataSize;
        // ���ڲ���CRC32�㷨����Ҫ��У������ݴ�С������4�ֽڶ���ģ�
        // ��ˣ�extDataSize���ڱ�ʾ��չ���ٸ��ֽں󣬲�������CRC32�㷨��Ҫ��
        size_t extDataSize = Frame::CRCSize - size % Frame::CRCSize;
        if (extDataSize == Frame::CRCSize) {
            // �������Ҫ��չ�κ��ֽڣ���extDataSize����Ϊ0
            extDataSize = 0;
        }
        // ����Ҫ���͵�����data���Ƶ�����֡����
        memcpy(m_sendingFrame.body, data, size);
        // ��ʵ�ʴ�С��body��������չ�ֽ���0
        memset(m_sendingFrame.body + size, 0, extDataSize);
        // ����CRC32У��ͣ�����ŵ�ʵ�ʴ�С��body�����4���ֽ���
        CRC32 crc = calcCRC32(reinterpret_cast<dword*>(&m_sendingFrame), 
            (sizeof(m_sendingFrame.header) + size + extDataSize) 
            / sizeof(dword));
        memcpy(m_sendingFrame.body + size + extDataSize, &crc, Frame::CRCSize);
        // ���㷢������֡��С
        m_sendingFrameSize = sizeof(m_sendingFrame.header) + 
            size + extDataSize + Frame::CRCSize;
        // ͨ��USART1��������֡
        SoC::getInstance()->sendByUSART1(&m_sendingFrame, m_sendingFrameSize);
        if (reliable) {
            // ���ڿɿ�����֡����Ҫ��ʼ���ط���ʱ�����������ҿ���TIM2��ʱ��
            m_timeoutCounter = ReliableSendTimeout;
            TIM_Cmd(TIM2, ENABLE);
        }
        return true;
    }
    return false;
}

void Protocol::processResend()
{
    // �ú���������ѭ���б�����
    if (m_isReliableSending && m_timeoutCounter == 0) {
        // ֻ�����ڷ��Ϳɿ�����֡�������ط���ʱ���������ں󣬲��ܽ���˴������
        TIM_Cmd(TIM2, DISABLE); // �ر�TIM2��ʱ��
        if (m_isReceivedAck) {
            // ����Ѿ����յ�ȷ��֡����λm_isReceivedAck��m_isReliableSending״̬
            m_isReceivedAck = false;
            m_isReliableSending = false;
        }
        --m_retry;
        SoC::getInstance()->sendByUSART1(&m_sendingFrame, m_sendingFrameSize);
        if (m_retry == 0) {
            // ����ط���������0����λm_isReceivedAck��m_isReliableSending״̬
            m_isReceivedAck = false;
            m_isReliableSending = false;
        }
        else {
            // ����ط������Դ���0�������³�ʼ���ط���ʱ�����������ҿ���TIM2��ʱ��
            m_timeoutCounter = ReliableSendTimeout;
            TIM_Cmd(TIM2, ENABLE);
        }
    }
}

bool Protocol::receive()
{
    if (m_hasToken) {
        // ����мǺ�Ҫ������ִ����������Զ���
        switch (m_state) {
        case psInStartToken:
            parseInStartToken();
            break;
        case psInTypeAndId:
            parseInTypeAndId();
            break;
        case psInDataSize:
            parseInDataSize();
            break;
        case psInDataSizeAnti:
            parseInDataSizeAnti();
            break;
        case psInData:
            parseInData();
            break;
        case psInCRC0:
            parseInCRC0();
            break;
        case psInCRC1:
            parseInCRC1();
            break;
        case psInCRC2:
            parseInCRC2();
            break;
        case psInCRC3:
            if (parseInCRC3()) {
                return true;
            }
            break;
        } // switch
    } // if
    else {
        // ���û�мǺţ����ȡ��һ���Ǻ�
        nextToken();
    } // if else
    return false;
}

void Protocol::getReceiveData(void** data, size_t& size)
{
    *data = m_receivingFrame.body;
    size = m_receivingFrame.header.dataSize;
}

void Protocol::handleUSART1()
{
    if (__HAL_USART_GET_FLAG(&huart1, USART_IT_RXNE) != RESET) {
        byte token = USART_ReceiveData(USART1); // ��UART1����һ���ֽ���Ϊ�Ǻ�
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
        m_instance->m_tokenQueue.push(token); // ���Ǻ�ѹ��ǺŶ�����
    }
    if (__HAL_USART_GET_FLAG(&huart1, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART1); // ���STM32��UART������λ��BUG
    }
}

void Protocol::handleTIM2()
{
    // ÿ��1ms��handleTIM2()������1��
    if (TIM_GetITStatus(TIM2, TIM_IT_CC1) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);
        if (m_instance->m_timeoutCounter > 0) {
            // ��m_timeoutCounter����0ʱ���������Լ���ֱ��0ʱ�����ʾ��ʱ����
            --m_instance->m_timeoutCounter;
        }
    }
}

void Protocol::nextToken()
{
    DISABLE_INTERRUPT();
    m_hasToken = m_tokenQueue.pop(m_token); // ֻ�Ǽ򵥵شӼǺŶ����е���һ���Ǻ�
    ENABLE_INTERRUPT();
}

void Protocol::sendAck(byte id)
{
    // ����һ��ȷ��֡���Ա���ͨ�ŶԷ�֪�����շ��͵�����֡�Ѿ���ȷ����
    m_ackFrame.id = id;
    SoC::getInstance()->sendByUSART1(&m_ackFrame, sizeof(m_ackFrame));
}

void Protocol::parseInStartToken()
{
    if (m_token == StartToken) {
        m_receivingFrame.header.startToken = m_token;
        m_state = psInTypeAndId;
    }
    nextToken();
}

void Protocol::parseInTypeAndId()
{
    m_receivingFrame.header.type = m_token & 0x0F;
    m_receivingFrame.header.id = m_token >> 4;
    switch (m_receivingFrame.header.type) {
    case Frame::tyReliableData:
    case Frame::tyUnreliableData:
        m_state = psInDataSize;
        break;
    case Frame::tyAck:
        if (m_receivingFrame.header.id == m_sendingFrame.header.id) {
            // �����յ����͵�����֡��Ӧ��ȷ��֡ʱ������m_isReceivedAckΪtrue��
            // �Ա���ȷ�ϳɹ���send()�����л�ȴ���״̬
            m_isReceivedAck = true;
            m_isReliableSending = false; // ��λm_isReliableSending״̬
            TIM_Cmd(TIM2, DISABLE); // �ر�TIM2��ʱ��
        }
        m_state = psInStartToken;
        break;
    default:
        m_state = psInStartToken;
        break;
    }
    nextToken();
}

void Protocol::parseInDataSize()
{
    if (m_token > 0) {
        m_receivingFrame.header.dataSize = m_token;
        // ����ΪCRC32�㷨Ҫ�����չ�����ݴ�С���ο�send()����
        m_extDataSize = Frame::CRCSize - m_receivingFrame.header.dataSize 
            % Frame::CRCSize;
        if (m_extDataSize == Frame::CRCSize) {
            m_extDataSize = 0;
        }
        // ���������չ��С���ڵ������ݴ�С
        m_totalDataSize = m_receivingFrame.header.dataSize + m_extDataSize;
        m_state = psInDataSizeAnti;
        nextToken();
    }
    else {
        m_state = psInStartToken;
    }
}

void Protocol::parseInDataSizeAnti()
{
    m_receivingFrame.header.dataSizeAnti = m_token;
    if (static_cast<byte>(~m_receivingFrame.header.dataSize) == 
        m_receivingFrame.header.dataSizeAnti) {
        // ������ݴ�СУ����ȷ��������������������ֽڵ�׼������
        m_dataIndex = 0;
        m_state = psInData;
        nextToken();
    }
    else {
        m_state = psInStartToken;
    }
}

void Protocol::parseInData()
{
    // ��ÿһ�����յ��ļǺſ���һ�������ֽڣ����洢��body��
    m_receivingFrame.body[m_dataIndex++] = m_token;
    if (m_dataIndex >= m_totalDataSize) {
        // ������յ����е������ֽڣ�����Խ���CRC32����
        m_crc = calcCRC32(reinterpret_cast<dword*>(&m_receivingFrame), 
            (sizeof(m_receivingFrame.header) + m_totalDataSize) 
            / sizeof(dword));
        m_state = psInCRC0;
    }
    nextToken();
}

void Protocol::parseInCRC0()
{
    m_crcInBytes[0] = m_token;
    m_state = psInCRC1;
    nextToken();
}

void Protocol::parseInCRC1()
{
    m_crcInBytes[1] = m_token;
    m_state = psInCRC2;
    nextToken();
}

void Protocol::parseInCRC2()
{
    m_crcInBytes[2] = m_token;
    m_state = psInCRC3;
    nextToken();
}

bool Protocol::parseInCRC3()
{
    m_crcInBytes[3] = m_token;
    if (m_crc == *reinterpret_cast<CRC32*>(m_crcInBytes)) {
        // ���CRC32У��ɹ�������Ϊһ������������֡�Ѿ����յ���������true
        if (m_receivingFrame.header.type == Frame::tyReliableData) {
            // ����ǽ��յ����ǿɿ�����֡������������һ��ȷ��֡���Է�
            sendAck(m_receivingFrame.header.id);
        }
        m_state = psInStartToken;
        nextToken();
        return true; // ��ʾ�ѽ��յ�һ������������֡
    }
    m_state = psInStartToken;
    return false; // ��ʾ����֡�Բ�����
}

} // namespace

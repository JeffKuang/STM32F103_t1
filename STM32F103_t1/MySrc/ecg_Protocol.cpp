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
    // 检查输入参数的正确性
    assert_param(data != NULL);
    assert_param(size > 0);
    assert_param(size <= Frame::MaxDataSize);
    if (!m_isReliableSending) {
        // 只有不在发送可靠数据帧时，才能进入此处代码块
        // 根据reliable的值来设置m_isReliableSending
        m_isReliableSending = reliable; 
        if (reliable) {
            // 初始化重发次数，在processResend()中会被自减
            m_retry = ReliableSendRetry; 
        }
        // 使用相应的参数对发送帧sendingFrame进行初始化
        m_sendingFrame.header.type = reliable ? 
            Frame::tyReliableData : Frame::tyUnreliableData;
        ++m_sendingFrame.header.id;
        m_sendingFrame.header.dataSize = size;
        m_sendingFrame.header.dataSizeAnti = ~m_sendingFrame.header.dataSize;
        // 由于采用CRC32算法，其要求被校验的数据大小必须是4字节对齐的，
        // 因此，extDataSize用于表示扩展多少个字节后，才能满足CRC32算法的要求
        size_t extDataSize = Frame::CRCSize - size % Frame::CRCSize;
        if (extDataSize == Frame::CRCSize) {
            // 如果不需要扩展任何字节，则extDataSize被设为0
            extDataSize = 0;
        }
        // 把需要发送的数据data复制到发送帧体中
        memcpy(m_sendingFrame.body, data, size);
        // 把实际大小的body中最后的扩展字节置0
        memset(m_sendingFrame.body + size, 0, extDataSize);
        // 计算CRC32校验和，并存放到实际大小的body的最后4个字节中
        CRC32 crc = calcCRC32(reinterpret_cast<dword*>(&m_sendingFrame), 
            (sizeof(m_sendingFrame.header) + size + extDataSize) 
            / sizeof(dword));
        memcpy(m_sendingFrame.body + size + extDataSize, &crc, Frame::CRCSize);
        // 计算发送数据帧大小
        m_sendingFrameSize = sizeof(m_sendingFrame.header) + 
            size + extDataSize + Frame::CRCSize;
        // 通过USART1发送数据帧
        SoC::getInstance()->sendByUSART1(&m_sendingFrame, m_sendingFrameSize);
        if (reliable) {
            // 对于可靠数据帧，需要初始化重发超时计数器，并且开启TIM2定时器
            m_timeoutCounter = ReliableSendTimeout;
            TIM_Cmd(TIM2, ENABLE);
        }
        return true;
    }
    return false;
}

void Protocol::processResend()
{
    // 该函数会在主循环中被调用
    if (m_isReliableSending && m_timeoutCounter == 0) {
        // 只有正在发送可靠数据帧，并且重发超时计数器到期后，才能进入此处代码块
        TIM_Cmd(TIM2, DISABLE); // 关闭TIM2定时器
        if (m_isReceivedAck) {
            // 如果已经接收到确认帧，则复位m_isReceivedAck和m_isReliableSending状态
            m_isReceivedAck = false;
            m_isReliableSending = false;
        }
        --m_retry;
        SoC::getInstance()->sendByUSART1(&m_sendingFrame, m_sendingFrameSize);
        if (m_retry == 0) {
            // 如果重发次数减到0，则复位m_isReceivedAck和m_isReliableSending状态
            m_isReceivedAck = false;
            m_isReliableSending = false;
        }
        else {
            // 如果重发次数仍大于0，则重新初始化重发超时计数器，并且开启TIM2定时器
            m_timeoutCounter = ReliableSendTimeout;
            TIM_Cmd(TIM2, ENABLE);
        }
    }
}

bool Protocol::receive()
{
    if (m_hasToken) {
        // 如果有记号要处理，则执行下面解析自动机
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
        // 如果没有记号，则读取下一个记号
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
        byte token = USART_ReceiveData(USART1); // 从UART1接收一个字节作为记号
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
        m_instance->m_tokenQueue.push(token); // 将记号压入记号队列中
    }
    if (__HAL_USART_GET_FLAG(&huart1, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART1); // 解决STM32的UART错误标记位的BUG
    }
}

void Protocol::handleTIM2()
{
    // 每隔1ms，handleTIM2()被调用1次
    if (TIM_GetITStatus(TIM2, TIM_IT_CC1) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_CC1);
        if (m_instance->m_timeoutCounter > 0) {
            // 当m_timeoutCounter大于0时，对其作自减，直到0时，则表示超时到达
            --m_instance->m_timeoutCounter;
        }
    }
}

void Protocol::nextToken()
{
    DISABLE_INTERRUPT();
    m_hasToken = m_tokenQueue.pop(m_token); // 只是简单地从记号队列中弹出一个记号
    ENABLE_INTERRUPT();
}

void Protocol::sendAck(byte id)
{
    // 发送一个确认帧，以便让通信对方知道它刚发送的数据帧已经正确到达
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
            // 当接收到发送的数据帧对应的确认帧时，设置m_isReceivedAck为true，
            // 以表明确认成功，send()函数中会等待该状态
            m_isReceivedAck = true;
            m_isReliableSending = false; // 复位m_isReliableSending状态
            TIM_Cmd(TIM2, DISABLE); // 关闭TIM2定时器
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
        // 计算为CRC32算法要求而扩展的数据大小，参考send()函数
        m_extDataSize = Frame::CRCSize - m_receivingFrame.header.dataSize 
            % Frame::CRCSize;
        if (m_extDataSize == Frame::CRCSize) {
            m_extDataSize = 0;
        }
        // 计算包括扩展大小在内的总数据大小
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
        // 如果数据大小校验正确，则做好逐个接收数据字节的准备工作
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
    // 将每一个接收到的记号看作一个数据字节，并存储在body中
    m_receivingFrame.body[m_dataIndex++] = m_token;
    if (m_dataIndex >= m_totalDataSize) {
        // 如果接收到所有的数据字节，则可以进行CRC32计算
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
        // 如果CRC32校验成功，则认为一个完整的数据帧已经接收到，并返回true
        if (m_receivingFrame.header.type == Frame::tyReliableData) {
            // 如果是接收到的是可靠数据帧，则立即发送一个确认帧给对方
            sendAck(m_receivingFrame.header.id);
        }
        m_state = psInStartToken;
        nextToken();
        return true; // 表示已接收到一个完整的数据帧
    }
    m_state = psInStartToken;
    return false; // 表示数据帧仍不可用
}

} // namespace

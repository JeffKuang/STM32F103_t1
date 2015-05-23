/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_PROTOCOL_H
#define ECG_PROTOCOL_H

#include "ecg_Core.h"
#include "ecg_NonCopyable.h"
#include "ecg_Checksum.h"
#include "ecg_BoundedQueue.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
/**
 * 通信协议
 * 详细内容，请参考《ECG采集板软件规格书》
 */
class Protocol : public NonCopyable
{
public:
    enum {
        StartToken = 0x7F, // 开始记号
        BufferSize = 4096, // 记号缓冲区大小，单位为字节
    };
public:
    /**
    * 构造器
    */
    Protocol();

    /**
    * 析构器
    */
    ~Protocol();

    /**
     * 初始化
     */
    void initialize();

    /**
     * 发送数据
     * @param[in] data 要发送的数据缓冲区
     * @param[in] size 缓冲区大小，单位字节
     * @param[in] reliable 是否可靠，如果是true，则启用重发确认机制
     */
    bool send(const void* data, size_t size, bool reliable = false);

    /**
     * 处理重发发送数据
     */
    void processResend();

    /**
     * 接收数据
     */
    bool receive();

    /**
     * 获得接收数据
     * @param[out] data 要接收的数据缓冲区
     * @param[in] size 缓冲区大小，单位字节
     */
    void getReceiveData(void** data, size_t& size);
private:
    enum {
        ReliableSendRetry = 10, // 可靠数据帧的错误重发次数
        ReliableSendTimeout = 500, // 可靠数据帧的错误重发超时，单位为毫秒
    };

// 设置内存为非对齐
#pragma pack(push, 1)
    struct Frame {
        // 帧类型，用于表示可靠数据帧和非可靠数据帧
        enum Type {
            tyReliableData = 0x0A, // 可靠数据帧
            tyUnreliableData = 0x0B, // 非可靠数据帧
            tyAck = 0x0C, // 确认帧
        };

        enum {
            CRCSize = 4, // CRC32大小，单位为字节
            MaxBodySize = 256, // 最大帧体大小，单位为字节
            MaxDataSize = 255, // 最大数据大小，单位为字节
            InitialId = 255, // 最初帧ID
        };

        // 帧头
        struct Header {
            byte startToken;
            struct {
                byte type: 4;
                byte id: 4;
            };
            byte dataSize;
            byte dataSizeAnti;
        } header;

        // 帧体
        byte body[MaxBodySize + CRCSize];
    };

    // 确认帧
    struct AckFrame {
        byte startToken;
        struct {
            byte type: 4;
            byte id: 4;
        };
    };
#pragma pack(pop)

    // 定义记号队列类型
    typedef BoundedQueue<byte, BufferSize> TokenQueue;

    // 解析状态
    enum ParseState {
        psInStartToken, // 处于开始记号状态
        psInTypeAndId, // 处于帧类型和ID状态
        psInDataSize, // 处于数据大小状态
        psInDataSizeAnti, // 处于数据大小反码状态
        psInData, // 处于数据状态
        psInCRC0, // 处于CRC字节0状态
        psInCRC1, // 处于CRC字节1状态
        psInCRC2, // 处于CRC字节2状态
        psInCRC3, // 处于CRC字节3状态
    };
private:
    /**
     * USART1中断处理程序
     */
    static void handleUSART1();

    /**
     * TIM2中断处理程序
     */
    static void handleTIM2();

    /**
     * 从记号队列中获得下一个记号，并存放在m_token中
     */
    void nextToken();

    /**
     * 发送确认帧
     * @param id 帧ID
     */
    void sendAck(byte id);

    /**
     * 解析处于开始记号状态
     */
    void parseInStartToken();

    /**
     * 解析处于帧类型和ID状态
     */
    void parseInTypeAndId();

    /**
     * 解析处于数据大小状态
     */
    void parseInDataSize();

    /**
     * 解析处于数据大小反码状态
     */
    void parseInDataSizeAnti();

    /**
     * 解析处于数据状态
     */
    void parseInData();

    /**
     * 解析处于CRC字节0状态
     */
    void parseInCRC0();

    /**
     * 解析处于CRC字节1状态
     */
    void parseInCRC1();

    /**
     * 解析处于CRC字节2状态
     */
    void parseInCRC2();

    /**
     * 解析处于CRC字节3状态
     */
    bool parseInCRC3();
private:
    static Protocol* m_instance;
    TokenQueue m_tokenQueue; // 记号队列
    Frame m_sendingFrame; // 用于发送的数据帧
    size_t m_sendingFrameSize; // 用于发送的数据帧的大小
    Frame m_receivingFrame; // 用于接收的帧（包括数据帧和确认帧）
    AckFrame m_ackFrame; // 用于发送确认帧
    bool m_isReliableSending; // 是否正在发送可靠数据帧
    bool m_isReceivedAck; // 是否收到确认帧
    size_t m_retry; // 可靠数据帧的错误重发次数
    volatile dword m_timeoutCounter; // 可靠数据帧的错误重发超时计数器
    ParseState m_state; // 解析状态
    byte m_token; // 记号
    bool m_hasToken; // 是否有记号，即表示m_token是否可用
    size_t m_dataIndex; // 帧载荷的数据索引，用于逐个数据字节的接收
    size_t m_extDataSize; // 扩展帧数据大小
    size_t m_totalDataSize; // 总的帧数据大小
    CRC32 m_crc; // CRC32校验和
    byte m_crcInBytes[Frame::CRCSize]; // 以字节数组表示的CRC32校验和（以小端方式存储）
};

} // namespace

#endif // ECG_PROTOCOL_H
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
 * ͨ��Э��
 * ��ϸ���ݣ���ο���ECG�ɼ����������顷
 */
class Protocol : public NonCopyable
{
public:
    enum {
        StartToken = 0x7F, // ��ʼ�Ǻ�
        BufferSize = 4096, // �ǺŻ�������С����λΪ�ֽ�
    };
public:
    /**
    * ������
    */
    Protocol();

    /**
    * ������
    */
    ~Protocol();

    /**
     * ��ʼ��
     */
    void initialize();

    /**
     * ��������
     * @param[in] data Ҫ���͵����ݻ�����
     * @param[in] size ��������С����λ�ֽ�
     * @param[in] reliable �Ƿ�ɿ��������true���������ط�ȷ�ϻ���
     */
    bool send(const void* data, size_t size, bool reliable = false);

    /**
     * �����ط���������
     */
    void processResend();

    /**
     * ��������
     */
    bool receive();

    /**
     * ��ý�������
     * @param[out] data Ҫ���յ����ݻ�����
     * @param[in] size ��������С����λ�ֽ�
     */
    void getReceiveData(void** data, size_t& size);
private:
    enum {
        ReliableSendRetry = 10, // �ɿ�����֡�Ĵ����ط�����
        ReliableSendTimeout = 500, // �ɿ�����֡�Ĵ����ط���ʱ����λΪ����
    };

// �����ڴ�Ϊ�Ƕ���
#pragma pack(push, 1)
    struct Frame {
        // ֡���ͣ����ڱ�ʾ�ɿ�����֡�ͷǿɿ�����֡
        enum Type {
            tyReliableData = 0x0A, // �ɿ�����֡
            tyUnreliableData = 0x0B, // �ǿɿ�����֡
            tyAck = 0x0C, // ȷ��֡
        };

        enum {
            CRCSize = 4, // CRC32��С����λΪ�ֽ�
            MaxBodySize = 256, // ���֡���С����λΪ�ֽ�
            MaxDataSize = 255, // ������ݴ�С����λΪ�ֽ�
            InitialId = 255, // ���֡ID
        };

        // ֡ͷ
        struct Header {
            byte startToken;
            struct {
                byte type: 4;
                byte id: 4;
            };
            byte dataSize;
            byte dataSizeAnti;
        } header;

        // ֡��
        byte body[MaxBodySize + CRCSize];
    };

    // ȷ��֡
    struct AckFrame {
        byte startToken;
        struct {
            byte type: 4;
            byte id: 4;
        };
    };
#pragma pack(pop)

    // ����ǺŶ�������
    typedef BoundedQueue<byte, BufferSize> TokenQueue;

    // ����״̬
    enum ParseState {
        psInStartToken, // ���ڿ�ʼ�Ǻ�״̬
        psInTypeAndId, // ����֡���ͺ�ID״̬
        psInDataSize, // �������ݴ�С״̬
        psInDataSizeAnti, // �������ݴ�С����״̬
        psInData, // ��������״̬
        psInCRC0, // ����CRC�ֽ�0״̬
        psInCRC1, // ����CRC�ֽ�1״̬
        psInCRC2, // ����CRC�ֽ�2״̬
        psInCRC3, // ����CRC�ֽ�3״̬
    };
private:
    /**
     * USART1�жϴ������
     */
    static void handleUSART1();

    /**
     * TIM2�жϴ������
     */
    static void handleTIM2();

    /**
     * �ӼǺŶ����л����һ���Ǻţ��������m_token��
     */
    void nextToken();

    /**
     * ����ȷ��֡
     * @param id ֡ID
     */
    void sendAck(byte id);

    /**
     * �������ڿ�ʼ�Ǻ�״̬
     */
    void parseInStartToken();

    /**
     * ��������֡���ͺ�ID״̬
     */
    void parseInTypeAndId();

    /**
     * �����������ݴ�С״̬
     */
    void parseInDataSize();

    /**
     * �����������ݴ�С����״̬
     */
    void parseInDataSizeAnti();

    /**
     * ������������״̬
     */
    void parseInData();

    /**
     * ��������CRC�ֽ�0״̬
     */
    void parseInCRC0();

    /**
     * ��������CRC�ֽ�1״̬
     */
    void parseInCRC1();

    /**
     * ��������CRC�ֽ�2״̬
     */
    void parseInCRC2();

    /**
     * ��������CRC�ֽ�3״̬
     */
    bool parseInCRC3();
private:
    static Protocol* m_instance;
    TokenQueue m_tokenQueue; // �ǺŶ���
    Frame m_sendingFrame; // ���ڷ��͵�����֡
    size_t m_sendingFrameSize; // ���ڷ��͵�����֡�Ĵ�С
    Frame m_receivingFrame; // ���ڽ��յ�֡����������֡��ȷ��֡��
    AckFrame m_ackFrame; // ���ڷ���ȷ��֡
    bool m_isReliableSending; // �Ƿ����ڷ��Ϳɿ�����֡
    bool m_isReceivedAck; // �Ƿ��յ�ȷ��֡
    size_t m_retry; // �ɿ�����֡�Ĵ����ط�����
    volatile dword m_timeoutCounter; // �ɿ�����֡�Ĵ����ط���ʱ������
    ParseState m_state; // ����״̬
    byte m_token; // �Ǻ�
    bool m_hasToken; // �Ƿ��мǺţ�����ʾm_token�Ƿ����
    size_t m_dataIndex; // ֡�غɵ�����������������������ֽڵĽ���
    size_t m_extDataSize; // ��չ֡���ݴ�С
    size_t m_totalDataSize; // �ܵ�֡���ݴ�С
    CRC32 m_crc; // CRC32У���
    byte m_crcInBytes[Frame::CRCSize]; // ���ֽ������ʾ��CRC32У��ͣ���С�˷�ʽ�洢��
};

} // namespace

#endif // ECG_PROTOCOL_H
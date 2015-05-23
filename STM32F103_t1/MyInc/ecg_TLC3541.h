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
 * ģ��/����ת����TLC3541
 * TLC3541��ͨ�Žӿ���SPI��ģʽ������ԭ��ͼ����ƣ���STM32��SPI2��ģʽ��֮ͨ�ţ�
 * ��������ϵͳ��ʵʱ�Ժ����ܵ�Ҫ�󶼽ϸߣ���ˣ�TLC3541��ʵ�ֲ���SPI���ж�ģʽ��
 * ��ͨ���Զ�������ȷ����ÿһ�������׶Ρ�
 */
class TLC3541 : public NonCopyable
{
public:
    enum {
        MinSamplingValue = 0, // ADC����С����ֵ
        MaxSamplingValue = 16383, // ADC��������ֵ
    };
public:
    /**
     * ������
     */
    TLC3541();

    /**
     * ������
     */
    ~TLC3541();

    /**
     * ��ʼ��
     */
    void initialize();

    /**
     * ��λ
     */
    void reset();

    /**
     * ��ʼ����
     */
    void startSampling();

    /**
     * ����Ƿ����ڲ���
     * @return ��ֵ
     */
    bool isSampling();

    /**
     * ͬ���ȴ�����������
     */
    void waitForSamplingResult();

    /**
     * ��ò������
     * @return ��ֵ
     */
    word getSamplingResult();
private:
    enum {
        ResetSamplingValue = 0x3FC0, // ������֤ADC��λ�Ƿ�ɹ��Ĳ���ֵ���ο�TLC3541����飩
    };

    // ����״̬
    enum SamplingState {
        ssInSendMSBClock, // ���ڷ��͸��ֽ�ʱ�ӵ�״̬
        ssInReceiveMSB, // ���ڽ��ո��ֽ����ݵ�״̬
        ssInSendLSBClock, // ���ڷ��͵��ֽ�ʱ�ӵ�״̬
        ssInReceiveLSB, // ���ڽ��յ��ֽ����ݵ�״̬
        ssInSendExtraClock, // ���ڷ��Ͷ���ʱ�ӵ�״̬
        ssInReceiveExtra, // ���ڽ��ն������ݵ�״̬
        ssInEnd, // ���ڽ�����״̬
    };
private:
    /**
     * SPI2�жϴ������
     */
    static void handleSPI2();

    /**
     * ADCƬѡ
     */
    void select();

    /**
     * ADCȡ��Ƭѡ
     */
    void deselect();

    /**
     * ͬ��˫���������ݣ��ȷ��ͺ��������
     */
    byte transmit(byte data);

    /**
     * �����Զ���
     */
    void sampling();

    /**
     * �������ڷ��͸��ֽ�ʱ�ӵ�״̬
     */
    void parseInSendMSBClock();

    /**
     * �������ڽ��ո��ֽ����ݵ�״̬
     */
    void parseInReceiveMSB();

    /**
     * �������ڷ��͵��ֽ�ʱ�ӵ�״̬
     */
    void parseInSendLSBClock();

    /**
     * �������ڽ��յ��ֽ����ݵ�״̬
     */
    void parseInReceiveLSB();

    /**
     * �������ڷ��Ͷ���ʱ�ӵ�״̬
     */
    void parseInSendExtraClock();

    /**
     * �������ڽ��ն������ݵ�״̬
     */
    void parseInReceiveExtra();

    /**
     * �������ڽ�����״̬
     */
    void parseInEnd();
private:
    static TLC3541* m_instance;
    volatile word m_samplingResult; // ADC�Ĳ��������16λ��ֵ��ֻ�е�14λ��Ч
    volatile SamplingState m_state; // ADC�Ĳ���״̬�����ڲ����Զ���
    volatile bool m_isSampling; // ��ʾADC�Ƿ����ڲ���״̬
};

} // namespace

#endif // ECG_TLC3541_H
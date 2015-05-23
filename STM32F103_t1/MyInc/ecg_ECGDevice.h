/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_ECGDEVICE_H
#define ECG_ECGDEVICE_H

#include "ecg_Core.h"
#include "ecg_NonCopyable.h"
#include "ecg_Lead.h"
#include "ecg_TLC3541.h"
#include "ecg_Checksum.h"
#include <string.h>

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
class ECGDevice;
class ECGDeviceCallback;

////////////////////////////////////////////////////////////////////////////////
/**
 * ECG����
 *
 * ��ʾ����һ��ECG������
 */
struct ECGLead
{
    friend class ECGDevice;
public:
    /**
     * ������
     */
    ECGLead()
    {
        initialize();
    }

    /**
     * ��ʼ��
     */
    void initialize()
    {
        type = ltI;
        point = 0;
        lastPoint = 0;
        gainType = lgtX1;
        lastGainType = lgtX1;
        alarm = false;
        lastAlarm = false;
        isValidPoint = false;
        lastIsValidPoint = false;
        prior = NULL;
        next = NULL;
        gainTypeBackup = lgtX1;
        resetPointOffsets();
    }

    /**
     * ��λ���������Ӧ�Ĳ�����ƫ����������֮��
     */
    void resetPointOffsets()
    {
        for (byte leadGainType = lgtX1; leadGainType < LeadGainTypeCount; ++leadGainType) {
            pointOffsets[leadGainType] = 0;
            pointOffsetSums[leadGainType] = 0;
        }
    }

    /**
     * ����޷��ŵĲ�����
     */
    unsigned short getUnsignedPoint()
    {
        return BaseLineLeadPoint + point; // �ɻ��ߵ�ѹAD�������з��Ų�����õ�
    }

    /**
     * ����޷��ŵ����Ĳ�����
     */
    unsigned short getUnsignedLastPoint()
    {
        return BaseLineLeadPoint + lastPoint; // ������getUnsignedPoint()
    }


    LeadType type; // ��������
    LeadPoint point; // �����㣬����ʵʱ����
    LeadPoint lastPoint; // ���Ĳ����㣬�������ݰ�ȫ���ʣ������Ǳ�ʾ��point�����һ�θ���
    LeadGainType gainType; // ��������
    LeadGainType lastGainType; // �����������ͣ��������ݰ�ȫ���ʣ������Ǳ�ʾ��gainType�����һ�θ���
    bool alarm; // ����״̬������ָʾ�Ƿ���ڵ���������߼�����ѹ����
    bool lastAlarm; // ���ľ���״̬���������ݰ�ȫ���ʣ������Ǳ�ʾ��alarm�����һ�θ���
    bool isValidPoint; // ���������Ƿ����
    bool lastIsValidPoint; // ���ĵ��������Ƿ���ã��������ݰ�ȫ���ʣ������Ǳ�ʾ��isValidPoint�����һ�θ���
private:
    enum {
        BaseLineLeadPoint = 8192, // ���ߵ�ѹ��AD�����൱��2.5V
    };
private:
    /**
     * ���������Ĳ�������
     */
    void copyToLast()
    {
        lastPoint = point;
        lastAlarm = alarm;
        lastGainType = gainType;
        lastIsValidPoint = isValidPoint;
    }
private:
    ECGLead* prior; // ָ��˫�������ǰһ��Ԫ��
    ECGLead* next; //  ָ��˫������ĺ�һ��Ԫ��
    LeadPoint pointOffsets[LeadGainTypeCount]; // ������ƫ���������飬ÿ��Ԫ�ض�Ӧһ�����棬�����ź�У׼
    int pointOffsetSums[LeadGainTypeCount]; // ������ƫ����֮�͵����飬ÿ��Ԫ�ض�Ӧһ�����棬�����ź�У׼
    LeadGainType gainTypeBackup;
};

////////////////////////////////////////////////////////////////////////////////
/**
 * �����˲�������
 */
enum BuiltInFilterType {
    bftFilter0 = 0x00,
    bftFilter1 = 0x01,
};

enum {
    BuiltInFilterTypeCount = 2,
};

inline bool isBuiltInFilterType(byte filterType)
{
    switch (filterType) {
    case bftFilter0:
    case bftFilter1:
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
/**
 * ECG�豸ָ�������ڲɼ������ĵ��źŵĵ����豸����ģ�顣
 */
class ECGDevice : public NonCopyable
{
public:
    /**
     * ������
     */
    ECGDevice();

    /**
     * ������
     */
    ~ECGDevice();

    /**
     * ��ʼ��
     */
    void initialize();

    /**
     * ��ʼ����
     */
    void startSampling();

    /**
     * ֹͣ����
     */
    void stopSampling();

    /**
     * ����Ƿ����ڲ���
     * @return ��ֵ
     */
    bool isSampling();

    /**
     * ��ò����Ƿ����
     * @return ��ֵ
     */
    bool isSamplingPeriodCompleted();

    /**
     * ����������
     */
    void clearSamplingPeriodCompletedFlag();

    /**
     * ��õ���
     * @param[in] leadType ��������
     * @return ��ֵ
     */
    ECGLead& getLead(LeadType leadType);

    /**
     * �����ź��Ƿ�λ
     * @param[in] signalReset ��ֵ
     */
    void setSignalReset(bool signalReset);

    /**
     * ��ʼУ׼�ź�
     */
    void startCalibrateSignal();

    /**
     * ֹͣУ׼�ź�
     */
    void stopCalibrateSignal();

    /**
     * �Ƿ�����У׼�ź�
     * @return ��ֵ
     */
    bool isCalibratingSignal();

    /**
     * �Ƿ����У׼�ź�
     * @return ��ֵ
     */
    bool isCalibrateSignalCompleted();

    /**
     * ���У׼�ź�
     */
    void clearCalibratingSignal();

    /**
     * ���������˲���ʹ��
     * @param[in] filterType �˲�������
     * @param[in] enabled ��ֵ
     */
    void setupBuiltInFilter(BuiltInFilterType filterType, bool enabled);
private:
    enum {
        MaxCalibrateLeadPointCount = 1000, // ����У׼�źŲ�����������ͨ����1000�㼴��
        CalibrateResultAddressInFlash = 0x0800F800, // ������2ҳ�׵�ַ��Flash Size = 64K
        CalibrateResultPaddingSize = 21, // У׼���������ֽ�������������CRC32�㷨��4�ֽڶ����Ҫ��

        SamplingTimeSliceCount = 4, // ��ÿ�����Ĳ���ʱ��125us�ֳ�4�ݣ�ÿ��Ϊһ������ʱ��Ƭ31.25us = (1/4)*125us
    };

    /**
     * ����״̬
     */
    enum SamplingState {
        ssInReady, // ���ھ���״̬
        ssInSampling, // ���ڲ���״̬
    };
// �����ڴ�Ϊ�Ƕ���
#pragma pack(push, 1)
    // У׼������ݽṹ������ֱ�Ӵ洢��Flash����
    union CalibrateResult {
        CRC32 calcCRC() const
        {
            return calcCRC32(reinterpret_cast<const dword*>(this), sizeof(pointOffsets) / sizeof(dword));
        }

        struct {
            short pointOffsets[LeadTypeCount][LeadGainTypeCount];
            dword crc;
        };
        dword padding[CalibrateResultPaddingSize];
    };
#pragma pack(pop)
private:
    /**
     * SysTick�жϴ������
     */
    static void handleSysTick();

    /**
     * ѡ��ǰ������ֱ��д��Ӧ��GPIO
     */
    void selectLeadType(LeadType leadType);

    /**
     * ѡ��ǰ���棬ֱ��д��Ӧ��GPIO
     */
    void selectLeadGainType(LeadGainType leadGainType);

    /**
     * ��⵱ǰ��������״̬��ֱ�Ӷ���Ӧ��GPIO
     */
    bool checkLeadAlarm();

    /**
     * Ӧ�ö�������
     */
    void applyExtraLeadGain(LeadGainType leadGainType, LeadPoint& leadPoint);

    /**
     * ���Ʋ����㷶Χ
     */
    void limitLeadPointRange(LeadPoint& leadPoint);

    /**
     * ���ADC������
     */
    LeadPoint getAdcSamplingPoint();

    /**
     * �������ھ���״̬
     */
    void parseInReady();

    /**
     * �������ڲ���״̬
     */
    void parseInSampling();

    /**
     * ��������0
     */
    void samplingTask0();

    /**
     * ��������1
     */
    void samplingTask1();

    /**
     * ��������2
     */
    void samplingTask2();

    /**
     * ��������3
     */
    void samplingTask3();

    /**
     * �������̣�ÿ������ʱ��Ƭ������һ�Σ�����Ϊ31.25us
     */
    void sampling();

    /**
     * ��λ����״̬
     */
    void resetSampling();

    /**
     * ��Flash��װ��У׼���
     */
    void loadCalibrateResult();

    /**
     * ����У׼�����Flash��
     */
    void saveCalibrateResult();

    /**
     * ���У׼�Ƿ����
     */
    void checkCalibrateCompleted();
private:
    static ECGDevice* m_instance;
    TLC3541 m_adc; // ��ʾTLC3541 ADC
    ECGLead m_leads[LeadTypeCount]; // ���������飬ÿһ������Ӧһ�ֵ������ͣ����ڷ���
    ECGLead* m_lead; // ָ��m_leads�е�һ��Ԫ�أ���ʾ��ǰ���ڲ����ĵ���
    volatile byte m_samplingTimeSlice; // ����ʱ��Ƭ��ֻ��4��ȡֵ��0,1,2,3
    volatile bool m_isSampling; // �Ƿ����ڲ���������
    volatile SamplingState m_samplingState; // ��ǰ�Ĳ���״̬
    volatile bool m_needAdcStartSimpling; // �����ڲ���ʱ��Ƭ1�У������Ƿ���Ҫ��ʼADC���첽����
    volatile bool m_isFirstSamplingPeriod; // �����ж��Ƿ����ڵ�һ���������ڣ�ÿ�ε���startSampling()����Ϊ�ǵ�һ���������ڣ���һ����������Ϊ1ms
    volatile bool m_isSamplingPeriodCompleted; // �����ж�һ�����������Ƿ��Ѿ���ɣ��Ա��ȡ���е����Ĳ�������
    volatile bool m_isCalibratingSignal; // �����ж��Ƿ���У׼�ź���
    volatile bool m_isCalibrateSignalCompleted; // �����ж�У׼�ź��Ƿ��Ѿ���ɣ��Ա��л���֪ͨ����
    volatile byte m_calibratedLeadGainType; // ����ָʾУ׼�źŹ����еĵ�ǰ����
    volatile size_t m_calibratedLeadPointCount; // ���ڼ�����У׼�źŵĵ����
};

} // namespace

#endif // ECG_ECGDEVICE_H
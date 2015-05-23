/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#include "ecg_ECGDevice.h"
#include "ecg_SoC.h"
#include "ecg_SysUtils.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
ECGDevice* ECGDevice::m_instance = NULL;

////////////////////////////////////////////////////////////////////////////////
ECGDevice::ECGDevice()
    : m_lead(&m_leads[LeadTypeCount - 1]),
    m_samplingTimeSlice(0),
    m_isSampling(false),
    m_samplingState(ssInReady),
    m_needAdcStartSimpling(false),
    m_isFirstSamplingPeriod(true),
    m_isSamplingPeriodCompleted(false),
    m_isCalibratingSignal(false),
    m_isCalibrateSignalCompleted(false),
    m_calibratedLeadGainType(lgtX1),
    m_calibratedLeadPointCount(0)
{
    m_instance = this;

    for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
        ECGLead& lead = m_leads[leadType];
        lead.type = static_cast<LeadType>(leadType);
    }

    // ��m_leads��ʼ����һ��ѭ��˫����������ͼ��
    //
    // ��-> I <--> II <--> V1 <--> V2 <--> V3 <--> V4 <--> V5 <--> V6 <-��
    // ��---------------------------------------------------------------��

    // m_lead�����ָ��V6����

    m_leads[ltI].prior = &m_leads[ltV6];
    m_leads[ltII].prior = &m_leads[ltI];
    m_leads[ltV1].prior = &m_leads[ltII];
    m_leads[ltV2].prior = &m_leads[ltV1];
    m_leads[ltV3].prior = &m_leads[ltV2];
    m_leads[ltV4].prior = &m_leads[ltV3];
    m_leads[ltV5].prior = &m_leads[ltV4];
    m_leads[ltV6].prior = &m_leads[ltV5];

    m_leads[ltI].next = &m_leads[ltII];
    m_leads[ltII].next = &m_leads[ltV1];
    m_leads[ltV1].next = &m_leads[ltV2];
    m_leads[ltV2].next = &m_leads[ltV3];
    m_leads[ltV3].next = &m_leads[ltV4];
    m_leads[ltV4].next = &m_leads[ltV5];
    m_leads[ltV5].next = &m_leads[ltV6];
    m_leads[ltV6].next = &m_leads[ltI];
}

ECGDevice::~ECGDevice()
{
}

void ECGDevice::initialize()
{
    m_adc.initialize();
    setSignalReset(false);
    for (byte filterType = bftFilter0; filterType < BuiltInFilterTypeCount; 
		++filterType) {
        setupBuiltInFilter(static_cast<BuiltInFilterType>(filterType), false);
    }
    loadCalibrateResult();
    SoC::getInstance()->setIsr(SoC::itSysTick, &handleSysTick);
    resetSampling();
}

void ECGDevice::startSampling()
{
    if (!m_isSampling) {
        // ÿ����ʼ����ʱ����Ҫ��λ��ص����裬�Լ�һЩ״̬����
        m_adc.reset();
        resetSampling();
        m_samplingTimeSlice = 0;
        m_isSampling = true;
    }
}

void ECGDevice::stopSampling()
{
    m_isSampling = false;
}

bool ECGDevice::isSampling()
{
    return m_isSampling;
}

bool ECGDevice::isSamplingPeriodCompleted()
{
    return m_isSamplingPeriodCompleted;
}

void ECGDevice::clearSamplingPeriodCompletedFlag()
{
    m_isSamplingPeriodCompleted = false;
}

ECGLead& ECGDevice::getLead(LeadType leadType)
{
    return m_leads[leadType];
}

void ECGDevice::setSignalReset(bool signalReset)
{
#if PCB_VERSION < 3
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, signalReset ? GPIO_PIN_RESET : GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, signalReset ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

void ECGDevice::startCalibrateSignal()
{
    if (!m_isCalibratingSignal) {
        // Ϊ��ʼУ׼�ź�����ʼ������
        m_calibratedLeadGainType = lgtX1;
        m_calibratedLeadPointCount = 0;
        for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
            ECGLead& lead = m_leads[leadType];
            lead.gainTypeBackup = lead.gainType; // ���ݵ�ǰ���棬�Ա�У׼��ɺ�ԭ֮
            lead.gainType = static_cast<LeadGainType>(m_calibratedLeadGainType);
            for (byte leadGainType = lgtX1; leadGainType < LeadGainTypeCount; 
				++leadGainType) {
                lead.pointOffsets[leadGainType] = 0;
                lead.pointOffsetSums[leadGainType] = 0;
            }
        }
        resetSampling();
        // ����ʼ����ɺ����������״̬
        m_isCalibrateSignalCompleted = false;
        m_isCalibratingSignal = true;
    }
}

void ECGDevice::stopCalibrateSignal()
{
    if (m_isCalibratingSignal) {
        m_isCalibratingSignal = false;
        // ��ֹͣ����ʱ������ÿ�������Ĳ�����ƫ����
        for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
            ECGLead& lead = m_leads[leadType];
            for (byte leadGainType = lgtX1; leadGainType < LeadGainTypeCount; 
				++leadGainType) {
                lead.pointOffsets[leadGainType] = lead.pointOffsetSums[leadGainType] / 
					static_cast<int>(m_calibratedLeadPointCount);
            }
            lead.gainType = lead.gainTypeBackup;
        }
        // ����У׼�����Flash
        saveCalibrateResult();
    }
}

bool ECGDevice::isCalibratingSignal()
{
    return m_isCalibratingSignal;
}

bool ECGDevice::isCalibrateSignalCompleted()
{
    return m_isCalibrateSignalCompleted;
}

void ECGDevice::clearCalibratingSignal()
{
    // ��λÿһ�����Ĳ���ƫ����
    for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
        m_leads[leadType].resetPointOffsets();
    }
    // ���Flash�е�У׼���
    HAL_FLASH_Unlock();
    FLASH_PageErase(CalibrateResultAddressInFlash);
    HAL_FLASH_Lock();
}

void ECGDevice::setupBuiltInFilter(BuiltInFilterType filterType, bool enabled)
{
    switch (filterType) {
    case bftFilter0:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;
    case bftFilter1:
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;
    }
}

void ECGDevice::selectLeadType(LeadType leadType)
{
    switch (leadType) {
    case ltI:
#if PCB_VERSION < 3
        // Pin4,5,6: 000
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
#else
        // Pin4,5,6: 111
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SET);
#endif
        break;
    case ltII:
#if PCB_VERSION < 3
        // Pin4,5,6: 100
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
#else
        // Pin4,5,6: 011
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SET);
#endif
        break;
    case ltV1:
#if PCB_VERSION < 3
        // Pin4,5,6: 010
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
#else
        // Pin4,5,6: 101
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_6, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
#endif
        break;
    case ltV2:
#if PCB_VERSION < 3
        // Pin4,5,6: 110
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_SET);
#else
        // Pin4,5,6: 001
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
#endif
        break;
    case ltV3:
#if PCB_VERSION < 3
        // Pin4,5,6: 001
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
#else
        // Pin4,5,6: 110
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
#endif
        break;
    case ltV4:
#if PCB_VERSION < 3
        // Pin4,5,6: 101
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_6, GPIO_PIN_SET);
#else
        // Pin4,5,6: 010
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_6, GPIO_PIN_RESET);
#endif
        break;
    case ltV5:
#if PCB_VERSION < 3
        // Pin4,5,6: 011
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SET);
#else
        // Pin4,5,6: 100
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
#endif
        break;
    case ltV6:
#if PCB_VERSION < 3
        // Pin4,5,6: 111
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SET);
#else
        // Pin4,5,6: 000
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
#endif
        break;
    }
}

void ECGDevice::selectLeadGainType(LeadGainType leadGainType)
{
    switch (leadGainType) {
    case lgtX1:
#if PCB_VERSION < 3
        // Pin0,1: 10
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
#else
        // Pin0,1: 01
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
#endif
        break;
    case lgtX2:
    case lgtX4:
#if PCB_VERSION < 3
        // Pin0,1: 01
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
#else
        // Pin0,1: 10
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
#endif
        break;
    case lgtX0p5:
    case lgtX0p25:
#if PCB_VERSION < 3
        // Pin0,1: 00
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
#else
        // Pin0,1: 11
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_SET);
#endif
        break;
    }
}

bool ECGDevice::checkLeadAlarm()
{
#if PCB_VERSION < 3
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_SET ? true : false;
#else
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10) == GPIO_PIN_SET ? false : true;
#endif
}

void ECGDevice::applyExtraLeadGain(LeadGainType leadGainType, LeadPoint& leadPoint)
{
    // Ŀǰֻ��x4��x0.25�����������ڶ���ģ���Ҫͨ�������ʽ���Ŵ������С
    switch (leadGainType) {
        case lgtX4:
			// ����ʵ��Ӳ���Ѿ�����Ϊx2���棬�������ֻҪ�Ŵ�2�����ɣ�������4��
            leadPoint *= 2; 
            break;
        case lgtX0p25:
			// ����ʵ��Ӳ���Ѿ�����Ϊx0.5���棬�������ֻҪ��С2�����ɣ�������4��
            leadPoint /= 2; 
            break;
    }
}

void ECGDevice::limitLeadPointRange(LeadPoint& leadPoint)
{
    // ���Ʋ����㲻�ܳ���[+2.5V,-2.5V]��Χ
    if (leadPoint < -ECGLead::BaseLineLeadPoint) {
        leadPoint = -ECGLead::BaseLineLeadPoint;
    }
    else if (leadPoint > ECGLead::BaseLineLeadPoint) {
        leadPoint = ECGLead::BaseLineLeadPoint;
    }
}

LeadPoint ECGDevice::getAdcSamplingPoint()
{
    return ECGLead::BaseLineLeadPoint - 
		static_cast<LeadPoint>(m_adc.getSamplingResult());
}

void ECGDevice::parseInReady()
{
    selectLeadGainType(m_lead->next->gainType);
    selectLeadType(m_lead->next->type);
    m_samplingState = ssInSampling;
}

void ECGDevice::parseInSampling()
{
    bool tempLeadAlarm = checkLeadAlarm(); // ���ݵ�ǰ��������״̬

    // ����Ӳ��ѡ������͵���
    selectLeadGainType(m_lead->next->gainType);
    selectLeadType(m_lead->next->type);
    
    DISABLE_INTERRUPT();
    if (m_adc.isSampling()) {
        m_lead->prior->isValidPoint = false;
    }
    else {
        // ���ADC�����㣬����ȥ��ƫ��������
        m_lead->prior->point = getAdcSamplingPoint() - 
			m_lead->prior->pointOffsets[m_lead->prior->gainType];
        m_lead->prior->isValidPoint = true;
    }
    ENABLE_INTERRUPT();
    
    if (!m_isCalibratingSignal) {
        // Ӧ�ö�������棬���磺x4��x0.25����ΪĿǰӲ����δ֧��
        applyExtraLeadGain(m_lead->prior->gainType, m_lead->prior->point);
    }

    // ���Ʋ�����ķ�Χ
    limitLeadPointRange(m_lead->prior->point);

    if (m_isCalibratingSignal && !m_isCalibrateSignalCompleted) {
        // �ۼ�ÿһ�εĲ�����
        m_lead->prior->pointOffsetSums[m_lead->prior->gainType] 
			+= m_lead->prior->point;
    }
    
    if (m_lead->type == ltI) {
        // ��m_lead->type == ltI����ʱ����ʾ��һ�β������ڸս���������һ�β������ڼ�����ʼ
        if (m_isFirstSamplingPeriod) {
            // ���Ե�һ�β�������
            m_isFirstSamplingPeriod = false;
        }
        else {
            // �ӵڶ��β������ڼ�����ʼʱ��������һ�β������ڵĲ�������
            for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
                m_leads[leadType].copyToLast();
            }
            // ����У׼�ź�
            if (m_isCalibratingSignal && !m_isCalibrateSignalCompleted) {
                ++m_calibratedLeadPointCount;
                checkCalibrateCompleted();
            }
            // 
            m_isSamplingPeriodCompleted = true;
        } // if
    } // if

    // �����ȱ��ݵĵ�������״̬�����Ƶ���ǰ�ĵ���
    m_lead->alarm = tempLeadAlarm;
}

void ECGDevice::samplingTask0()
{
    // ������
}

void ECGDevice::samplingTask1()
{
    if (m_needAdcStartSimpling) {
        m_needAdcStartSimpling = false;
        m_adc.startSampling();
    }
}

void ECGDevice::samplingTask2()
{
    // ������
}

void ECGDevice::samplingTask3()
{
    switch (m_samplingState) {
        case ssInReady:
            parseInReady();
            break;
        case ssInSampling:
            parseInSampling();
            break;
    } // switch
    m_needAdcStartSimpling = true;
    m_lead = m_lead->next;
}

void ECGDevice::sampling()
{
    switch (m_samplingTimeSlice) {
    case 0: // ʱ��Ƭ0
        samplingTask0();
        break;
    case 1: // ʱ��Ƭ1
        samplingTask1();
        break;
    case 2: // ʱ��Ƭ2
        samplingTask2();
        break;
    case 3: // ʱ��Ƭ3
        samplingTask3();
        break;
    }
}

void ECGDevice::resetSampling()
{
    m_samplingState = ssInReady;
    m_needAdcStartSimpling = false;
    m_lead = &m_leads[LeadTypeCount - 1];
    m_isFirstSamplingPeriod = true;
    clearSamplingPeriodCompletedFlag();
    for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
        ECGLead& lead = m_leads[leadType];
        lead.alarm = false;
        lead.point = 0;
    }
}

void ECGDevice::loadCalibrateResult()
{
    const CalibrateResult* calibrateResult = 
		reinterpret_cast<const CalibrateResult*>(CalibrateResultAddressInFlash);
    // Ϊȷ�����ݿɿ��ԣ���Ҫ�Դ����Flash�е�У׼�������CRC32У��
    if (calibrateResult->calcCRC() == calibrateResult->crc) {
        // װ��ÿһ�������Ĳ���ƫ����
        for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
            ECGLead& lead = m_leads[leadType];
            for (byte leadGainType = lgtX1; leadGainType <= lgtX0p25; ++leadGainType) {
                lead.pointOffsets[leadGainType] = 
					calibrateResult->pointOffsets[leadType][leadGainType];
            }
        }
    }
}

void ECGDevice::saveCalibrateResult()
{
    // ��ÿһ�������Ĳ���ƫ������װ��У׼������ݽṹ
    CalibrateResult calibrateResult;
    for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
        ECGLead& lead = m_leads[leadType];
        for (byte leadGainType = lgtX1; leadGainType < LeadGainTypeCount; 
			++leadGainType) {
            calibrateResult.pointOffsets[leadType][leadGainType] = 
				lead.pointOffsets[leadGainType];
        }
    }
    // ������������CRC32У����
    calibrateResult.crc = calibrateResult.calcCRC();
    // ������������Flash
    HAL_FLASH_Unlock();
    FLASH_PageErase(CalibrateResultAddressInFlash);
    for (size_t i = 0; i < CalibrateResultPaddingSize; ++i) {
        FLASH_PROC_PROGRAMWORD(FLASH_PROC_PROGRAMWORD, CalibrateResultAddressInFlash 
			+ sizeof(dword) * i, calibrateResult.padding[i]);
    }
    HAL_FLASH_Lock();
}

void ECGDevice::checkCalibrateCompleted()
{
    if (m_calibratedLeadPointCount >= MaxCalibrateLeadPointCount) {
        // ��У׼�Ĳ����������㹻�󣬷�����2���������
        if (m_calibratedLeadGainType < (LeadGainTypeCount - 1)) {
            // �����滹û�е������ʱ�����л�����һ������
            ++m_calibratedLeadGainType;
            m_calibratedLeadPointCount = 0; // ��λУ׼�Ĳ�����������Ա㿪ʼ��һ�������У׼
            for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
                m_leads[leadType].gainType = static_cast<LeadGainType>(m_calibratedLeadGainType);
            }
        }
        else {
            // �����е���ҪУ׼�����涼������ɺ󣬲���Ϊ����У׼���������
            m_isCalibrateSignalCompleted = true;
        }
    }
}

void ECGDevice::handleSysTick()
{
    if (m_instance->m_isSampling) {
        // �����������
        m_instance->sampling();
        // �л�����һ��ʱ��Ƭ
        if (++m_instance->m_samplingTimeSlice == SamplingTimeSliceCount) {
            m_instance->m_samplingTimeSlice = 0;
        }
    }
}

} // namespace

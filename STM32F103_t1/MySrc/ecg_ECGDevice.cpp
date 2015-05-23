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

    // 把m_leads初始化成一个循环双向链表，如下图：
    //
    // ┌-> I <--> II <--> V1 <--> V2 <--> V3 <--> V4 <--> V5 <--> V6 <-┐
    // └---------------------------------------------------------------┘

    // m_lead最初是指向V6导联

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
        // 每当开始采样时，都要复位相关的外设，以及一些状态变量
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
        // 为开始校准信号做初始化工作
        m_calibratedLeadGainType = lgtX1;
        m_calibratedLeadPointCount = 0;
        for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
            ECGLead& lead = m_leads[leadType];
            lead.gainTypeBackup = lead.gainType; // 备份当前增益，以便校准完成后还原之
            lead.gainType = static_cast<LeadGainType>(m_calibratedLeadGainType);
            for (byte leadGainType = lgtX1; leadGainType < LeadGainTypeCount; 
				++leadGainType) {
                lead.pointOffsets[leadGainType] = 0;
                lead.pointOffsetSums[leadGainType] = 0;
            }
        }
        resetSampling();
        // 当初始化完成后，设置下面的状态
        m_isCalibrateSignalCompleted = false;
        m_isCalibratingSignal = true;
    }
}

void ECGDevice::stopCalibrateSignal()
{
    if (m_isCalibratingSignal) {
        m_isCalibratingSignal = false;
        // 当停止增益时，计算每个导联的采样点偏移量
        for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
            ECGLead& lead = m_leads[leadType];
            for (byte leadGainType = lgtX1; leadGainType < LeadGainTypeCount; 
				++leadGainType) {
                lead.pointOffsets[leadGainType] = lead.pointOffsetSums[leadGainType] / 
					static_cast<int>(m_calibratedLeadPointCount);
            }
            lead.gainType = lead.gainTypeBackup;
        }
        // 保存校准结果到Flash
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
    // 复位每一导联的采样偏移量
    for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
        m_leads[leadType].resetPointOffsets();
    }
    // 清除Flash中的校准结果
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
    // 目前只有x4和x0.25两个增益属于额外的，需要通过软件方式来放大或者缩小
    switch (leadGainType) {
        case lgtX4:
			// 假设实际硬件已经设置为x2增益，因此这里只要放大2倍即可，而不是4倍
            leadPoint *= 2; 
            break;
        case lgtX0p25:
			// 假设实际硬件已经设置为x0.5增益，因此这里只要缩小2倍即可，而不是4倍
            leadPoint /= 2; 
            break;
    }
}

void ECGDevice::limitLeadPointRange(LeadPoint& leadPoint)
{
    // 限制采样点不能超出[+2.5V,-2.5V]范围
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
    bool tempLeadAlarm = checkLeadAlarm(); // 备份当前导联警报状态

    // 操作硬件选择增益和导联
    selectLeadGainType(m_lead->next->gainType);
    selectLeadType(m_lead->next->type);
    
    DISABLE_INTERRUPT();
    if (m_adc.isSampling()) {
        m_lead->prior->isValidPoint = false;
    }
    else {
        // 获得ADC采样点，并减去其偏移量（误差）
        m_lead->prior->point = getAdcSamplingPoint() - 
			m_lead->prior->pointOffsets[m_lead->prior->gainType];
        m_lead->prior->isValidPoint = true;
    }
    ENABLE_INTERRUPT();
    
    if (!m_isCalibratingSignal) {
        // 应用额外的增益，比如：x4和x0.25，因为目前硬件并未支持
        applyExtraLeadGain(m_lead->prior->gainType, m_lead->prior->point);
    }

    // 限制采样点的范围
    limitLeadPointRange(m_lead->prior->point);

    if (m_isCalibratingSignal && !m_isCalibrateSignalCompleted) {
        // 累加每一次的采样点
        m_lead->prior->pointOffsetSums[m_lead->prior->gainType] 
			+= m_lead->prior->point;
    }
    
    if (m_lead->type == ltI) {
        // 当m_lead->type == ltI成立时，表示上一次采样周期刚结束，而新一次采样周期即将开始
        if (m_isFirstSamplingPeriod) {
            // 忽略第一次采样周期
            m_isFirstSamplingPeriod = false;
        }
        else {
            // 从第二次采样周期即将开始时，复制上一次采样周期的采样数据
            for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
                m_leads[leadType].copyToLast();
            }
            // 处理校准信号
            if (m_isCalibratingSignal && !m_isCalibrateSignalCompleted) {
                ++m_calibratedLeadPointCount;
                checkCalibrateCompleted();
            }
            // 
            m_isSamplingPeriodCompleted = true;
        } // if
    } // if

    // 将早先备份的导联警报状态，复制到当前的导联
    m_lead->alarm = tempLeadAlarm;
}

void ECGDevice::samplingTask0()
{
    // 空任务
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
    // 空任务
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
    case 0: // 时间片0
        samplingTask0();
        break;
    case 1: // 时间片1
        samplingTask1();
        break;
    case 2: // 时间片2
        samplingTask2();
        break;
    case 3: // 时间片3
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
    // 为确保数据可靠性，需要对存放在Flash中的校准结果进行CRC32校验
    if (calibrateResult->calcCRC() == calibrateResult->crc) {
        // 装载每一个导联的采样偏移量
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
    // 用每一个导联的采样偏移量来装配校准结果数据结构
    CalibrateResult calibrateResult;
    for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
        ECGLead& lead = m_leads[leadType];
        for (byte leadGainType = lgtX1; leadGainType < LeadGainTypeCount; 
			++leadGainType) {
            calibrateResult.pointOffsets[leadType][leadGainType] = 
				lead.pointOffsets[leadGainType];
        }
    }
    // 计算采样结果的CRC32校验码
    calibrateResult.crc = calibrateResult.calcCRC();
    // 保存采样结果到Flash
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
        // 当校准的采样点数量足够后，分以下2种情况处理
        if (m_calibratedLeadGainType < (LeadGainTypeCount - 1)) {
            // 当增益还没有迭代完成时，则切换到下一种增益
            ++m_calibratedLeadGainType;
            m_calibratedLeadPointCount = 0; // 复位校准的采样点计数，以便开始下一种增益的校准
            for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
                m_leads[leadType].gainType = static_cast<LeadGainType>(m_calibratedLeadGainType);
            }
        }
        else {
            // 当所有的需要校准的增益都迭代完成后，才认为整个校准过程完成了
            m_isCalibrateSignalCompleted = true;
        }
    }
}

void ECGDevice::handleSysTick()
{
    if (m_instance->m_isSampling) {
        // 进入采样过程
        m_instance->sampling();
        // 切换到下一个时间片
        if (++m_instance->m_samplingTimeSlice == SamplingTimeSliceCount) {
            m_instance->m_samplingTimeSlice = 0;
        }
    }
}

} // namespace

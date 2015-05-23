/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#include "ecg_Application.h"
#include "ecg_SoC.h"


namespace ecg {

#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUILD_NUMBER 1

#define DEVICE_ID 1
#define DEVICE_NAME "ECG Analog Board"
#define DEVICE_VENDOR "Heart Test Labs."
#define DEVICE_DESCRIPTION "This device is a part of MyoVista."

////////////////////////////////////////////////////////////////////////////////
Application Application::m_instance;

////////////////////////////////////////////////////////////////////////////////
Application::Application()
    : m_lastSamplingResultIndex(InitialLastSamplingResultIndex),
	m_AuthSuccess(false)
{
#if USE_AES
    const Aes128::Key key = {
        0x30, 0x0d, 0x7f, 0xb5, 0x26, 0x9f, 0x4a, 0x1e,
        0xab, 0x1b, 0xa9, 0xf3, 0xd4, 0x2e, 0xa9, 0xbf};
    m_aes.setKey(key);
#endif
}

Application::~Application()
{
}

Application* Application::getInstance()
{
    return &m_instance;
}

void Application::initialize()
{
    // 当应用程序初始化的时候，也要初始化协议和ECG设备
    m_protocol.initialize();
    m_device.initialize();
}

void Application::run()
{
    // 主循环，永远不需要退出，直到电源关闭
    for (;;) {
        // 每次循环都依次执行以下多个任务
        FEED_WATCH_DOG(); // 每次循环都必须喂狗，属于单片机常用技术
        processCommand(); // 解析并处理主机命令
        processSampling();
        processCalibrateSignalCompleted();
        m_protocol.processResend();
    }
}

void Application::processCommand()
{
    if (m_protocol.receive()) {
        // 如果m_protocol.receive()返回true，则表示协议层已经接收到一个完整的数据帧，
        // 下面的工作是获得该接收的数据，并解析为命令，然后执行相应的处理函数。
        void* data = NULL;
        size_t size = 0;
		// data和size的值会被m_protocol.getReceiveData()函数修改
        m_protocol.getReceiveData(&data, size); 
        if (data != NULL && size > 0) {
#if USE_AES
            if (m_aes.decipher(data, data, size)) { // 解密data
#endif
                // 解密成功后，执行下面处理
				// 将data解析为命令ID，因为命令ID正好位于data的前2个字节
                word cmdId = *reinterpret_cast<word*>(data); 
                
                // 对于不同的命令ID，执行相应的命令处理函数
                switch (cmdId) {
                case ciGetVersionInfo:
                    handleGetVersionInfoCmd(
						reinterpret_cast<GetVersionInfoCmd*>(data));
                    break;
                case ciGetDeviceInfo:
                    handleGetDeviceInfoCmd(
						reinterpret_cast<GetDeviceInfoCmd*>(data));
                    break;
                case ciStartSampling:
                    handleStartSamplingCmd(
						reinterpret_cast<StartSamplingCmd*>(data));
                    break;
                case ciStopSampling:
                    handleStopSamplingCmd(
						reinterpret_cast<StopSamplingCmd*>(data));
                    break;
                case ciSetSignalReset:
                    handleSetSignalResetCmd(
						reinterpret_cast<SetSignalResetCmd*>(data));
                    break;
                case ciCalibrateSignal:
                    handleCalibrateSignalCmd(
						reinterpret_cast<CalibrateSignalCmd*>(data));
                    break;
                case ciSetLeadGainType:
                    handleSetLeadGainTypeCmd(
						reinterpret_cast<SetLeadGainTypeCmd*>(data));
                    break;
                case ciSetBuiltInFilter:
                    handleSetBuiltInFilterCmd(
						reinterpret_cast<SetBuiltInFilterCmd*>(data));
                    break;
                case ciClearCalibrateSignal:
                    handleClearCalibrateSignalCmd(
						reinterpret_cast<ClearCalibrateSignalCmd*>(data));
                    break;
				case  ciAuthenticationDevice:
					handleAuthenticationDeviceCmd(
						reinterpret_cast<AuthenticationDeviceCmd*>(data));
					break;
				case ciLoadSecret:
					handleLoadSecretCmd(reinterpret_cast<LoadSecretCmd*>(data));
					break;
                case ciKillFirmware:
                    handleKillFirmwareCmd(reinterpret_cast<KillFirmwareCmd*>(data));
                    break;
                default:
                    // 对于不知名的命令ID，在这里暂不做更进一步处理
                    assert_param(false);
                    break;
                } // switch
#if USE_AES
            } // if
#endif
        }
        else {
            // 通常只有程序存在BUG，才可能到达这里
            assert_param(false);
        }
    } // if
}

void Application::processSampling()
{
    if (m_device.isSamplingPeriodCompleted()) {
        // 一个采样周期完成后，执行下面处理
        // 注意：所谓采样周期完成，是指一组导联（I、II、V1~V6）的相关数据已经采样完成，
		// 并可以完全读取
		// 一定要清除采样周期完成标记，以便能再次在正确的时机进入此处代码片断
        m_device.clearSamplingPeriodCompletedFlag(); 
        
        // 以下是装配采样结果命令
        m_samplingResultCmd.id = ciSamplingResult;
		// 采样索引Index以自增方式工作
        m_samplingResultCmd.params.index = ++m_lastSamplingResultIndex; 
        bool isValidSampling = true; // 表示完整的一组导联的采样数据是可用的
        DISABLE_INTERRUPT();
        for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
            ECGLead& lead = m_device.getLead(static_cast<LeadType>(leadType));
            if (lead.lastIsValidPoint) {
                // 对于可用的采样数据，用它们来装配采样结果命令
                SamplingResultParams::Lead& cmdLead = 
					m_samplingResultCmd.params.leads[leadType];
                cmdLead.point = lead.getUnsignedLastPoint();
                cmdLead.gainType = lead.lastGainType;
                cmdLead.alarm = lead.lastAlarm;
            }
            else {
                // 如果任何一个导联的采样数据不可用，则认为一组采样都不可用，
                // 以保证主机接收到的数据的正确性和完整性
                isValidSampling = false;
                break;
            }
        }
        ENABLE_INTERRUPT();
        if (isValidSampling) {
            // 如果一组导联的采样数据是可用的，则发送已经装配好的采样结果命令
            sendCommand(m_samplingResultCmd);
        }
    }
}

void Application::handleAuthenticationDeviceCmd(AuthenticationDeviceCmd* cmd)
{
	DISABLE_INTERRUPT();
	uchar i = 0;
	for (i = 0; i < MaxCPUIDLen; i++)
	{
		CPUID[i] = cmd->params.authid[i];
	}

	if (m_SecureAuth.AuthDevice(&CPUID[0]))
	{
		m_AuthSuccess = true;
	}
	ENABLE_INTERRUPT();
}

void Application::handleLoadSecretCmd(LoadSecretCmd* cmd)
{
	DISABLE_INTERRUPT();
	uchar i=0;
	for (i = 0; i < MaxCPUIDLen; i++)
	{
		CPUID[i] = cmd->params.cpuid[i];
	}

	m_SecureAuth.LoadSecret(&CPUID[0]);
	ENABLE_INTERRUPT();
}

void Application::processCalibrateSignalCompleted()
{
    if (m_device.isCalibratingSignal() && m_device.isCalibrateSignalCompleted()) {
        // 当设备正在校准信号，并且校准信号完成时，执行下面处理
		// 主动停止校准信号，其内部会将校准结果保存到Flash中
        m_device.stopCalibrateSignal(); 
        // 装配并发送校准信号完成命令
        m_calibrateSignalCompletedCmd.id = ciCalibrateSignalCompleted;
        sendCommand(m_calibrateSignalCompletedCmd, true);
    }
}

void Application::handleGetVersionInfoCmd(GetVersionInfoCmd* cmd)
{
    m_versioinInfoCmd.id = ciVersioinInfo;
    m_versioinInfoCmd.params.majorVersion = MAJOR_VERSION;
    m_versioinInfoCmd.params.minorVersion = MINOR_VERSION;
    m_versioinInfoCmd.params.buildNumber = BUILD_NUMBER;
    sendCommand(m_versioinInfoCmd, true);
}

void Application::handleGetDeviceInfoCmd(GetDeviceInfoCmd* cmd)
{
    memset(&m_deviceInfoCmd, 0, sizeof(m_deviceInfoCmd));
    m_deviceInfoCmd.id = ciDeviceInfo;
    m_deviceInfoCmd.params.id = DEVICE_ID;
    strcpy(m_deviceInfoCmd.params.name, DEVICE_NAME);
    strcpy(m_deviceInfoCmd.params.vendor, DEVICE_VENDOR);
    strcpy(m_deviceInfoCmd.params.description, DEVICE_DESCRIPTION);
    sendCommand(m_deviceInfoCmd, true);
}

void Application::handleStartSamplingCmd(StartSamplingCmd* cmd)
{
	if (m_AuthSuccess)
	{
		if (!m_device.isSampling()) {
			m_lastSamplingResultIndex = InitialLastSamplingResultIndex;
			m_device.startSampling();
		}
	}
}

void Application::handleStopSamplingCmd(StopSamplingCmd* cmd)
{
    m_device.stopSampling();
}

void Application::handleSetSignalResetCmd(SetSignalResetCmd* cmd)
{
    DISABLE_INTERRUPT();
    m_device.setSignalReset(cmd->params.signalReset);
    ENABLE_INTERRUPT();
}

void Application::handleCalibrateSignalCmd(CalibrateSignalCmd* cmd)
{
    DISABLE_INTERRUPT();
    if (!m_device.isCalibratingSignal()) {
        m_device.startCalibrateSignal();
    }
    ENABLE_INTERRUPT();
}

void Application::handleSetLeadGainTypeCmd(SetLeadGainTypeCmd* cmd)
{
    DISABLE_INTERRUPT();
    if (!m_device.isCalibratingSignal() &&
        isLeadType(cmd->params.leadType) &&
        isLeadGainType(cmd->params.leadGainType)) {
        m_device.getLead(static_cast<LeadType>(cmd->params.leadType)).gainType 
			= static_cast<LeadGainType>(cmd->params.leadGainType);
    }
    ENABLE_INTERRUPT();
}

void Application::handleSetBuiltInFilterCmd(SetBuiltInFilterCmd* cmd)
{
    DISABLE_INTERRUPT();
    if (isBuiltInFilterType(cmd->params.filterType)) {
        m_device.setupBuiltInFilter(
			static_cast<BuiltInFilterType>(cmd->params.filterType), 
			cmd->params.enabled);
    }
    ENABLE_INTERRUPT();
}

void Application::handleClearCalibrateSignalCmd(ClearCalibrateSignalCmd* cmd)
{
    DISABLE_INTERRUPT();
    m_device.clearCalibratingSignal();
    ENABLE_INTERRUPT();
}

void Application::handleKillFirmwareCmd(KillFirmwareCmd* cmd)
{
    DISABLE_INTERRUPT();
    HAL_FLASH_Unlock();
    FLASH_PageErase(ValidFirmwareTagAddressInFlash);
    HAL_FLASH_Lock();
    NVIC_SystemReset();
    ENABLE_INTERRUPT();
}

} // namespace


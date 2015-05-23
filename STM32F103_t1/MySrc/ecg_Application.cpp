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
    // ��Ӧ�ó����ʼ����ʱ��ҲҪ��ʼ��Э���ECG�豸
    m_protocol.initialize();
    m_device.initialize();
}

void Application::run()
{
    // ��ѭ������Զ����Ҫ�˳���ֱ����Դ�ر�
    for (;;) {
        // ÿ��ѭ��������ִ�����¶������
        FEED_WATCH_DOG(); // ÿ��ѭ��������ι�������ڵ�Ƭ�����ü���
        processCommand(); // ������������������
        processSampling();
        processCalibrateSignalCompleted();
        m_protocol.processResend();
    }
}

void Application::processCommand()
{
    if (m_protocol.receive()) {
        // ���m_protocol.receive()����true�����ʾЭ����Ѿ����յ�һ������������֡��
        // ����Ĺ����ǻ�øý��յ����ݣ�������Ϊ���Ȼ��ִ����Ӧ�Ĵ�������
        void* data = NULL;
        size_t size = 0;
		// data��size��ֵ�ᱻm_protocol.getReceiveData()�����޸�
        m_protocol.getReceiveData(&data, size); 
        if (data != NULL && size > 0) {
#if USE_AES
            if (m_aes.decipher(data, data, size)) { // ����data
#endif
                // ���ܳɹ���ִ�����洦��
				// ��data����Ϊ����ID����Ϊ����ID����λ��data��ǰ2���ֽ�
                word cmdId = *reinterpret_cast<word*>(data); 
                
                // ���ڲ�ͬ������ID��ִ����Ӧ���������
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
                    // ���ڲ�֪��������ID���������ݲ�������һ������
                    assert_param(false);
                    break;
                } // switch
#if USE_AES
            } // if
#endif
        }
        else {
            // ͨ��ֻ�г������BUG���ſ��ܵ�������
            assert_param(false);
        }
    } // if
}

void Application::processSampling()
{
    if (m_device.isSamplingPeriodCompleted()) {
        // һ������������ɺ�ִ�����洦��
        // ע�⣺��ν����������ɣ���ָһ�鵼����I��II��V1~V6������������Ѿ�������ɣ�
		// ��������ȫ��ȡ
		// һ��Ҫ�������������ɱ�ǣ��Ա����ٴ�����ȷ��ʱ������˴�����Ƭ��
        m_device.clearSamplingPeriodCompletedFlag(); 
        
        // ������װ������������
        m_samplingResultCmd.id = ciSamplingResult;
		// ��������Index��������ʽ����
        m_samplingResultCmd.params.index = ++m_lastSamplingResultIndex; 
        bool isValidSampling = true; // ��ʾ������һ�鵼���Ĳ��������ǿ��õ�
        DISABLE_INTERRUPT();
        for (byte leadType = ltI; leadType < LeadTypeCount; ++leadType) {
            ECGLead& lead = m_device.getLead(static_cast<LeadType>(leadType));
            if (lead.lastIsValidPoint) {
                // ���ڿ��õĲ������ݣ���������װ������������
                SamplingResultParams::Lead& cmdLead = 
					m_samplingResultCmd.params.leads[leadType];
                cmdLead.point = lead.getUnsignedLastPoint();
                cmdLead.gainType = lead.lastGainType;
                cmdLead.alarm = lead.lastAlarm;
            }
            else {
                // ����κ�һ�������Ĳ������ݲ����ã�����Ϊһ������������ã�
                // �Ա�֤�������յ������ݵ���ȷ�Ժ�������
                isValidSampling = false;
                break;
            }
        }
        ENABLE_INTERRUPT();
        if (isValidSampling) {
            // ���һ�鵼���Ĳ��������ǿ��õģ������Ѿ�װ��õĲ����������
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
        // ���豸����У׼�źţ�����У׼�ź����ʱ��ִ�����洦��
		// ����ֹͣУ׼�źţ����ڲ��ὫУ׼������浽Flash��
        m_device.stopCalibrateSignal(); 
        // װ�䲢����У׼�ź��������
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


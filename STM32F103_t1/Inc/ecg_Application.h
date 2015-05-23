/*
 *
 * Copyright 2008-2010 Gary Chen, All Rights Reserved
 *
 * Written by Lisen (wzlyhcn@gmail.com)
 *
 */
#ifndef ECG_APPLICATION_H
#define ECG_APPLICATION_H

// ����������䵱���Ƿ�ʹ��AES�����㷨��
// ���USE_AES��ֵΪ�������ʾʹ��AES������Ϊ����
#define USE_AES 1

#include "ecg_Core.h"
#include "ecg_NonCopyable.h"
#include "ecg_ECGDevice.h"
#include "ecg_Protocol.h"
#include "ecg_Command.h"
#include "ecg_Aes.h"
#include "ecg_SecureAuth.h"


namespace ecg {

/**
 * ��������ID
 * ��1�ֽڵĻ�������ID������2�ֽڵ���������ID
 * @param id ��������ID
 * @return ��������ID
 */
#define __MakeCommandId(id) (((word)(id) << 8) | ((word)(~(byte)(id) + 1) & 0x00FF))

////////////////////////////////////////////////////////////////////////////////
/**
 * Ӧ�ó���
 */
class Application : public NonCopyable
{
public:
    /**
     * ���ʵ��
     * @return ��ֵ
     */
    static Application* getInstance();

    /**
     * ��ʼ��
     */
    void initialize();

    /**
     * ����
     */
    void run();
protected:
    /**
     * ������
     */
    Application();

    /**
     * ������
     */
    ~Application();
private:
    enum {
        MaxDeviceNameLen = 31, // �豸������󳤶ȣ���λ���ֽ�
        MaxDeviceDescLen = 127, // �豸������󳤶ȣ���λ���ֽ�
        MaxDeviceVendorLen = 31, // �豸������󳤶ȣ���λ���ֽ�

        InitialLastSamplingResultIndex = 0xFF, // ��ʼ���������������ֵ

		MaxCPUIDLen = 8,	// CPU ID ��󳤶ȣ���λ���ֽ�

        ValidFirmwareTagAddressInFlash = 0x0800FC00, // ���һҳ�׵�ַ��Flash Size = 64K
    };

    enum CommandId {
        // �������������������ID
        ciGetVersionInfo = __MakeCommandId(0xA1), // ��ð汾��Ϣ����ID
        ciGetDeviceInfo = __MakeCommandId(0xA2), // ����豸��Ϣ����ID
        ciStartSampling = __MakeCommandId(0xA3), // ��ʼ��������ID
        ciStopSampling = __MakeCommandId(0xA4), // ֹͣ��������ID
        ciSetSignalReset = __MakeCommandId(0xA5), // �����źŸ�λ״̬����ID
        ciCalibrateSignal = __MakeCommandId(0xA6), // У׼�ź�����ID
        ciSetLeadGainType = __MakeCommandId(0xA7), // ���õ���������������ID
        ciSetBuiltInFilter = __MakeCommandId(0xA8), // ���������˲�������ID���ѷ�����
        ciClearCalibrateSignal = __MakeCommandId(0xA9), // ���У׼�ź�����ID
		ciAuthenticationDevice = __MakeCommandId(0xAA), // �����֤
		ciLoadSecret = __MakeCommandId(0xAB), // ��������
        ciKillFirmware = __MakeCommandId(0xC1), // �ݻٹ̼�����ID���ѷ�����

        // ���巢�͵�����������ID
        ciVersioinInfo = __MakeCommandId(0xB1), // �汾��Ϣ����ID
        ciDeviceInfo = __MakeCommandId(0xB2), // �豸��Ϣ����ID
        ciSamplingResult = __MakeCommandId(0xB3), // �����������ID
        ciCalibrateSignalCompleted = __MakeCommandId(0xB4), // У׼�ź��������ID
    };

// �����ڴ�Ϊ�Ƕ���
#pragma pack(push, 1)
    // �����źŸ�λ�Ĳ���
    struct SetSignalResetParams {
        byte signalReset;
    };

    // ���õ����������͵Ĳ���
    struct SetLeadGainTypeParams {
        byte leadType;
        byte leadGainType;
    };

    // ���������˲����Ĳ���
    struct SetBuiltInFilterParams {
        byte filterType;
        byte enabled;
    };

    // �汾��Ϣ�Ĳ���
    struct VersioinInfoParams {
        byte majorVersion;
        byte minorVersion;
        byte buildNumber;
    };

    // �豸��Ϣ�Ĳ���
    struct DeviceInfoParams {
        byte id;
        char name[MaxDeviceNameLen + 1];
        char description[MaxDeviceDescLen + 1];
        char vendor[MaxDeviceVendorLen + 1];
    };

    // ��������Ĳ���
    struct SamplingResultParams {
        byte index;
        struct Lead {
            struct {
                byte alarm: 1;
                byte gainType: 4;
            };
            ushort point;
        } leads[LeadTypeCount];
    };

	// CPU ID
	struct CPUIDParams{
		byte cpuid[MaxCPUIDLen];
	};

	struct AuthIDParams{
		byte authid[MaxCPUIDLen];
	};

    // ���������������������
    typedef CommandNP<word, Aes128::Block> GetVersionInfoCmd; // ��ð汾��Ϣ����
    typedef CommandNP<word, Aes128::Block> GetDeviceInfoCmd; // ����豸��Ϣ����
    typedef CommandNP<word, Aes128::Block> StartSamplingCmd; // ��ʼ��������
    typedef CommandNP<word, Aes128::Block> StopSamplingCmd; // ֹͣ��������
    typedef Command<word, SetSignalResetParams, Aes128::Block> SetSignalResetCmd; // �����źŸ�λ״̬����
    typedef CommandNP<word, Aes128::Block> CalibrateSignalCmd; // У׼�ź�����
    typedef Command<word, SetLeadGainTypeParams, Aes128::Block> SetLeadGainTypeCmd; // ���õ���������������
    typedef Command<word, SetBuiltInFilterParams, Aes128::Block> SetBuiltInFilterCmd; // ���������˲�������ѷ�����
    typedef CommandNP<word, Aes128::Block> ClearCalibrateSignalCmd; // ���У׼�ź�����
	typedef Command<word, AuthIDParams, Aes128::Block> AuthenticationDeviceCmd;	// �����֤����
	typedef Command<word, CPUIDParams, Aes128::Block> LoadSecretCmd;	// װ����������
    typedef CommandNP<word, Aes128::Block> KillFirmwareCmd; // �ݻٹ̼�����ѷ�����

    // ���巢�͵�����������
    typedef Command<word, VersioinInfoParams, Aes128::Block> VersioinInfoCmd; // �汾��Ϣ����
    typedef Command<word, DeviceInfoParams, Aes128::Block> DeviceInfoCmd; // �豸��Ϣ����
    typedef Command<word, SamplingResultParams, Aes128::Block[2]> SamplingResultCmd; // �����������
    typedef CommandNP<word, Aes128::Block> CalibrateSignalCompletedCmd; // У׼�ź��������
#pragma pack(pop)
private:
    /**
     * ϵͳ��ʱ���жϴ������
     */
    static void handleSysTick();

    /**
     * ��������
     * ��run()�б�����
     */
    void processCommand();

    /**
     * �������
     * ��run()�б�����
     */
    void processSampling();

    /**
     * ����У׼�ź����
     * ��run()�б�����
     */
    void processCalibrateSignalCompleted();

    /**
     * ��ð汾��Ϣ����Ĵ�����
     */
    void handleGetVersionInfoCmd(GetVersionInfoCmd* cmd);

    /**
     * ����豸��Ϣ����Ĵ�����
     */
    void handleGetDeviceInfoCmd(GetDeviceInfoCmd* cmd);

    /**
     * ��ʼ��������Ĵ�����
     */
    void handleStartSamplingCmd(StartSamplingCmd* cmd);

    /**
     * ֹͣ��������Ĵ�����
     */
    void handleStopSamplingCmd(StopSamplingCmd* cmd);

    /**
     * �����źŸ�λ״̬����Ĵ�����
     */
    void handleSetSignalResetCmd(SetSignalResetCmd* cmd);

    /**
     * У׼�ź�����Ĵ�����
     */
    void handleCalibrateSignalCmd(CalibrateSignalCmd* cmd);

    /**
     * ���õ���������������Ĵ�����
     */
    void handleSetLeadGainTypeCmd(SetLeadGainTypeCmd* cmd);

    /**
     * ���������˲�������Ĵ��������ѷ�����
     */
    void handleSetBuiltInFilterCmd(SetBuiltInFilterCmd* cmd);

    /**
     * ���У׼�ź�����Ĵ�����
     */
    void handleClearCalibrateSignalCmd(ClearCalibrateSignalCmd* cmd);

    /**
     * �����֤�������
     */
    void handleAuthenticationDeviceCmd(AuthenticationDeviceCmd* cmd);


    /**
     * װ�������������
     */
    void handleLoadSecretCmd(LoadSecretCmd* cmd);


    /**
     * �ݻٹ̼�����Ĵ��������ѷ�����
     */
    void handleKillFirmwareCmd(KillFirmwareCmd* cmd);
private:
    /**
     * ��������
     * ���USE_AES��Ϊ0�����ڷ���֮ǰ���������м���
     */
    template <typename Command> void sendCommand(Command& command, bool reliable = false)
    {
#if USE_AES
        if (m_aes.encipher(&command, &command, sizeof(command))) {
#endif
            m_protocol.send(&command, sizeof(command));
#if USE_AES
        }
#endif
    }
private:
    static Application m_instance;
	SecureAuth m_SecureAuth;
    Protocol m_protocol;
    ECGDevice m_device;
    VersioinInfoCmd m_versioinInfoCmd;
    DeviceInfoCmd m_deviceInfoCmd;
    SamplingResultCmd m_samplingResultCmd;
    CalibrateSignalCompletedCmd m_calibrateSignalCompletedCmd;
    volatile byte m_lastSamplingResultIndex;
	uchar CPUID[MaxCPUIDLen];
	volatile bool m_AuthSuccess;
#if USE_AES
    Aes128 m_aes;
#endif
};

} // namespace

#endif // ECG_APPLICATION_H
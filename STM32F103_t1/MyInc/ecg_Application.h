/*
 *
 * Copyright 2008-2010 Gary Chen, All Rights Reserved
 *
 * Written by Lisen (wzlyhcn@gmail.com)
 *
 */
#ifndef ECG_APPLICATION_H
#define ECG_APPLICATION_H

// 定义在命令传输当中是否使用AES加密算法，
// 如果USE_AES的值为非零则表示使用AES，否则为禁用
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
 * 制作命令ID
 * 把1字节的基本命令ID制作成2字节的完整命令ID
 * @param id 基本命令ID
 * @return 完整命令ID
 */
#define __MakeCommandId(id) (((word)(id) << 8) | ((word)(~(byte)(id) + 1) & 0x00FF))

////////////////////////////////////////////////////////////////////////////////
/**
 * 应用程序
 */
class Application : public NonCopyable
{
public:
    /**
     * 获得实例
     * @return 该值
     */
    static Application* getInstance();

    /**
     * 初始化
     */
    void initialize();

    /**
     * 运行
     */
    void run();
protected:
    /**
     * 构造器
     */
    Application();

    /**
     * 析构器
     */
    ~Application();
private:
    enum {
        MaxDeviceNameLen = 31, // 设备名称最大长度，单位是字节
        MaxDeviceDescLen = 127, // 设备描述最大长度，单位是字节
        MaxDeviceVendorLen = 31, // 设备厂商最大长度，单位是字节

        InitialLastSamplingResultIndex = 0xFF, // 初始的最后采样结果索引值

		MaxCPUIDLen = 8,	// CPU ID 最大长度，单位是字节

        ValidFirmwareTagAddressInFlash = 0x0800FC00, // 最后一页首地址，Flash Size = 64K
    };

    enum CommandId {
        // 定义接收来自主机命令ID
        ciGetVersionInfo = __MakeCommandId(0xA1), // 获得版本信息命令ID
        ciGetDeviceInfo = __MakeCommandId(0xA2), // 获得设备信息命令ID
        ciStartSampling = __MakeCommandId(0xA3), // 开始采样命令ID
        ciStopSampling = __MakeCommandId(0xA4), // 停止采样命令ID
        ciSetSignalReset = __MakeCommandId(0xA5), // 设置信号复位状态命令ID
        ciCalibrateSignal = __MakeCommandId(0xA6), // 校准信号命令ID
        ciSetLeadGainType = __MakeCommandId(0xA7), // 设置导联增益类型命令ID
        ciSetBuiltInFilter = __MakeCommandId(0xA8), // 设置内置滤波器命令ID（已废弃）
        ciClearCalibrateSignal = __MakeCommandId(0xA9), // 清除校准信号命令ID
		ciAuthenticationDevice = __MakeCommandId(0xAA), // 身份认证
		ciLoadSecret = __MakeCommandId(0xAB), // 下载密码
        ciKillFirmware = __MakeCommandId(0xC1), // 摧毁固件命令ID（已废弃）

        // 定义发送到主机的命令ID
        ciVersioinInfo = __MakeCommandId(0xB1), // 版本信息命令ID
        ciDeviceInfo = __MakeCommandId(0xB2), // 设备信息命令ID
        ciSamplingResult = __MakeCommandId(0xB3), // 采样结果命令ID
        ciCalibrateSignalCompleted = __MakeCommandId(0xB4), // 校准信号完成命令ID
    };

// 设置内存为非对齐
#pragma pack(push, 1)
    // 设置信号复位的参数
    struct SetSignalResetParams {
        byte signalReset;
    };

    // 设置导联增益类型的参数
    struct SetLeadGainTypeParams {
        byte leadType;
        byte leadGainType;
    };

    // 设置内置滤波器的参数
    struct SetBuiltInFilterParams {
        byte filterType;
        byte enabled;
    };

    // 版本信息的参数
    struct VersioinInfoParams {
        byte majorVersion;
        byte minorVersion;
        byte buildNumber;
    };

    // 设备信息的参数
    struct DeviceInfoParams {
        byte id;
        char name[MaxDeviceNameLen + 1];
        char description[MaxDeviceDescLen + 1];
        char vendor[MaxDeviceVendorLen + 1];
    };

    // 采样结果的参数
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

    // 定义接收来自主机的命令
    typedef CommandNP<word, Aes128::Block> GetVersionInfoCmd; // 获得版本信息命令
    typedef CommandNP<word, Aes128::Block> GetDeviceInfoCmd; // 获得设备信息命令
    typedef CommandNP<word, Aes128::Block> StartSamplingCmd; // 开始采样命令
    typedef CommandNP<word, Aes128::Block> StopSamplingCmd; // 停止采样命令
    typedef Command<word, SetSignalResetParams, Aes128::Block> SetSignalResetCmd; // 设置信号复位状态命令
    typedef CommandNP<word, Aes128::Block> CalibrateSignalCmd; // 校准信号命令
    typedef Command<word, SetLeadGainTypeParams, Aes128::Block> SetLeadGainTypeCmd; // 设置导联增益类型命令
    typedef Command<word, SetBuiltInFilterParams, Aes128::Block> SetBuiltInFilterCmd; // 设置内置滤波器命令（已废弃）
    typedef CommandNP<word, Aes128::Block> ClearCalibrateSignalCmd; // 清除校准信号命令
	typedef Command<word, AuthIDParams, Aes128::Block> AuthenticationDeviceCmd;	// 身份认证命令
	typedef Command<word, CPUIDParams, Aes128::Block> LoadSecretCmd;	// 装载密码命令
    typedef CommandNP<word, Aes128::Block> KillFirmwareCmd; // 摧毁固件命令（已废弃）

    // 定义发送到主机的命令
    typedef Command<word, VersioinInfoParams, Aes128::Block> VersioinInfoCmd; // 版本信息命令
    typedef Command<word, DeviceInfoParams, Aes128::Block> DeviceInfoCmd; // 设备信息命令
    typedef Command<word, SamplingResultParams, Aes128::Block[2]> SamplingResultCmd; // 采样结果命令
    typedef CommandNP<word, Aes128::Block> CalibrateSignalCompletedCmd; // 校准信号完成命令
#pragma pack(pop)
private:
    /**
     * 系统定时器中断处理程序
     */
    static void handleSysTick();

    /**
     * 处理命令
     * 在run()中被调用
     */
    void processCommand();

    /**
     * 处理采样
     * 在run()中被调用
     */
    void processSampling();

    /**
     * 处理校准信号完成
     * 在run()中被调用
     */
    void processCalibrateSignalCompleted();

    /**
     * 获得版本信息命令的处理函数
     */
    void handleGetVersionInfoCmd(GetVersionInfoCmd* cmd);

    /**
     * 获得设备信息命令的处理函数
     */
    void handleGetDeviceInfoCmd(GetDeviceInfoCmd* cmd);

    /**
     * 开始采样命令的处理函数
     */
    void handleStartSamplingCmd(StartSamplingCmd* cmd);

    /**
     * 停止采样命令的处理函数
     */
    void handleStopSamplingCmd(StopSamplingCmd* cmd);

    /**
     * 设置信号复位状态命令的处理函数
     */
    void handleSetSignalResetCmd(SetSignalResetCmd* cmd);

    /**
     * 校准信号命令的处理函数
     */
    void handleCalibrateSignalCmd(CalibrateSignalCmd* cmd);

    /**
     * 设置导联增益类型命令的处理函数
     */
    void handleSetLeadGainTypeCmd(SetLeadGainTypeCmd* cmd);

    /**
     * 设置内置滤波器命令的处理函数（已废弃）
     */
    void handleSetBuiltInFilterCmd(SetBuiltInFilterCmd* cmd);

    /**
     * 清除校准信号命令的处理函数
     */
    void handleClearCalibrateSignalCmd(ClearCalibrateSignalCmd* cmd);

    /**
     * 身份认证命令处理函数
     */
    void handleAuthenticationDeviceCmd(AuthenticationDeviceCmd* cmd);


    /**
     * 装载密码命令处理函数
     */
    void handleLoadSecretCmd(LoadSecretCmd* cmd);


    /**
     * 摧毁固件命令的处理函数（已废弃）
     */
    void handleKillFirmwareCmd(KillFirmwareCmd* cmd);
private:
    /**
     * 发送命令
     * 如果USE_AES不为0，则在发送之前会对命令进行加密
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
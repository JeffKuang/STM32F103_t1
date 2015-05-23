#ifndef ECG_SECUREAUTH_H
#define ECG_SECUREAUTH_H
#include "ecg_SysUtils.h"
#include "ecg_SoC.h"


namespace ecg{

class SecureAuth{
public:
	SecureAuth();
	~SecureAuth();
	static SecureAuth* getInstance();

	uchar AuthDevice(uchar* cpuid);
	uchar LoadSecret(uchar* cpuid);

private:
	// DS28E15的命令
	enum{
		WriteMemoryCommand							= 0x55,
		ReadMemoryCommand							= 0xF0,
		WriteBlockProtectionCommand					= 0xC3,
		AuthenticatedWriteBlockProtectionCommand	= 0xCC,
		ReadProtectionCommand						= 0x80,
		WriteProtectionCommand						= 0x40,
		EPromProtectionCommand						= 0x20,
		AuthenticationCommand						= 0x10,
		ReadStausCommand							= 0xAA,
		ReadWriteScratchPadCommand					= 0x0F,
		LoadAndLockSecretCommand					= 0x33,
		ComputeAndLockSecretCommand					= 0x3C,
		ComputeAndReadPageMACCommand				= 0xA5,
		AuthenticatedWriteMemoryCommand				= 0x5A,

	};

	// DS28E15 EEPROM address 
	enum{
		Segment0	= 0x00,
		Segment1	= 0x01,
		Segment2	= 0x02,
		Segment3	= 0x03,
		Segment4	= 0x04,
		Segment5	= 0x05,
		Segment6	= 0x06,
		Segment7	= 0x07,
		
		Page0		= 0x00,
		Page1		= 0x01,

		Block0		= 0x00,
		Block1		= 0x01,
		Block2		= 0x10,
		Block3		= 0x11,

	};

	// SHA
	enum{
		SHA_256_INITIAL_LENGTH = 8,
		OUTPUTDATA = 1,
		INPUTDATA = 0,
		US = 7,
		MS = 122,
		REVERSE_ENDIAN = 1,
	};

	// 
	enum{
		SaveCPUIDAddressInFlash = 0x0800F400,		// 倒数第3页Flash首地址
		MaxWriteByteLen	= 8,						// 写Flash的最大字节数
	};

	ulong a32, b32, c32, d32, e32, f32, g32, h32; // SHA working variables
	ulong W32[16];                                // SHA message schedule
	ulong H32[8];                                 // last SHA result variables

	static SecureAuth m_instance;
	GPIO_TypeDef* mGPIOx;
	uint16_t mPin;
	uchar EEPROM[32];
	uchar EEPROM1[32];
	uchar Block_Status[4];

	bool Verify_outcome;

	uchar pbuf[20];
	uchar workbuffer[128];
	uchar MAC_Computer_Datainput[128];
	uchar Personality_Value[4];
	uchar RomID_Buffer[8];
	uchar MAC_Read_Value[32];
	uchar MAC_Write_Value[32];
	uchar Scratch_Readbuffer[32];
	uchar CRC8;
	ushort CRC16;

	uchar dowcrc(uchar x);
	ushort dowcrc16(ushort data);
	void delay_us(uint16_t cnt);
	void Delay_Ms(uint16_t cnt);
	void OneWireIOInditection(bool temp);
	uchar ow_reset(void);
	uchar read_bit(void);
	void write_bit(char bitval);
	uchar read_byte(void);
	void write_byte(char val);

	uchar ReadWriteScratchpad(bool ReadMode, uchar* Buffer);
	uchar LoadAndLockSecret(bool SecretLock_Enable);
	uchar ComputeAndReadPageMAC(bool Anonymous_Mode, uchar PageNum, uchar* MAC_Value);
	uchar ReadMemory(uchar Segment, uchar Page, uchar* Receive);
	uchar SettingAndLockSecret(uchar* secret, bool LockEnable);
	uchar ReadRomID(uchar* RomID);
	uchar ReadBlockStatus(uchar BlockNum, bool PersonalityByteIndicator, uchar* BlockStatus);
	uchar WriteBlockProtection(uchar Block, uchar ProtectOption);
	uchar ComputeAndLockSecret(uchar PageNum, bool LockEnable);
	uchar WriteMemoryPage(uchar PageNum, uchar* WriteData);
	void randnum(ushort i, uchar* output);
	uchar randnum1();

	uchar WriteMemory(uchar Segment, uchar Page, uchar* Buffer);	// write one segment(4 bytes)
	void SetEEPROM(uchar* input, uchar* output);
	void CalculateEEPROM(uchar* input, uchar* output);
	uchar VerifyEEPROM(uchar* readEEPROM, uchar* inputID);

	void CalculateNextSecret256(uchar* binding, uchar* partial, int page_num, uchar* manid);
	void set_secret(uchar* secret);
	bool VerifyMAC256(uchar * MT, short length, uchar* compare_MAC, bool SecretMode);
	void ComputeMAC256(uchar *MT, short length, uchar* MAC, bool SecretMode);
	void ComputeSHA256(uchar* message, short length, ushort skipconst, ushort reverse, uchar* digest);

	void sha_copy32(ulong* p1, ulong* p2, ushort length);
	void sha256_hashblock(uchar* message, ushort lastblock);
	void sha_prepareSchedule(uchar* message);
	ulong sha_getW(int index);
	ulong sha_bigsigma256_0(ulong x);
	ulong sha_bigsigma256_1(ulong x);
	ulong sha_littlesigma256_0(ulong x);
	ulong sha_littlesigma256_1(ulong x);
	void sha_writeResult(ushort reverse, uchar* outpointer);
	void sha_copyWordsToBytes32(ulong* input, uchar* output, ushort numwords);
	ulong sha_shr_32(ulong val, ushort r);
	ulong sha_rotr_32(ulong val, ushort r);
	ulong sha_maj(ulong x, ulong y, ulong z);
	ulong sha_ch(ulong x, ulong y, ulong z);

};

}
#endif	// ECG_SECUREAUTH_H
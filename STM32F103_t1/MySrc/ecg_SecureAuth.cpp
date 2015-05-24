/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#include "ecg_SecureAuth.h"
#include <stdio.h>
#include <stdlib.h>
#include <intrinsics.h>
#include <string.h>
#include <time.h>


namespace ecg{

#define One_Wire	((GPIO_TypeDef *) GPIOA_BASE)
#define One_Wire_IO	GPIO_PIN_12

static unsigned long SHA_256_Initial[] = {
	0x6a09e667,
	0xbb67ae85,
	0x3c6ef372,
	0xa54ff53a,
	0x510e527f,
	0x9b05688c,
	0x1f83d9ab,
	0x5be0cd19
};

static unsigned long SHA_CONSTANTS[] =  {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
	0xca273ece, 0xd186b8c7, 0xeada7dd6, 0xf57d4f7f, 
    0x06f067aa, 0x0a637dc5, 0x113f9804, 0x1b710b35,
	0x28db77f5, 0x32caab7b, 0x3c9ebe0a, 0x431d67c4, 
    0x4cc5d4be, 0x597f299c, 0x5fcb6fab, 0x6c44198c
};

static uchar dscrc_table[] = {
	0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
	157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
	35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
	190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
	70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
	219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
	101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
	248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
	140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
	17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
	175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
	50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
	202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
	87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
	233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
	116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
};

/* 固定的ScratchPad */
uchar Scratch_Fixed [32] = {
	0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
	0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
	0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
	0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11  //Scratch Data(Random Num);
};

/* 参于计算的ScratchPad */
uchar Scratch_Writebuffer[32] = {
	0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
	0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
	0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,
	0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11  //Scratch Data(Random Num);
};

/* 32字节的公用密钥 */
uchar Secret_256bit[32]={
	0x24,0x32,0x23,0x32,0x23,0x32,0x23,0x32,
	0x23,0x32,0x23,0x32,0x23,0x32,0x23,0x32,
	0x23,0x32,0x23,0x32,0x23,0x32,0x23,0x32,
	0x23,0x32,0x23,0x32,0x23,0x32,0x23,0x32  
};

static short oddparity[16] = { 
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 
};
static uchar SECRET[32];


SecureAuth SecureAuth::m_instance;
////////////////////////////////////////////////////////////////////////////////
SecureAuth::SecureAuth(){
	GPIO_InitTypeDef initTypeDef;
	initTypeDef.Pin = GPIO_PIN_12;
	initTypeDef.Speed = GPIO_SPEED_HIGH;
	initTypeDef.Mode = GPIO_MODE_OUTPUT_OD;
	HAL_GPIO_Init(GPIOA, &initTypeDef);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
	mGPIOx = GPIOA;
	mPin = GPIO_PIN_12;
	m_instance = *this;
}

SecureAuth::~SecureAuth()
{
}

void SecureAuth::randnum(ushort i, uchar* output)
{
	int j;
	srand((unsigned)time(NULL));
	for (j = 0; j < i; j++)
	{
		*output = (uchar)(rand() % 256);
		output++;
	}
}

uchar SecureAuth::randnum1()
{
	srand((unsigned)time(NULL));
	return (uchar)(rand() % 256);
}

SecureAuth* SecureAuth::getInstance()
{
	return &m_instance;
}

void SecureAuth::delay_us(uint16_t cnt)
{
	volatile uint i = 0;
	for (i = 0; i < cnt*US; i++)
	{
	}
}

void SecureAuth::Delay_Ms(uint16_t cnt)
{
	volatile uint i;
	for (i = 0; i < (cnt * MS / 10); i++)
	{
	}
}

void SecureAuth::OneWireIOInditection(bool temp)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	if (temp)
	{
		GPIO_InitStructure.Pin = GPIO_PIN_12;
		GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
		HAL_GPIO_Init((GPIO_TypeDef*)GPIOA_BASE, &GPIO_InitStructure);
	}
	else
	{
		GPIO_InitStructure.Pin = GPIO_PIN_12;
		GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
		HAL_GPIO_Init((GPIO_TypeDef*)GPIOA_BASE, &GPIO_InitStructure);
	}
}

uchar SecureAuth::ow_reset(void)
{
	uchar presence;
	OneWireIOInditection(OUTPUTDATA);
	HAL_GPIO_WritePin(One_Wire, One_Wire_IO, GPIO_PIN_RESET);
	// leave it low for 480s
	Delay_Ms(60);
	// allow line to return high
	HAL_GPIO_WritePin(One_Wire, One_Wire_IO, GPIO_PIN_SET);
	// wait for presence
	Delay_Ms(8);

	presence = ((One_Wire->IDR) & One_Wire_IO)? 1:0;
	Delay_Ms(10);
	return(presence);
}

uchar SecureAuth::read_bit(void)
{
	uchar vamm;
	HAL_GPIO_WritePin(One_Wire, One_Wire_IO, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(One_Wire, One_Wire_IO, GPIO_PIN_SET);

	vamm = ((One_Wire->IDR) & One_Wire_IO)? 1:0;
	Delay_Ms(5);
	return (vamm);
}

void SecureAuth::write_bit(char bitval)
{
	HAL_GPIO_WritePin(One_Wire, One_Wire_IO, GPIO_PIN_RESET);
	Delay_Ms(1);
	if (bitval != 0)
	{
		HAL_GPIO_WritePin(One_Wire, One_Wire_IO, GPIO_PIN_SET);
	}
	Delay_Ms(12);
	HAL_GPIO_WritePin(One_Wire, One_Wire_IO, GPIO_PIN_SET);
}

uchar SecureAuth::read_byte(void)
{
	uchar i;
	uchar value = 0;
	for (i = 0; i < 8; i++)
	{
		if (read_bit())
		{
			value |= 0x01 << i;
		}
	}
	return (value);
}

void SecureAuth::write_byte(char val)
{
	uchar i, temp;
	for (i = 0; i < 8; i++)
	{
		temp = val >> i;
		temp &= 0x01;
		write_bit(temp);
	}
}

uchar SecureAuth::dowcrc(uchar x)
{
	CRC8 = dscrc_table[CRC8 ^ x];
	return CRC8;
}

ushort SecureAuth::dowcrc16(ushort data)
{
	data = (data ^ (CRC16 & 0xff)) & 0xff;
	CRC16 >>= 8;

	if (oddparity[data & 0xf] ^ oddparity[data >> 4])
	{
		CRC16 ^= 0xC001;
	}
	data <<= 6;
	CRC16 ^= data;
	data <<= 1;
	CRC16 ^= data;

	return CRC16;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------
/* To read or write 32 Bytes Scratchpad buffer.
To ensure the integrity of the scratchpad data the master can rely on the CRC that
the DS28E15 generates and transmits after the scratchpad is filled to capacity
// Input parameter: 
   1. bool ReadMode : specify to read or write scratchpad buffer
   2. unsigned char *Buffer : Data buffer ; If read scratchpad ,Buffer is receiving data. 
      If write scratchpad ,Buffer is data ready for writing.
// Returns: AAh = success
            FFh = CRC Error
           
*/
uchar SecureAuth::ReadWriteScratchpad(bool ReadMode, uchar* Buffer)
{
	uchar cnt = 0, i;
//	memset(pbuf, 0x0, 20);
	if (ow_reset() != 0)
	{
		return 0;
	}
	write_byte(0xCC);

	pbuf[cnt++] = ReadWriteScratchPadCommand;
	pbuf[cnt++] = ReadMode? 0x0F:0x00;

	for (i = 0; i < cnt; i++)
	{
		write_byte(pbuf[i]);
	}

	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}

	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}

	if (ReadMode)
	{
		for (i = 0, cnt = 0; i < 32; cnt++, i++)
		{
			Buffer[cnt] = read_byte();
			pbuf[i] = Buffer[cnt];
		}
	}
	else{
		for (i = 0, cnt = 0; i < 32; cnt++, i++)
		{
			write_byte(Buffer[i]);
			pbuf[cnt] = Buffer[i];
		}
	}

	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	ow_reset();

	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}
	return 0xAA;
}

//------------------------------------------------------------------------------
/*
The Compute and Read Page MAC command is used to 
authenticate the DS28E15 to the master. 
The master computes the MAC from the same data 
and the expected secret. 
If both MACs are identical, the device is confirmed 
authentic within the application.
// Input parameter: 
   1.bool Anonymous_Mode: specify whether the device’s 
            ROM ID is used for the MAC computation.
            If anonymous mode , replacing the ROM ID 
            with FFh Bytes when MAC computation
   2.unsigned char PageNum:specifies the memory page 
            that is to be used for the MAC computation.
            Valid memory page numbers are 0 (page 0) 
            and 1 (page 1).
   3.unsigned char *MAC_Value: receiving data buffer 
            for 32 Bytes Computation MAC values
                     
// Returns: AAh = success
            FFh = CRC Error
            
*/
uchar SecureAuth::ComputeAndReadPageMAC(bool Anonymous_Mode, 
                                        uchar PageNum, uchar* MAC_Value)
{
	uchar cnt = 0, i, outcome;
//	memset(pbuf, 0x0, 20);
	if (ow_reset() != 0)
	{
		return 0;
	}

	write_byte(0xCC);

	pbuf[cnt++] = ComputeAndReadPageMACCommand;
	pbuf[cnt++] = Anonymous_Mode?(PageNum | 0xE0):PageNum;

	for (i = 0; i < cnt; i++)
	{
		write_byte(pbuf[i]);
	}
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}

	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}

	Delay_Ms(6000);
	outcome = read_byte();

	for (i = 0,cnt = 0; i < 32; i++,cnt++)
	{
		MAC_Value[i] = read_byte();
		pbuf[cnt] = MAC_Value[i];
	}

	/* Receive CRC */
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	/* calculate the CRC over this part */
	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}
	return outcome;
}

//-----------------------------------------------------------------
/*Reading  DS28E15 EEPROM Data From Specified starting Segment  to 
last Segment in Specified Page. The Read Memory command is used to 
read the user memory. The page number and segment number specify 
where the reading begins.After the last byte of the page is read, 
the DS28E15 transmits a CRC of the page data for the master to 
verify the data integrity.
// Input parameter: 
1. unsigned char Segment : specify the location within the selected 
   memory page at which the reading begins
2. unsigned char Page : specifies the memory page at which the 
   reading begins. Valid memory page numbers are 0 (page 0) 
   and 1 (page 1).
3. unsigned char *Receive : Receiving Data buffer                      
// Returns:  AAh = success
             FFh = CRC Error
*/
uchar SecureAuth::ReadMemory(uchar Segment, uchar Page, uchar* Receive)
{
	uchar cnt = 0, i;
//	memset(pbuf, 0x0, 20);
	if (ow_reset() != 0)
	{
		return 0;
	}
	write_byte(0xCC);	// skip ROM Command

	/* construct a packet to send */
	pbuf[cnt++] = ReadMemoryCommand;
	pbuf[cnt++] = ((Segment << 5) | Page) & 0xE1;

	/* Send Data */
	for (i = 0; i < cnt; i++)
	{
		write_byte(pbuf[i]);
	}

	/* Receive CRC */
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	/* calculate the CRC over this part */
	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}

	/* return result of inverted CRC */
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}

	/* Receive DS28E15 EEPROM Data */
	for (cnt = 0, i = Segment*4; i < 32; i++,cnt++)
	{
		Receive[cnt] = read_byte();
		pbuf[cnt] = Receive[cnt];
	}

	/* Receive CRC */
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	/* calculate the CRC over this part */
	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}

	ow_reset();
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}
	return 0xAA;
}

//-----------------------------------------------------------------
/*
The Load and Lock Secret command is used to install a predefined 
secret when the DS28E15 is first set up for an application. 
This command takes the data in the scratchpad and copies it 
to the secret memory.
// Input parameter: 
   1. bool SecretLock_Enable : specify whether Lock secret 
      after Loading the secret to Secret memory
// Returns: AAh = success
            FFh = CRC Error
            55h = The command failed because the secret was 
                  already locked.
*/
uchar SecureAuth::LoadAndLockSecret(bool SecretLock_Enable)
{
	uchar cnt = 0, i, outcome;
//	memset(pbuf, 0x0, 20);
	if (ow_reset() != 0)
	{
		return 0;
	}
	write_byte(0xCC);

	/* construct a packet to send */
	pbuf[cnt++] = LoadAndLockSecretCommand;
	pbuf[cnt++] = SecretLock_Enable ? 0xE0 : 0x00;

	/* Send Data */
	for (i = 0; i < cnt; i++)
	{
		write_byte(pbuf[i]);
	}

	/* Receive CRC */
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	/* calculate the CRC over this part */
	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}

	/* Send Release Byte */
	write_byte(0xAA);
	for (i = 0; i < 10; i++)
	{
		Delay_Ms(10000);
	}

	outcome = read_byte();
	ow_reset();
	return outcome;
}

//--------------------------------------------------------
/*
The Seting and Lock Secret command is used to install 
specified secret in application .
// Input parameter: 
   1.unsigned char *secret : Secret buffer ready for loading 
   2.bool LockEnable : specify whether locking secret after 
     loading completion
// Returns: AAh = success
            FFh = CRC Error
            55h = The command failed because the secret was 
                  already locked.

*/
uchar SecureAuth::SettingAndLockSecret(uchar* secret, bool LockEnable)
{
	uchar ret;
// 	BoolValue = LockEnable;
	if (ReadWriteScratchpad(false, secret) != 0xAA)
	{
		return 0xFF;
	}
	ret = LoadAndLockSecret(LockEnable);
	return ret;
}

//------------------------------------------------------------------
/*  Read BlockStatus 
The Read Block Status command is used to read the memory block 
protection settings and to read the device’s personality bytes. 
The parameter byte determines whether the command reads the block 
protection or the device personality.
// Input parameter: 
   1.unsigned char BlockNum : specify starting Block number for 
     protection status reading . DS28E15 include 4 Block, every 
     block include 16Bytes.
   2.bool PersonalityByteIndicator : specify reading memory block 
     protection settings or  reading the device’s personality   
   3.unsigned char *BlockStatus : storing data buffer for Block 
     protection setting or device's personality.
// Returns: AAh = success
            FFh = CRC Error
           
*/
uchar SecureAuth::ReadBlockStatus(uchar BlockNum, 
                                  bool PersonalityByteIndicator, 
                                  uchar* BlockStatus)
{
	uchar cnt = 0, i;
//	memset(pbuf, 0x0, 20);
	if (ow_reset() != 0)
	{
		return 0;
	}
	write_byte(0xCC);		// skip ROM command

	/* construct a packet to send */
	pbuf[cnt++] = ReadStausCommand;
	pbuf[cnt++] = PersonalityByteIndicator ? 
        0xE0 : (BlockNum & 0x03);

	/* Send Data */
	for (i = 0; i < cnt; i++)
	{
		write_byte(pbuf[i]);
	}

	/* Receive CRC */
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	/* calculate the CRC over this part */
	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}

	if (PersonalityByteIndicator)
	{
		for (cnt = 0,i = 0; i < 4; i++,cnt++)
		{
			BlockStatus[cnt] = read_byte();
			pbuf[cnt] = Block_Status[cnt];
		}
	}
	else
	{
		for (cnt = 0,i = BlockNum; i < 4; i++,cnt++)
		{
			BlockStatus[cnt] = read_byte();
			pbuf[cnt] = Block_Status[cnt];
		}
	}

	/* Receive CRC */
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	/* calculate the CRC over this part */
	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}
	
	ow_reset();
	return 0xAA;
}

//-------------------------------------------------------
/* Set Block Protection According to protection mode parameter 
The Write Block Protection command is used to set the 
protection of user memory blocks. The parameter byte specifies 
the desired protection modes.Once set, a protection cannot 
be reset.
// Input parameter: 
   1.unsigned char Block : specify Block number for 
     EEPROM protection . DS28E15 include 4 Block, every block 
     include 16Bytes.EEPROM protection apply to specified Block.
   2.unsigned char ProtectOption:  
     Blcok protection modes include :
        1. Read Protection; 
        2.Write Protection; 
        3.EPROM Emulation Mode (EM);
        4.Authentication Protection;                
// Returns: AAh = success
            FFh = CRC Error
            55h = Authentication Mac No Match 
*/
uchar SecureAuth::WriteBlockProtection(uchar Block, uchar ProtectOption)
{
	uchar cnt = 0, i, outcome;
//	memset(pbuf, 0x0, 20);
	if (ow_reset() != 0)
	{
		return 0;
	}
	write_byte(0xCC);

	/* construct a packet to send */
	pbuf[cnt++] = WriteBlockProtectionCommand;
	pbuf[cnt++] = ProtectOption | Block;

	/* Send Data */
	for (i = 0; i < cnt; i++)
	{
		write_byte(pbuf[i]);
	}

	/* Receive CRC */
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	/* calculate the CRC over this part */
	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}

	/* Send Release Byte */
	write_byte(0xAA);
	Delay_Ms(10000);

	outcome = read_byte();
	ow_reset();
	return outcome;
}

//--------------------------------------------------------------------------
/*Read the 64-bit ROM ID of DS28E15
// Input parameter: 
   1.RomID  :64Bits RomID Receiving Buffer 
                    
// Returns: 0x01 = success ,RomID CRC check is right ,
                    RomID Sotred in RomID Buffer
            0x00 = failure , maybe on_reset() Error ,
                   or CRC check Error;
*/
uchar SecureAuth::ReadRomID(uchar* RomID)
{
	uchar i;
	for (i = 0; i < 20; i++)
	{
		if (ow_reset() != 0)
		{
			return 0;
		}
		write_byte(0x33);
	}

	Delay_Ms(10);
	for (i = 0; i < 8; i++)
	{
		RomID[i] = read_byte();
	}

	/* CRC8 Check */
	CRC8 = 0;
	for (i = 0; i < 8; i++)
	{
		dowcrc(RomID[i]);
	}
	if (CRC8 != 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

//---------------------------------------------------------
/*
The Compute and Lock Secret command is used to install a computed, 
device specific secret when the DS28E15 is set up for an application. 
To increase the security level, one can use this command multiple 
times, each time with a different partial secret written to the 
scratchpad data. The initial secret must be loaded, but not locked, 
before Compute and Lock Secret can be executed.
// Input parameter: 
   1.unsigned char PageNum :specifies the memory page that is to 
     be used for the MAC computation. Valid memory page numbers 
     are 0 (page 0) and 1 (page 1).
   2.bool LockEnable:  specify whether the secret is to be write 
     protected (locked) after it is copied to the secret memory
                     
// Returns: AAh = success
            FFh = CRC Error
            55h = The command failed because the secret was 
                  already locked.

*/
uchar SecureAuth::ComputeAndLockSecret(uchar PageNum, bool LockEnable)
{
	uchar cnt = 0, i, outcome;
//	memset(pbuf, 0x0, 20);
	if (ow_reset() != 0)
	{
		return 0;
	}
	write_byte(0xCC);

	/* construct a packet to send */
	pbuf[cnt++] = ComputeAndLockSecretCommand;
	pbuf[cnt++] = LockEnable ? (PageNum | 0xE0) : (PageNum);

	/* Send Data */
	for (i = 0; i < cnt; i++)
	{
		write_byte(pbuf[i]);
	}

	/* Receive CRC */
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	/* calculate the CRC over this part */
	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}

	/* Send Release Byte */
	write_byte(0xAA);
	Delay_Ms(6000);
	for (i = 0; i < 10; i++)
	{
		Delay_Ms(10000);
	}

	outcome = read_byte();
	ow_reset();
	return outcome;
}

// Write one page to EEPROM memory of DS28E15 (32 bytes)
// Returns: AAh = success
// 			55h = The command failed because master 
//                authentication is required.
// 			33h = The command failed because the memory 
//                block is write protected.
// 			53h = The command failed because the memory 
//                block is write protected and requires 
//                master authentication.
// 			FFh = CRC Error
//          01h = PageNum Error
uchar SecureAuth::WriteMemoryPage(uchar PageNum, uchar* WriteData)
{
	uchar cnt = 0, check;
	if (PageNum > 1)
	{
		return 0x01;
	}

	for (cnt = 0; cnt < 8; cnt++)
	{
		check = WriteMemory(cnt, PageNum, &WriteData[cnt * 4]);
		if (check != 0xAA)
		{
			return check;
		}
	}
	return check;
}

//-------------------------------------------------------------------
/*Write 4-byte to EEPROM memory of DS28E15
This command is applicable only to memory blocks that do not require 
master authentication (authentication protection not activated). 
Additionally, the memory block must not be write protected.
// Input parameter: 
   1.unsigned char Segment : Specify Writing Segment Number;  
     DS28E15 EEPROM include 2 Pages ,Each Page 32Bytes ; 
     Each Pages include 4 Block ; every Block 16Bytes ;
     Eevry Block include 4 Segment ;every Segment include 4 Bytes;
     DS28E15 writing according to Segment alignment, 4 Bytes every 
     writing cycle.
   2.unsigned char Page: specify Writing Page Number;
   3.unsigned char *Buffer: Data Buffer writing to DS28E15 EEPROM
          
// Returns: AAh = success
            55h = The command failed because master authentication 
                  is required.
            33h = The command failed because the memory block is 
                  write protected.
            53h = The command failed because the memory block is 
                  write protected and requires master authentication.
            FFh = CRC Error
*/
uchar SecureAuth::WriteMemory(uchar Segment, uchar Page, uchar* Buffer)
{
	uchar cnt = 0, i, outcome;
//	memset(pbuf, 0x0, 20);
	if (ow_reset() != 0)
	{
		return 0;
	}
	write_byte(0xCC);

	/* construct a packet to send */
	pbuf[cnt++] = WriteMemoryCommand;
	pbuf[cnt++] = ((Segment << 5) | Page) & (0xE1);

	/* Send Data */
	for (i = 0; i < cnt; i++)
	{
		write_byte(pbuf[i]);
	}

	/* Receive CRC */
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	/* calculate the CRC over this part */
	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}

	/* return result of inverted CRC */
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}

	cnt = 0;
	/* Send Memory Data */
	for (cnt = 0; cnt < 4; cnt++)
	{
		pbuf[cnt] = Buffer[cnt];
		write_byte(Buffer[cnt]);
	}

	/* Receive CRC */
	pbuf[cnt++] = read_byte();
	pbuf[cnt++] = read_byte();

	/* calculate the CRC over this part */
	CRC16 = 0;
	for (i = 0; i < cnt; i++)
	{
		dowcrc16(pbuf[i]);
	}

	/* return result of inverted CRC */
	if (CRC16 != 0xB001)
	{
		return 0xFF;
	}

	/* Send Release Byte */
	write_byte(0xAA);
	Delay_Ms(10000);

	outcome = read_byte();
	ow_reset();
	return outcome;
}

//----------------------------------------------------------------------
// Prepair the block for hashing
//
void SecureAuth::sha_prepareSchedule(uchar* message)
{
	// we need to copy the initial message into the 16 W registers
	uchar i,j;
	ulong temp;
	for (i = 0; i < 16; i++)
	{
		temp = 0;
		for (j = 0; j < 4;j++)
		{
			temp = temp << 8;
			temp = temp | (*message & 0xff);
			message++;
		}

		W32[i] = temp;
	}
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong SecureAuth::sha_bigsigma256_0(ulong x)
{
	return sha_rotr_32(x,2) ^ sha_rotr_32(x,13) ^ sha_rotr_32(x,22);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong SecureAuth::sha_bigsigma256_1(ulong x)
{
	return sha_rotr_32(x,6) ^ sha_rotr_32(x,11) ^ sha_rotr_32(x,25);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong SecureAuth::sha_littlesigma256_0(ulong x)
{
	return sha_rotr_32(x,7) ^ sha_rotr_32(x,18) ^ sha_shr_32(x,3);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong SecureAuth::sha_littlesigma256_1(ulong x)
{
	return sha_rotr_32(x,17) ^ sha_rotr_32(x,19) ^ sha_shr_32(x,10);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong SecureAuth::sha_ch(ulong x, ulong y, ulong z)
{
	return (x & y) ^ ((~x) & z);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong SecureAuth::sha_maj(ulong x, ulong y, ulong z)
{
	ulong temp = x & y;
	temp ^= (x & z);
	temp ^= (y & z);
	return temp;  //(x & y) ^ (x & z) ^ (y & z);
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong SecureAuth::sha_rotr_32(ulong val, ushort r)
{
	val = val & 0xFFFFFFFFL;
	return ((val >> r) | (val << (32 - r))) & 0xFFFFFFFFL;
}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong SecureAuth::sha_shr_32(ulong val, ushort r)
{
	val = val & 0xFFFFFFFFL;
	return val >> r;
}


//----------------------------------------------------------------------
// SHA-256 support function
//
void SecureAuth::sha_copy32(ulong* p1, ulong* p2, ushort length)
{
	while (length > 0)
	{
		*p2++ = *p1++;
		length--;
	}
}

//----------------------------------------------------------------------
// SHA-256 support function
//
void SecureAuth::sha_copyWordsToBytes32(ulong* input, uchar* output, 
                                        ushort numwords)
{
	ulong temp;
	ushort i;

	for (i=0;i<numwords;i++)
	{
		temp = *input++;
		*output++ = (uchar)(temp >> 24);
		*output++ = (uchar)(temp >> 16);
		*output++ = (uchar)(temp >> 8);
		*output++ = (uchar)(temp);
	}
}

//----------------------------------------------------------------------
// SHA-256 support function
//
void SecureAuth::sha_writeResult(ushort reverse, uchar* outpointer)
{
	uchar i,tmp;;
	sha_copyWordsToBytes32(H32, outpointer, 8); 

	if (reverse)
	{
		for (i = 0; i < 16; i++)
		{  
			tmp = outpointer[i];
			outpointer[i] = outpointer[31-i];
			outpointer[31-i] = tmp;
		}
	}

}

//----------------------------------------------------------------------
// SHA-256 support function
//
ulong SecureAuth::sha_getW(int index)
{
	unsigned long newW;
	if (index < 16)
	{
		return W32[index];
	}

	newW = sha_littlesigma256_1(W32[(index-2)&0x0f]) + 
		W32[(index-7)&0x0f] + 
		sha_littlesigma256_0(W32[(index-15)&0x0f]) + 
		W32[(index-16)&0x0f];
	W32[index & 0x0f] = newW & 0xFFFFFFFFL;  // just in case...

	return newW;
}


//----------------------------------------------------------------------
// Hash a single block of data. 
//
void SecureAuth::sha256_hashblock(uchar* message, ushort lastblock)
{
	ushort sha1counter = 0;
	ushort sha1functionselect = 0;
	ushort i;
	ulong nodeT1, nodeT2;

	ulong Wt, Kt;

	// chunk the original message into the working schedule
	sha_prepareSchedule(message);

	a32 = H32[0];
	b32 = H32[1];
	c32 = H32[2];
	d32 = H32[3];
	e32 = H32[4];
	f32 = H32[5];
	g32 = H32[6];
	h32 = H32[7];

	// rounds
	for (i = 0; i < 64; i++)
	{
		Wt = sha_getW(i);
		Kt = SHA_CONSTANTS[i]; 

		nodeT1 = (h32 + sha_bigsigma256_1(e32) + 
            sha_ch(e32,f32,g32) + Kt + Wt);         // & 0xFFFFFFFFL;
		nodeT2 = (sha_bigsigma256_0(a32) + 
            sha_maj(a32,b32,c32));                  // & 0xFFFFFFFFL;
		h32 = g32;
		g32 = f32;
		f32 = e32;
		e32 = d32 + nodeT1;
		d32 = c32;
		c32 = b32;
		b32 = a32;
		a32 = nodeT1 + nodeT2;

		sha1counter++;
		if (sha1counter==20)
		{
			sha1functionselect++;
			sha1counter = 0;
		}			

	}

	if (!lastblock)
	{
		// now fix up our H array
		H32[0] += a32;
		H32[1] += b32;
		H32[2] += c32;
		H32[3] += d32;
		H32[4] += e32;
		H32[5] += f32;
		H32[6] += g32;
		H32[7] += h32;
	}
	else
	{
		// now fix up our H array
		H32[0] = a32;
		H32[1] = b32;
		H32[2] = c32;
		H32[3] = d32;
		H32[4] = e32;
		H32[5] = f32;
		H32[6] = g32;
		H32[7] = h32;
	}
}

//----------------------------------------------------------------------
// Computes SHA-256 given the data block 'message' with no padding. 
// The result is returned in 'digest'.   
//
// 'message'  - buffer containing the message 
// 'skipconst' - skip adding constant on last block (skipconst=1)
// 'reverse' - reverse order of digest (reverse=1, MSWord first, 
//             LSByte first)
// 'digest'  - result hash digest in byte order used by 1-Wire 
//              device
//
void SecureAuth::ComputeSHA256(uchar* message, short length, 
                               ushort skipconst, ushort reverse, 
                               uchar* digest)
{
	ushort bytes_per_block;
	ushort nonpaddedlength;
	ushort numblocks;
	ushort i,j;
	ulong bitlength;
	ushort markerwritten;
	ushort lastblock;

	ushort wordsize = 32;



	// if wordsize is 32 bits, we need 512 bit blocks.  else 1024 bit blocks.
	// that means 16 words are in one message.
	bytes_per_block = 16 * (wordsize / 8);
	// 1 byte for the '80' that follows the message, 8 or 16 bytes of length
	nonpaddedlength = length + 1 + (wordsize/4);
	numblocks = nonpaddedlength / bytes_per_block;
	if ((nonpaddedlength % bytes_per_block) != 0) 
	{
		// then there is some remainder we need to pad
		numblocks++;
	}

	sha_copy32(SHA_256_Initial, H32, SHA_256_INITIAL_LENGTH); 

	bitlength = 8 * length;
	markerwritten = 0;
	// 'length' is our number of bytes remaining.
	for (i = 0; i < numblocks; i++)
	{
		if (length > bytes_per_block)
		{
			memcpy(workbuffer, message, bytes_per_block);
			length -= bytes_per_block;
		}
		else if (length==bytes_per_block)
		{
			memcpy(workbuffer, message, length);
			length = 0;
		}
		else // length is less than number of bytes in a block
		{
			memcpy(workbuffer, message, length);
			// message is now used for temporary space
			message = workbuffer + length;     
			if (markerwritten == 0)
			{
				*message++ = 0x80;
				length++;
			}

			while (length < bytes_per_block)
			{
				// this loop is inserting padding, in this case all zeroes
				*message++ = 0;
				length++;
			}
			length = 0;
			// signify that we have already written the 80h
			markerwritten = 1;
		}

		// on the last block, put the bit length at the very end
		lastblock = (i == (numblocks - 1));
		if (lastblock)
		{
			// point at the last byte in the block
			message = workbuffer + bytes_per_block - 1;
			for (j = 0; j < wordsize/4; j++)
			{
				*message-- = (uchar)bitlength;
				bitlength = bitlength >> 8;
			}
		}

		// SHA in software 
		sha256_hashblock(workbuffer, (ushort)(lastblock && skipconst));
		message += bytes_per_block;
	}

	sha_writeResult(reverse, digest);
}

//----------------------------------------------------------------------
/*  Computes SHA-256 given the MT digest buffer after first iserting
    the secret at the correct location in the array defined by all Maxim
    devices. Since part of the input is secret it is called a Message
    Autnentication Code (MAC).
//  Input Parameter: 
    1.unsigned char *MT :buffer containing the message digest
    2.short length:Length of block to digest
    3.unsigned char *MAC:result MAC in byte order used by 1-Wire device
	4.bool SecretMode: 0-public secret   1-new secret
// Returns: None;    
*/
void SecureAuth::ComputeMAC256(uchar *MT, short length, 
                               uchar* MAC, bool SecretMode)
{
	uchar i,j;  
	uchar tmp[4]; 

	// check for two block format
	if (length == 119)
	{
		// insert secret
		if (SecretMode)
		{
			memcpy(&MT[64], &SECRET[0], 32);
		}
		else
		{
			memcpy(&MT[64], &Secret_256bit[0], 32);
		}

		// change to little endian for A1 devices
		if (REVERSE_ENDIAN)
		{
			for (i = 0; i < 108; i+=4)
			{
				for (j = 0; j < 4; j++)
					tmp[3 - j] = MT[i + j];

				for (j = 0; j < 4; j++)
					MT[i + j] = tmp[j];
			}
		}

		ComputeSHA256(MT, 119, true, true, MAC);
	}
	// one block format
	else
	{
		// insert secret
		if (SecretMode)
		{
			memcpy(&MT[0], &SECRET[0], 32);
		}
		else
		{
			memcpy(&MT[0], &Secret_256bit[0], 32);
		}

		// change to little endian for A1 devices
		if (REVERSE_ENDIAN)
		{
			for (i = 0; i < 56; i+=4)
			{
				for (j = 0; j < 4; j++)
					tmp[3 - j] = MT[i + j];

				for (j = 0; j < 4; j++)
					MT[i + j] = tmp[j];
			}
		}

		ComputeSHA256(MT, 55, true, true, MAC);
	}
	
}

//----------------------------------------------------------------------
/*Computes SHA-256 given the MT digest buffer after first iserting
  the secret at the correct location in the array defined by all Maxim
  devices. Since part of the input is secret it is called a Message
  Autnentication Code (MAC).
//  Input Parameter: 
    1.unsigned char * MT: buffer containing the message digest 
    2.short length:Length of block to digest
    3.unsigned char* compare_MAC : MAC in byte order used by 1-Wire
      device to compare with calculate MAC.        
*/
bool SecureAuth::VerifyMAC256(uchar * MT, short length, uchar* 
                              compare_MAC, bool SecretMode)
{
	uchar calc_mac[32];
	int i;

	// calculate the MAC
	ComputeMAC256(MT, length, calc_mac, SecretMode);

	// Compare calculated mac with one read from device
	for (i = 0; i < 32; i++)
	{
		if (compare_MAC[i] != calc_mac[i])
			return (bool)0;
	}
	return (bool)1;	
}

//----------------------------------------------------------------------
/* 
Setting secret buffer for SHA-256 Algrorithm
// Input parameter: 
   1.unsigned char *secret : Data buffer for secret seting;
// Returns: None;
*/
void SecureAuth::set_secret(uchar* secret)
{ 
   unsigned char i;

   for (i = 0; i < 32; i++)
      SECRET[i] = secret[i];   
}

//----------------------------------------------------------------------
/* Performs a Compute Next SHA-256 calculation given the provided 32-bytes
   of binding data and 8 byte partial secret. The first 8 bytes of the
   resulting MAC is set as the new secret.
//  Input Parameter:  
    1. unsigned char *binding: 32 byte buffer containing the binding data
    2. unsigned char *partial: 8 byte buffer with new partial secret
    3. int page_nim: page number that the compute is calculated on 
    4. unsigned char *manid: manufacturer ID
   Globals used : SECRET used in calculation and set to new secret
   Returns: TRUE if compute successful
            FALSE failed to do compute
*/
void SecureAuth::CalculateNextSecret256(uchar* binding, uchar* partial, 
                                        int page_num, uchar* manid)
{
	unsigned char MT[128];
	unsigned char MAC[64];

	// clear 
	memset(MT,0,128);

	// insert page data
	memcpy(&MT[0],binding,32);

	// insert challenge
	memcpy(&MT[32],partial,32);

	// insert ROM number or FF
	memcpy(&MT[96],RomID_Buffer,8);

	MT[106] = page_num;
 	MT[105] = manid[0];
 	MT[104] = manid[1];

	ComputeMAC256(MT, 119, MAC, false);

	// set the new secret to the first 32 bytes of MAC
	set_secret(MAC);

}

//--------------------------------------
//Validate the DS28E15 
//the result 1 declares validation
uchar SecureAuth::AuthDevice(uchar* cpuid)
{
	Verify_outcome = 0;
	uchar detV = ReadRomID(RomID_Buffer);
	if (!detV)
	{
		return Verify_outcome;
	}
	
	// calculate new key to SECRET[]
	if (ReadMemory(Segment0, Page1, EEPROM) == 0xAA)
	{
		// 校验CPU ID是否合法
		if (VerifyEEPROM(EEPROM, cpuid) != 0)
		{
			return Verify_outcome;		// CPU ID 不合法
		}
		
		// 计算唯一密钥
		// Personality_Value, EEPROM, Scratch_Writebuffer, 
        // Secret_256bit, RomID_Buffer 参与计算
		// 结果保存在SECRET[]
		ReadBlockStatus(0, (bool)1, Personality_Value);
		CalculateNextSecret256(EEPROM, Scratch_Writebuffer, 
            Page1, &Personality_Value[2]);
	}
	else
	{
		return Verify_outcome;			// 读取EEPROM出错
	}

	// AuthSecret
	// 1. firstly read MAC Value computed by DS28E15 
    //    according to Scratch value, secret , page data ,Rom id etc.
	ReadBlockStatus(0, true, Personality_Value);
	ReadWriteScratchpad(false, Scratch_Writebuffer);
	ComputeAndReadPageMAC(false, Page1, MAC_Read_Value);

	/* 2. calculate MAC by "ComputeMAC256 " according to same 
          128bytes input_data as below */
	if (ReadMemory(Segment0, Page1, EEPROM) == 0xAA)
	{
		memcpy(MAC_Computer_Datainput, EEPROM, 32);
		memcpy(&MAC_Computer_Datainput[32], Scratch_Writebuffer, 32);
		memcpy(&MAC_Computer_Datainput[96], RomID_Buffer, 8);
		memcpy(&MAC_Computer_Datainput[104], &Personality_Value[2], 2);
		MAC_Computer_Datainput[106] = 0x01;

		/* 3. compare with each other */
		Verify_outcome = VerifyMAC256(MAC_Computer_Datainput, 119, 
            MAC_Read_Value, true);
	}
	return Verify_outcome;
}

// save EEPROM to STM32 Flash and set EEPROM[]
void SecureAuth::SetEEPROM(uchar* input, uchar* output)
{
	uchar i,j,k,tmp1, tmp2;
	uchar* input_tmpprt;
	uchar* output_tmpprt;
	input_tmpprt = input;

	// write CPUID to STM32 Flash (0x0800F400) 8bytes
	HAL_FLASH_Unlock();
	FLASH_PageErase(SaveCPUIDAddressInFlash);
	for (i = 0; i < MaxWriteByteLen; i++)
	{
		FLASH_Program_HalfWord(SaveCPUIDAddressInFlash + 
            sizeof(uint16_t) * i, (uint16_t)(*input_tmpprt));
		input_tmpprt++;
	}
	HAL_FLASH_Lock();

	input_tmpprt = input;
	output_tmpprt = output;
	for (i = 0; i < MaxWriteByteLen; i++)
	{
		tmp1 = ((*input_tmpprt) & 0x0f) + (i + 1);
		*output_tmpprt = tmp1;
		output_tmpprt++;
		tmp1 = (((*input_tmpprt) & 0xf0) >> 4) + ((i + 1) * 4);
		*output_tmpprt = tmp1;
		output_tmpprt++;
		input_tmpprt++;
	}

	randnum(16, output_tmpprt);

	output_tmpprt = output;
	for (i = 0; i < 16; i++)
	{
		j = *output_tmpprt;
		k = *(output_tmpprt + 31 - i * 2);
		
		tmp1 = j & 0xf0;
		tmp1 |= (k & 0xf0) >> 4;

		tmp2 = j & 0xf;
		tmp2 |= (k & 0xf) << 4;

		*output_tmpprt = tmp1;
		*(output_tmpprt + 31 - i * 2) = tmp2;
		output_tmpprt++;
	}
}

// 根据密文反推CPU ID值
void SecureAuth::CalculateEEPROM(uchar* input, uchar* output)
{
	uchar* input_tmpprt;
	uchar i, j, k, tmp1, tmp2;
	uchar readEEPROM[32];

	memcpy(readEEPROM, input, 32);
	
	input_tmpprt = readEEPROM;
	for (i = 0; i < 16; i++)
	{
		j = *input_tmpprt;
		k = *(input_tmpprt + 31 - i * 2);
		
		tmp1 = j & 0xf0;
		tmp1 |= k & 0x0f;
		
		tmp2 = (j & 0x0f) << 4;
		tmp2 |= (k & 0xf0) >> 4;

		*input_tmpprt = tmp1;
		*(input_tmpprt + 31 - i * 2) = tmp2;

		input_tmpprt++;
	}

	input_tmpprt = readEEPROM;
	for (i = 0; i < MaxWriteByteLen; i++)
	{
		tmp1 = ((*input_tmpprt) - (i + 1)) & 0x0f;
		input_tmpprt++;
		tmp1 |= (((*input_tmpprt) - ((i + 1) * 4)) << 4) & 0xf0;
		*output = tmp1;
		input_tmpprt++;
		output++;
	}
	
}

// 校验CPU ID与保存的CPU ID 是否一致
uchar SecureAuth::VerifyEEPROM(uchar* readEEPROM, 
                               uchar* inputID)
{
	uchar i;
	uchar data[MaxWriteByteLen];
	uchar data1[MaxWriteByteLen];
	uchar* tmp;

	// read CPU id from flash
	for (i = 0; i < MaxWriteByteLen; i++)
	{
		tmp = (uchar*)(SaveCPUIDAddressInFlash + 
            sizeof(uint16_t) * i);
		data[i] = *tmp;
		if (data[i] != *inputID)
		{
			return 1;
		}
		inputID++;
	}
	
	CalculateEEPROM(readEEPROM, data1);

	for (i = 0; i < MaxWriteByteLen; i++)
	{
		if (data1[i] != data[i])
		{
			return 1;
		}
	}
	return 0;
}

uchar SecureAuth::LoadSecret(uchar* cpuid)
{
	uchar detV = ReadRomID(RomID_Buffer);
	if (!detV)
	{
		return 1;			// 读RomID出错
	}

	// 写EEPROM memory to DS28E15
	SetEEPROM(cpuid, EEPROM);
	if (WriteMemoryPage(Page1, EEPROM) != 0xAA)
	{
		return 2;			// 写出错
	}

	uchar test1[32];
	if (ReadMemory(Segment0, Page1, test1)!= 0xAA)
	{
		return 2;
	}

	// 计算唯一密钥
	ReadBlockStatus(0, true, Personality_Value);
	CalculateNextSecret256(EEPROM, Scratch_Writebuffer, 
        Page1, &Personality_Value[2]);

	// 装载密钥
	if (SettingAndLockSecret(SECRET, false) != 0xAA)
	{
		return 3;		// 装载密码出错
	}

	return 0;				// 成功
}


} // namespace
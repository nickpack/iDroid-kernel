/**
 * Copyright (c) 2011 Richard Ian Taylor.
 * Portions (c) 2008 Yidou Wang
 *
 * This file is part of the iDroid Project. (http://www.idroidproject.org).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/spi/spi.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/gpio.h>

#include <plat/zephyr2.h>

#define MT_INFO_FAMILYID 0xD1
#define MT_INFO_SENSORINFO 0xD3
#define MT_INFO_SENSORREGIONDESC 0xD0
#define MT_INFO_SENSORREGIONPARAM 0xA1
#define MT_INFO_SENSORDIM 0xD9

#define MAX_FINGER_ORIENTATION  16384
#define MAX_BUFFER_SIZE 0x400

#define NORMAL_SPEED (&MTNormalSpeed)
#define FAST_SPEED (&MTFastSpeed)

typedef struct MTFrameHeader
{
	u8 type;
	u8 frameNum;
	u8 headerLen;
	u8 unk_3;
	u32 timestamp;
	u8 unk_8;
	u8 unk_9;
	u8 unk_A;
	u8 unk_B;
	u16 unk_C;
	u16 isImage;

	u8 numFingers;
	u8 fingerDataLen;
	u16 unk_12;
	u16 unk_14;
	u16 unk_16;
} __attribute__((packed)) MTFrameHeader;

typedef struct FingerData
{
	u8 id;
	u8 event;
	u8 unk_2;
	u8 unk_3;
	s16 x;
	s16 y;
	s16 rel_x;
	s16 rel_y;
	u16 size_major;
	u16 size_minor;
	u16 orientation;
	u16 force_major;
	u16 force_minor;
	u16 unk_16;
	u16 unk_18;
	u16 unk_1A;
} __attribute__((packed)) FingerData;

typedef struct MTSPISetting
{
	int speed;
	int txDelay;
	int rxDelay;
} MTSPISetting;

const MTSPISetting MTNormalSpeed = {83000, 5, 10};
const MTSPISetting MTFastSpeed = {4500000, 0, 5};

struct zephyr2_data
{
	u8 OutputPacket[MAX_BUFFER_SIZE];
	u8 InputPacket[MAX_BUFFER_SIZE];
	u8 GetInfoPacket[MAX_BUFFER_SIZE];
	u8 GetResultPacket[MAX_BUFFER_SIZE];

	int InterfaceVersion;
	int MaxPacketSize;
	int FamilyID;
	int FlipNOP;
	int SensorWidth;
	int SensorHeight;
	int SensorColumns;
	int SensorRows;
	int BCDVersion;
	int Endianness;
	u8* SensorRegionDescriptor;
	int SensorRegionDescriptorLen;
	u8* SensorRegionParam;
	int SensorRegionParamLen;

	u8 min_pressure;

	int CurNOP;

	int irq;

	const struct firmware *firmware;

	struct input_dev *input_dev;
	struct spi_device *spi_dev;

	int irq_count;
	spinlock_t irq_lock;
	struct work_struct irq_work;
};

static int makeBootloaderDataPacket(u8* output, u32 destAddress, const u8* data, int dataLen, int* cksumOut);
static void sendExecutePacket(struct zephyr2_data *_z2);

static bool loadConstructedFirmware(struct zephyr2_data *_z2, const u8* firmware, int len);
static int loadProxCal(struct zephyr2_data *_z2, const u8* firmware, int len);
static int loadCal(struct zephyr2_data *_z2, const u8* firmware, int len);
static bool determineInterfaceVersion(struct zephyr2_data *_z2);

static bool getReportInfo(struct zephyr2_data *_z2, int id, u8* err, u16* len);
static bool getReport(struct zephyr2_data *_z2, int id, u8* buffer, int* outLen);

static void newPacket(struct zephyr2_data *_z2, const u8* data, int len);

static inline u32 zephyr2_getU32(u8 *_buf)
{
	return (_buf[2] << 24)
		| (_buf[3] << 16)
		| (_buf[0] << 8)
		| _buf[1];
}

static inline void zephyr2_makeU32(u8 *_buf, u32 _val)
{
	_buf[2] = _val >> 24;
	_buf[3] = (_val >> 16) & 0xFF;
	_buf[0] = (_val >> 8) & 0xFF;
	_buf[1] = _val & 0xFF;
}
static inline u16 zephyr2_getU16(u8 *_buf)
{
	return (_buf[0] << 8)
		| _buf[1];
}

static inline void zephyr2_makeU16(u8 *_buf, u16 _val)
{
	_buf[0] = _val >> 8;
	_buf[1] = _val & 0xFF;
}

static inline u16 zephyr2_getU16R(u8 *_buf)
{
	return (_buf[1] << 8)
		| _buf[0];
}

static inline void zephyr2_makeU16R(u8 *_buf, u16 _val)
{
	_buf[1] = _val >> 8;
	_buf[0] = _val & 0xFF;
}

static inline u32 zephyr2_u32Sum(u8 *_buf, u32 _len)
{
	u32 i;
	u32 checksum = 0;

	for(i = 0; i < _len; i++)
	{
		checksum += _buf[i];
	}

	return checksum;
}

static inline u16 zephyr2_u16Sum(u8 *_buf, u32 _len)
{
	u32 i;
	u16 checksum = 0;

	for(i = 0; i < _len; i++)
	{
		checksum += _buf[i];
	}

	return checksum;
}

static inline u16 zephyr2_makeU16Sum(u8 *_buf, u32 _len)
{
	u16 chk = zephyr2_u16Sum(_buf, _len);
	zephyr2_makeU16(_buf+_len, chk);
	return chk;
}

static inline u16 zephyr2_makeU16SumR(u8 *_buf, u32 _len)
{
	u16 chk = zephyr2_u16Sum(_buf, _len);
	zephyr2_makeU16R(_buf+_len, chk);
	return chk;
}

static inline u32 zephyr2_makeU32Sum(u8 *_buf, u32 _len)
{
	u32 chk = zephyr2_u32Sum(_buf, _len);
	zephyr2_makeU32(_buf+_len, chk);
	return chk;
}

static int zephyr2_tx(struct zephyr2_data *_z2, const MTSPISetting* setting, const u8* outBuffer, int outLen)
{
	int ret;
	struct spi_transfer tx = {
		.tx_buf = outBuffer,
		.len = outLen,
		.speed_hz = setting->speed,
		.delay_usecs = setting->txDelay * 1000,
	};
	struct spi_message msg;

	spi_message_init(&msg);
	spi_message_add_tail(&tx, &msg);

	ret = spi_sync(_z2->spi_dev, &msg);
	if(ret != 0)
		dev_err(&_z2->spi_dev->dev, "tx failed (%d).\n", ret);
	return ret;
}

static int zephyr2_txrx(struct zephyr2_data *_z2, const MTSPISetting* setting, const u8* outBuffer, int outLen, u8* inBuffer, int inLen)
{
	int ret;
	int sz = (outLen > inLen) ? outLen : inLen;
	struct spi_transfer tx = {
		.tx_buf = outBuffer,
		.rx_buf = inBuffer,
		.len = sz,
		.speed_hz = setting->speed,
		.delay_usecs = setting->rxDelay * 1000,
	};

	struct spi_message msg;

	spi_message_init(&msg);
	spi_message_add_tail(&tx, &msg);

	ret = spi_sync(_z2->spi_dev, &msg);
	if(ret != 0)
		dev_err(&_z2->spi_dev->dev, "tx failed (%d).\n", ret);
	return ret;
}

static u16 zephyr2_shortAck(struct zephyr2_data *_z2)
{
	u8 tx[2];
	u8 rx[2];

	zephyr2_makeU16(tx, 0x1aa1);

	zephyr2_txrx(_z2, NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	return zephyr2_getU16(rx);
}

static u32 zephyr2_longAck(struct zephyr2_data *_z2)
{
	u8 tx[8];
	u8 rx[8];

	zephyr2_makeU16(tx, 0x1aa1);

	zephyr2_makeU16(tx+2, 0x18e1);
	zephyr2_makeU16(tx+4, 0x18e1);
	zephyr2_makeU16(tx+6, 0x18e1);

	zephyr2_txrx(_z2, NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	return zephyr2_getU32(rx+2);
}

static u32 readRegister(struct zephyr2_data *_z2, u32 address)
{
	u8 tx[8];
	u8 rx[8];

	zephyr2_makeU16(tx, 0x1c73);
	zephyr2_makeU32(tx+2, address);
	zephyr2_makeU16Sum(tx+2, 4);

	zephyr2_txrx(_z2, NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	return zephyr2_longAck(_z2);
}

static u32 writeRegister(struct zephyr2_data *_z2, u32 address, u32 value, u32 mask)
{
	u8 tx[16];
	u8 rx[16];

	zephyr2_makeU16(tx, 0x1e33);
	zephyr2_makeU32(tx+2, address);
	zephyr2_makeU32(tx+6, mask);
	zephyr2_makeU32(tx+10, value);
	zephyr2_makeU16Sum(tx+2, sizeof(u32)*3);

	zephyr2_txrx(_z2, NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	return zephyr2_shortAck(_z2) == 0x4AD1;
}


static void newPacket(struct zephyr2_data *_z2, const u8* data, int len)
{
	int i, num=0;
	FingerData* finger;
	MTFrameHeader* header = (MTFrameHeader*) data;
	if(header->type != 0x44 && header->type != 0x43)
		printk("zephyr2: unknown frame type 0x%x\n", header->type);

	finger = (FingerData*)(data + (header->headerLen));

	if(header->headerLen < 12)
		printk("zephyr2: no finger data in frame\n");

	for(i = 0; i < header->numFingers; ++i)
	{
		int fmaj = max(0, finger->force_major - (int)_z2->min_pressure);
		int fmin = max(0, finger->force_minor - (int)_z2->min_pressure);

		if(fmaj && fmin)
		{
			input_report_abs(_z2->input_dev, ABS_MT_TOUCH_MAJOR, fmaj);
			input_report_abs(_z2->input_dev, ABS_MT_TOUCH_MINOR, fmin);
			input_report_abs(_z2->input_dev, ABS_MT_WIDTH_MAJOR, finger->size_major);
			input_report_abs(_z2->input_dev, ABS_MT_WIDTH_MINOR, finger->size_minor);
			input_report_abs(_z2->input_dev, ABS_MT_ORIENTATION, MAX_FINGER_ORIENTATION - finger->orientation);
			input_report_abs(_z2->input_dev, ABS_MT_TRACKING_ID, finger->id);
			input_report_abs(_z2->input_dev, ABS_MT_POSITION_X, finger->x);
			input_report_abs(_z2->input_dev, ABS_MT_POSITION_Y, _z2->SensorHeight - finger->y);

			if(!num++)
			{
				input_report_abs(_z2->input_dev, ABS_X, finger->x);
				input_report_abs(_z2->input_dev, ABS_Y, _z2->SensorHeight - finger->y);
			}
		}

		input_mt_sync(_z2->input_dev);

		finger = (FingerData*) (((u8*) finger) + header->fingerDataLen);
	}

	input_report_key(_z2->input_dev, BTN_TOUCH, num > 0);
	input_report_key(_z2->input_dev, BTN_TOOL_FINGER, num == 1);
	input_report_key(_z2->input_dev, BTN_TOOL_DOUBLETAP, num == 2);
	input_report_key(_z2->input_dev, BTN_TOOL_TRIPLETAP, num == 3);
	input_report_key(_z2->input_dev, BTN_TOOL_QUADTAP, num == 4);
	input_sync(_z2->input_dev);
}

static bool readResultData(struct zephyr2_data *_z2, int len)
{
	u32 checksum;
	int i;
	int packetLen;

	if(len > MAX_BUFFER_SIZE)
	{
		printk("zephyr2: Result too big for buffer! We have %d bytes, we need %d bytes!\n", MAX_BUFFER_SIZE, len);
		len = MAX_BUFFER_SIZE;
	}

	memset(_z2->GetResultPacket, 0, MAX_BUFFER_SIZE);

	if(_z2->FlipNOP)
		_z2->GetResultPacket[0] = 0xEB;
	else
		_z2->GetResultPacket[0] = 0xEA;

	_z2->GetResultPacket[1] = _z2->CurNOP;
	_z2->GetResultPacket[2] = 1;

	checksum = 0;
	for(i = 0; i < 14; ++i)
		checksum += _z2->GetResultPacket[i];

	_z2->GetResultPacket[len - 2] = checksum & 0xFF;
	_z2->GetResultPacket[len - 1] = (checksum >> 8) & 0xFF;

	zephyr2_txrx(_z2, NORMAL_SPEED, _z2->GetResultPacket, len, _z2->InputPacket, len);

	if(_z2->InputPacket[0] != 0xEA && !(_z2->FlipNOP && _z2->InputPacket[0] == 0xEB))
	{
		printk("zephyr2: frame header wrong: got 0x%02X\n", _z2->InputPacket[0]);
		msleep(1);
		return false;
	}

	checksum = 0;
	for(i = 0; i < 5; ++i)
		checksum += _z2->InputPacket[i];

	if((checksum & 0xFF) != 0)
	{
		printk("zephyr2: LSB of first five bytes of frame not zero: got 0x%02X\n", checksum);
		msleep(1);
		return false;
	}

	packetLen = (_z2->InputPacket[2] & 0xFF) | ((_z2->InputPacket[3] & 0xFF) << 8);

	if(packetLen <= 2)
		return true;

	checksum = 0;
	for(i = 5; i < (5 + packetLen - 2); ++i)
		checksum += _z2->InputPacket[i];
	if((_z2->InputPacket[len - 2] | (_z2->InputPacket[len - 1] << 8)) != checksum)
	{
		printk("zephyr2: packet checksum wrong 0x%02X instead of 0x%02X\n", checksum, (_z2->InputPacket[len - 2] | (_z2->InputPacket[len - 1] << 8)));
		msleep(1);
		return false;
	}

	newPacket(_z2, _z2->InputPacket + 5, packetLen - 2);
	return true;
}

static bool zephyr2_readFrameLength(struct zephyr2_data *_z2, int* len)
{
	u8 tx[16];
	u8 rx[16];
	u32 checksum;

	memset(tx, 0, sizeof(tx));

	if(_z2->FlipNOP)
		tx[0] = 0xEB;
	else
		tx[0] = 0xEA;

	tx[1] = _z2->CurNOP;
	tx[2] = 0;

	zephyr2_makeU16SumR(tx, 14);

	zephyr2_txrx(_z2, NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	checksum = zephyr2_u16Sum(rx, 14);
	if((rx[14] | (rx[15] << 8)) != checksum)
	{
		udelay(1000);
		return false;
	}

	*len = (rx[1] & 0xFF) | ((rx[2] & 0xFF) << 8);

	return true;
}

static int zephyr2_readFrame(struct zephyr2_data *_z2)
{
	int ret = 0;
	int len = 0;

	if(!zephyr2_readFrameLength(_z2, &len))
	{
		printk("zephyr2: error getting frame length\r\n");
		len = 0;
		ret = -1;
	}

	if(len)
	{
		if(!readResultData(_z2, len + 5))
		{
			printk("zephyr2: error getting frame data\r\n");
			msleep(1);
			ret = -1;
		}

		ret = 1;
	}

	if(_z2->FlipNOP)
	{
		if(_z2->CurNOP == 1)
			_z2->CurNOP = 2;
		else
			_z2->CurNOP = 1;
	}

	return ret;
}


static int shortControlRead(struct zephyr2_data *_z2, int id, u8* buffer, int size)
{
	u32 checksum;
	int i;

	memset(_z2->GetInfoPacket, 0, MAX_BUFFER_SIZE);
	_z2->GetInfoPacket[0] = 0xE6;
	_z2->GetInfoPacket[1] = id;
	_z2->GetInfoPacket[2] = 0;
	_z2->GetInfoPacket[3] = size & 0xFF;
	_z2->GetInfoPacket[4] = (size >> 8) & 0xFF;

	checksum = 0;
	for(i = 0; i < 5; ++i)
		checksum += _z2->GetInfoPacket[i];

	_z2->GetInfoPacket[14] = checksum & 0xFF;
	_z2->GetInfoPacket[15] = (checksum >> 8) & 0xFF;

	zephyr2_txrx(_z2, NORMAL_SPEED, _z2->GetInfoPacket, 16, _z2->InputPacket, 16);

	udelay(25);

	_z2->GetInfoPacket[2] = 1;

	zephyr2_txrx(_z2, NORMAL_SPEED, _z2->GetInfoPacket, 16, _z2->InputPacket, 16);

	checksum = 0;
	for(i = 0; i < 14; ++i)
		checksum += _z2->InputPacket[i];

	if((_z2->InputPacket[14] | (_z2->InputPacket[15] << 8)) != checksum)
		return false;

	memcpy(buffer, &_z2->InputPacket[3], size);

	return true;
}

static int longControlRead(struct zephyr2_data *_z2, int id, u8* buffer, int size)
{
	u32 checksum;
	int i;

	memset(_z2->GetInfoPacket, 0, 0x200);
	_z2->GetInfoPacket[0] = 0xE7;
	_z2->GetInfoPacket[1] = id;
	_z2->GetInfoPacket[2] = 0;
	_z2->GetInfoPacket[3] = size & 0xFF;
	_z2->GetInfoPacket[4] = (size >> 8) & 0xFF;

	checksum = 0;
	for(i = 0; i < 5; ++i)
		checksum += _z2->GetInfoPacket[i];

	_z2->GetInfoPacket[14] = checksum & 0xFF;
	_z2->GetInfoPacket[15] = (checksum >> 8) & 0xFF;

	zephyr2_txrx(_z2, NORMAL_SPEED, _z2->GetInfoPacket, 16, _z2->InputPacket, 16);

	_z2->GetInfoPacket[2] = 1;
	_z2->GetInfoPacket[14] = 0;
	_z2->GetInfoPacket[15] = 0;
	_z2->GetInfoPacket[3 + size] = checksum & 0xFF;
	_z2->GetInfoPacket[3 + size + 1] = (checksum >> 8) & 0xFF;

	zephyr2_txrx(_z2, NORMAL_SPEED, _z2->GetInfoPacket, size + 5, _z2->InputPacket, size + 5);

	checksum = 0;
	for(i = 0; i < (size + 3); ++i)
		checksum += _z2->InputPacket[i];

	if((_z2->InputPacket[3 + size] | (_z2->InputPacket[3 + size + 1] << 8)) != checksum)
		return false;

	memcpy(buffer, &_z2->InputPacket[3], size);

	return true;
}

static bool getReportInfo(struct zephyr2_data *_z2, int id, u8* err, u16* len)
{
	u8 tx[16];
	u8 rx[16];
	u32 checksum;
	int i;
	int try;

	for(try = 0; try < 4; ++try)
	{
		memset(tx, 0, sizeof(tx));

		tx[0] = 0xE3;
		tx[1] = id;

		checksum = 0;
		for(i = 0; i < 14; ++i)
			checksum += tx[i];

		tx[14] = checksum & 0xFF;
		tx[15] = (checksum >> 8) & 0xFF;

		zephyr2_txrx(_z2, NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		if(rx[0] != 0xE3)
			continue;

		checksum = 0;
		for(i = 0; i < 14; ++i)
			checksum += rx[i];

		if((rx[14] | (rx[15] << 8)) != checksum)
			continue;

		*err = rx[2];
		*len = (rx[4] << 8) | rx[3];

		return true;
	}

	return false;
}

static bool getReport(struct zephyr2_data *_z2, int id, u8* buffer, int* outLen)
{
	u8 err;
	u16 len;
	int try;

	if(!getReportInfo(_z2, id, &err, &len))
		return false;

	if(err)
		return false;

	*outLen = len;

	for(try = 0; try < 4; ++try)
	{
		if(len < 12)
		{
			if(shortControlRead(_z2, id, buffer, len))
				return true;
		} else
		{
			if(longControlRead(_z2, id, buffer, len))
				return true;
		}
	}

	return false;
}

static bool determineInterfaceVersion(struct zephyr2_data *_z2)
{
	u8 tx[16];
	u8 rx[16];
	u32 checksum;
	int i;
	int try;

	memset(tx, 0, sizeof(tx));

	tx[0] = 0xE2;

	checksum = 0;
	for(i = 0; i < 14; ++i)
		checksum += tx[i];

	// Note that the byte order changes to little-endian after main firmware load

	tx[14] = checksum & 0xFF;
	tx[15] = (checksum >> 8) & 0xFF;

	for(try = 0; try < 4; ++try)
	{
		zephyr2_txrx(_z2, NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		if(rx[0] == 0xE2)
		{
			checksum = 0;
			for(i = 0; i < 14; ++i)
				checksum += rx[i];

			if((rx[14] | (rx[15] << 8)) == checksum)
			{
				_z2->InterfaceVersion = rx[2];
				_z2->MaxPacketSize = (rx[4] << 8) | rx[3];
				printk("zephyr2: interface version %d, max packet size: %d\n", _z2->InterfaceVersion, _z2->MaxPacketSize);

				return true;
			}
		}

		_z2->InterfaceVersion = 0;
		_z2->MaxPacketSize = 1000;
		msleep(3);
	}

	printk("zephyr2: failed getting interface version!\n");

	return false;
}

static bool loadConstructedFirmware(struct zephyr2_data *_z2, const u8* firmware, int len)
{
	int try;

	for(try = 0; try < 5; ++try)
	{
		uint16_t val;

		printk("zephyr2: uploading firmware\n");

		zephyr2_tx(_z2, FAST_SPEED, firmware, len);

		udelay(300);

		val = zephyr2_shortAck(_z2);
		if(val == 0x4BC1)
			return true;

		printk("zephyr2: strange ack: 0x%x\n", val);
	}

	return false;
}

static int loadProxCal(struct zephyr2_data *_z2, const u8* firmware, int len)
{
	u32 address = 0x400180;
	const u8* data = firmware;
	int left = (len + 3) & ~0x3;
	int try;

	while(left > 0)
	{
		int toUpload = left;
		if(toUpload > (MAX_BUFFER_SIZE - 0x10))
		{
			printk("zephyr2: prox-cal too big for buffer.\n");
			toUpload = MAX_BUFFER_SIZE - 0x10;
		}

		_z2->OutputPacket[0] = 0x18;
		_z2->OutputPacket[1] = 0xE1;

		makeBootloaderDataPacket(_z2->OutputPacket + 2, address, data, toUpload, NULL);

		for(try = 0; try < 5; ++try)
		{
			printk("zephyr2: uploading prox calibration data packet\r\n");

			zephyr2_tx(_z2, FAST_SPEED, _z2->OutputPacket, toUpload + 0x10);
			udelay(300);

			if(zephyr2_shortAck(_z2) == 0x4BC1)
				break;
		}

		if(try == 5)
			return false;

		address += toUpload;
		data += toUpload;
		left -= toUpload;
	}

	return true;
}

static int loadCal(struct zephyr2_data *_z2, const u8* firmware, int len)
{
	u32 address = 0x400200;
	const u8* data = firmware;
	int left = (len + 3) & ~0x3;
	int try;

	while(left > 0)
	{
		int toUpload = left;
		if(toUpload > 0x3F0)
			toUpload = 0x3F0;

		_z2->OutputPacket[0] = 0x18;
		_z2->OutputPacket[1] = 0xE1;

		makeBootloaderDataPacket(_z2->OutputPacket + 2, address, data, toUpload, NULL);

		for(try = 0; try < 5; ++try)
		{
			printk("zephyr2: uploading calibration data packet\r\n");

			zephyr2_tx(_z2, FAST_SPEED, _z2->OutputPacket, toUpload + 0x10);
			udelay(300);

			if(zephyr2_shortAck(_z2) == 0x4BC1)
				break;
		}

		if(try == 5)
			return false;

		address += toUpload;
		data += toUpload;
		left -= toUpload;
	}

	return true;
}

static u32 zephyr2_getCalibration(struct zephyr2_data *_z2)
{
	u8 tx[2];
	u8 rx[2];

	tx[0] = 0x1F;
	tx[1] = 0x01;

	printk("zephyr2: requesting calibration...\n");

	zephyr2_txrx(_z2, NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	msleep(65);

	return zephyr2_longAck(_z2);
}

static int zephyr2_calibrate(struct zephyr2_data *_z2)
{
	u32 zephyr2_version;

	zephyr2_version = readRegister(_z2, 0x10008FFC);

	printk("zephyr2: detected Zephyr2 version 0x%0X\n", zephyr2_version);

	if(zephyr2_version != 0x5A020028 && zephyr2_version != 0x5A02002A) // TODO: What the hell causes this to crash? ;_;
	{
#define init_register(addr, a, b) if(!writeRegister(_z2, addr, (a), (b))) { \
	printk("zephyr2: error initialising register " #addr "\n"); return false; }

		printk("zephyr2: Initialising Registers...\n");
		// -- BEGIN INITIALISING REGISTERS -- //

		init_register(0x10001C04, 0x16E4, 0x1FFF);
		init_register(0x10001C08, 0x840000, 0xFF0000);
		init_register(0x1000300C, 0x05, 0x85);
		init_register(0x1000304C, 0x20, 0xFFFFFFFF);

		// --- END INITIALISING REGISTERS --- //
		printk("zephyr2: Initialised Registers\n");

#undef init_register
	}

	printk("zephyr2: calibration complete with 0x%x\n", zephyr2_getCalibration(_z2));

	return true;
}

static void sendExecutePacket(struct zephyr2_data *_z2)
{
	u8 tx[12];
	u8 rx[12];

	tx[0] = 0x1D;
	tx[1] = 0x53;

	tx[2] = 0x18;
	tx[3] = 0x00;
	tx[4] = 0x10;
	tx[5] = 0x00;
	tx[6] = 0x00;
	tx[7] = 0x01;
	tx[8] = 0x00;
	tx[9] = 0x00;

	zephyr2_makeU16Sum(tx+2, 8);
	zephyr2_txrx(_z2, NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	printk("zephyr2: execute packet sent\r\n");
}

static int makeBootloaderDataPacket(u8* output, u32 destAddress, const u8* data, int dataLen, int* cksumOut)
{
	u32 checksum;
	int i;

	// This seems to be middle-endian! I've never seen this before.

	output[0] = 0x30;
	output[1] = 0x01;
	zephyr2_makeU16(output+2, dataLen >> 2);
	zephyr2_makeU32(output+4, destAddress);
	zephyr2_makeU16Sum(output+2, 6);

	for(i = 0; i < dataLen; i += 4)
	{
		output[10 + i + 0] = data[i + 1];
		output[10 + i + 1] = data[i + 0];
		output[10 + i + 2] = data[i + 3];
		output[10 + i + 3] = data[i + 2];
	}

	checksum = zephyr2_makeU32Sum(output+10, dataLen);

	if(cksumOut)
		*cksumOut = checksum;

	return dataLen;
}

static void zephyr2_irq_work(struct work_struct* work)
{
	struct zephyr2_data *z2 = container_of(work, struct zephyr2_data, irq_work);
	unsigned long flags;

	dev_dbg(&z2->spi_dev->dev, "irq entered (%d).\n", z2->irq_count);

	spin_lock_irqsave(&z2->irq_lock, flags);
	
	z2->irq_count++;

	if(z2->irq_count > 1)
	{
		spin_unlock_irqrestore(&z2->irq_lock, flags);
		return;
	}

	while(z2->irq_count > 0)
	{
		spin_unlock_irqrestore(&z2->irq_lock, flags);

		zephyr2_readFrame(z2);

		spin_lock_irqsave(&z2->irq_lock, flags);
		z2->irq_count--;
	}
	spin_unlock_irqrestore(&z2->irq_lock, flags);
	
	dev_dbg(&z2->spi_dev->dev, "irq exited (%d).\n", z2->irq_count);
}

static irqreturn_t zephyr2_irq(int irq, void* pToken)
{
	struct zephyr2_data *z2 = (struct zephyr2_data *)pToken;

	dev_dbg(&z2->spi_dev->dev, "irq.\n");
	schedule_work(&z2->irq_work);
	return IRQ_HANDLED;
}

static int zephyr2_setup(struct zephyr2_data *_z2)
{
	int i;
	int ret;
	int err;
	u8* reportBuffer;
	int reportLen;

	struct zephyr2_platform_data *pdata = _z2->spi_dev->dev.platform_data;

	if(!pdata->calibration)
	{
		printk("zephyr2: could not find calibration data\n");
		return -1;
	}

	// Power up the device (turn it off then on again. ;])
	printk("zephyr2: Powering Up Multitouch!\n");
	pdata->power(pdata, 0);
	msleep(200);
	pdata->power(pdata, 0);
	msleep(15);

	gpio_direction_output(pdata->reset_gpio, 0);
	for(i = 0; i < 4; ++i)
	{
		gpio_set_value(pdata->reset_gpio, 0);
		msleep(200);
		gpio_set_value(pdata->reset_gpio, 1);
		msleep(15);

		// Send Firmware
		printk("zephyr2: Sending Firmware...\n");
		if(loadConstructedFirmware(_z2, _z2->firmware->data, _z2->firmware->size))
			break;
	}

	if(i == 4)
	{
			printk("zephyr2: could not load preconstructed firmware\n");
			err = -1;
			kfree(_z2->InputPacket);
			kfree(_z2->OutputPacket);
			kfree(_z2->GetInfoPacket);
			kfree(_z2->GetResultPacket);
			return err;
	}

	printk("zephyr2: loaded %d byte preconstructed firmware\n", _z2->firmware->size);

	if(pdata->prox_cal)
	{
		if(!loadProxCal(_z2, pdata->prox_cal, pdata->prox_cal_size))
		{
			printk("zephyr2: could not load proximity calibration data\n");
			err = -1;
			kfree(_z2->InputPacket);
			kfree(_z2->OutputPacket);
			kfree(_z2->GetInfoPacket);
			kfree(_z2->GetResultPacket);
			return err;
		}

		printk("zephyr2: loaded %d byte proximity calibration data\n", pdata->prox_cal_size);
	}

	if(!loadCal(_z2, pdata->calibration, pdata->calibration_size))
	{
		printk("zephyr2: could not load calibration data\n");
		err = -1;
		kfree(_z2->InputPacket);
		kfree(_z2->OutputPacket);
		kfree(_z2->GetInfoPacket);
		kfree(_z2->GetResultPacket);
		return err;
	}


	printk("zephyr2: loaded %d byte calibration data\n", pdata->calibration_size);

	if(!zephyr2_calibrate(_z2))
	{
		err = -1;
		kfree(_z2->InputPacket);
		kfree(_z2->OutputPacket);
		kfree(_z2->GetInfoPacket);
		kfree(_z2->GetResultPacket);
		return err;
	}

	sendExecutePacket(_z2);

	msleep(1);

	printk("zephyr2: Determining interface version...\n");
	if(!determineInterfaceVersion(_z2))
	{
		err = -1;
		kfree(_z2->InputPacket);
		kfree(_z2->OutputPacket);
		kfree(_z2->GetInfoPacket);
		kfree(_z2->GetResultPacket);
		return err;
	}

	reportBuffer = (u8*) kmalloc(_z2->MaxPacketSize, GFP_KERNEL);

	if(!getReport(_z2, MT_INFO_FAMILYID, reportBuffer, &reportLen))
	{
		printk("zephyr2: failed getting family id!\n");
		err = -1;
		kfree(reportBuffer);
		kfree(_z2->InputPacket);
		kfree(_z2->OutputPacket);
		kfree(_z2->GetInfoPacket);
		kfree(_z2->GetResultPacket);
		return err;
	}

	_z2->FamilyID = reportBuffer[0];

	if(!getReport(_z2, MT_INFO_SENSORINFO, reportBuffer, &reportLen))
	{
		printk("zephyr2: failed getting sensor info!\r\n");
		err = -1;
		kfree(reportBuffer);
		kfree(_z2->InputPacket);
		kfree(_z2->OutputPacket);
		kfree(_z2->GetInfoPacket);
		kfree(_z2->GetResultPacket);
		return err;
	}

	_z2->SensorColumns = reportBuffer[2];
	_z2->SensorRows = reportBuffer[1];
	_z2->BCDVersion = ((reportBuffer[3] & 0xFF) << 8) | (reportBuffer[4] & 0xFF);
	_z2->Endianness = reportBuffer[0];


	if(!getReport(_z2, MT_INFO_SENSORREGIONDESC, reportBuffer, &reportLen))
	{
		printk("zephyr2: failed getting sensor region descriptor!\r\n");
		err = -1;
		kfree(reportBuffer);
		kfree(_z2->InputPacket);
		kfree(_z2->OutputPacket);
		kfree(_z2->GetInfoPacket);
		kfree(_z2->GetResultPacket);
		return err;
	}


	_z2->SensorRegionDescriptorLen = reportLen;
	_z2->SensorRegionDescriptor = (u8*) kmalloc(reportLen, GFP_KERNEL);
	memcpy(_z2->SensorRegionDescriptor, reportBuffer, reportLen);

	if(!getReport(_z2, MT_INFO_SENSORREGIONPARAM, reportBuffer, &reportLen))
	{
		printk("zephyr2: failed getting sensor region param!\r\n");
		err = -1;
		kfree(_z2->SensorRegionDescriptor);
		kfree(reportBuffer);
		kfree(_z2->InputPacket);
		kfree(_z2->OutputPacket);
		kfree(_z2->GetInfoPacket);
		kfree(_z2->GetResultPacket);
		return err;
	}


	_z2->SensorRegionParamLen = reportLen;
	_z2->SensorRegionParam = (u8*) kmalloc(reportLen, GFP_KERNEL);
	memcpy(_z2->SensorRegionParam, reportBuffer, reportLen);

	if(!getReport(_z2, MT_INFO_SENSORDIM, reportBuffer, &reportLen))
	{
		printk("zephyr2: failed getting sensor surface dimensions!\r\n");
		err = -1;
		kfree(_z2->SensorRegionParam);
		kfree(_z2->SensorRegionDescriptor);
		kfree(reportBuffer);
		kfree(_z2->InputPacket);
		kfree(_z2->OutputPacket);
		kfree(_z2->GetInfoPacket);
		kfree(_z2->GetResultPacket);
		return err;
	}


	_z2->SensorWidth = (9000 - *((u32*)&reportBuffer[0])) * 84 / 73;
	_z2->SensorHeight = (13850 - *((u32*)&reportBuffer[4])) * 84 / 73;

	printk("Family ID                : 0x%x\n", _z2->FamilyID);
	printk("Sensor rows              : 0x%x\n", _z2->SensorRows);
	printk("Sensor columns           : 0x%x\n", _z2->SensorColumns);
	printk("Sensor width             : 0x%x\n", _z2->SensorWidth);
	printk("Sensor height            : 0x%x\n", _z2->SensorHeight);
	printk("BCD Version              : 0x%x\n", _z2->BCDVersion);
	printk("Endianness               : 0x%x\n", _z2->Endianness);
	printk("Sensor region descriptor :");

	for(i = 0; i < _z2->SensorRegionDescriptorLen; ++i)
		printk(" %02x", _z2->SensorRegionDescriptor[i]);
	printk("\n");

	printk("Sensor region param      :");
	for(i = 0; i < _z2->SensorRegionParamLen; ++i)
		printk(" %02x", _z2->SensorRegionParam[i]);
	printk("\n");

	if(_z2->BCDVersion > 0x23)
		_z2->FlipNOP = true;
	else
		_z2->FlipNOP = false;

	kfree(reportBuffer);


	_z2->input_dev = input_allocate_device();
	if(!_z2->input_dev)
	{
		err = -1;
		kfree(_z2->SensorRegionParam);
		kfree(_z2->SensorRegionDescriptor);
		kfree(_z2->InputPacket);
		kfree(_z2->OutputPacket);
		kfree(_z2->GetInfoPacket);
		kfree(_z2->GetResultPacket);
		return err;
	}

	_z2->input_dev->name = "iPhone Zephyr 2 Multitouch Screen";
	_z2->input_dev->phys = "multitouch0";
	_z2->input_dev->id.vendor = 0x05AC;
	_z2->input_dev->id.product = 0;
	_z2->input_dev->id.version = 0x0000;
	_z2->input_dev->dev.parent = &_z2->spi_dev->dev;

	__set_bit(EV_KEY, _z2->input_dev->evbit);
	__set_bit(EV_ABS, _z2->input_dev->evbit);
	__set_bit(BTN_TOUCH, _z2->input_dev->keybit);
	__set_bit(BTN_TOOL_FINGER, _z2->input_dev->keybit);
	__set_bit(BTN_TOOL_DOUBLETAP, _z2->input_dev->keybit);
	__set_bit(BTN_TOOL_TRIPLETAP, _z2->input_dev->keybit);
	__set_bit(BTN_TOOL_QUADTAP, _z2->input_dev->keybit);

	input_set_abs_params(_z2->input_dev, ABS_X, 0, _z2->SensorWidth, 0, 0);
	input_set_abs_params(_z2->input_dev, ABS_Y, 0, _z2->SensorHeight, 0, 0);
	input_set_abs_params(_z2->input_dev, ABS_MT_TOUCH_MAJOR, 0, max(_z2->SensorHeight, _z2->SensorWidth), 0, 0);
	input_set_abs_params(_z2->input_dev, ABS_MT_TOUCH_MINOR, 0, max(_z2->SensorHeight, _z2->SensorWidth), 0, 0);
	input_set_abs_params(_z2->input_dev, ABS_MT_WIDTH_MAJOR, 0, max(_z2->SensorHeight, _z2->SensorWidth), 0, 0);
	input_set_abs_params(_z2->input_dev, ABS_MT_WIDTH_MINOR, 0, max(_z2->SensorHeight, _z2->SensorWidth), 0, 0);
	input_set_abs_params(_z2->input_dev, ABS_MT_ORIENTATION, -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0, 0);
	input_set_abs_params(_z2->input_dev, ABS_MT_POSITION_X, 0, _z2->SensorWidth, 0, 0);
	input_set_abs_params(_z2->input_dev, ABS_MT_POSITION_Y, 0, _z2->SensorHeight, 0, 0);

	/* not sure what the actual max is */
	input_set_abs_params(_z2->input_dev, ABS_MT_TRACKING_ID, 0, 32, 0, 0);

	ret = input_register_device(_z2->input_dev);
	if(ret != 0)
	{
		err = -1;
		kfree(_z2->SensorRegionParam);
		kfree(_z2->SensorRegionDescriptor);
		kfree(_z2->InputPacket);
		kfree(_z2->OutputPacket);
		kfree(_z2->GetInfoPacket);
		kfree(_z2->GetResultPacket);
		return err;
	}

	_z2->CurNOP = 1;

	zephyr2_readFrame(_z2);

	return 0;
}

// Device Attributes
static ssize_t zephyr2_min_pressure_show(struct device *_dev, struct device_attribute *_attr, char *_buf)
{
	struct spi_device *spi_dev = container_of(_dev, struct spi_device, dev);
	struct zephyr2_data *z2 = (struct zephyr2_data*)spi_get_drvdata(spi_dev);
	
	return sprintf(_buf, "%u\n", (unsigned int)z2->min_pressure);
}

static ssize_t zephyr2_min_pressure_store(struct device *_dev, struct device_attribute *_attr, const char* _buf, size_t _count)
{
	struct spi_device *spi_dev = container_of(_dev, struct spi_device, dev);
	struct zephyr2_data *z2 = (struct zephyr2_data*)spi_get_drvdata(spi_dev);
	
	unsigned int new_val;
	ssize_t ret = sscanf(_buf, "%u\n", &new_val);
	
	if(ret <= 0)
		return ret;

	if(new_val >= 255)
		return 0;

	z2->min_pressure = (u8)new_val;

	return ret;
}

static DEVICE_ATTR(min_pressure, 0666, &zephyr2_min_pressure_show, &zephyr2_min_pressure_store);

static int zephyr2_probe(struct spi_device *_dev)
{
	struct zephyr2_data *z2 = kzalloc(sizeof(struct zephyr2_data), GFP_KERNEL);
	struct zephyr2_platform_data *pdata = _dev->dev.platform_data;
	int ret;

	_dev->bits_per_word = 8;
	ret = spi_setup(_dev);
	if(ret)
	{
		dev_err(&_dev->dev, "failed to setup SPI device.\n");
		return ret;
	}

	z2 = kzalloc(sizeof(struct zephyr2_data), GFP_KERNEL);
	if(!z2)
	{
		dev_err(&_dev->dev, "failed to allocate state.\n");
		ret = -ENOMEM;
		goto exit;
	}

	INIT_WORK(&z2->irq_work, &zephyr2_irq_work);
	spin_lock_init(&z2->irq_lock);

	z2->spi_dev = _dev;
	z2->min_pressure = 100;
	z2->irq_count = 0;

	ret = device_create_file(&_dev->dev, &dev_attr_min_pressure);
	if(ret)
	{
		dev_err(&_dev->dev, "failed to create min_pressure attribute.\n");
		goto err_state;
	}

	ret = request_firmware(&z2->firmware, "zephyr2.bin", &_dev->dev);
	if(ret < 0)
	{
		dev_err(&_dev->dev, "failed to retrieve firmware.\n");
		goto err_state;
	}

	ret = gpio_request(pdata->reset_gpio, "zephyr2-reset");
	if(ret < 0)
	{
		dev_err(&_dev->dev, "failed to acquire zephyr2 reset GPIO.\n");
		goto err_fw;
	}

	ret = gpio_request(pdata->touch_gpio, "zephyr2-touch");
	if(ret < 0)
	{
		dev_err(&_dev->dev, "failed to acquire zephyr2 touch GPIO.\n");
		goto err_rst;
	}

	gpio_direction_input(pdata->touch_gpio);
	z2->irq = gpio_to_irq(pdata->touch_gpio);
	ret = request_irq(z2->irq, zephyr2_irq, IRQF_TRIGGER_HIGH, "zephyr2", z2);
	if(ret < 0)
	{
		dev_err(&_dev->dev, "failed to request GPIO irq for touch.\n");
		goto err_touch;
	}

	ret = zephyr2_setup(z2);
	if(ret < 0)
	{
		dev_err(&_dev->dev, "failed to setup zephyr2!\n");
		goto err_irq;
	}

	spi_set_drvdata(_dev, z2);
	goto exit;

err_irq:
	free_irq(z2->irq, z2);

err_touch:
	gpio_free(pdata->touch_gpio);

err_rst:
	gpio_free(pdata->reset_gpio);

err_fw:
	release_firmware(z2->firmware);

err_state:
	kfree(z2);

exit:
	return ret;
}

static int zephyr2_remove(struct spi_device *_dev)
{
	struct zephyr2_data *z2 = spi_get_drvdata(_dev);
	struct zephyr2_platform_data *pdata = _dev->dev.platform_data;

	device_remove_file(&_dev->dev, &dev_attr_min_pressure);

	pdata->power(pdata, 0);
	
	free_irq(z2->irq, z2);
	release_firmware(z2->firmware);
	gpio_free(pdata->touch_gpio);
	gpio_free(pdata->reset_gpio);
	
	kfree(z2);	
	return 0;
}

// TODO: power management -- Ricky26
#define zephyr2_shutdown NULL
#define zephyr2_suspend NULL
#define zephyr2_resume NULL

static const struct spi_device_id zephyr2_ids[] = {
	{"zephyr2", 0},
	{},
};

struct spi_driver zephyr2_driver = {
	.driver = {
		.name = "zephyr2",
	},

	.id_table = zephyr2_ids,
	.probe = zephyr2_probe,
	.remove = zephyr2_remove,
	.shutdown = zephyr2_shutdown,
	.suspend = zephyr2_suspend,
	.resume = zephyr2_resume,
};

static int __init zephyr2_init(void)
{
	int ret;
	ret = spi_register_driver(&zephyr2_driver);
	if(ret)
		printk("zephyr2: failed to register driver.\n");
	return ret;
}
module_init(zephyr2_init);

static void __exit zephyr2_exit(void)
{
	spi_unregister_driver(&zephyr2_driver);
}
module_exit(zephyr2_exit);

MODULE_DESCRIPTION("iPhone Zephyr 2 Multitouch Driver");
MODULE_AUTHOR("Yiduo Wang");
MODULE_LICENSE("GPL");

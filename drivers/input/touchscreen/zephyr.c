/*
 * iphone-mt.c - Zephyr Touchscreen driver, used in iPhone.
 *
 * Authors: Yidou Wang, Patrick Wildt, Ricky Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <mach/iphone-spi.h>
#include <mach/gpio.h>

#ifdef CONFIG_IPHONE_2G
#define MT_GPIO_POWER 0x804
#define MT_ATN_INTERRUPT 0xa3
#else
#define MT_GPIO_POWER 0x701
#define MT_ATN_INTERRUPT 0x9b
#endif

#ifdef CONFIG_IPHONE_3G
#define MT_SPI 1
#define MT_SPI_CS GPIO_SPI1_CS0
#else
#define MT_SPI 2
#define MT_SPI_CS GPIO_SPI2_CS0
#endif

#define MT_INFO_FAMILYID 0xD1
#define MT_INFO_SENSORINFO 0xD3
#define MT_INFO_SENSORREGIONDESC 0xD0
#define MT_INFO_SENSORREGIONPARAM 0xA1
#define MT_INFO_SENSORDIM 0xD9

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
} MTFrameHeader;

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
} FingerData;

#define MAX_FINGER_ORIENTATION  16384

static irqreturn_t multitouch_atn(int irq, void* pToken);

volatile int GotATN;
spinlock_t GotATNLock;

static u8* OutputPacket;
static u8* InputPacket;
static u8* GetInfoPacket;
static u8* GetResultPacket;

static int InterfaceVersion;
static int MaxPacketSize;
static int FamilyID;
static int SensorWidth;
static int SensorHeight;
static int SensorColumns;
static int SensorRows;
static int BCDVersion;
static int Endianness;
static u8* SensorRegionDescriptor;
static int SensorRegionDescriptorLen;
static u8* SensorRegionParam;
static int SensorRegionParamLen;

static u8 SensorMinPressure = 100;

// This is flipped between 0x64 and 0x65 for every transaction
static int CurNOP;

typedef struct MTSPISetting
{
	int speed;
	int txDelay;
	int rxDelay;
} MTSPISetting;

const MTSPISetting MTNormalSpeed = {83000, 5, 10};
const MTSPISetting MTFastSpeed = {4500000, 0, 10};

#define NORMAL_SPEED (&MTNormalSpeed)
#define FAST_SPEED (&MTFastSpeed)

static int mt_spi_txrx(const MTSPISetting* setting, const u8* outBuffer, int outLen, u8* inBuffer, int inLen);
static int mt_spi_tx(const MTSPISetting* setting, const u8* outBuffer, int outLen);

static int makeBootloaderDataPacket(u8* output, u32 destAddress, const u8* data, int dataLen, int* cksumOut);
static bool verifyUpload(int checksum);
static void sendExecutePacket(void);
static void sendBlankDataPacket(void);

static bool loadASpeedFirmware(const u8* firmware, int len);
static bool loadMainFirmware(const u8* firmware, int len);
static bool determineInterfaceVersion(void);

static bool getReportInfo(int id, u8* err, u16* len);
static bool getReport(int id, u8* buffer, int* outLen);

static bool readFrameLength(int* len);
static int readFrame(void);
static bool readResultData(int len);

static void newPacket(const u8* data, int len);
static void multitouch_atn_handler(struct work_struct* work);

bool MultitouchOn = false;
bool FirmwareLoaded = false;

static struct device* multitouch_dev = NULL;
struct input_dev* input_dev;

u8* aspeed_fw;
size_t aspeed_fw_size;
u8* main_fw;
size_t main_fw_size;

DECLARE_WORK(multitouch_workqueue, &multitouch_atn_handler);

void multitouch_on(void)
{
	if(!MultitouchOn)
	{
		printk("multitouch: powering on\n");
		iphone_gpio_pin_output(MT_GPIO_POWER, 0);
		msleep(200);
		iphone_gpio_pin_output(MT_GPIO_POWER, 1);

		msleep(15);
		MultitouchOn = true;
	}
}

int multitouch_setup(const u8* ASpeedFirmware, int ASpeedFirmwareLen, const u8* mainFirmware, int mainFirmwareLen)
{
	int i;
	int ret;

	printk("multitouch: A-Speed firmware at 0x%08x - 0x%08x, Main firmware at 0x%08x - 0x%08x\n",
			(u32) ASpeedFirmware, (u32)(ASpeedFirmware + ASpeedFirmwareLen),
			(u32) mainFirmware, (u32)(mainFirmware + mainFirmwareLen));

	OutputPacket = (u8*) kmalloc(0x400, GFP_KERNEL);
	InputPacket = (u8*) kmalloc(0x400, GFP_KERNEL);
	GetInfoPacket = (u8*) kmalloc(0x400, GFP_KERNEL);
	GetResultPacket = (u8*) kmalloc(0x400, GFP_KERNEL);

	memset(GetInfoPacket, 0x82, 0x400);
	memset(GetResultPacket, 0x68, 0x400);

	request_irq(MT_ATN_INTERRUPT + IPHONE_GPIO_IRQS, multitouch_atn, IRQF_TRIGGER_FALLING, "iphone-multitouch", (void*) 0);

	multitouch_on();

	printk("multitouch: Sending A-Speed firmware...\n");
	if(!loadASpeedFirmware(ASpeedFirmware, ASpeedFirmwareLen))
	{
		kfree(InputPacket);
		kfree(OutputPacket);
		kfree(GetInfoPacket);
		kfree(GetResultPacket);
		return -1;
	}

	msleep(1);

	printk("multitouch: Sending main firmware...\n");
	if(!loadMainFirmware(mainFirmware, mainFirmwareLen))
	{
		kfree(InputPacket);
		kfree(OutputPacket);
		kfree(GetInfoPacket);
		kfree(GetResultPacket);
		return -1;
	}

	msleep(1);

	printk("multitouch: Determining interface version...\n");
	if(!determineInterfaceVersion())
	{
		kfree(InputPacket);
		kfree(OutputPacket);
		kfree(GetInfoPacket);
		kfree(GetResultPacket);
		return -1;
	}

	{
		u8 reportBuffer[MaxPacketSize];
		int reportLen;

		if(!getReport(MT_INFO_FAMILYID, reportBuffer, &reportLen))
		{
			printk("multitouch: failed getting family id!\n");
			kfree(InputPacket);
			kfree(OutputPacket);
			kfree(GetInfoPacket);
			kfree(GetResultPacket);
			return -1;
		}

		FamilyID = reportBuffer[0];

		if(!getReport(MT_INFO_SENSORINFO, reportBuffer, &reportLen))
		{
			printk("multitouch: failed getting sensor info!\n");
			kfree(InputPacket);
			kfree(OutputPacket);
			kfree(GetInfoPacket);
			kfree(GetResultPacket);
			return -1;
		}

		SensorColumns = reportBuffer[2];
		SensorRows = reportBuffer[1];
		BCDVersion = ((reportBuffer[3] & 0xFF) << 8) | (reportBuffer[4] & 0xFF);
		Endianness = reportBuffer[0];

		if(!getReport(MT_INFO_SENSORREGIONDESC, reportBuffer, &reportLen))
		{
			printk("multitouch: failed getting sensor region descriptor!\n");
			kfree(InputPacket);
			kfree(OutputPacket);
			kfree(GetInfoPacket);
			kfree(GetResultPacket);
			return -1;
		}

		SensorRegionDescriptorLen = reportLen;
		SensorRegionDescriptor = (u8*) kmalloc(reportLen, GFP_KERNEL);
		memcpy(SensorRegionDescriptor, reportBuffer, reportLen);

		if(!getReport(MT_INFO_SENSORREGIONPARAM, reportBuffer, &reportLen))
		{
			printk("multitouch: failed getting sensor region param!\n");
			kfree(InputPacket);
			kfree(OutputPacket);
			kfree(GetInfoPacket);
			kfree(GetResultPacket);
			kfree(SensorRegionDescriptor);
			return -1;
		}

		SensorRegionParamLen = reportLen;
		SensorRegionParam = (u8*) kmalloc(reportLen, GFP_KERNEL);
		memcpy(SensorRegionParam, reportBuffer, reportLen);

		if(!getReport(MT_INFO_SENSORDIM, reportBuffer, &reportLen))
		{
			printk("multitouch: failed getting sensor surface dimensions!\n");
			kfree(InputPacket);
			kfree(OutputPacket);
			kfree(GetInfoPacket);
			kfree(GetResultPacket);
			kfree(SensorRegionDescriptor);
			kfree(SensorRegionParam);
			return -1;
		}

		SensorWidth = (9000 - *((u32*)&reportBuffer[0])) * 84 / 73;
		SensorHeight = (13850 - *((u32*)&reportBuffer[4])) * 84 / 73;
	}

	printk("Family ID                : 0x%x\n", FamilyID);
	printk("Sensor rows              : 0x%x\n", SensorRows);
	printk("Sensor columns           : 0x%x\n", SensorColumns);
	printk("Sensor width             : 0x%x\n", SensorWidth);
	printk("Sensor height            : 0x%x\n", SensorHeight);
	printk("BCD Version              : 0x%x\n", BCDVersion);
	printk("Endianness               : 0x%x\n", Endianness);
	printk("Sensor region descriptor :");
	for(i = 0; i < SensorRegionDescriptorLen; ++i)
		printk(" %02x", SensorRegionDescriptor[i]);
	printk("\n");

	printk("Sensor region param      :");
	for(i = 0; i < SensorRegionParamLen; ++i)
		printk(" %02x", SensorRegionParam[i]);
	printk("\n");

	input_dev = input_allocate_device();
	if(!input_dev)
	{
		kfree(InputPacket);
		kfree(OutputPacket);
		kfree(GetInfoPacket);
		kfree(GetResultPacket);
		kfree(SensorRegionDescriptor);
		kfree(SensorRegionParam);
		return -1;
	}


	input_dev->name = "iPhone Zephyr Multitouch Screen";
	input_dev->phys = "multitouch0";
	input_dev->id.vendor = 0x05AC;
	input_dev->id.product = 0;
	input_dev->id.version = 0x0000;
	input_dev->dev.parent = multitouch_dev;
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(input_dev, ABS_X, 0, SensorWidth, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, SensorHeight, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, max(SensorHeight, SensorWidth), 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, max(SensorHeight, SensorWidth), 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, max(SensorHeight, SensorWidth), 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MINOR, 0, max(SensorHeight, SensorWidth), 0, 0);
	input_set_abs_params(input_dev, ABS_MT_ORIENTATION, -MAX_FINGER_ORIENTATION, MAX_FINGER_ORIENTATION, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, SensorWidth, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, SensorHeight, 0, 0);

	/* not sure what the actual max is */
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 32, 0, 0);

	ret = input_register_device(input_dev);
	if(ret != 0)
	{
		kfree(InputPacket);
		kfree(OutputPacket);
		kfree(GetInfoPacket);
		kfree(GetResultPacket);
		kfree(SensorRegionDescriptor);
		kfree(SensorRegionParam);
		return -1;
	}

	CurNOP = 0x64;
	GotATN = 0;
	spin_lock_init(&GotATNLock);

	FirmwareLoaded = true;

	readFrame();

	return 0;
}

static void multitouch_atn_handler(struct work_struct* work)
{
	unsigned long flags;

	if(!FirmwareLoaded)
		return;

	spin_lock_irqsave(&GotATNLock, flags);
	while(GotATN)
	{
		--GotATN;
		spin_unlock_irqrestore(&GotATNLock, flags);
		while(readFrame() == 1);
		spin_lock_irqsave(&GotATNLock, flags);
	}
	spin_unlock_irqrestore(&GotATNLock, flags);
}

static void newPacket(const u8* data, int len)
{
	int i;
	FingerData* finger;
	MTFrameHeader* header = (MTFrameHeader*) data;
	if(header->type != 0x44 && header->type != 0x43)
		printk("multitouch: unknown frame type 0x%x\n", header->type);

	finger = (FingerData*)(data + (header->headerLen));

	if(header->headerLen < 12)
		printk("multitouch: no finger data in frame\n");

	for(i = 0; i < header->numFingers; ++i)
	{
		if(finger->force_major > SensorMinPressure)
		{
			finger->force_major -= SensorMinPressure;
		}
		else
			finger->force_major = 0;

		if(finger->force_minor > SensorMinPressure)
		{
			finger->force_minor -= SensorMinPressure;
		}
		else 
			finger->force_minor = 0;

		if(finger->force_major > 0 || 
				finger->force_minor > 0)
		{
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, finger->force_major);
			input_report_abs(input_dev, ABS_MT_TOUCH_MINOR, finger->force_minor);
			input_report_abs(input_dev, ABS_MT_WIDTH_MAJOR, finger->size_major);
			input_report_abs(input_dev, ABS_MT_WIDTH_MINOR, finger->size_minor);
			input_report_abs(input_dev, ABS_MT_ORIENTATION, MAX_FINGER_ORIENTATION - finger->orientation);
			input_report_abs(input_dev, ABS_MT_TRACKING_ID, finger->id);
			input_report_abs(input_dev, ABS_MT_POSITION_X, finger->x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, SensorHeight - finger->y);
		}

		input_mt_sync(input_dev);

		finger = (FingerData*) (((u8*) finger) + header->fingerDataLen);
	}

	if(header->numFingers > 0)
	{
		finger = (FingerData*)(data + (header->headerLen));

		if (finger->force_minor > 0)
		{
			input_report_abs(input_dev, ABS_X, finger->x);
			input_report_abs(input_dev, ABS_Y, SensorHeight - finger->y);
			input_report_key(input_dev, BTN_TOUCH, finger->size_minor > 0);
		}
		else input_report_key(input_dev, BTN_TOUCH, 0);
	}

	input_sync(input_dev);
}

static int readFrame()
{
	int try = 0;

	for(try = 0; try < 4; ++try)
	{
		int len = 0;
		if(!readFrameLength(&len))
		{
			printk("multitouch: error getting frame length\n");
			msleep(1);
			continue;
		}

		if(len)
		{
			if(!readResultData(len + 1))
			{
				printk("multitouch: error getting frame data\n");
				msleep(1);
				continue;
			}

			if(CurNOP == 0x64)
				CurNOP = 0x65;
			else
				CurNOP = 0x64;

			return 1;
		}

		return 0;
	}

	return -1;
}

static bool readResultData(int len)
{
	int try = 0;
	for(try = 0; try < 4; ++try)
	{
		int checksum;
		int myChecksum;
		int i;

		mt_spi_txrx(NORMAL_SPEED, GetResultPacket, len, InputPacket, len);

		if(InputPacket[0] != 0xAA)
		{
			msleep(1);
			continue;
		}

		checksum = ((InputPacket[len - 2] & 0xFF) << 8) | (InputPacket[len - 1] & 0xFF);
		myChecksum = 0;

		for(i = 1; i < (len - 2); ++i)
			myChecksum += InputPacket[i];

		myChecksum &= 0xFFFF;

		if(myChecksum != checksum)
		{
			msleep(1);
			continue;
		}

		newPacket(InputPacket + 1, len - 3);
		return true;
	}

	return false;

}

static bool readFrameLength(int* len)
{
	int try;
	u8 tx[8];
	u8 rx[8];
	memset(tx, CurNOP, sizeof(tx));

	try = 0;
	for(try = 0; try < 4; ++try)
	{
		int tLen;
		int tLenCkSum;
		int checksum;

		mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		if(rx[0] != 0xAA)
		{
			msleep(1);
			continue;
		}

		tLen = ((rx[4] & 0xFF) << 8) | (rx[5] & 0xFF);
		tLenCkSum = (rx[4] + rx[5]) & 0xFFFF;
		checksum = ((rx[6] & 0xFF) << 8) | (rx[7] & 0xFF);
		if(tLenCkSum != checksum)
		{
			msleep(1);
			continue;
		}

		if(tLen > MaxPacketSize)
		{
			printk("multitouch: device unexpectedly requested to transfer a %d byte packet. Max size = %d\n", tLen, MaxPacketSize);
			msleep(1);
			continue;
		}

		*len = tLen;

		return true;
	}

	return false;
}

static bool getReport(int id, u8* buffer, int* outLen)
{
	u8 err;
	u16 len;
	int try;
	if(!getReportInfo(id, &err, &len))
		return false;

	if(err)
		return false;

	try = 0;
	for(try = 0; try < 4; ++try)
	{
		int checksum;
		int myChecksum;
		int i;

		GetInfoPacket[1] = id;
		mt_spi_txrx(NORMAL_SPEED, GetInfoPacket, len + 6, InputPacket, len + 6);

		if(InputPacket[0] != 0xAA)
		{
			msleep(1);
			continue;
		}

		checksum = ((InputPacket[len + 4] & 0xFF) << 8) | (InputPacket[len + 5] & 0xFF);
		myChecksum = id;

		for(i = 0; i < len; ++i)
			myChecksum += InputPacket[i + 4];

		myChecksum &= 0xFFFF;

		if(myChecksum != checksum)
		{
			msleep(1);
			continue;
		}

		*outLen = len;
		memcpy(buffer, &InputPacket[4], len);

		return true;
	}

	return false;
}

static bool getReportInfo(int id, u8* err, u16* len)
{
	u8 tx[8];
	u8 rx[8];

	int try;
	for(try = 0; try < 4; ++try)
	{
		int checksum;
		int myChecksum;

		memset(tx, 0x8F, 8);
		tx[1] = id;

		mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		if(rx[0] != 0xAA)
		{
			msleep(1);
			continue;
		}

		checksum = ((rx[6] & 0xFF) << 8) | (rx[7] & 0xFF);
		myChecksum = (id + rx[4] + rx[5]) & 0xFFFF;

		if(checksum != myChecksum)
		{
			msleep(1);
			continue;
		}

		*err = (rx[4] >> 4) & 0xF;
		*len = ((rx[4] & 0xF) << 8) | (rx[5] & 0xFF);

		return true;
	}

	return false;
}

static bool determineInterfaceVersion()
{
	u8 tx[4];
	u8 rx[4];

	int try;
	for(try = 0; try < 4; ++try)
	{
		memset(tx, 0xD0, 4);

		mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

		if(rx[0] == 0xAA)
		{
			InterfaceVersion = rx[1];
			MaxPacketSize = (rx[2] << 8) | rx[3];

			printk("multitouch: interface version %d, max packet size: %d\n", InterfaceVersion, MaxPacketSize);
			return true;
		}

		InterfaceVersion = 0;
		MaxPacketSize = 1000;
		msleep(3);
	}

	printk("multitouch: failed getting interface version!\n");

	return false;
}

static bool loadASpeedFirmware(const u8* firmware, int len)
{
	u32 address = 0x40000000;
	const u8* data = firmware;
	int left = len;

	while(left > 0)
	{
		int checksum;
		int try;
		int toUpload = left;
		if(toUpload > 0x3F8)
			toUpload = 0x3F8;

		makeBootloaderDataPacket(OutputPacket, address, data, toUpload, &checksum);

		for(try = 0; try < 5; ++try)
		{
			printk("multitouch: uploading data packet\n");
			mt_spi_tx(NORMAL_SPEED, OutputPacket, 0x400);

			udelay(300);

			if(verifyUpload(checksum))
				break;
		}

		if(try == 5)
			return false;

		address += toUpload;
		data += toUpload;
		left -= toUpload;
	}

	sendExecutePacket();

	return true;
}

static bool loadMainFirmware(const u8* firmware, int len)
{
	int checksum = 0;

	int i;
	for(i = 0; i < len; ++i)
		checksum += firmware[i];

	for(i = 0; i < 5; ++i)
	{
		sendBlankDataPacket();

		printk("multitouch: uploading main firmware\n");
		mt_spi_tx(FAST_SPEED, firmware, len);

		if(verifyUpload(checksum))
			break;
	}

	if(i == 5)
		return false;

	sendExecutePacket();

	return true;
}

static bool verifyUpload(int checksum)
{
	u8 tx[4];
	u8 rx[4];

	tx[0] = 5;
	tx[1] = 0;
	tx[2] = 0;
	tx[3] = 6;

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	if(rx[0] != 0xD0 || rx[1] != 0x0)
	{
		printk("multitouch: data verification failed type bytes, got %02x %02x %02x %02x -- %x\n", rx[0], rx[1], rx[2], rx[3], checksum);
		return false;
	}

	if(rx[2] != ((checksum >> 8) & 0xFF))
	{
		printk("multitouch: data verification failed upper checksum, %02x != %02x\n", rx[2], (checksum >> 8) & 0xFF);
		return false;
	}

	if(rx[3] != (checksum & 0xFF))
	{
		printk("multitouch: data verification failed lower checksum, %02x != %02x\n", rx[3], checksum & 0xFF);
		return false;
	}

	printk("multitouch: data verification successful\n");
	return true;
}


static void sendExecutePacket(void)
{
	u8 tx[4];
	u8 rx[4];

	tx[0] = 0xC4;
	tx[1] = 0;
	tx[2] = 0;
	tx[3] = 0xC4;

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	printk("multitouch: execute packet sent\n");
}

static void sendBlankDataPacket(void)
{
	u8 tx[4];
	u8 rx[4];

	tx[0] = 0xC2;
	tx[1] = 0;
	tx[2] = 0;
	tx[3] = 0;

	mt_spi_txrx(NORMAL_SPEED, tx, sizeof(tx), rx, sizeof(rx));

	printk("multitouch: blank data packet sent\n");
}

static int makeBootloaderDataPacket(u8* output, u32 destAddress, const u8* data, int dataLen, int* cksumOut)
{
	int checksum;
	int i;

	if(dataLen > 0x3F8)
		dataLen = 0x3F8;

	output[0] = 0xC2;
	output[1] = (destAddress >> 24) & 0xFF;
	output[2] = (destAddress >> 16) & 0xFF;
	output[3] = (destAddress >> 8) & 0xFF;
	output[4] = destAddress & 0xFF;
	output[5] = 0;

	checksum = 0;

	for(i = 0; i < dataLen; ++i)
	{
		u8 byte = data[i];
		checksum += byte;
		output[6 + i] = byte;
	}

	for(i = 0; i < 6; ++i)
	{
		checksum += output[i];
	}

	memset(output + dataLen + 6, 0, 0x3F8 - dataLen);
	output[0x3FE] = (checksum >> 8) & 0xFF;
	output[0x3FF] = checksum & 0xFF;

	*cksumOut = checksum;

	return dataLen;
}

static irqreturn_t multitouch_atn(int irq, void* pToken)
{
	unsigned long flags;

	if(!FirmwareLoaded)
		return IRQ_HANDLED;

	spin_lock_irqsave(&GotATNLock, flags);
	++GotATN;
	spin_unlock_irqrestore(&GotATNLock, flags);

	schedule_work(&multitouch_workqueue);
	return IRQ_HANDLED;
}


int mt_spi_tx(const MTSPISetting* setting, const u8* outBuffer, int outLen)
{
	int ret;
	iphone_spi_set_baud(MT_SPI, setting->speed, SPIOption13Setting0, 1, 1, 1);
	iphone_gpio_pin_output(MT_SPI_CS, 0);
	msleep(setting->txDelay);
	ret = iphone_spi_tx(MT_SPI, outBuffer, outLen, true, false);
	iphone_gpio_pin_output(MT_SPI_CS, 1);
	return ret;
}

int mt_spi_txrx(const MTSPISetting* setting, const u8* outBuffer, int outLen, u8* inBuffer, int inLen)
{
	int ret;
	iphone_spi_set_baud(MT_SPI, setting->speed, SPIOption13Setting0, 1, 1, 1);
	iphone_gpio_pin_output(MT_SPI_CS, 0);
	msleep(setting->rxDelay);
	ret = iphone_spi_txrx(MT_SPI, outBuffer, outLen, inBuffer, inLen, true);
	iphone_gpio_pin_output(MT_SPI_CS, 1);
	return ret;
}

static void got_main(const struct firmware* fw, void *context)
{
	if(!fw)
	{
		printk("multitouch: couldn't get main firmware, trying again...\n");
		request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, "zephyr_main.bin", multitouch_dev, NULL, got_main);
		return;
	}

	main_fw = kmalloc(fw->size, GFP_KERNEL);
	main_fw_size = fw->size;
	memcpy(main_fw, fw->data, fw->size);

	printk("multitouch: initializing multitouch\n");
	multitouch_setup(aspeed_fw, aspeed_fw_size, main_fw, main_fw_size);

	/* caller will call release_firmware */
}

static void got_aspeed(const struct firmware* fw, void *context)
{
	if(!fw)
	{
		printk("multitouch: couldn't get a-speed firmware, trying again...\n");
		request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, "zephyr_aspeed.bin", multitouch_dev, NULL, got_aspeed);
		return;
	}

	aspeed_fw = kmalloc(fw->size, GFP_KERNEL);
	aspeed_fw_size = fw->size;
	memcpy(aspeed_fw, fw->data, fw->size);

	printk("multitouch: requesting main firmware\n");
	request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, "zephyr_main.bin", multitouch_dev, NULL, got_main);

	/* caller will call release_firmware */
}

static int iphone_multitouch_probe(struct platform_device *pdev)
{
	/* this driver is such a hack */
	if(multitouch_dev)
		return -EBUSY;

	multitouch_dev = &pdev->dev;

	printk("multitouch: requesting A-Speed firmware\n");
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, "zephyr_aspeed.bin", multitouch_dev, NULL, got_aspeed);
}

static int iphone_multitouch_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver iphone_multitouch_driver = {
	.probe = iphone_multitouch_probe,
	.remove = iphone_multitouch_remove,
	.suspend = NULL, /* optional but recommended */
	.resume = NULL,   /* optional but recommended */
	.driver = {
		.owner = THIS_MODULE,
		.name = "iphone-multitouch",
	},
};

static struct platform_device iphone_multitouch_dev = {
	.name = "iphone-multitouch",
	.id = -1,
};

static int __init iphone_multitouch_init(void)
{
	int ret;

	ret = platform_driver_register(&iphone_multitouch_driver);

	if (!ret) {
		ret = platform_device_register(&iphone_multitouch_dev);

		if (ret != 0) {
			platform_driver_unregister(&iphone_multitouch_driver);
		}
	}
	return ret;
}

static void __exit iphone_multitouch_exit(void)
{
	platform_device_unregister(&iphone_multitouch_dev);
	platform_driver_unregister(&iphone_multitouch_driver);
}

module_init(iphone_multitouch_init);
module_exit(iphone_multitouch_exit);

MODULE_DESCRIPTION("iPhone Zephyr multitouch driver");
MODULE_AUTHOR("Yiduo Wang");
MODULE_LICENSE("GPL");

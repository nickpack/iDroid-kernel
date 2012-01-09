#ifndef I2C_H
#define I2C_H

typedef enum I2CError {
	I2CNoError = 0
} I2CError;

typedef enum I2CState {
	I2CDone = 0,
	I2CSetup = 1,
	I2CTx = 2,
	I2CRx = 3,
	I2CRxSetup = 4,
	I2CFinish = 5
} I2CState;


typedef struct I2CInfo {
	struct mutex* rw_sem;
	uint32_t frequency;
	I2CError error_code;
	int is_write;
	int cursor;
	int send_stop;
	I2CState state;
	int operation_result;
	uint32_t address;
	uint32_t iiccon_settings;
	uint32_t current_iicstat;
	int num_regs;
	const uint8_t* registers;
	int bufferLen;
	uint8_t* buffer;
	uint32_t iic_scl_gpio;
	uint32_t iic_sda_gpio;
	uint32_t register_IICCON;
	uint32_t register_IICSTAT;
	uint32_t register_IICADD;
	uint32_t register_IICDS;
	uint32_t register_IICLC;
	uint32_t register_14;
	uint32_t register_18;
	uint32_t register_1C;
	uint32_t register_20;
} I2CInfo;

extern struct platform_device iphone_i2c;

I2CError iphone_i2c_recv(int bus, int iicaddr, int send_stop, void* buffer, int len);
I2CError iphone_i2c_send(int bus, int iicaddr, int send_stop, const void* buffer, int len);

/* Legacy support */
I2CError iphone_i2c_rx(int bus, int iicaddr, const uint8_t* registers, int num_regs, void* buffer, int len);
I2CError iphone_i2c_tx(int bus, int iicaddr, void* buffer, int len);

#endif

#define KEYPAD 0


typedef struct {
    unsigned int value;
    unsigned int reg_num;
    unsigned int read;
} DT2_data;

#define DT2_IOCTL_MAGIC 'r'

#define DT2_RLY0_ON             _IO(DT2_IOCTL_MAGIC,0x01)
#define DT2_RLY0_OFF            _IO(DT2_IOCTL_MAGIC,0x02)
#define DT2_RLY1_ON             _IO(DT2_IOCTL_MAGIC,0x03)
#define DT2_RLY1_OFF            _IO(DT2_IOCTL_MAGIC,0x04)
#define DT2_MOTION_ON		_IO(DT2_IOCTL_MAGIC,0x05)
#define DT2_MOTION_OFF		_IO(DT2_IOCTL_MAGIC,0x06)
#define DT2_SENSOR0_ON		_IO(DT2_IOCTL_MAGIC,0x07)
#define DT2_SENSOR0_OFF		_IO(DT2_IOCTL_MAGIC,0x08)
#define DT2_SENSOR1_ON		_IO(DT2_IOCTL_MAGIC,0x09)
#define DT2_SENSOR1_OFF		_IO(DT2_IOCTL_MAGIC,0x0A)
#define DT2_RLY0_STATUS		_IOR(DT2_IOCTL_MAGIC,0x0B, DT2_data)
#define DT2_RLY1_STATUS		_IOR(DT2_IOCTL_MAGIC,0x0C, DT2_data)
#define DT2_LDR_ON		_IO(DT2_IOCTL_MAGIC,0x0D)
#define DT2_LDR_OFF		_IO(DT2_IOCTL_MAGIC,0x0E)
#define DT2_IR_ON		_IO(DT2_IOCTL_MAGIC,0x0F)
#define DT2_IR_OFF		_IO(DT2_IOCTL_MAGIC,0x10)
#define DT2_LDR_SENSE_LOW	_IOW(DT2_IOCTL_MAGIC,0x11, DT2_data)
#define DT2_LDR_SENSE_MEDIUM	_IOW(DT2_IOCTL_MAGIC,0x12, DT2_data)
#define DT2_LDR_SENSE_HIGH	_IOW(DT2_IOCTL_MAGIC,0x13, DT2_data)
#define DT2_IR_LEVEL1		_IO(DT2_IOCTL_MAGIC,0x14)
#define DT2_IR_LEVEL2		_IO(DT2_IOCTL_MAGIC,0x15)
#define DT2_IR_LEVEL3		_IO(DT2_IOCTL_MAGIC,0x16)
#define DT2_SENSOR0_SENSE	_IOW(DT2_IOCTL_MAGIC,0x17, DT2_data)
#define DT2_SENSOR1_SENSE	_IOW(DT2_IOCTL_MAGIC,0x18, DT2_data)
#define DT2_MOTION_SENSE	_IOW(DT2_IOCTL_MAGIC,0x19, DT2_data)

#define READ_REG_BYTE           0xAA
#define AUDIOSW_REG             0xBB
#define RELAY0_REG              0xBC
#define RELAY1_REG              0xBD
#define LED_LIGHT_REG           0XBE
#define ECHO_POWER_REG          0xC0
#define SENSOR0_REG             0xE1
#define SENSOR1_REG             0xE2
#define MOTION_REG              0xE3
#define SHOCK_REG               0xE4
#define LED_RED_REG             0xE5
#define LED_GREEN_REG           0xE6
#define DOOR_SENSOR_REG         0xE7
#define LED_FLASH_EN1_REG       0xE8
#define LED_FLASH_EN2_REG       0xE9
#define LED_FLASH_BOTH_REG      0xEA
#define LED_PHONE_BUTTON_REG    0xEB
#define SENSOR0_EN_REG          0x30
#define SENSOR1_EN_REG          0x31
#define MOTION_EN_REG           0x32
#define LED_ONLINE_REG          0x33
#define LDR_EN_REG              0x34
#define IR_EN_REG               0x36
#define RELAY0_EN_REG           0x37
#define RELAY1_EN_REG           0x38
#define SENSOR0_SENSE_REG       0X40
#define SENSOR1_SENSE_REG       0X41
#define MOTION_SENSE_REG        0X42
#define IR_LEVEL_REG            0x50
#define LDR_READ_REG            0x60
#define SHUTDOWN_REG            0x90
#define KEYPAD_EN_REG           0x70
#define KEYPRESS_REG            0x71

#define KEYPAD_0		0x30
#define KEYPAD_1		0x31
#define KEYPAD_2		0x32
#define KEYPAD_3		0x33
#define KEYPAD_4		0x34
#define KEYPAD_5		0x35
#define KEYPAD_6		0x36
#define KEYPAD_7		0x37
#define KEYPAD_8		0x38
#define KEYPAD_9		0x39
#define KEYPAD_ASTERISK		0x2A
#define KEYPAD_HASHTAG		0x23

void i2c_read_data(int reg_add);
static void work_i2c_read(struct work_struct *pWork);



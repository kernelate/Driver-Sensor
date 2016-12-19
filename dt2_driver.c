/*
	Doortalk 2 driver 
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include <linux/input.h>

#include "dt2_driver.h"
#define EC_DEBUG 1
#define DEV_NAME "dt2_mcu_driver_dev"
#define VERSION     "1.1.0"
#if EC_DEBUG
#define DPRINTK(fmt, args...)   printk(fmt,## args)
#else
#define DPRINTK(fmt, args...)
#endif
#define REG0_DEFAULT	0x4690  //9	//0xa400
#define REG1_DEFAULT	0x2692	//0x2a02
#define REG2_DEFAULT	0x0004
#define REG3_DEFAULT	0x0006
#define REG4_DEFAULT	0x0008
#define REG5_DEFAULT	0x000a


//* TO TEST WAKEUP
#include <linux/proc_fs.h>	
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>

#include <linux/suspend.h>
enum {  //from kernel/power/earlysuspend.c
    SUSPEND_REQUESTED = 0x1,
    SUSPENDED = 0x2,
    SUSPEND_REQUESTED_AND_SUSPENDED = SUSPEND_REQUESTED | SUSPENDED,
};
extern suspend_state_t get_suspend_state(void);
//*/
#define DEBUG_MSG

struct delayed_work init_ec_dwork;
struct delayed_work ldr_read_work;

static struct   i2c_client *this_client = NULL;	
//static struct   dt2_data *dt2_this = NULL;
struct dt2_data {
    struct input_dev *dev;
    struct timer_list   rd_timer;           // keyscan timer
    struct i2c_client *client;
    unsigned int                wakeup_delay;           // key wakeup delay
};

static struct input_dev *sensor_dev;

//#define MAX_KEYCODE_CNT 15

#if KEYPAD
#define MAX_KEYCODE_CNT 27
int sensor_keycode[MAX_KEYCODE_CNT]={	KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5,KEY_F6,KEY_F7,KEY_F8,KEY_LEFTCTRL,KEY_RIGHTCTRL,KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_LEFTALT,KEY_RIGHTALT, KEY_POWER,KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,KEY_0,KEY_KPASTERISK,KEY_MACRO,
};
#else
#define MAX_KEYCODE_CNT 15
int sensor_keycode[MAX_KEYCODE_CNT]={KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5,KEY_F6,KEY_F7,KEY_F8,KEY_LEFTCTRL,KEY_RIGHTCTRL,KEY_LEFTSHIFT,KEY_RIGHTSHIFT,KEY_LEFTALT,KEY_RIGHTALT, KEY_POWER,};
#endif

int ldr_value = 0x00;
int ldr_last_status = 0;
int ldr_sensitivity_low = 30;
int ldr_sensitivity_medium = 50;
int ldr_sensitivity_high = 70;
int motion_sensitivity = 20;
int sensor0_sensitivity = 20;
int sensor1_sensitivity = 20;

//=========================INIT KEYPRESS==========================//

void  init_sensor_keypress(void){
	int error, key = 0, code = 0;
	sensor_dev = input_allocate_device();
	if (!sensor_dev) {
    		printk(KERN_ERR "SENSOR:error allocating input device not enough memory\n");
    	}
    	set_bit(EV_KEY, sensor_dev->evbit);
	for(key = 0; key < MAX_KEYCODE_CNT; key++){
		code = sensor_keycode[key];
		if(code<=0)
               		continue;
		 set_bit(code & KEY_MAX, sensor_dev->keybit);
       }
	sensor_dev->name="sensor_key";
	error=input_register_device(sensor_dev);
	if(error){
    		printk(KERN_ERR "SENSOR:Failed to register input device\n");
	}
	return;
}

void send_keyEvent(unsigned int code)
{
    input_report_key(sensor_dev, code, 1); //key_down
    input_sync(sensor_dev);
    input_report_key(sensor_dev, code, 0); //key_up
    input_sync(sensor_dev);
}

void ldr_ir_service(struct work_struct *pWork)
{
//	printk("RAN_EDIT: %s\n", __func__);

//	ldr_value = i2c_smbus_read_byte_data(this_client,LDR_READ_REG);

//	printk("RAN_EDIT: %d\n", ldr_value);
	if (ldr_value > 100){
		ldr_value = 100;
	}	
	if(ldr_value <= ldr_sensitivity_low && ldr_last_status != 1)
	{
		ldr_last_status = 1;
        send_keyEvent(KEY_F7);
#ifdef DEBUG_MSG
		printk("RAN_EDIT: IR is off\n");
#endif        
		schedule_delayed_work(&ldr_read_work,500);
	}
	else if (ldr_value >= ldr_sensitivity_low && ldr_value < ldr_sensitivity_medium && ldr_last_status != ldr_sensitivity_low)
	{
		ldr_last_status = ldr_sensitivity_low;
		send_keyEvent(KEY_F1); //key press
#ifdef DEBUG_MSG        
		printk("RAN_EDIT: LDR sensitivity = low\n");
#endif        
		schedule_delayed_work(&ldr_read_work,500);
	}
	else if (ldr_value >= ldr_sensitivity_medium && ldr_value < ldr_sensitivity_high && ldr_last_status != ldr_sensitivity_medium)
	{
		ldr_last_status = ldr_sensitivity_medium;
        send_keyEvent(KEY_F2);
#ifdef DEBUG_MSG
		printk("RAN_EDIT: LDR sensitivity = medium\n");
#endif        
		schedule_delayed_work(&ldr_read_work,500);
	}
	else if (ldr_value >= ldr_sensitivity_high && ldr_value <= 100 && ldr_last_status != ldr_sensitivity_high)
	{
		ldr_last_status = ldr_sensitivity_high;
        send_keyEvent(KEY_F3);
#ifdef DEBUG_MSG        
		printk("RAN_EDIT: LDR sensitivity = high\n");
#endif        
		schedule_delayed_work(&ldr_read_work,500);
	}
	else
		schedule_delayed_work(&ldr_read_work,500);	

}

static int dt2_init_client(struct i2c_client *client)
{
	int status = 0;
	struct dt2_data *data = i2c_get_clientdata(client);
	data->client = client;

//	printk("RAN_EDIT: %s\n", __func__);
	return status;
}

static int __devexit dt2_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));
	return 0;
}

static int __devinit dt2_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct dt2_data *data;

#ifdef DEBUG_MSG
	printk("RAN_EDIT: I2C- PROBE START\n");
#endif
	data = kzalloc(sizeof(struct dt2_data), GFP_KERNEL);
//	if(!data)
    	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		err = -ENOMEM;
#ifdef DEBUG_MSG        
		printk("RAN_EDIT: !i2c_check_functionality\n");
#endif        
		goto exit;
	}

#ifdef DEBUG_MSG
	printk("RAN_EDIT: i2c init\n");
#endif
	i2c_set_clientdata (client, data);
    	data->client=client;
    	this_client=client;	
	/* Initialize the chip */
	err = dt2_init_client(client);
	if(err)
		goto exit_free;
	
	goto exit;
	
	exit_free:
		kfree(data);
	exit:
		return err;
}

static const struct i2c_device_id dt2_id[] =
{
	{ DEV_NAME, 0 },
	{  }
};

MODULE_DEVICE_TABLE(i2c, dt2_id);

struct i2c_driver dt2_driver = 
{
	.driver =
	{
		.name = DEV_NAME,
		.owner = THIS_MODULE,
	},
	.id_table = dt2_id,
	.probe    = dt2_probe,
	.remove   = dt2_remove
};    

static long dt2_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int len;
	int tmp_data;
	DT2_data ec_data;
	len=0;
	tmp_data = 0x00;
#ifdef DEBUG_MSG
	printk("O_O ioctl accessed\n");
#endif
	switch(cmd){
    case DT2_RLY0_STATUS:
		tmp_data=i2c_smbus_read_byte_data(this_client,RELAY0_REG);
#ifdef DEBUG_MSG        
       	printk("RAN_EDIT: RELAY0=%d\n",tmp_data);
#endif            
		ec_data.reg_num=RELAY0_REG;
		ec_data.value=tmp_data;
#ifdef DEBUG_MSG        
	    printk("RAN_EDIT: EC_DATA.VALUE=%d\n",ec_data.value);
#endif            
        copy_to_user((DT2_data *)arg, &ec_data, sizeof(DT2_data));
    	break;
   	case DT2_RLY1_STATUS:
   		tmp_data=i2c_smbus_read_byte_data(this_client,RELAY1_REG);
#ifdef DEBUG_MSG            
       	printk("RAN_EDIT: RELAY1=%d\n",tmp_data);
#endif            
   		ec_data.reg_num=RELAY1_REG;
		ec_data.value=tmp_data;
       	copy_to_user((DT2_data *)arg, &ec_data, sizeof(DT2_data));
   		break;
    case DT2_RLY0_ON:
#ifdef DEBUG_MSG        
       	printk("RAN_EDIT: RLY0_ON\n");
#endif            
		i2c_smbus_write_byte_data(this_client,RELAY0_REG,0x01);
       	break;
    case DT2_RLY0_OFF:
#ifdef DEBUG_MSG        
	    printk("RAN_EDIT: RLY0_OFF\n");
#endif            
		i2c_smbus_write_byte_data(this_client,RELAY0_REG,0x00);
	    break;
    case DT2_RLY1_ON:
#ifdef DEBUG_MSG        
	    printk("RAN_EDIT: RLY1_ON\n");
#endif            
		i2c_smbus_write_byte_data(this_client,RELAY1_REG,0x01);
	    break;
    case DT2_RLY1_OFF:
#ifdef DEBUG_MSG        
	    printk("RAN_EDIT: RLY1_OFF\n");
#endif            
		i2c_smbus_write_byte_data(this_client,RELAY1_REG,0x00);
	    break;
	case DT2_MOTION_ON:
#ifdef DEBUG_MSG    
   		printk("RAN_EDIT: MOTION_ON\n");
#endif        
		i2c_smbus_write_byte_data(this_client,MOTION_EN_REG,0x02);
	    break;
    case DT2_MOTION_OFF:
#ifdef DEBUG_MSG        
	    printk("RAN_EDIT: MOTION_OFF\n");
#endif            
		i2c_smbus_write_byte_data(this_client,MOTION_EN_REG,0x00);
	    break;
	case DT2_SENSOR0_ON:
#ifdef DEBUG_MSG    
	    printk("RAN_EDIT: SENSOR0_ON\n");
#endif            
		i2c_smbus_write_byte_data(this_client,SENSOR0_EN_REG,0x01);
        break;
    case DT2_SENSOR0_OFF:
#ifdef DEBUG_MSG        
	    printk("RAN_EDIT: SENSOR0_OFF\n");
#endif            
		i2c_smbus_write_byte_data(this_client,SENSOR0_EN_REG,0x00);
	    break;
	case DT2_SENSOR1_ON:
#ifdef DEBUG_MSG    
	    printk("RAN_EDIT: SENSOR1_ON\n");
#endif            
		i2c_smbus_write_byte_data(this_client,SENSOR1_EN_REG,0x01);
	    break;
    case DT2_SENSOR1_OFF:
#ifdef DEBUG_MSG        
	    printk("RAN_EDIT: SENSOR1_OFF\n");
#endif           
		i2c_smbus_write_byte_data(this_client,SENSOR1_EN_REG,0x00);
	    break;
	case DT2_LDR_ON:
#ifdef DEBUG_MSG    
	    printk("RAN_EDIT: LDR_ON\n");
#endif            
		i2c_smbus_write_byte_data(this_client,LDR_EN_REG,0x01);
	    break;
	case DT2_LDR_OFF:
#ifdef DEBUG_MSG
	    printk("RAN_EDIT: LDR_OFF\n");
#endif            
		i2c_smbus_write_byte_data(this_client,LDR_EN_REG,0x00);
	    break;
	case DT2_IR_ON:
#ifdef DEBUG_MSG    
	    printk("RAN_EDIT: IR_ON\n");
#endif            
		i2c_smbus_write_byte_data(this_client,IR_EN_REG,0x01);
	    break;
	case DT2_IR_OFF:
#ifdef DEBUG_MSG    
	    printk("RAN_EDIT: IR_OFF\n");
#endif            
		i2c_smbus_write_byte_data(this_client,IR_LEVEL_REG,0x00);
       	break;
	case DT2_IR_LEVEL1:
#ifdef DEBUG_MSG    
        printk("RAN_EDIT: IR_LEVEL1\n");
#endif            
		i2c_smbus_write_byte_data(this_client,IR_LEVEL_REG,0x01);
	    break;
	case DT2_IR_LEVEL2:
#ifdef DEBUG_MSG    
	    printk("RAN_EDIT: IR_LEVEL2\n");
#endif            
		i2c_smbus_write_byte_data(this_client,IR_LEVEL_REG,0x02);
        break;
	case DT2_IR_LEVEL3:
#ifdef DEBUG_MSG    
        printk("RAN_EDIT: IR_LEVEL2\n");
#endif                
		i2c_smbus_write_byte_data(this_client,IR_LEVEL_REG,0x03);
        break;
	case DT2_LDR_SENSE_LOW:
#ifdef DEBUG_MSG    
        printk("RAN_EDIT: LDR_SENSITIVITY_LOW\n");
#endif                
	   	len = copy_from_user(&ec_data, (struct __user DT2_data *) arg, sizeof(ec_data));
		if (len > 0) {
			return -EFAULT;
		}
		ldr_sensitivity_low = ec_data.value;
#ifdef DEBUG_MSG        
		printk("RAN_EDIT: ldr_sensitivity_low: %d\n", ldr_sensitivity_low);	
#endif        
        break;
	case DT2_LDR_SENSE_MEDIUM:
#ifdef DEBUG_MSG    
        printk("RAN_EDIT: LDR_SENSITIVITY_MEDIUM\n");
#endif                
	    len = copy_from_user(&ec_data, (struct __user DT2_data *) arg, sizeof(ec_data));
		if (len > 0) {
			return -EFAULT;
		}
		ldr_sensitivity_medium = ec_data.value;	
#ifdef DEBUG_MSG        
		printk("RAN_EDIT: ldr_sensitivity_medium: %d\n", ldr_sensitivity_medium);
#endif        
        break;
	case DT2_LDR_SENSE_HIGH:
#ifdef DEBUG_MSG    
        printk("RAN_EDIT: LDR_SENSITIVITY_HIGH\n");
#endif                
	    len = copy_from_user(&ec_data, (struct __user DT2_data *) arg, sizeof(ec_data));
		if (len > 0) {
			return -EFAULT;
		}
		ldr_sensitivity_high = ec_data.value;	
#ifdef DEBUG_MSG        
		printk("RAN_EDIT: ldr_sensitivity_high: %d\n", ldr_sensitivity_high);
#endif        
        break;
	case DT2_SENSOR0_SENSE:
#ifdef DEBUG_MSG    
        printk("RAN_EDIT: MCU_SENSOR0_SENSE\n");
#endif                
	    len = copy_from_user(&ec_data, (struct __user DT2_data *) arg, sizeof(ec_data));
		if (len > 0) {
			return -EFAULT;
		}
		sensor0_sensitivity = ec_data.value;	
#ifdef DEBUG_MSG        
		printk("RAN_EDIT: sensor0_sensitivity: %d\n", sensor0_sensitivity);
#endif       
		i2c_smbus_write_byte_data(this_client,SENSOR0_SENSE_REG,sensor0_sensitivity);
        break;
	case DT2_SENSOR1_SENSE:
#ifdef DEBUG_MSG    
        printk("RAN_EDIT: MCU_SENSOR1_SENSE\n");
#endif                
	    len = copy_from_user(&ec_data, (struct __user DT2_data *) arg, sizeof(ec_data));
		if (len > 0) {
			return -EFAULT;
		}
		sensor1_sensitivity = ec_data.value;	
#ifdef DEBUG_MSG        
		printk("RAN_EDIT: sensor1_sensitivity: %d\n", sensor1_sensitivity);
#endif   
		i2c_smbus_write_byte_data(this_client,SENSOR1_SENSE_REG,sensor1_sensitivity);
        break;
	case DT2_MOTION_SENSE:
#ifdef DEBUG_MSG    
        printk("RAN_EDIT: MCU_MOTION_SENSE\n");
#endif                
	   	len = copy_from_user(&ec_data, (struct __user DT2_data *) arg, sizeof(ec_data));
		if (len > 0) {
			return -EFAULT;
		}
		motion_sensitivity = ec_data.value;	
#ifdef DEBUG_MSG        
		printk("RAN_EDIT: motion_sensitivity: %d\n", motion_sensitivity);
#endif        
		i2c_smbus_write_byte_data(this_client,MOTION_SENSE_REG, motion_sensitivity);
        break;

	default:
			DPRINTK("O_O ioctl cmd not found\n");
            return -1;
            break;
	}
	return 0;
}

void init_ldr_read_work(void)
{
	INIT_DELAYED_WORK(&ldr_read_work, ldr_ir_service);
	schedule_delayed_work(&ldr_read_work,500);
}

void deinit_ldr_read_work(void){
	cancel_delayed_work_sync(&ldr_read_work);
}

struct delayed_work i2c_read_work;
void init_i2c_read_work(void){
	INIT_DELAYED_WORK(&i2c_read_work,work_i2c_read);			
	schedule_delayed_work(&i2c_read_work,500);
}


void deinit_i2c_read_work(void){
	cancel_delayed_work_sync(&i2c_read_work);
}

static void work_i2c_read(struct work_struct *pWork) {
//	printk("O_O %s\n", __func__);
		//unsigned char reg_add=0xFF;
		//unsigned char reg_data=0xFF;
	i2c_read_data(READ_REG_BYTE);
	schedule_delayed_work(&i2c_read_work,100);
}

void i2c_read_data_handler(int reg_add,int reg_data){
//	printk("O_O %s\n", __func__);

	switch(reg_add){
		case READ_REG_BYTE:
			if(reg_data<0xFF){	
				if(reg_data !=0xCE){		
					udelay(500);			
					i2c_read_data(reg_data);
				}
			}
		break;	
		case SENSOR0_REG:
			if(reg_data==0x01){
                send_keyEvent(KEY_LEFTSHIFT);
#ifdef DEBUG_MSG
			printk("RAN_EDIT: read sensor 0 on\n");
#endif
			}
			if(reg_data==0x00){
                send_keyEvent(KEY_RIGHTSHIFT);
#ifdef DEBUG_MSG
			printk("RAN_EDIT: read sensor 0 off\n");
#endif
			}

		break;	
		case SENSOR1_REG:
			if(reg_data==0x01){
                send_keyEvent(KEY_LEFTCTRL);
#ifdef DEBUG_MSG
				printk("RAN_EDIT: read sensor 1 on\n");
#endif
				}
			if(reg_data==0x00){
                send_keyEvent(KEY_RIGHTCTRL);
#ifdef DEBUG_MSG
				printk("RAN_EDIT: read sensor 1 off\n");
#endif
				}
				break;	
		case SHOCK_REG:
#ifdef DEBUG_MSG
			printk("RAN_EDIT: read Shock\n");
#endif
			if(reg_data==0x01){
                send_keyEvent(KEY_F5);
#ifdef DEBUG_MSG
				printk("RAN_EDIT: SHOCK DETECTED\n");
#endif
			}
		break;	
		case MOTION_REG:
#ifdef DEBUG_MSG
			printk("RAN_EDIT: read Motion\n");
#endif
			if(reg_data==0x01){
                send_keyEvent(KEY_F4);
				printk("se-mcu-debug:suspend state=%d\n",get_suspend_state());
				if((get_suspend_state() & SUSPENDED) > 0){
			            printk("se-mcu-debug: POWER KEY PRESSED\n");
                        send_keyEvent(KEY_POWER);
				}
#ifdef DEBUG_MSG
				printk("RAN_EDIT: MOTION DETECTED\n");
#endif
			}
		break;	
		case DOOR_SENSOR_REG:
#ifdef DEBUG_MSG
			printk("RAN_EDIT: read DOOR_SENSOR\n");
#endif
			if(reg_data==0x01){
                send_keyEvent(KEY_LEFTALT);
#ifdef DEBUG_MSG
				printk("RAN_EDIT: DOOR_SENSOR PRESENT\n");
#endif
			}
			if(reg_data==0x00){
                send_keyEvent(KEY_RIGHTALT);
#ifdef DEBUG_MSG
				printk("RAN_EDIT: DOOR_SENSOR PRESENT\n");
#endif
			}
		break;

		case LDR_EN_REG:
#ifdef DEBUG_MSG
			printk("RAN_EDIT: read LDR_EN_REG\n");
#endif
			if(reg_data==0x01){
#ifdef DEBUG_MSG
				printk("RAN_EDIT: LDR ENABLE\n");
#endif
			}
			if(reg_data==0x00){
#ifdef DEBUG_MSG
				printk("RAN_EDIT: LDR DISABLE\n");
#endif
			}
		break;		

		case IR_EN_REG:
#ifdef DEBUG_MSG
			printk("RAN_EDIT: read IR_EN_REG\n");
#endif
			if(reg_data==0x01){
#ifdef DEBUG_MSG
				printk("RAN_EDIT: IR ENABLE\n");
#endif
			}
			if(reg_data==0x00){
#ifdef DEBUG_MSG
				printk("RAN_EDIT: IR DISABLE\n");
#endif
			}
		break;	

		case IR_LEVEL_REG:
#ifdef DEBUG_MSG
			printk("RAN_EDIT: read IR_LEVEL_REG\n");
#endif
			if(reg_data==0x01){
#ifdef DEBUG_MSG
				printk("RAN_EDIT: IR LEVEL 1\n");
#endif
			}
			if(reg_data==0x02){
#ifdef DEBUG_MSG
				printk("RAN_EDIT: IR LEVEL 2\n");
#endif
			}
			if(reg_data==0x03){
#ifdef DEBUG_MSG
				printk("RAN_EDIT: IR LEVEL 3\n");
#endif
			}
		break;

		case LDR_READ_REG:
#ifdef DEBUG_MSG
			printk("RAN_EDIT: read LDR_READ_REG\n");
#endif
			ldr_value = reg_data;
		break;
#if KEYPAD
        case KEYPRESS_REG:
            if(reg_data != 0xFF)
            {
                switch(reg_data)
                {
                    //ROW1
                    case KEYPAD_1:
			printk("RAN_EDIT: KEYPAD_1\n");
                        send_keyEvent(KEY_1);
                        break;
                    case KEYPAD_2:
			printk("RAN_EDIT: KEYPAD_2\n");
                        send_keyEvent(KEY_2);
                        break;
                    case KEYPAD_3:
			printk("RAN_EDIT: KEYPAD_3\n");
                        send_keyEvent(KEY_3);
                        break;
                    //ROW2
                    case KEYPAD_4:
			printk("RAN_EDIT: KEYPAD_4\n");
                        send_keyEvent(KEY_4);
                        break;
                    case KEYPAD_5:
			printk("RAN_EDIT: KEYPAD_5\n");
                        send_keyEvent(KEY_5);
                        break;
                    case KEYPAD_6:
			printk("RAN_EDIT: KEYPAD_6\n");
                        send_keyEvent(KEY_6);
                        break;
                    //ROW3
                    case KEYPAD_7:
			printk("RAN_EDIT: KEYPAD_7\n");
                        send_keyEvent(KEY_7);
                        break;
                    case KEYPAD_8:
			printk("RAN_EDIT: KEYPAD_8\n");
                        send_keyEvent(KEY_8);
                        break;
                    case KEYPAD_9:
			printk("RAN_EDIT: KEYPAD_9\n");
                        send_keyEvent(KEY_9);
                        break;
                    //ROW4
                    case KEYPAD_ASTERISK:
			printk("RAN_EDIT: KEYPAD_*\n");
                        send_keyEvent(KEY_KPASTERISK);
                        break;
                    case KEYPAD_0:
			printk("RAN_EDIT: KEYPAD_0\n");
                        send_keyEvent(KEY_0);
                        break;
                    case KEYPAD_HASHTAG:
			printk("RAN_EDIT: KEYPAD_#\n");
                        send_keyEvent(KEY_MACRO);
                        break;                        
                }
            }
		break;
#endif        
	
	default:
#ifdef DEBUG_MSG
			printk("%s : Register Address (%d) not found\n",__func__,reg_add);
#endif
		break;	
	}
}

void i2c_read_data(int reg_add){
	unsigned char reg_data;

//	printk("O_O %s\n", __func__);
	reg_data=i2c_smbus_read_byte_data(this_client,reg_add);
//	printk("RAN_EDIT: reg_add= %x reg_data=%x\n",reg_add,reg_data);
	i2c_read_data_handler(reg_add,reg_data);
}

static int dt2_open(struct inode *inode, struct file *file)
{
//	printk("RAN_EDIT: dt2_open\n");
	return 0;
}

static int dt2_close(struct inode *inode, struct file *file)
{
//	printk("RAN_EDIT: dt2_close\n");
	return 0;
}

static long dt2_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int dt2_open(struct inode *inode, struct file *file);
static int dt2_close(struct inode *inode, struct file *file);

static struct file_operations dt2_fops = {
        .owner = THIS_MODULE,
        .open = dt2_open,
        .release = dt2_close,
        .unlocked_ioctl = dt2_ioctl,
};

static struct miscdevice dt2_dev = 
{
	MISC_DYNAMIC_MINOR,
	DEV_NAME,
	&dt2_fops,
};

static int __init dt2_drv_init(void)
{
	int ret;

//	printk("RAN_EDIT: %s\n", __func__);
	ret = i2c_add_driver(&dt2_driver);
	if (ret)
	{
		printk("i2c add driver failed\n");
		return -1;
	}
	ret = misc_register(&dt2_dev);
	if(ret)
	{
		printk("could not register cs6422 device\n");
		return -1;
	}
	
//	printk("RAN_EDIT: IO ENABLE\n");
	i2c_smbus_write_byte_data(this_client,SENSOR0_EN_REG,0x01);
	i2c_smbus_write_byte_data(this_client,SENSOR1_EN_REG,0x01);
#if KEYPAD    
	i2c_smbus_write_byte_data(this_client,KEYPAD_EN_REG,0x01);
#endif
	i2c_smbus_write_byte_data(this_client,MOTION_EN_REG,0x02);
	i2c_smbus_write_byte_data(this_client,LDR_EN_REG,0x01);
	i2c_smbus_write_byte_data(this_client,IR_EN_REG,0x01);
   	i2c_smbus_write_byte_data(this_client,RELAY0_EN_REG,0x01);
   	i2c_smbus_write_byte_data(this_client,RELAY1_EN_REG,0x01);

	init_sensor_keypress();
	init_i2c_read_work();
	init_ldr_read_work();
	return 0;	
}

static void __exit dt2_drv_exit (void)
{
//	printk("RAN_EDIT: %s\n", __func__);
	deinit_i2c_read_work();
	deinit_ldr_read_work();
	i2c_del_driver (&dt2_driver);
    	misc_deregister(&dt2_dev);
}


module_init(dt2_drv_init);
module_exit(dt2_drv_exit);

MODULE_AUTHOR("Randy M. Aguinaldo (ranaguinaldo@gmail.com)");
MODULE_DESCRIPTION("Doortalk 2 driver");
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");

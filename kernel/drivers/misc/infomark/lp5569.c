
//#define DEBUG

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>

#include "ledanim.h"

#define LED_PIN_COUNT_FRONT 6
#define LED_PIN_COUNT_BACK 3

#define ENABLE_CALIBRATE_PWM

#define LP5569_REG_ENABLE				0x00
#define LP5569_REG_LED_PWM_BASE			0x16
#define LP5569_REG_LED_CURRENT_BASE		0x22
#define LP5569_REG_MISC					0x2F

#define LP5569_ENABLE				0x40

/* MISC */
#define LP5569_CP_MODE_1_X			0x08
#define LP5569_CP_MODE_1_5_X		0x10
#define LP5569_CP_MODE_AUTO			0x18
#define LP5569_EN_AUTO_INCR			0x40

#define LP5569_OUTPUT_CURRENT_FOR_FRONT			0x10
#define LP5569_OUTPUT_CURRENT_FOR_BACK			0x30

#define MAX_LED_GROUP	2

#if !defined(SECURITY_FOR_FRIENDS)
#define MAX_SIZE 4096
#define SECURITY_FOR_FRIENDS
#endif

enum {
	GROUP_INDEX_BACK = 0,
	GROUP_INDEX_FRONT = 1
};


#if defined(ENABLE_CALIBRATE_PWM)
static unsigned char calpwm_tbl_255[] = { /* 0xff */
	  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,
	 16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
	 32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
	 48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
	 64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
	 80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,
	 96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
	112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
	128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
	144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
	176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
	192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
	208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
	224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
};
static unsigned char calpwm_tbl_64[] = { /* 0x40 */
	  0,   0,   0,   0,   1,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,   3,
	  4,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   7,
	  8,   8,   8,   8,   9,   9,   9,   9,  10,  10,  10,  10,  11,  11,  11,  11,
	 12,  12,  12,  12,  13,  13,  13,  13,  14,  14,  14,  14,  15,  15,  15,  15,
	 16,  16,  16,  16,  17,  17,  17,  17,  18,  18,  18,  18,  19,  19,  19,  19,
	 20,  20,  20,  20,  21,  21,  21,  21,  22,  22,  22,  22,  23,  23,  23,  23,
	 24,  24,  24,  24,  25,  25,  25,  25,  26,  26,  26,  26,  27,  27,  27,  27,
	 28,  28,  28,  28,  29,  29,  29,  29,  30,  30,  30,  30,  31,  31,  31,  32,
	 32,  32,  32,  33,  33,  33,  33,  34,  34,  34,  34,  35,  35,  35,  35,  36,
	 36,  36,  36,  37,  37,  37,  37,  38,  38,  38,  38,  39,  39,  39,  39,  40,
	 40,  40,  40,  41,  41,  41,  41,  42,  42,  42,  42,  43,  43,  43,  43,  44,
	 44,  44,  44,  45,  45,  45,  45,  46,  46,  46,  46,  47,  47,  47,  47,  48,
	 48,  48,  48,  49,  49,  49,  49,  50,  50,  50,  50,  51,  51,  51,  51,  52,
	 52,  52,  52,  53,  53,  53,  53,  54,  54,  54,  54,  55,  55,  55,  55,  56,
	 56,  56,  56,  57,  57,  57,  57,  58,  58,  58,  58,  59,  59,  59,  59,  60,
	 60,  60,  60,  61,  61,  61,  61,  62,  62,  62,  62,  63,  63,  63,  63,  64,
};
static unsigned char calpwm_tbl_96[] = { /* 0x60 */
	  0,   0,   0,   1,   1,   2,   2,   2,   3,   3,   3,   4,   4,   5,   5,   5,
	  6,   6,   6,   7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  11,  11,  11,
	 12,  12,  12,  13,  13,  14,  14,  14,  15,  15,  16,  16,  16,  17,  17,  17,
	 18,  18,  19,  19,  19,  20,  20,  20,  21,  21,  22,  22,  22,  23,  23,  23,
	 24,  24,  25,  25,  25,  26,  26,  26,  27,  27,  28,  28,  28,  29,  29,  29,
	 30,  30,  31,  31,  31,  32,  32,  32,  33,  33,  34,  34,  34,  35,  35,  35,
	 36,  36,  37,  37,  37,  38,  38,  38,  39,  39,  40,  40,  40,  41,  41,  41,
	 42,  42,  43,  43,  43,  44,  44,  44,  45,  45,  46,  46,  46,  47,  47,  48,
	 48,  48,  49,  49,  49,  50,  50,  51,  51,  51,  52,  52,  52,  53,  53,  54,
	 54,  54,  55,  55,  55,  56,  56,  57,  57,  57,  58,  58,  58,  59,  59,  60,
	 60,  60,  61,  61,  61,  62,  62,  63,  63,  63,  64,  64,  64,  65,  65,  66,
	 66,  66,  67,  67,  67,  68,  68,  69,  69,  69,  70,  70,  70,  71,  71,  72,
	 72,  72,  73,  73,  73,  74,  74,  75,  75,  75,  76,  76,  76,  77,  77,  78,
	 78,  78,  79,  79,  80,  80,  80,  81,  81,  81,  82,  82,  83,  83,  83,  84,
	 84,  84,  85,  85,  86,  86,  86,  87,  87,  87,  88,  88,  89,  89,  89,  90,
	 90,  90,  91,  91,  92,  92,  92,  93,  93,  93,  94,  94,  95,  95,  95,  96,
};
#endif


struct lp5569_led_group {
	struct lp5569_priv * priv;
	int index;
	int led_start;
	int led_count;
	unsigned char max_current;
#if defined(ENABLE_CALIBRATE_PWM)
	unsigned char * calpwm_tbl_r;
	unsigned char * calpwm_tbl_g;
	unsigned char * calpwm_tbl_b;
#endif
};

struct lp5569_priv {
	struct device *dev;
	struct i2c_client *client;

	int led_enable_gpio;

	char is_power_on;

	unsigned char led_brightness[LED_PIN_COUNT_FRONT + LED_PIN_COUNT_BACK];

	struct work_struct	led_update_work;
	struct work_struct	led_power_enable_work;

#if defined(ENABLE_CALIBRATE_PWM)
	char use_calibrate_pwm;
#endif

	char in_use[MAX_LED_GROUP];
	struct lp5569_led_group *led_group[MAX_LED_GROUP];
};

static int lp5569_i2c_write_device(struct lp5569_priv *priv,
				    unsigned char reg,
				    unsigned char value)
{
	int rc = 0;

	rc = i2c_smbus_write_byte_data(priv->client, reg, value);
	if (rc < 0) {
		dev_err(priv->dev, "%s Error reg:0x%02x\n", __func__, reg);
	} else {
		dev_dbg(priv->dev, "%s OK reg:0x%02x value:%d OK\n", __func__, reg, value);
	}

	return rc;
}

static int lp5569_apply_max_current(struct lp5569_led_group *grp)
{
	int i;
	int base = LP5569_REG_LED_CURRENT_BASE + grp->led_start;

	for(i = 0; i < grp->led_count; i++){
		lp5569_i2c_write_device(grp->priv, base + i, grp->max_current);
	}
	return 0;
}

static int lp5569_get_led_brightness(struct device *dev, int index, unsigned char * value)
{
	struct lp5569_led_group * grp = (struct lp5569_led_group * )dev;

	if(!grp)
		return -1;

	if(index < 0 || index >= grp->led_count)
		return -1;
	
	*value = grp->priv->led_brightness[grp->led_start + index];
	return 0;
}

static int lp5569_set_led_brightness(struct device *dev, int index, unsigned char value)
{
	struct lp5569_led_group * grp = (struct lp5569_led_group * )dev;

	if(!grp)
		return -1;

	if(index < 0 || index >= grp->led_count)
		return -1;

	grp->priv->led_brightness[grp->led_start + index] = value;
	return 0;
}

static int lp5569_led_update(struct device *dev)
{
	struct lp5569_led_group * grp = (struct lp5569_led_group *)dev;

	if(grp)
		schedule_work(&grp->priv->led_update_work);

	return 0;
}

static int lp5569_led_power_enable(struct device *dev, int enable)
{
	struct lp5569_led_group * grp = (struct lp5569_led_group *)dev;
	if(grp && grp->priv->in_use[grp->index] != (char)enable){
		grp->priv->in_use[grp->index] = (char)enable;
		schedule_work(&grp->priv->led_power_enable_work);
	}
	return 0;
}

static int lp5569_get_max_current(struct device *dev, unsigned char *value)
{
	struct lp5569_led_group * grp = (struct lp5569_led_group *)dev;
	if(!grp)
		return -1;
	*value = grp->max_current;
	return 0;
}

static int lp5569_set_max_current(struct device *dev, unsigned char value)
{
	struct lp5569_led_group * grp = (struct lp5569_led_group *)dev;

	if(!grp)
		return -1;
		
	if(grp->max_current != value){
		grp->max_current = value;
		if(grp->priv->in_use[grp->index]){
			lp5569_apply_max_current(grp);
			lp5569_led_update(dev);
		}
	}
	return 0;
}

static void lp5569_led_update_work(struct work_struct *work)
{
	int i,g;
	int start,end;
	struct lp5569_led_group * grp;
	struct lp5569_priv *priv = container_of(work, struct lp5569_priv, led_update_work);

	if(priv->is_power_on == 0){
		dev_err(priv->dev, "LP5569 is not power on\n");
		return;
	}

	for(g = 0; g < MAX_LED_GROUP; g++){
		grp = priv->led_group[g];
		start = grp->led_start;
		end = start + grp->led_count;
		for(i = start; i < end; i+=3){
#if defined(ENABLE_CALIBRATE_PWM)
			if(priv->use_calibrate_pwm){
				lp5569_i2c_write_device(priv, LP5569_REG_LED_PWM_BASE + (i + 0), grp->calpwm_tbl_r[priv->led_brightness[i + 0]]);
				lp5569_i2c_write_device(priv, LP5569_REG_LED_PWM_BASE + (i + 1), grp->calpwm_tbl_g[priv->led_brightness[i + 1]]);
				lp5569_i2c_write_device(priv, LP5569_REG_LED_PWM_BASE + (i + 2), grp->calpwm_tbl_b[priv->led_brightness[i + 2]]);
			} else {
				lp5569_i2c_write_device(priv, LP5569_REG_LED_PWM_BASE + (i + 0), priv->led_brightness[i + 0]);
				lp5569_i2c_write_device(priv, LP5569_REG_LED_PWM_BASE + (i + 1), priv->led_brightness[i + 1]);
				lp5569_i2c_write_device(priv, LP5569_REG_LED_PWM_BASE + (i + 2), priv->led_brightness[i + 2]);
			}
#else
			lp5569_i2c_write_device(priv, LP5569_REG_LED_PWM_BASE + (i + 0), priv->led_brightness[i + 0]);
			lp5569_i2c_write_device(priv, LP5569_REG_LED_PWM_BASE + (i + 1), priv->led_brightness[i + 1]);
			lp5569_i2c_write_device(priv, LP5569_REG_LED_PWM_BASE + (i + 2), priv->led_brightness[i + 2]);
#endif
		}
	}
}

static int _lp5569_led_power_enable(struct i2c_client *client, int enable) 
{
	struct lp5569_priv * priv = i2c_get_clientdata(client);

	if(enable){	
		if(priv->is_power_on){
			dev_dbg(priv->dev, "LP5569 already powered on\n");
			return 0;
		}
		dev_dbg(priv->dev, "led-enable gpio set to high\n");
		gpio_direction_output(priv->led_enable_gpio, 1);

		if(lp5569_i2c_write_device(priv, LP5569_REG_ENABLE, LP5569_ENABLE) == 0){
			int i;

			dev_dbg(priv->dev, "LP5569 power on success\n");
			priv->is_power_on = 1;

			/* set max_current */
			for(i = 0; i < MAX_LED_GROUP; i++){
				lp5569_apply_max_current(priv->led_group[i]);
			}

			/* CP_MODE to 1x mode */
			lp5569_i2c_write_device(priv, LP5569_REG_MISC, LP5569_CP_MODE_1_X);
			
		} else {
			dev_err(priv->dev, "LP5569 power on failed");
			priv->is_power_on = 0;
			//gpio_direction_output(priv->led_enable_gpio, 0);
			return -1;
		}
	} else {
		if(priv->is_power_on == 0){
			dev_dbg(priv->dev, "LP5569 already powered off\n");
			return 0;
		}
		
		if(lp5569_i2c_write_device(priv, LP5569_REG_ENABLE, 0) != 0){
			dev_err(priv->dev, "LP5569 power off failed");
		}
		gpio_direction_output(priv->led_enable_gpio, 0);
		priv->is_power_on = 0;
	}
	return 0;
}

static void lp5569_led_power_enable_work(struct work_struct *work)
{
	int i;
	char enable;
	struct lp5569_priv *priv = container_of(work, struct lp5569_priv, led_power_enable_work);

	enable = 0;
	for(i = 0 ; i < MAX_LED_GROUP; i++){
		if(priv->in_use[i]){
			enable = 1; break;
		}
	}
	if(enable != priv->is_power_on){
		_lp5569_led_power_enable(priv->client, (enable ? 1 : 0));
	}
}

////////////////////////////////////////////////////////////////////////////
// sysfs attr
static ssize_t lp5569_max_current_show(struct lp5569_led_group * grp, char *buf)
{
//--start stmdqls for security
#if defined(SECURITY_FOR_FRIENDS)
	return snprintf(buf, MAX_SIZE, "%u\n", grp->max_current);
#else
    return sprintf(buf, "%u\n", grp->max_current);
#endif
}

static ssize_t lp5569_max_current_store(struct lp5569_led_group * grp, const char *buf, size_t len)
{
	unsigned long value;

	if(!grp)
		return -EINVAL;

	if (strict_strtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 255)
		return -EINVAL;

	if((unsigned char)value != grp->max_current){
		struct lp5569_priv * priv = grp->priv;
		grp->max_current = (unsigned char)value;
		
		if(priv->in_use[grp->index]){
			lp5569_apply_max_current(grp);
			lp5569_led_update((struct device*)grp);
		}
	}
	return len;
}

static ssize_t lp5569_max_current_for_frontled_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct lp5569_priv *priv = dev_get_drvdata(dev);
	if(!priv) return -EINVAL;

	return lp5569_max_current_show(priv->led_group[GROUP_INDEX_FRONT], buf);
}

static ssize_t lp5569_max_current_for_frontled_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp5569_priv *priv = dev_get_drvdata(dev);
	if(!priv) return -EINVAL;
	return lp5569_max_current_store(priv->led_group[GROUP_INDEX_FRONT], buf, len);
}

static ssize_t lp5569_max_current_for_backled_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct lp5569_priv *priv = dev_get_drvdata(dev);
	if(!priv) return -EINVAL;
	return lp5569_max_current_show(priv->led_group[GROUP_INDEX_BACK], buf);
}

static ssize_t lp5569_max_current_for_backled_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp5569_priv *priv = dev_get_drvdata(dev);
	if(!priv) return -EINVAL;
	return lp5569_max_current_store(priv->led_group[GROUP_INDEX_BACK], buf, len);
}

static DEVICE_ATTR(max_current, S_IRUGO | S_IWUSR,
		lp5569_max_current_for_frontled_show,
		lp5569_max_current_for_frontled_store);

static DEVICE_ATTR(max_current_for_backled, S_IRUGO | S_IWUSR,
		lp5569_max_current_for_backled_show,
		lp5569_max_current_for_backled_store);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_max_current.attr,
	&dev_attr_max_current_for_backled.attr,
	NULL
};

static struct attribute_group lp5569_attribute_group = {
	.attrs = sysfs_attrs
};
// sysfs
////////////////////////////////////////////////////////////////////////////


struct lp5569_led_group led_groups[MAX_LED_GROUP] = {
	{
		.priv = NULL,
		.index = GROUP_INDEX_BACK,
		.led_start = 0,
		.led_count = LED_PIN_COUNT_BACK,
		.max_current = LP5569_OUTPUT_CURRENT_FOR_BACK,
#if defined(ENABLE_CALIBRATE_PWM)
		.calpwm_tbl_r = calpwm_tbl_255,
		.calpwm_tbl_g = calpwm_tbl_255,
		.calpwm_tbl_b = calpwm_tbl_96,
#endif
	},{
		.priv = NULL,
		.index = GROUP_INDEX_FRONT,
		.led_start = LED_PIN_COUNT_BACK,
		.led_count = LED_PIN_COUNT_FRONT,
		.max_current = LP5569_OUTPUT_CURRENT_FOR_FRONT,
#if defined(ENABLE_CALIBRATE_PWM)
		.calpwm_tbl_r = calpwm_tbl_255,
		.calpwm_tbl_g = calpwm_tbl_64,
		.calpwm_tbl_b = calpwm_tbl_96,
#endif
	}
};

static struct leddrv_func leddrv_ops_for_front = {
	.num_leds = LED_PIN_COUNT_FRONT,
	.get_led_brightness = lp5569_get_led_brightness,
	.set_led_brightness = lp5569_set_led_brightness,
	.update = lp5569_led_update,
	.power_enable = lp5569_led_power_enable,
	.get_max_current = lp5569_get_max_current,
	.set_max_current = lp5569_set_max_current
};

static struct leddrv_func leddrv_ops_for_back = {
	.num_leds = LED_PIN_COUNT_BACK,
	.get_led_brightness = lp5569_get_led_brightness,
	.set_led_brightness = lp5569_set_led_brightness,
	.update = lp5569_led_update,
	.power_enable = lp5569_led_power_enable,
	.get_max_current = lp5569_get_max_current,
	.set_max_current = lp5569_set_max_current
};

static int lp5569_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct lp5569_priv * priv;
	int rc;

	priv = devm_kzalloc(&client->dev, sizeof(struct lp5569_priv), GFP_KERNEL);

	if (!priv) {
		dev_err(&client->dev, " -ENOMEM\n");
		rc = -ENOMEM;
		goto err;
	}

	priv->client = client;
	priv->dev = &client->dev;

	led_groups[GROUP_INDEX_BACK].priv = priv;
	led_groups[GROUP_INDEX_FRONT].priv = priv;

	priv->led_group[GROUP_INDEX_BACK] = &led_groups[GROUP_INDEX_BACK];
	priv->led_group[GROUP_INDEX_FRONT] = &led_groups[GROUP_INDEX_FRONT];

#if defined(ENABLE_CALIBRATE_PWM)
	priv->use_calibrate_pwm = 1;
#endif

	i2c_set_clientdata(client, priv);
	dev_set_drvdata(&client->dev, priv);

	if (client->dev.of_node) {
		priv->led_enable_gpio = of_get_named_gpio(client->dev.of_node, "infr,led-enable-gpio", 0);
		if (priv->led_enable_gpio != 0 && gpio_is_valid(priv->led_enable_gpio)){
			rc = gpio_request(priv->led_enable_gpio, "led-enable"); 
			if(rc){
				dev_err(&client->dev, "request led-enable gpio failed, rc=%d\n", rc); 
				goto err;
			}			
		} else {
			dev_err(&client->dev, "invalid led-enable gpio \n"); 
			rc = -EINVAL;
			goto err;
		}
	}

	INIT_WORK(&priv->led_update_work, lp5569_led_update_work);
	INIT_WORK(&priv->led_power_enable_work, lp5569_led_power_enable_work);

	if(_lp5569_led_power_enable(client, 1) == 0){
		priv->in_use[GROUP_INDEX_BACK] = priv->in_use[GROUP_INDEX_FRONT] = 1;

		leddrv_ops_for_back.dev = (struct device *)priv->led_group[GROUP_INDEX_BACK];
		leddrv_ops_for_front.dev = (struct device *)priv->led_group[GROUP_INDEX_FRONT];	

		if(register_leddrv_func(&leddrv_ops_for_front) != 0) {
			dev_err(&client->dev, "register_leddrv_func() failed\n"); 
		}
		if(register_backled_leddrv_func(&leddrv_ops_for_back) != 0) {
			dev_err(&client->dev, "register_backled_leddrv_func() failed\n"); 
		}
	}

	rc = sysfs_create_group(&client->dev.kobj, &lp5569_attribute_group);
	if (rc < 0) {
		dev_err(&client->dev, "sysfs registration failed\n");
		goto err;
	}

	return rc;


err:
	return rc;
}

static int lp5569_i2c_remove(struct i2c_client *client)
{
	struct lp5569_priv * priv = i2c_get_clientdata(client);

	dev_info(priv->dev, "%s\n", __func__);

	sysfs_remove_group(&priv->dev->kobj, &lp5569_attribute_group);

	if (priv->led_enable_gpio != 0 && gpio_is_valid(priv->led_enable_gpio)){
		gpio_free(priv->led_enable_gpio);
		priv->led_enable_gpio = 0;
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int lp5569_suspend(struct device *dev)
{
	return 0;
}

static int lp5569_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct i2c_device_id lp5569_i2c_id[] = {
	{"lp5569", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lp5569_i2c_id);

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops lp5569_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lp5569_suspend, lp5569_resume)
};
#endif

#if defined(CONFIG_OF)
static const struct of_device_id lp5569_of_match[] = {
	{.compatible = "infr,lp5569"},
	{},
};

MODULE_DEVICE_TABLE(of, lp5569_of_match);
#endif

static struct i2c_driver lp5569_i2c_driver = {
	.driver = {
		.name = "lp5569",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm = &lp5569_pm_ops,
#endif
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(lp5569_of_match),
#endif
	},
	.probe = lp5569_i2c_probe,
	.remove = lp5569_i2c_remove,
	.id_table = lp5569_i2c_id,
};
module_i2c_driver(lp5569_i2c_driver);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("LP5569 LED IC control driver");
MODULE_LICENSE("GPL v2");

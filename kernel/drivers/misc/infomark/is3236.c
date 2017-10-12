
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
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>

#include "ledanim.h"

#define LED_PIN_COUNT 36

#define IS3236A_SHUTDOWN_REG_ADDR		0x00
#define IS3236A_PWM_UPDATE_REG_ADDR		0x25
#define IS3236A_OUTPUT_FREQ_REG_ADDR	0x4B

#define IS3236A_PWM_REG_START_ADDR	0x01
#define IS3236A_CTRL_REG_START_ADDR	0x26

#define IS3236A_OUTPUT_CURRENT_MAX			0x00
#define IS3236A_OUTPUT_CURRENT_MAX_DIV_2	0x02
#define IS3236A_OUTPUT_CURRENT_MAX_DIV_3	0x04
#define IS3236A_OUTPUT_CURRENT_MAX_DIV_4	0x06

#define IS3236A_OUTPUT_CURRENT	IS3236A_OUTPUT_CURRENT_MAX_DIV_3

struct is3236_priv {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;

	int led_enable_gpio;

	char is_power_on;
	char request_power_on;

	unsigned char led_brightness[LED_PIN_COUNT];

	struct work_struct	led_update_work;
	struct work_struct	led_power_enable_work;

	unsigned char max_current;

};

static bool is3236_volatile(struct device *dev, unsigned int reg)
{
	return true;
}

static bool is3236_writeable(struct device *dev, unsigned int reg)
{
	return true;
}

static const struct regmap_config is3236_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = is3236_writeable,
	.volatile_reg = is3236_volatile,
	.cache_type = REGCACHE_NONE,
	.max_register = 128,
};

static int is3236_i2c_write_device(struct is3236_priv *priv,
				    unsigned char reg,
				    unsigned char value)
{
	int rc = 0;

	rc = regmap_write(priv->regmap, reg, value);
	if (rc < 0) {
		dev_err(priv->dev, "%s Error reg:0x%02x\n", __func__, reg);
	} else {
		dev_dbg(priv->dev, "%s OK reg:0x%02x value:%d OK\n", __func__, reg, value);
	}

	return rc;
}

static int is3236_apply_max_current(struct is3236_priv *priv)
{
	int i;
	for(i = 0; i < LED_PIN_COUNT; i++){
		is3236_i2c_write_device(priv, IS3236A_CTRL_REG_START_ADDR + i, 
			((priv->is_power_on ? 1 : 0) | (priv->max_current << 1)));
	}

	return 0;
}

static int is3236_get_led_brightness(struct device *dev, int index, unsigned char * value)
{	
	struct is3236_priv * priv = dev_get_drvdata(dev);

	if(index < 0 || index >= LED_PIN_COUNT)
		return -1;

	*value = priv->led_brightness[index];
	return 0;
}

static int is3236_set_led_brightness(struct device *dev, int index, unsigned char value)
{	
	struct is3236_priv * priv = dev_get_drvdata(dev);

	if(index < 0 || index >= LED_PIN_COUNT)
		return -1;

	priv->led_brightness[index] = value;
	return 0;
}



static int is3236_led_update(struct device *dev)
{
	struct is3236_priv * priv = dev_get_drvdata(dev);
	if(priv)
		schedule_work(&priv->led_update_work);

	return 0;
}

static int is3236_led_power_enable(struct device *dev, int enable)
{
	struct is3236_priv * priv = dev_get_drvdata(dev);
	if(priv) {
		priv->request_power_on = (enable ? 1 : 0);
		schedule_work(&priv->led_power_enable_work);
	}
	return 0;
}


static int is3236_get_max_current(struct device *dev, unsigned char *value)
{
	struct is3236_priv * priv = dev_get_drvdata(dev);
	if(priv) {
		switch(priv->max_current){
			case 0:
				*value = 255;
				break;
			case 1:
				*value = 128;
				break;
			case 2:
				*value = 85;
				break;
			case 3:
				*value = 64;
				break;
		}		
	}
	return 0;
}

static int is3236_set_max_current(struct device *dev, unsigned char value)
{
	struct is3236_priv * priv = dev_get_drvdata(dev);
	if(priv) {
		unsigned char v;
		if(value <= 64){
			v = 3;
		} else if(value <= 85){
			v = 2;
		} else if(value <= 128) {
			v = 1;
		} else {
			v = 0;
		}
		if(v != priv->max_current){
			priv->max_current = v;
			is3236_apply_max_current(priv);
		}
	}
	return 0;
}

static void is3236_led_update_work(struct work_struct *work)
{
	int i;
	struct is3236_priv *priv = container_of(work, struct is3236_priv, led_update_work);

	if(priv->is_power_on == 0){
		dev_err(priv->dev, "IS3236A is not power on\n");
		return;
	}

	for(i = 0 ; i < LED_PIN_COUNT; i++){
		is3236_i2c_write_device(priv, IS3236A_PWM_REG_START_ADDR + i, priv->led_brightness[i]);
	}	
	is3236_i2c_write_device(priv, IS3236A_PWM_UPDATE_REG_ADDR, 0xff);
}

static int _is3236_led_power_enable(struct i2c_client *client, int enable) 
{
	struct is3236_priv * priv = i2c_get_clientdata(client);

	if(enable){	
		if(priv->is_power_on){
			dev_dbg(priv->dev, "IS3236A already powered on\n");
			return 0;
		}
		dev_dbg(priv->dev, "led-enable gpio set to high\n");
		gpio_direction_output(priv->led_enable_gpio, 1);

		if(is3236_i2c_write_device(priv, IS3236A_SHUTDOWN_REG_ADDR, 1) == 0){
			int i;

			dev_dbg(priv->dev, "IS3236A power on success\n");
			is3236_i2c_write_device(priv, IS3236A_OUTPUT_FREQ_REG_ADDR, 1);
			priv->is_power_on = 1;

			for(i = 0; i < LED_PIN_COUNT; i++){
				is3236_i2c_write_device(priv, IS3236A_CTRL_REG_START_ADDR + i, (1 | (priv->max_current << 1)));
			}
		} else {
			dev_err(priv->dev, "IS3236A power on failed");
			priv->is_power_on = 0;
			gpio_direction_output(priv->led_enable_gpio, 0);
			return -1;
		}
	} else {
		if(priv->is_power_on == 0){
			dev_dbg(priv->dev, "IS3236A already powered off\n");
			return 0;
		}
		
		if(is3236_i2c_write_device(priv, IS3236A_SHUTDOWN_REG_ADDR, 0) != 0){
			dev_err(priv->dev, "IS3236A power on failed");
		}
		gpio_direction_output(priv->led_enable_gpio, 0);
		priv->is_power_on = 0;
	}
	return 0;
}

static void is3236_led_power_enable_work(struct work_struct *work)
{
	struct is3236_priv *priv = container_of(work, struct is3236_priv, led_power_enable_work);
	_is3236_led_power_enable(priv->client, (priv->request_power_on ? 1 : 0));
}

////////////////////////////////////////////////////////////////////////////
// sysfs attr

static ssize_t is3236_max_current_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct is3236_priv * priv = dev_get_drvdata(dev);
	if(!priv) return 0;

	return sprintf(buf, "%u\n", priv->max_current);
}

static ssize_t is3236_max_current_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct is3236_priv * priv = dev_get_drvdata(dev);
	unsigned long value;	

	if(!priv) 
		return -EINVAL;

	if (strict_strtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 3)
		return -EINVAL;

	if((unsigned char)value != priv->max_current){
		priv->max_current = (unsigned char)value;		
		is3236_apply_max_current(priv);
		if(priv->is_power_on){
			is3236_led_update(priv->dev);
		}
	}

	return len;
}
static DEVICE_ATTR(max_current, S_IRUGO | S_IWUSR,
		is3236_max_current_show,
		is3236_max_current_store);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_max_current.attr,
	NULL
};

static struct attribute_group is3236_attribute_group = {
	.attrs = sysfs_attrs
};
////////////////////////////////////////////////////////////////////////////

static struct leddrv_func leddrv_ops = {
	.num_leds = LED_PIN_COUNT,
	.get_led_brightness = is3236_get_led_brightness,
	.set_led_brightness = is3236_set_led_brightness,
	.update = is3236_led_update,
	.power_enable = is3236_led_power_enable,
	.get_max_current = is3236_get_max_current,
	.set_max_current = is3236_set_max_current
};

static int is3236_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{	
	struct is3236_priv * priv;
	int rc;

	priv = devm_kzalloc(&client->dev, sizeof(struct is3236_priv), GFP_KERNEL);

	if (!priv) {
		dev_err(&client->dev, " -ENOMEM\n");
		rc = -ENOMEM;
		goto err;
	}

	priv->client = client;
	priv->dev = &client->dev;
	i2c_set_clientdata(client, priv);
	dev_set_drvdata(&client->dev, priv);

	priv->max_current = IS3236A_OUTPUT_CURRENT >> 1;

	priv->regmap = devm_regmap_init_i2c(client, &is3236_i2c_regmap);

	if (IS_ERR(priv->regmap)) {
		rc = PTR_ERR(priv->regmap );
		dev_err(&client->dev, "Failed to allocate register map: %d\n", rc);
		goto err;
	}

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

	INIT_WORK(&priv->led_update_work, is3236_led_update_work);
	INIT_WORK(&priv->led_power_enable_work, is3236_led_power_enable_work);

	if(_is3236_led_power_enable(client, 1) == 0){
		leddrv_ops.dev = priv->dev;
		if(register_leddrv_func(&leddrv_ops) != 0) {
			dev_err(&client->dev, "register_leddrv_func() failed\n"); 
		}
	}

	rc = sysfs_create_group(&client->dev.kobj, &is3236_attribute_group);
	if (rc < 0) {
		dev_err(&client->dev, "sysfs registration failed\n");
		goto err;
	}

	return rc;


err:
	return rc;
}

static int is3236_i2c_remove(struct i2c_client *client)
{
	struct is3236_priv * priv = i2c_get_clientdata(client);

	dev_info(priv->dev, "%s\n", __func__);

	sysfs_remove_group(&priv->dev->kobj, &is3236_attribute_group);

	if (priv->led_enable_gpio != 0 && gpio_is_valid(priv->led_enable_gpio)){
		gpio_free(priv->led_enable_gpio);
		priv->led_enable_gpio = 0;
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int is3236_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	if(client){
		_is3236_led_power_enable(client, 0);
	}
	return 0;
}

static int is3236_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	if(client){
		_is3236_led_power_enable(client, 1);
	}
	return 0;
}
#endif

static const struct i2c_device_id is3236_i2c_id[] = {
	{"is3236", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, is3236_i2c_id);

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops is3236_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(is3236_suspend, is3236_resume)
};
#endif

#if defined(CONFIG_OF)
static const struct of_device_id is3236_of_match[] = {
	{.compatible = "infr,is3236a"},
	{},
};

MODULE_DEVICE_TABLE(of, is3236_of_match);
#endif

static struct i2c_driver is3236_i2c_driver = {
	.driver = {
		.name = "is3236",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm = &is3236_pm_ops,
#endif
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(is3236_of_match),
#endif
	},
	.probe = is3236_i2c_probe,
	.remove = is3236_i2c_remove,
	.id_table = is3236_i2c_id,
};
module_i2c_driver(is3236_i2c_driver);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("IS3236A LED IC control driver");
MODULE_LICENSE("GPL v2");

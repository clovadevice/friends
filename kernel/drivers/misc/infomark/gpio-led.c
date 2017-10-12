#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/err.h>

#include "ledanim.h"

#define LED_PIN_COUNT 3

enum {
	LED_RED = 0,
	LED_GREEN,
	LED_BLUE
};

struct gpio_led_priv {
	struct device *dev;

	int gpios[LED_PIN_COUNT];
	unsigned char led_brightness[LED_PIN_COUNT];
	unsigned char led_brightness_old[LED_PIN_COUNT];
};

static int gpio_led_get_led_brightness(struct device *dev, int index, unsigned char * value)
{	
	struct gpio_led_priv * priv = dev_get_drvdata(dev);
	if(index < 0 || index >= LED_PIN_COUNT)
		return -1;
	*value = priv->led_brightness[index];
	return 0;
}

static int gpio_led_set_led_brightness(struct device *dev, int index, unsigned char value)
{	
	struct gpio_led_priv * priv = dev_get_drvdata(dev);
	if(index < 0 || index >= LED_PIN_COUNT)
		return -1;

	priv->led_brightness[index] = value ? 1 : 0;
	return 0;
}

static int gpio_led_led_update(struct device *dev)
{
	int i;
	struct gpio_led_priv * priv = dev_get_drvdata(dev);

	for(i = 0 ; i < LED_PIN_COUNT; i++){
		if(priv->led_brightness_old[i] != priv->led_brightness[i]){
			gpio_direction_output(priv->gpios[i], priv->led_brightness[i]);
			priv->led_brightness_old[i] = priv->led_brightness[i];
		}
	}
	return 0;
}

static int gpio_led_led_power_enable(struct device *dev, int enable)
{
	return 0;
}

static int gpio_led_get_max_current(struct device *dev, unsigned char *value)
{
	return 0;
}

static int gpio_led_set_max_current(struct device *dev, unsigned char value)
{
	return 0;
}

static struct leddrv_func leddrv_ops = {
	.num_leds = LED_PIN_COUNT,
	.get_led_brightness = gpio_led_get_led_brightness,
	.set_led_brightness = gpio_led_set_led_brightness,
	.update = gpio_led_led_update,
	.power_enable = gpio_led_led_power_enable,
	.get_max_current = gpio_led_get_max_current,
	.set_max_current = gpio_led_set_max_current
};



static struct gpio_led_priv *gpio_led_create_of(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct gpio_led_priv *priv;	
	char name[24] = {0,};
	int i, rc;	

	priv = devm_kzalloc(&pdev->dev, sizeof(struct gpio_led_priv), GFP_KERNEL);
	if (!priv)
		return ERR_PTR(-ENOMEM);

	priv->dev = &pdev->dev;
	priv->gpios[LED_RED] = of_get_named_gpio(np, "infr,gpio-led-red", 0);
	priv->gpios[LED_GREEN] = of_get_named_gpio(np, "infr,gpio-led-green", 0);
	priv->gpios[LED_BLUE] = of_get_named_gpio(np, "infr,gpio-led-blue", 0);

	
	for(rc = 0, i = 0 ; i < LED_PIN_COUNT; i++){
		if(priv->gpios[i] == 0 || !gpio_is_valid(priv->gpios[i])){
			rc = 1; break;
		}
	}
	if(rc){
		kfree(priv);
		return ERR_PTR(-ENODEV);
	}

	for(i = 0 ; i < LED_PIN_COUNT; i++){
		snprintf(name, sizeof(name) - 1, "gpio-led-%d", i);
		rc = gpio_request(priv->gpios[i], name);
		if(rc){
			dev_err(priv->dev, "request gpio-led-%d failed, rc=%d\n", i, rc);
			for(--i; i >= 0; i--){
				gpio_free(priv->gpios[i]);
				priv->gpios[i] = 0;
			}			
			break;
		}
	}
	if(rc){
		kfree(priv);
		return ERR_PTR(-ENODEV);
	}

	return priv;
}

static int gpio_led_probe(struct platform_device *pdev)
{
	struct gpio_led_priv *priv;
	struct pinctrl *pinctrl;

	pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pinctrl))
		dev_warn(&pdev->dev, "devm_pinctrl_get_select_default failed\n");

	priv = gpio_led_create_of(pdev);
	if (IS_ERR(priv))
		return PTR_ERR(priv);

	platform_set_drvdata(pdev, priv);

	leddrv_ops.dev = priv->dev;
	if(register_backled_leddrv_func(&leddrv_ops) != 0) {
		dev_err(priv->dev, "register_leddrv_func() failed\n"); 
	}
	return 0;
}

static int gpio_led_remove(struct platform_device *pdev)
{
	struct gpio_led_priv *priv = platform_get_drvdata(pdev);
	if(priv){
		int i;
		for(i = 0 ; i < LED_PIN_COUNT; i++){
			if(priv->gpios[i] != 0){
				gpio_free(priv->gpios[i]);
				priv->gpios[i] = 0;
			}
		}
		kfree(priv);
		priv = NULL;
	}
	platform_set_drvdata(pdev, NULL);
	return 0;
}


static const struct of_device_id of_gpio_led_match[] = {
	{ .compatible = "infr,gpio-led", },
	{},
};

static struct platform_driver gpio_led_driver = {
	.probe		= gpio_led_probe,
	.remove		= gpio_led_remove,
	.driver		= {
		.name	= "gpio-led",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(of_gpio_led_match),
	},
};

module_platform_driver(gpio_led_driver);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("GPIO LED control driver");
MODULE_LICENSE("GPL v2");

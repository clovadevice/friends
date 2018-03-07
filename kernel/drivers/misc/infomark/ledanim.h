#ifndef __LEDANIM_H__
#define __LEDANIM_H__


#define LED_NAME_LEN	20

struct ledanim_t {
	char name[LED_NAME_LEN];
	short interval;
	unsigned long anim_count;
	unsigned char * anim;
	unsigned long repeat;
};

struct led_brightness_t {
	int index;
	unsigned char red;
	unsigned char green;
	unsigned char blue;
};

struct led_blinking_t {
	struct led_brightness_t led_brightness;
	unsigned long on_period;
	unsigned long off_period;
};


struct leddrv_func {
	int num_leds;
	struct device *dev;
	int (*get_led_brightness)(struct device *dev, int index, unsigned char * value);
	int (*set_led_brightness)(struct device *dev, int index, unsigned char value);
	int (*update)(struct device *dev);
	int (*power_enable)(struct device *dev, int enable);
	int (*get_max_current)(struct device *dev, unsigned char * value);
	int (*set_max_current)(struct device *dev, unsigned char value);
};

int register_leddrv_func(struct leddrv_func * func);
int register_backled_leddrv_func(struct leddrv_func * func);

struct led_priority_t {
    int priority;
};

struct led_blinking_priority_t {
	struct led_blinking_t led_blinking;
	struct led_priority_t led_priority;
	bool status;
};

enum {
	STOP_LED = 0,
	RUNNING_LED
};

enum {
	PRIORITY_HIGH = 1,
	PRIORITY_SAME,
	PRIORITY_LOW
};

#define LEDANIM_IOCTL_NUM	'L'

#define LED_GET_BRIGHTNESS	_IOWR(LEDANIM_IOCTL_NUM, 1, void *)
#define LED_SET_BRIGHTNESS	_IOWR(LEDANIM_IOCTL_NUM, 2, void *)
#define LED_ANIM_START		_IOWR(LEDANIM_IOCTL_NUM, 3, void *)
#define LED_ANIM_STOP		_IOWR(LEDANIM_IOCTL_NUM, 4, void *)
#define LED_GET_COUNT		_IOWR(LEDANIM_IOCTL_NUM, 5, void *)
#define LED_SET_FRAME		_IOWR(LEDANIM_IOCTL_NUM, 6, void *)
#define LED_SET_PRIORITY	_IOWR(LEDANIM_IOCTL_NUM, 7, void *)
#define LED_BLINKING		_IOWR(LEDANIM_IOCTL_NUM, 8, void *)
#define LED_SET_BRIGHTNESS_PRIORITY	_IOWR(LEDANIM_IOCTL_NUM, 9, void *)

/* [START] 2017.08.21 kkkim Led*/
#define LED_EMER    100
#define LED_HIGHEST 90
#define LED_HIGH    70
#define LED_NORMAL  50
#define LED_LOW     30
#define LED_LOWEST  10
#define LED_DEFAULT 0
#define LED_INIT    255
/* [END] 2017.08.21 kkkim Led*/

#define LED_PRIORITY_COUNT	8

#endif

/*
 *  max17048_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2012 Nvidia Cooperation
 *  Chandler Zhang <chazhang@nvidia.com>
 *  Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 *  Copyright (C) 2013 LGE Inc.
 *  ChoongRyeol Lee <choongryeol.lee@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/power/max17048_battery.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/debugfs.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#define MODE_REG      0x06
#define VCELL_REG     0x02
#define SOC_REG       0x04
#define VERSION_REG   0x08
#define HIBRT_REG     0x0A
#define CONFIG_REG    0x0C
#define OCV_REG       0x0E
#define VALRT_REG     0x14
#define CRATE_REG     0x16
#define VRESET_REG    0x18
#define STATUS_REG    0x1A
#define UNLOCK_REG    0x3E
#define TABLE_REG     0x40
#define RCOMPSEG_REG  0x80
#define MAX17048_CMD  0xFF
#define CFG_ALRT_MASK    0x0020
#define CFG_ATHD_MASK    0x001F
#define CFG_ALSC_MASK    0x0040
#define CFG_RCOMP_MASK    0xFF00
#define CFG_RCOMP_SHIFT    8
#define CFG_ALSC_SHIFT   6
#define STAT_RI_MASK     0x0100
#define STAT_CLEAR_MASK  0xFF00
#define MAX17048_VERSION_11    0x11
#define MAX17048_VERSION_12    0x12
#define MAX17048_UNLOCK_VALUE	0x4a57
#define MAX17048_RESET_VALUE	0x5400

#define MAX17048_DELAY		1000

#define CUSTOM_MODEL_SIZE	64
#define RCOMP_SEG_SIZE		32

//#define MAX17048_DEBUG

struct max17048_chip {
	struct i2c_client *client;
	struct power_supply batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *smb_psy;
	struct max17048_platform_data *pdata;
	struct dentry *dent;
	struct notifier_block pm_notifier;
	struct delayed_work monitor_work;
	struct device *dev;
	int vcell;
	int soc;
	int capacity_level;
	int rcomp;
	int rcompseg;
	int rcomp_co_hot;
	int rcomp_co_cold;
	int alert_threshold;
	int max_mvolt;
	int min_mvolt;
	int full_soc;
	int empty_soc;
	int batt_tech;
	int fcc_mah;
	int voltage;
	int lasttime_voltage;
	int lasttime_capacity_level;
	int chg_state;
	int batt_temp;
	int lasttime_batt_temp;
	int batt_health;
	int batt_current;
	int model_soccheck_A;
	int model_soccheck_B;
	int ocv_test;
};

uint8_t max17048_custom_model_data[CUSTOM_MODEL_SIZE];
uint8_t max17048_rcomp_seg[RCOMP_SEG_SIZE];

uint16_t g_rcomp, g_ocv;

static struct max17048_chip *ref;
static int max17048_get_prop_current(struct max17048_chip *chip);
static int max17048_get_prop_temp(struct max17048_chip *chip);
static int max17048_clear_interrupt(struct max17048_chip *chip);
struct debug_reg {
	char  *name;
	u8  reg;
};

static int bound_check(int max, int min, int val)
{
	val = max(min, val);
	val = min(max, val);
	return val;
}
static int adjust_check(int max, int min, int val)
{
	if (val > max) {
		val = 100;
	}
	else if (val < min) {
		val = 0;
	}

	return val;
}

static int max17048_write_word(struct i2c_client *client, int reg, u16 value)
{
	int ret;
	ret = i2c_smbus_write_word_data(client, reg, swab16(value));
	if (ret < 0)
		dev_err(&client->dev, "%s(): Failed in writing register"
					"0x%02x err %d\n", __func__, reg, ret);
	return ret;
}
static int max17048_read_word(struct i2c_client *client, int reg)
{
	int ret;
	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s(): Failed in reading register"
					"0x%02x err %d\n", __func__, reg, ret);
	else
		ret = (int)swab16((uint16_t)(ret & 0x0000ffff));
	return ret;
}
static int max17048_masked_write_word(struct i2c_client *client, int reg,
			       u16 mask, u16 val)
{
	s32 rc;
	u16 temp;
	temp = max17048_read_word(client, reg);
	if (temp < 0) {
		pr_err("max17048_read_word failed: reg=%03X, rc=%d\n",
				reg, temp);
		return temp;
	}
	if ((temp & mask) == (val & mask))
		return 0;
	temp &= ~mask;
	temp |= val & mask;
	rc = max17048_write_word(client, reg, temp);
	if (rc) {
		pr_err("max17048_write_word failed: reg=%03X, rc=%d\n",
				reg, rc);
		return rc;
	}
	return 0;
}

/* Using Quickstart instead of reset for Power Test
*  DO NOT USE THIS COMMAND ANOTHER SCENE.
*/
/*static int max17048_set_reset(struct max17048_chip *chip)
{
	max17048_write_word(chip->client, MODE_REG, 0x4000);
	pr_info("%s: Reset (Quickstart)\n", __func__);
	return 0;
}*/
static int max17048_get_capacity_from_soc(struct max17048_chip *chip)
{
	u8 buf[2];
	int batt_soc = 0;
	buf[0] = (chip->soc & 0x0000FF00) >> 8;
	buf[1] = (chip->soc & 0x000000FF);
	pr_debug("%s: SOC raw = 0x%x%x\n", __func__, buf[0], buf[1]);
	batt_soc = (((int)buf[0]*256)+buf[1])*19531; // 0.001953125 19bit
//	batt_soc = (((int)buf[0]*256)+buf[1])*39062; // 0.00390625  18bit
	batt_soc = (batt_soc - (chip->empty_soc * 1000000))
			/ ((chip->full_soc - chip->empty_soc) * 10000);
	batt_soc = bound_check(100, 0, batt_soc);
	batt_soc = adjust_check(100, 0, batt_soc);
	return batt_soc;
}
static int max17048_get_vcell(struct max17048_chip *chip)
{
	int vcell;
	vcell = max17048_read_word(chip->client, VCELL_REG);
	if (vcell < 0) {
		pr_err("%s: err %d\n", __func__, vcell);
		return vcell;
	} else {
		chip->vcell = vcell >> 4;
		chip->voltage = (chip->vcell * 5) >> 2;
	}
	return 0;
}

static int max17048_get_soc(struct max17048_chip *chip)
{
	int soc;
	soc = max17048_read_word(chip->client, SOC_REG);
	if (soc < 0) {
		pr_err("%s: err %d\n", __func__, soc);
		return soc;
	} else {
		chip->soc = soc;
		chip->capacity_level = max17048_get_capacity_from_soc(chip);
#ifdef MAX17048_DEBUG 
		pr_err("%s: chip->soc=%d, chip->capacity_level=%d\n", __func__, chip->soc, chip->capacity_level);
#endif
	}
	return 0;
}
static uint16_t max17048_get_version(struct max17048_chip *chip)
{
	return (uint16_t) max17048_read_word(chip->client, VERSION_REG);
}
static int max17048_set_rcomp(struct max17048_chip *chip)
{
	int ret;
	int scale_coeff;
	int rcomp;
	int temp;
	temp = chip->batt_temp / 10;
	if (temp > 20)
		scale_coeff = chip->rcomp_co_hot;
	else if (temp < 20)
		scale_coeff = chip->rcomp_co_cold;
	else
		scale_coeff = 0;
	rcomp = chip->rcomp * 1000 - (temp-20) * scale_coeff;
	rcomp = bound_check(255, 0, rcomp / 1000);
#ifdef MAX17048_DEBUG 
	pr_err("%s: new RCOMP = 0x%02X\n", __func__, rcomp);
#endif
	rcomp = rcomp << CFG_RCOMP_SHIFT;
	ret = max17048_masked_write_word(chip->client,
			CONFIG_REG, CFG_RCOMP_MASK, rcomp);
	if (ret < 0)
		pr_err("%s: failed to set rcomp\n", __func__);
	return ret;
}

static bool get_usb_status(struct max17048_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (chip->usb_psy) {
		chip->usb_psy->get_property(chip->usb_psy, POWER_SUPPLY_PROP_PRESENT , &ret);
	}

	return ret.intval;
}

static void check_cutoff(struct max17048_chip *chip)
{
	if ((chip->capacity_level <= 0) || (chip->voltage <= 3400)) {
		pm_wakeup_event(chip->dev, 3000);
		power_supply_changed(&chip->batt_psy);
	}
	else if ((chip->voltage <= 3300) && (chip->capacity_level > 5) && (get_usb_status(chip) == false)) {
		chip->capacity_level = 5;
		power_supply_changed(&chip->batt_psy);
	}
}

static void max17048_work(struct work_struct *work)
{
	struct max17048_chip *chip =
		container_of(work, struct max17048_chip, monitor_work.work);
	int ret = 0, voltage_diff, temp_diff;

	max17048_get_prop_temp(chip);
	max17048_get_prop_current(chip);
	ret = max17048_set_rcomp(chip);
	if (ret)
		pr_err("%s : failed to set rcomp\n", __func__);
	max17048_get_vcell(chip);
	max17048_get_soc(chip);
	if (chip->voltage < chip->lasttime_voltage) {
		voltage_diff = chip->lasttime_voltage - chip->voltage;
	}
	else if (chip->lasttime_voltage < chip->voltage) {
		voltage_diff = chip->voltage - chip->lasttime_voltage;
	}
	else {
		voltage_diff = 0;
	}

	if (chip->batt_temp < chip->lasttime_batt_temp)	{
		temp_diff = chip->lasttime_batt_temp - chip->batt_temp;
	}
	else if (chip->lasttime_batt_temp < chip->batt_temp) {
		temp_diff = chip->batt_temp - chip->lasttime_batt_temp;
	}
	else {
		temp_diff = 0;
	}

	if (voltage_diff >= 50 || chip->capacity_level != chip->lasttime_capacity_level || temp_diff >= 5) {
		chip->lasttime_voltage = chip->voltage;
		chip->lasttime_capacity_level = chip->capacity_level;
		chip->lasttime_batt_temp = chip->batt_temp;
		power_supply_changed(&chip->batt_psy);
		check_cutoff(chip);
	}

	ret = max17048_clear_interrupt(chip);
	if (ret < 0)
		pr_err("%s : error clear alert irq register.\n", __func__);
#ifdef MAX17048_DEBUG 
	pr_err("%s: raw soc = 0x%04X raw vcell = 0x%04X\n", __func__, chip->soc, chip->vcell);
	pr_err("%s: SOC = %d vbatt_mv = %d\n", __func__, chip->capacity_level, chip->voltage);
	pr_err("%s: ibatt_ua = %d batt_temp = %d\n", __func__, chip->batt_current, chip->batt_temp);
#endif
	schedule_delayed_work(&chip->monitor_work, MAX17048_DELAY);
}

static int max17048_clear_interrupt(struct max17048_chip *chip)
{
	int ret;
	pr_debug("%s.\n", __func__);
	ret = max17048_masked_write_word(chip->client,
			CONFIG_REG, CFG_ALRT_MASK, 0);
	if (ret < 0) {
		pr_err("%s: failed to clear alert status bit\n", __func__);
		return ret;
	}
	ret = max17048_masked_write_word(chip->client,
			STATUS_REG, STAT_CLEAR_MASK, 0);
	if (ret < 0) {
		pr_err("%s: failed to clear status reg\n", __func__);
		return ret;
	}
	return 0;
}
static int max17048_set_athd_alert(struct max17048_chip *chip, int level)
{
	int ret;
	pr_debug("%s.\n", __func__);
	level = bound_check(32, 1, level);
	level = 32 - level;
	ret = max17048_masked_write_word(chip->client,
			CONFIG_REG, CFG_ATHD_MASK, level);
	if (ret < 0)
		pr_err("%s: failed to set athd alert\n", __func__);
	return ret;
}
static int max17048_set_alsc_alert(struct max17048_chip *chip, bool enable)
{
	int ret;
	u16 val;
	pr_debug("%s. with %d\n", __func__, enable);
	val = (u16)(!!enable << CFG_ALSC_SHIFT);
	ret = max17048_masked_write_word(chip->client,
			CONFIG_REG, CFG_ALSC_MASK, val);
	if (ret < 0)
		pr_err("%s: failed to set alsc alert\n", __func__);
	return ret;
}
/*ssize_t max17048_store_status(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf,
			  size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max17048_chip *chip = i2c_get_clientdata(client);
	if (!chip)
		return -ENODEV;
	if (strncmp(buf, "reset", 5) == 0) {
		max17048_set_reset(chip);
		schedule_delayed_work(&chip->monitor_work, 0);
	} else {
		return -EINVAL;
	}
	return count;
}
*/
static int max17048_parse_dt(struct device *dev,
		struct max17048_chip *chip)
{
	struct device_node *dev_node = dev->of_node;
	int ret = 0;
	int prop_len;

	////
#ifdef MAX17048_DEBUG 
	int i;
#endif
	ret = of_property_read_u32(dev_node, "max17048,rcomp",
				&chip->rcomp);
	if (ret) {
		pr_err("%s: failed to read rcomp\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG 
	pr_err("%s: rcomp=%d\n", __func__, chip->rcomp);
#endif

	chip->rcompseg = 0;
	ret = of_property_read_u32(dev_node, "max17048,rcompseg",
				&chip->rcompseg);
	if (ret) {
		pr_err("%s: failed to read rcompseg", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG 
	pr_err("%s: rcompseg=%d\n", __func__, chip->rcompseg);
#endif

	ret = of_property_read_u32(dev_node, "max17048,rcomp-co-hot",
				&chip->rcomp_co_hot);
	if (ret) {
		pr_err("%s: failed to read rcomp_co_hot\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG 
	pr_err("%s: rcomp_co_hot=%d\n", __func__, chip->rcomp_co_hot);
#endif

	ret = of_property_read_u32(dev_node, "max17048,rcomp-co-cold",
				&chip->rcomp_co_cold);
	if (ret) {
		pr_err("%s: failed to read rcomp_co_cold\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: rcomp_co_cold=%d\n", __func__, chip->rcomp_co_cold);
#endif

	ret = of_property_read_u32(dev_node, "max17048,alert_threshold",
				&chip->alert_threshold);
	if (ret) {
		pr_err("%s: failed to read alert_threshold\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: alert_threshold=%d\n", __func__, chip->alert_threshold);
#endif

	ret = of_property_read_u32(dev_node, "max17048,max-mvolt",
				   &chip->max_mvolt);
	if (ret) {
		pr_err("%s: failed to read max voltage\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: max_mvolt=%d\n", __func__, chip->max_mvolt);
#endif

	ret = of_property_read_u32(dev_node, "max17048,min-mvolt",
				   &chip->min_mvolt);
	if (ret) {
		pr_err("%s: failed to read min voltage\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: min_mvolt=%d\n", __func__, chip->min_mvolt);
#endif

	ret = of_property_read_u32(dev_node, "max17048,full-soc",
				   &chip->full_soc);
	if (ret) {
		pr_err("%s: failed to read full soc\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: full_soc=%d\n", __func__, chip->full_soc);
#endif

	ret = of_property_read_u32(dev_node, "max17048,empty-soc",
				   &chip->empty_soc);
	if (ret) {
		pr_err("%s: failed to read empty soc\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: empty_soc=%d\n", __func__, chip->empty_soc);
#endif

	ret = of_property_read_u32(dev_node, "max17048,batt-tech",
				   &chip->batt_tech);
	if (ret) {
		pr_err("%s: failed to read batt technology\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: batt_tech=%d\n", __func__, chip->batt_tech);
#endif

	ret = of_property_read_u32(dev_node, "max17048,fcc-mah",
				   &chip->fcc_mah);
	if (ret) {
		pr_err("%s: failed to read batt fcc\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: fcc_mah=%d\n", __func__, chip->fcc_mah);
#endif

	ret = of_property_read_u32(dev_node, "max17048,soc-check-a",
				   &chip->model_soccheck_A);
	if (ret) {
		pr_err("%s: failed to read soc check A\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: model_soccheck_A=0x%x\n", __func__, chip->model_soccheck_A);
#endif

	ret = of_property_read_u32(dev_node, "max17048,soc-check-b",
				   &chip->model_soccheck_B);
	if (ret) {
		pr_err("%s: failed to read soc check B\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: model_soccheck_B=0x%x\n", __func__, chip->model_soccheck_B);
#endif

	ret = of_property_read_u32(dev_node, "max17048,ocv-test",
				   &chip->ocv_test);
	if (ret) {
		pr_err("%s: failed to read ocv test\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: ocv_test=0x%x\n", __func__, chip->ocv_test);
#endif

	if (!of_find_property(dev_node, "max17048,custom-model-data", &prop_len)) {
		pr_err("%s: failed to read custom-model-data\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: custom-model-data prop_len=%d\n", __func__, prop_len);
#endif

	prop_len /= sizeof(uint8_t);
	if (prop_len != CUSTOM_MODEL_SIZE) {
		pr_err("invalid length of custom-model-data expected length=%d\n",
				CUSTOM_MODEL_SIZE);
		goto out;
	}

	ret = of_property_read_u8_array(dev_node, "max17048,custom-model-data",
					max17048_custom_model_data, prop_len);
	if (ret) {
		pr_err("invalid custom-model-data ret=%d\n", ret);
		goto out;
	}
#ifdef MAX17048_DEBUG
	for (i=0; i < CUSTOM_MODEL_SIZE; i++) {
		pr_err("max17048_custom_model_data[%d]=0x%x\n", i, max17048_custom_model_data[i]);
	}
#endif

	if (!of_find_property(dev_node, "max17048,rcomp_seg", &prop_len)) {
		pr_err("%s: failed to read rcomp_seg\n", __func__);
		goto out;
	}
#ifdef MAX17048_DEBUG
	pr_err("%s: rcomp_seg prop_len=%d\n", __func__, prop_len);
#endif

	prop_len /= sizeof(uint8_t);
	if (prop_len != RCOMP_SEG_SIZE) {
		pr_err("invalid length of rcomp_seg expected length=%d\n",
				CUSTOM_MODEL_SIZE);
		goto out;
	}

	ret = of_property_read_u8_array(dev_node, "max17048,rcomp_seg",
					max17048_rcomp_seg, prop_len);
	if (ret) {
		pr_err("invalid rcomp_seg ret=%d\n", ret);
		goto out;
	}
#ifdef MAX17048_DEBUG
	for (i=0; i < RCOMP_SEG_SIZE; i++) {
		pr_err("max17048_rcomp_seg[%d]=0x%x\n", i, max17048_rcomp_seg[i]);
	}
#endif

#ifdef MAX17048_DEBUG
	pr_err("%s: rcomp = %d rcomp_co_hot = %d rcomp_co_cold = %d",
			__func__, chip->rcomp, chip->rcomp_co_hot,
			chip->rcomp_co_cold);
	pr_err("%s: alert_thres = %d full_soc = %d empty_soc = %d\n",
			__func__, chip->alert_threshold,
			chip->full_soc, chip->empty_soc);
#endif
out:
	return ret;
}

static int max17048_get_prop_status(struct max17048_chip *chip)
{
	if (get_usb_status(chip) == 1) {
#ifdef MAX17048_DEBUG
		pr_err("%s: POWER_SUPPLY_STATUS_CHARGING\n", __func__);
#endif
		return POWER_SUPPLY_STATUS_CHARGING;
	}
	else {
#ifdef MAX17048_DEBUG
		pr_err("%s: POWER_SUPPLY_STATUS_DISCHARGING\n", __func__);
#endif
		return POWER_SUPPLY_STATUS_DISCHARGING;
	}
}

static int max17048_get_prop_vbatt_uv(struct max17048_chip *chip)
{
	max17048_get_vcell(chip);
	return chip->voltage * 1000;
}
static int max17048_get_prop_present(struct max17048_chip *chip)
{
	// FIXME - need to implement
	return true;
}
#define DEFAULT_TEMP    250
/*#ifdef CONFIG_SENSORS_QPNP_ADC_VOLTAGE
static int qpnp_get_battery_temp(int *temp)
{
	int ret = 0;
	struct qpnp_vadc_result results;
	if (qpnp_vadc_is_ready()) {
		*temp = DEFAULT_TEMP;
		return 0;
	}
	ret = qpnp_vadc_read(LR_MUX1_BATT_THERM, &results);
	if (ret) {
		pr_err("%s: Unable to read batt temp\n", __func__);
		*temp = DEFAULT_TEMP;
		return ret;
	}
	*temp = (int)results.physical;
	return 0;
}
#endif*/
static int max17048_get_prop_temp(struct max17048_chip *chip)
{
	union power_supply_propval ret = {0,};

	if (chip->smb_psy) {
		chip->smb_psy->get_property(chip->smb_psy, POWER_SUPPLY_PROP_TEMP , &ret);
	}

	chip->batt_temp =  ret.intval;

#ifdef MAX17048_DEBUG
	pr_debug("%s: batt_temp = %d\n", __func__, chip->batt_temp);
#endif

	return chip->batt_temp;
}
/*#ifdef CONFIG_SENSORS_QPNP_ADC_CURRENT
static int qpnp_get_battery_current(int *current_ua)
{
	struct qpnp_iadc_result i_result;
	int ret;
	if (qpnp_iadc_is_ready()) {
		pr_err("%s: qpnp iadc is not ready!\n", __func__);
		*current_ua = 0;
		return 0;
	}
	ret = qpnp_iadc_read(EXTERNAL_RSENSE, &i_result);
	if (ret) {
		pr_err("%s: failed to read iadc\n", __func__);
		*current_ua = 0;
		return ret;
	}
	*current_ua = -i_result.result_ua;
	return 0;
}
#endif*/
static int max17048_get_prop_current(struct max17048_chip *chip)
{
#if defined(CONFIG_TARGET_PRODUCT_IF_S300N)
	union power_supply_propval ret = {0,};

	if (chip->smb_psy) {
		chip->smb_psy->get_property(chip->smb_psy, POWER_SUPPLY_PROP_CURRENT_NOW , &ret);
	}

	chip->batt_current =  ret.intval;
#ifdef MAX17048_DEBUG
	pr_debug("%s: current_now = %d\n", __func__, chip->batt_current);
#endif
#else
/*#ifdef CONFIG_SENSORS_QPNP_ADC_CURRENT
	int ret;
	ret = qpnp_get_battery_current(&chip->batt_current);
	if (ret)
		pr_err("%s: failed to get batt current.\n", __func__);
#else*/
//	pr_warn("%s: batt current is not supported!\n", __func__);
	chip->batt_current = 0;
//#endif
	pr_debug("%s: ibatt_ua = %d\n", __func__, chip->batt_current);
#endif
	return chip->batt_current;
}
static enum power_supply_property max17048_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};
static int max17048_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17048_chip *chip = container_of(psy,
				struct max17048_chip, batt_psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max17048_get_prop_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max17048_get_prop_present(chip);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = chip->batt_tech;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chip->max_mvolt * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = chip->min_mvolt * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max17048_get_prop_vbatt_uv(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = max17048_get_prop_temp(chip);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->capacity_level;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = max17048_get_prop_current(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->fcc_mah;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17048_initialize(struct max17048_chip *chip)
{
	int ret, i, rcompseg;
	struct i2c_client *client = chip->client;
	uint16_t soc_check;

start:
	/******************************************************************************
	Step1 : unlock model access 
			This enables access to the OCV and table registers
	******************************************************************************/
	ret = max17048_write_word(client, UNLOCK_REG, MAX17048_UNLOCK_VALUE);
	if (ret < 0)
		return ret;

	/******************************************************************************
	Step 2. Read RCOMP and OCV
		The RCOMP and OCV Register will be modified during the process of loading 
		the custom model. Read and store these value so that they can be written 
		back to the	device after the model has been loaded.
	******************************************************************************/
	g_rcomp = max17048_read_word(client, CONFIG_REG);
	if (g_rcomp < 0) {
		pr_err("%s: error g_rcomp %d\n", __func__, g_rcomp);
		return g_rcomp;
	}

	g_ocv = max17048_read_word(client, OCV_REG);
	if (g_ocv < 0) {
		pr_err("%s: error g_ocv %d\n", __func__, g_ocv);
		return g_rcomp;
	}

	/******************************************************************************
	Step 2.5. Verify Model Access Unlocked
		If Model Access was correctly unlocked in Step 1, then the OCV bytes read
		in Step 2 will not be 0xFF. If the values of both bytes are 0xFF,
		that indicates that Model Access was not correctly unlocked and the
		sequence should be repeated from Step 1.
	******************************************************************************/
	if (g_ocv == 0xFFFF) {
		pr_err("%s: Failed in unlocking max17048 err: %d\n", __func__, g_ocv);
		goto start;
	}

	/******************************************************************************
	Step 3. Write OCV
		OCVTest_High_Byte and OCVTest_Low_Byte values
	******************************************************************************/
	ret = max17048_write_word(client, OCV_REG, chip->ocv_test);
	//ret = max17048_write_word(client, OCV_REG, 0x0000);
	if (ret < 0)
		return ret;

	/******************************************************************************
	Step 4. Write RCOMP to a Maximum Value (0xFF00)
	******************************************************************************/
	ret = max17048_write_word(client, CONFIG_REG, 0xFF00);
	if (ret < 0)
		return ret;

	/******************************************************************************
	Step 5. Write the Model
		Once the model is unlocked, the host software must write the 64 byte model
		to the device. The model is located between memory 0x40 and 0x7F.
	******************************************************************************/
	for (i = 0; i < 4; i += 1) {
		if (i2c_smbus_write_i2c_block_data(client,
			(TABLE_REG+i*16), 16,
				&max17048_custom_model_data[i*0x10]) < 0) {
			pr_err("%s: error writing model data\n", __func__);
			return -1;
		}
	}

	/******************************************************************************
	Step 5.1 Write RCOMPSeg (MAX17048/MAX17049 only)
	******************************************************************************/
	if (chip->rcompseg == 0) {
		rcompseg = RCOMPSEG_REG;
	}
	else {
		rcompseg = chip->rcompseg;
	}

	for (i = 0; i < 2; i += 1) {
		if (i2c_smbus_write_i2c_block_data(client,
			(rcompseg+i*16), 16,
				&max17048_rcomp_seg[i*0x10]) < 0) {
			pr_err("%s: error writing RCOMPSeg\n", __func__);
			return -1;
		}
	}
	
	/******************************************************************************
	Step 6. Delay at least 150ms
		This delay must be at least 150mS, but the upper limit is not critical
		in this step.
	******************************************************************************/
	mdelay(150);

	/******************************************************************************
	Step 7. Write OCV
	******************************************************************************/
	ret = max17048_write_word(client, OCV_REG, chip->ocv_test);
	//ret = max17048_write_word(client, OCV_REG, 0x0000);
	if (ret < 0)
		return ret;

	/******************************************************************************
	Step 7.1 Disable Hibernate (MAX17048 only)
		The IC updates SOC less frequently in hibernate mode, so make sure it
		is not hibernating
	******************************************************************************/
	ret = max17048_write_word(client, HIBRT_REG, 0x0000);
	if (ret < 0)
		return ret;

	/******************************************************************************
	Step 7.2. Lock Model Access (MAX17048 only)
		To allow the ModelGauge algorithm to run in MAX17048 only, the
		model must be locked. This is harmless but unnecessary for MAX17040/3/4
	******************************************************************************/
	ret = max17048_write_word(client, UNLOCK_REG, 0x0000);
	if (ret < 0)
		return ret;

	/******************************************************************************
	Step 8. Delay between 150ms and 600ms
		This delay must be between 150ms and 600ms. Delaying beyond 600ms could
		cause the verification to fail.
	******************************************************************************/
	mdelay(200);

	/******************************************************************************
	Step 9. Read SOC Register and compare to expected result
		There will be some variation in the SOC register due to the ongoing
		activity of the ModelGauge algorithm. Therefore, the upper byte of the RCOMP
		register is verified to be within a specified range to verify that the
		model was loaded correctly. This value is not an indication of the state of
		the actual battery.
	******************************************************************************/
	soc_check = max17048_read_word(client, SOC_REG);
	if (soc_check < 0) {
		pr_err("%s: error read SOC_REG %d\n", __func__, soc_check);
		return soc_check;
	}

	if (!((soc_check >> 8) >= chip->model_soccheck_A &&
				(soc_check >> 8) <=  chip->model_soccheck_B)) {
		pr_err("%s: soc comparison failed %d\n", __func__, soc_check);
		return soc_check;
	} else {
		pr_err("MAX17048 Custom data loading successfull\n");
	}
	
	/******************************************************************************
	Step 9.1. Unlock Model Access (MAX17048 only)
		To write OCV, MAX17048 requires model access to be unlocked.
	******************************************************************************/
	ret = max17048_write_word(client, UNLOCK_REG, MAX17048_UNLOCK_VALUE);
	if (ret < 0)
		return ret;

	/******************************************************************************
	Step 10. Restore RCOMP and OCV
		Write RCOMP and OCV Register back to the original values
	******************************************************************************/
	ret = max17048_write_word(client, CONFIG_REG, g_rcomp);
	if (ret < 0)
		return ret;
	ret = max17048_write_word(client, OCV_REG, g_ocv);
	if (ret < 0)
		return ret;

	/******************************************************************************
	Step 11. Lock Model Access
	******************************************************************************/
	ret = max17048_write_word(client, UNLOCK_REG, 0x0000);
	if (ret < 0)
		return ret;

	/******************************************************************************
	Step 11.5. Delay at least 150msec
		This delay must be at least 150mS before reading the SOC Register to allow
		the correct value to be calculated by the device.
	******************************************************************************/
	mdelay(150);

	return 0;
}

static int max17048_hw_init(struct max17048_chip *chip)
{
	int ret;
	ret = max17048_masked_write_word(chip->client,
			STATUS_REG, STAT_RI_MASK, 0);
	if (ret) {
		pr_err("%s: failed to clear ri bit\n", __func__);
		return ret;
	}
	ret = max17048_set_athd_alert(chip, chip->alert_threshold);
	if (ret) {
		pr_err("%s: failed to set athd alert threshold\n", __func__);
		return ret;
	}
	ret = max17048_set_alsc_alert(chip, true);
	if (ret) {
		pr_err("%s: failed to set alsc alert\n", __func__);
		return ret;
	}
	return 0;
}
static int max17048_pm_notifier(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct max17048_chip *chip = container_of(notifier,
				struct max17048_chip, pm_notifier);
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		max17048_set_alsc_alert(chip, false);
		cancel_delayed_work_sync(&chip->monitor_work);
		break;
	case PM_POST_SUSPEND:
		schedule_delayed_work(&chip->monitor_work, 0);
		max17048_set_alsc_alert(chip, true);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
static int max17048_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct max17048_chip *chip;
	int ret;
	uint16_t version;

	pr_info("%s: start\n", __func__);
	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_WORD_DATA)) {
		pr_err("%s: i2c_check_functionality fail\n", __func__);
		return -EIO;
	}
	chip->client = client;
	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		pr_err("usb supply not found deferring probe\n");
		ret = -EPROBE_DEFER;
		goto error;
	}

	chip->smb_psy = power_supply_get_by_name("battery");
	if (!chip->smb_psy) {
		pr_err("SMB supply not found deferring probe\n");
		ret = -EPROBE_DEFER;
		goto error;
	}
	
	if (&client->dev.of_node) {
		ret = max17048_parse_dt(&client->dev, chip);
		if (ret) {
			pr_err("%s: failed to parse dt\n", __func__);
			goto  error;
		}
	} else {
		chip->pdata = client->dev.platform_data;
	}
	i2c_set_clientdata(client, chip);
	version = max17048_get_version(chip);
#ifdef MAX17048_DEBUG
	dev_info(&client->dev, "MAX17048 Fuel-Gauge Ver 0x%x\n", version);
#endif
	if (version != MAX17048_VERSION_11 &&
	    version != MAX17048_VERSION_12) {
		pr_err("%s: Not supported version: 0x%x\n", __func__,
				version);
		ret = -ENODEV;
		goto error;
	}

	ret = max17048_initialize(chip);
	if (ret < 0) {
		pr_err("%s: Error Initializing fuel-gauge\n", __func__);
		goto error;
	}

	ref = chip;
	chip->lasttime_voltage = 0;
	chip->lasttime_capacity_level = 0;
	chip->lasttime_batt_temp = 0;
	chip->dev = &client->dev;
	chip->batt_psy.name = "fg";
	chip->batt_psy.type = POWER_SUPPLY_TYPE_BMS;
	chip->batt_psy.get_property = max17048_get_property;
	chip->batt_psy.properties = max17048_battery_props;
	chip->batt_psy.num_properties = ARRAY_SIZE(max17048_battery_props);

	ret = power_supply_register(&client->dev, &chip->batt_psy);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto error1;
	}

	INIT_DEFERRABLE_WORK(&chip->monitor_work, max17048_work);

	
	ret = max17048_hw_init(chip);
	if (ret) {
		pr_err("%s: failed to init hw.\n", __func__);
		goto error1;
	}
	chip->pm_notifier.notifier_call = max17048_pm_notifier;
	ret = register_pm_notifier(&chip->pm_notifier);
	if (ret) {
		pr_err("%s: failed to register pm notifier\n", __func__);
		goto error1;
	}
	schedule_delayed_work(&chip->monitor_work, 0);
	pr_info("%s: done\n", __func__);
	return 0;
error1:
	power_supply_unregister(&chip->batt_psy);
error:
	ref = NULL;
	kfree(chip);
	return ret;
}
static int max17048_remove(struct i2c_client *client)
{
	struct max17048_chip *chip = i2c_get_clientdata(client);
	unregister_pm_notifier(&chip->pm_notifier);
	power_supply_unregister(&chip->batt_psy);
	ref = NULL;
	kfree(chip);
	return 0;
}
static struct of_device_id max17048_match_table[] = {
	{ .compatible = "maxim,max17048", },
	{ },
};
static const struct i2c_device_id max17048_id[] = {
	{ "max17048", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max17048_id);
static struct i2c_driver max17048_i2c_driver = {
	.driver	= {
		.name = "max17048",
		.owner = THIS_MODULE,
		.of_match_table = max17048_match_table,
	},
	.probe = max17048_probe,
	.remove = max17048_remove,
	.id_table = max17048_id,
};
static int __init max17048_init(void)
{
	return i2c_add_driver(&max17048_i2c_driver);
}
module_init(max17048_init);
static void __exit max17048_exit(void)
{
	i2c_del_driver(&max17048_i2c_driver);
}
module_exit(max17048_exit);
MODULE_DESCRIPTION("MAX17048 Fuel Gauge");
MODULE_LICENSE("GPL");
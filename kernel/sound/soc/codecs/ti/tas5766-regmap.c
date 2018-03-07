/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas2559-regmap.c
**
** Description:
**     I2C driver with regmap for Texas Instruments TAS2559 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

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
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "tas2559.h"
#include "tas2559-core.h"
#include "tas2560.h"
#include <linux/reboot.h>
#include "tas5766.h" 

#ifdef CONFIG_TAS2559_CODEC
#include "tas2559-codec.h"
#endif

#ifdef CONFIG_TAS2559_MISC
#include "tas2559-misc.h"
#endif

static char tmp_buf[BUF_MAX] = {0,};

static int SPK_LDO_GPIO = 0; 

struct notifier_block reboot_notifier;
struct notifier_block panic_notifier;


/*
* tas2559_i2c_write_device : write single byte to device
* platform dependent, need platform specific support
*/
static int tas2559_i2c_write_device(struct tas2559_priv *pTAS2559,
				    unsigned char addr,
				    unsigned char reg,
				    unsigned char value)
{
	int nResult = 0;

	pTAS2559->client->addr = addr;
	nResult = regmap_write(pTAS2559->mpRegmap, reg, value);

	if(addr == 0) return 0;

	if (nResult < 0)
		dev_err(pTAS2559->dev, "%s[0x%x] Error %d\n",
			__func__, addr, nResult);

	return nResult;
}

/*
* tas2559_i2c_bulkwrite_device : write multiple bytes to device
* platform dependent, need platform specific support
*/
static int tas2559_i2c_bulkwrite_device(struct tas2559_priv *pTAS2559,
					unsigned char addr,
					unsigned char reg,
					unsigned char *pBuf,
					unsigned int len)
{
	int nResult = 0;

	pTAS2559->client->addr = addr;
	nResult = regmap_bulk_write(pTAS2559->mpRegmap, reg, pBuf, len);

	if (nResult < 0)
		dev_err(pTAS2559->dev, "%s[0x%x] Error %d\n",
			__func__, addr, nResult);
	
	return nResult;
}

/*
* tas2559_i2c_read_device : read single byte from device
* platform dependent, need platform specific support
*/
static int tas2559_i2c_read_device(struct tas2559_priv *pTAS2559,
				   unsigned char addr,
				   unsigned char reg,
				   unsigned char *p_value)
{
	int nResult = 0;
	unsigned int val = 0;

	pTAS2559->client->addr = addr;
	nResult = regmap_read(pTAS2559->mpRegmap, reg, &val);

	if (nResult < 0) {
		dev_err(pTAS2559->dev, "%s[0x%x] Error %d\n",
			__func__, addr, nResult);
	} else {
		*p_value = (unsigned char)val;
	}

	return nResult;
}

/*
* tas2559_i2c_bulkread_device : read multiple bytes from device
* platform dependent, need platform specific support
*/
static int tas2559_i2c_bulkread_device(struct tas2559_priv *pTAS2559,
				       unsigned char addr,
				       unsigned char reg,
				       unsigned char *p_value,
				       unsigned int len)
{
	int nResult = 0;

	pTAS2559->client->addr = addr;
	nResult = regmap_bulk_read(pTAS2559->mpRegmap, reg, p_value, len);

	if (nResult < 0)
		dev_err(pTAS2559->dev, "%s[0x%x] Error %d\n",
			__func__, addr, nResult);

	return nResult;
}

static int tas2559_i2c_update_bits(struct tas2559_priv *pTAS2559,
				   unsigned char addr,
				   unsigned char reg,
				   unsigned char mask,
				   unsigned char value)
{
	int nResult = 0;

	pTAS2559->client->addr = addr;
	nResult = regmap_update_bits(pTAS2559->mpRegmap, reg, mask, value);

	if (nResult < 0)
		dev_err(pTAS2559->dev, "%s[0x%x] Error %d\n",
			__func__, addr, nResult);

	return nResult;
}

/*
* tas2559_change_book_page : switch to certain book and page
* platform independent, don't change unless necessary
*/
static int tas2559_change_book_page(struct tas2559_priv *pTAS2559,
				    enum channel chn,
				    unsigned char nBook,
				    unsigned char nPage)
{
	int nResult = 0;

	if (chn & DevA) {
		if (pTAS2559->mnDevACurrentBook == nBook) {
			if (pTAS2559->mnDevACurrentPage != nPage) {
				nResult = tas2559_i2c_write_device(pTAS2559,
								   pTAS2559->mnDevAAddr, TAS2559_BOOKCTL_PAGE, nPage);

				if (nResult >= 0)
					pTAS2559->mnDevACurrentPage = nPage;
			}
		} else {
			nResult = tas2559_i2c_write_device(pTAS2559,
							   pTAS2559->mnDevAAddr, TAS2559_BOOKCTL_PAGE, 0);

			if (nResult >= 0) {
				pTAS2559->mnDevACurrentPage = 0;
				nResult = tas2559_i2c_write_device(pTAS2559,
								   pTAS2559->mnDevAAddr, TAS2559_BOOKCTL_REG, nBook);
				pTAS2559->mnDevACurrentBook = nBook;

				if (nPage != 0) {
					nResult = tas2559_i2c_write_device(pTAS2559,
									   pTAS2559->mnDevAAddr, TAS2559_BOOKCTL_PAGE, nPage);
					pTAS2559->mnDevACurrentPage = nPage;
				}
			}
		}
	}

	return nResult;
}

/*
* tas2559_dev_read :
* platform independent, don't change unless necessary
*/
static int tas2559_dev_read(struct tas2559_priv *pTAS2559,
			    enum channel chn,
			    unsigned int nRegister,
			    unsigned int *pValue)
{
	int nResult = 0;
	unsigned char Value = 0;

	mutex_lock(&pTAS2559->dev_lock);

	if (pTAS2559->mbTILoadActive) {
		if (!(nRegister & 0x80000000)) {
			/* let only reads from TILoad pass. */
			goto end;
		}

		nRegister &= ~0x80000000;

		dev_dbg(pTAS2559->dev, "TiLoad R CH[%d] REG B[%d]P[%d]R[%d]\n",
			chn,
			TAS2559_BOOK_ID(nRegister),
			TAS2559_PAGE_ID(nRegister),
			TAS2559_PAGE_REG(nRegister));
	}

	nResult = tas2559_change_book_page(pTAS2559, chn,
					   TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister));

	if (nResult >= 0) {
		if (chn == DevA)
			nResult = tas2559_i2c_read_device(pTAS2559,
							  pTAS2559->mnDevAAddr, TAS2559_PAGE_REG(nRegister), &Value);
		else
			if (chn == DevB) {
			} else {
				dev_err(pTAS2559->dev, "%s， read chn ERROR %d\n", __func__, chn);
				nResult = -EINVAL;
			}

		if (nResult >= 0)
			*pValue = Value;
	}

end:

	mutex_unlock(&pTAS2559->dev_lock);
	return nResult;
}

/*
* tas2559_dev_write :
* platform independent, don't change unless necessary
*/
static int tas2559_dev_write(struct tas2559_priv *pTAS2559,
			     enum channel chn,
			     unsigned int nRegister,
			     unsigned int nValue)
{
	int nResult = 0;

	mutex_lock(&pTAS2559->dev_lock);

	if ((nRegister == 0xAFFEAFFE) && (nValue == 0xBABEBABE)) {
		pTAS2559->mbTILoadActive = true;
		dev_dbg(pTAS2559->dev, "TiLoad Active\n");
		goto end;
	}

	if ((nRegister == 0xBABEBABE) && (nValue == 0xAFFEAFFE)) {
		pTAS2559->mbTILoadActive = false;
		dev_dbg(pTAS2559->dev, "TiLoad DeActive\n");
		goto end;
	}

	if (pTAS2559->mbTILoadActive) {
		if (!(nRegister & 0x80000000)) {
			/* let only writes from TILoad pass. */
			goto end;
		}

		nRegister &= ~0x80000000;
		dev_dbg(pTAS2559->dev, "TiLoad W CH[%d] REG B[%d]P[%d]R[%d] =0x%x\n",
			chn, TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister),
			TAS2559_PAGE_REG(nRegister), nValue);
	}

	nResult = tas2559_change_book_page(pTAS2559,
					   chn, TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister));

	if (nResult >= 0) {
		if (chn & DevA)
			nResult = tas2559_i2c_write_device(pTAS2559,
							   pTAS2559->mnDevAAddr, TAS2559_PAGE_REG(nRegister), nValue);

		if (chn & DevB)
			nResult = tas2559_i2c_write_device(pTAS2559,
							   pTAS2559->mnDevBAddr, TAS2559_PAGE_REG(nRegister), nValue);
	}

end:

	mutex_unlock(&pTAS2559->dev_lock);
	return nResult;
}

/*
* tas2559_dev_bulk_read :
* platform independent, don't change unless necessary
*/
static int tas2559_dev_bulk_read(struct tas2559_priv *pTAS2559,
				 enum channel chn,
				 unsigned int nRegister,
				 unsigned char *pData,
				 unsigned int nLength)
{
	int nResult = 0;
	unsigned char reg = 0;

	mutex_lock(&pTAS2559->dev_lock);

	if (pTAS2559->mbTILoadActive) {
		if (!(nRegister & 0x80000000)) {
			/* let only writes from TILoad pass. */
			goto end;
		}

		nRegister &= ~0x80000000;
		dev_dbg(pTAS2559->dev, "TiLoad BR CH[%d] REG B[%d]P[%d]R[%d], count=%d\n",
			chn, TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister),
			TAS2559_PAGE_REG(nRegister), nLength);
	}

	nResult = tas2559_change_book_page(pTAS2559, chn,
					   TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister));

	if (nResult >= 0) {
		reg = TAS2559_PAGE_REG(nRegister);

		if (chn == DevA)
			nResult = tas2559_i2c_bulkread_device(pTAS2559,
							      pTAS2559->mnDevAAddr, reg, pData, nLength);
		else
			if (chn == DevB)
				nResult = tas2559_i2c_bulkread_device(pTAS2559,
								      pTAS2559->mnDevBAddr, reg, pData, nLength);
			else {
				dev_err(pTAS2559->dev, "%s, chn ERROR %d\n", __func__, chn);
				nResult = -EINVAL;
			}
	}

end:

	mutex_unlock(&pTAS2559->dev_lock);
	return nResult;
}

/*
* tas2559_dev_bulk_write :
* platform independent, don't change unless necessary
*/
static int tas2559_dev_bulk_write(struct tas2559_priv *pTAS2559,
				  enum channel chn,
				  unsigned int nRegister,
				  unsigned char *pData,
				  unsigned int nLength)
{
	int nResult = 0;
	unsigned char reg = 0;

	mutex_lock(&pTAS2559->dev_lock);

	if (pTAS2559->mbTILoadActive) {
		if (!(nRegister & 0x80000000)) {
			/* let only writes from TILoad pass. */
			goto end;
		}

		nRegister &= ~0x80000000;
		dev_dbg(pTAS2559->dev, "TiLoad BW CH[%d] REG B[%d]P[%d]R[%d], count=%d\n",
			chn, TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister),
			TAS2559_PAGE_REG(nRegister), nLength);
	}

	nResult = tas2559_change_book_page(pTAS2559, chn,
					   TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister));

	if (nResult >= 0) {
		reg = TAS2559_PAGE_REG(nRegister);

		if (chn & DevA)
			nResult = tas2559_i2c_bulkwrite_device(pTAS2559,
							       pTAS2559->mnDevAAddr, reg, pData, nLength);

		if (chn & DevB)
			nResult = tas2559_i2c_bulkwrite_device(pTAS2559,
							       pTAS2559->mnDevBAddr, reg, pData, nLength);
	}

end:

	mutex_unlock(&pTAS2559->dev_lock);
	return nResult;
}

/*
* tas2559_dev_update_bits :
* platform independent, don't change unless necessary
*/
static int tas2559_dev_update_bits(
	struct tas2559_priv *pTAS2559,
	enum channel chn,
	unsigned int nRegister,
	unsigned int nMask,
	unsigned int nValue)
{
	int nResult = 0;

	mutex_lock(&pTAS2559->dev_lock);

	if (pTAS2559->mbTILoadActive) {
		if (!(nRegister & 0x80000000)) {
			/* let only writes from TILoad pass. */
			goto end;
		}

		nRegister &= ~0x80000000;
		dev_dbg(pTAS2559->dev, "TiLoad SB CH[%d] REG B[%d]P[%d]R[%d], mask=0x%x, value=0x%x\n",
			chn, TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister),
			TAS2559_PAGE_REG(nRegister), nMask, nValue);
	}

	nResult = tas2559_change_book_page(pTAS2559,
					   chn, TAS2559_BOOK_ID(nRegister), TAS2559_PAGE_ID(nRegister));

	if (nResult >= 0) {
		if (chn & DevA)
			nResult = tas2559_i2c_update_bits(pTAS2559,
							  pTAS2559->mnDevAAddr, TAS2559_PAGE_REG(nRegister), nMask, nValue);
}

end:

	mutex_unlock(&pTAS2559->dev_lock);
	return nResult;
}

static bool tas5766_volatile(struct device *pDev, unsigned int nRegister)
{
	return true;
}

static bool tas5766_writeable(struct device *pDev, unsigned int nRegister)
{
	return true;
}

static const struct regmap_config tas5766_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = tas5766_writeable,
	.volatile_reg = tas5766_volatile,
	.cache_type = REGCACHE_NONE,
	.max_register = 128,
};

int tas5766_parse_dt(struct device *dev, struct tas2559_priv *pTAS2559)
{
	struct device_node *np = dev->of_node;
	int rc = 0, ret = 0;
	unsigned int value;
	

	pTAS2559->mnDevAGPIOIRQ = of_get_named_gpio(np, "ti,tas5766-irq-gpio", 0);
	
	if (!gpio_is_valid(pTAS2559->mnDevAGPIOIRQ))
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n","ti,tas5766-irq-gpio", np->full_name, pTAS2559->mnDevAGPIOIRQ);
	

	pTAS2559->ldo_enable_gpio = of_get_named_gpio(np, "infr,ldo-enable-gpio", 0);

	if (!gpio_is_valid(pTAS2559->ldo_enable_gpio))
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n","infr,ldo-enable-gpio", np->full_name, pTAS2559->ldo_enable_gpio);
	else
		dev_dbg(pTAS2559->dev, "%s, tas5766 ldo-enable-gpio %d\n", __func__, pTAS2559->ldo_enable_gpio);

	
	rc = of_property_read_u32(np, "ti,tas5766-addr", &value);
	
	if (rc) {
		dev_err(pTAS2559->dev, "Looking up %s property in node %s failed %d\n","ti,tas5766-addr", np->full_name, rc);
		ret = -EINVAL;
		goto end;
	} else {
		pTAS2559->mnDevAAddr = value;
		dev_dbg(pTAS2559->dev, "ti,tas5766 addr=0x%x\n", pTAS2559->mnDevAAddr);
	}

	rc = of_property_read_u32(np, "infr,tune-id", &value);
	
	if (rc) 
	{
		pTAS2559->tune_id = 0x2;
		dev_err(pTAS2559->dev, "erorr infr,tune-id = %d",pTAS2559->tune_id);
	}
	else
	{
		pTAS2559->tune_id = value;
		dev_dbg(pTAS2559->dev, "infr,tune-id = %d",pTAS2559->tune_id);
	}
	 				
	end:
	
		return ret;
}

static void transmit_registers(struct tas2559_priv *pTAS2559, cfg_reg *r, int n)
    {
        int i = 0;
		//int k = 0;
		int ret = 0;
		
		//printk(=============================transmit_registers=====================================\n");
        while (i < n) {
			
            switch (r[i].command) {
            case CFG_META_SWITCH:
                // Used in legacy applications.  Ignored here.
                break;
            case CFG_META_DELAY:
                //delay(r[i].param); //wjhyun todo
                break;
            case CFG_META_BURST:
				pTAS2559->client->addr = pTAS2559->mnDevAAddr;
				memset(tmp_buf,0x00,BUF_MAX);
				memcpy(tmp_buf,&(r[i+1].param),r[i].param-1);
				ret = regmap_bulk_write(pTAS2559->mpRegmap,r[i+1].command,tmp_buf,r[i].param-1);

				if (ret < 0)
					printk("WON : I2C write error block\n");

				#if 0
				printk("\n+++++++++++++++++[CFG_META_BURST, reg 0x%x size %d]++++++++++++++++++++++\n",r[i+1].command,r[i].param);
				for(k=0;k<=r[i].param-1;k++)
				{
					printk("0x%x,",tmp_buf[k]);
					if(k%10 == 9)
						printk("\n");
				}
				printk("\n-------------------------------------------------------------------------\n");
				#endif
                i +=  (r[i].param + 1)/2;
                break;
            default:
				pTAS2559->client->addr = pTAS2559->mnDevAAddr;
				//printk("WON : r[%d].command 0x%x r[%d].param 0x%x\n",i,r[i].command,i,r[i].param);
				ret = regmap_write(pTAS2559->mpRegmap,r[i].command,r[i].param);
				
				//if (ret < 0)
					//printk("WON : I2C write error\n");
				
                break;
            }

            i++;
        }
		//printk("===================================transmit_registers===============================\n");
    }
  
static void SPK_LDO_ENABLE(int enable)
{	
	pr_err("SPK_LDO_ENABLE %d\n",enable);
	gpio_direction_output(SPK_LDO_GPIO, enable);
}

static int amp_halt(struct notifier_block *nb, unsigned long event, void *buf)
{
	pr_err("amp_halt\n");
	SPK_LDO_ENABLE(0);
	return NOTIFY_OK;
}

/*
* tas2559_i2c_probe :
* platform dependent
* should implement hardware reset functionality
*/
static int tas5766_i2c_probe(struct i2c_client *pClient,
			     const struct i2c_device_id *pID)
{
	struct tas2559_priv *pTAS2559;
	int nResult;
	struct clk *ext_clk;

	dev_info(&pClient->dev, "%s enter\n", __func__);

	pTAS2559 = devm_kzalloc(&pClient->dev, sizeof(struct tas2559_priv), GFP_KERNEL);

	if (!pTAS2559) {
		dev_err(&pClient->dev, " -ENOMEM\n");
		nResult = -ENOMEM;
		goto err;
	}

	pTAS2559->client = pClient;
	pTAS2559->dev = &pClient->dev;
	i2c_set_clientdata(pClient, pTAS2559);
	dev_set_drvdata(&pClient->dev, pTAS2559);

	pTAS2559->mpRegmap = devm_regmap_init_i2c(pClient, &tas5766_i2c_regmap);

	if (IS_ERR(pTAS2559->mpRegmap)) {
		nResult = PTR_ERR(pTAS2559->mpRegmap);
		dev_err(&pClient->dev, "Failed to allocate register map: %d\n",
			nResult);
		goto err;
	}

	if (pClient->dev.of_node)
		tas5766_parse_dt(&pClient->dev, pTAS2559);
	

	if (gpio_is_valid(pTAS2559->ldo_enable_gpio)) 
	{
		nResult = gpio_request(pTAS2559->ldo_enable_gpio, "TAS2559-LDO-EN");
	
		if (nResult < 0) 
		{
			dev_err(pTAS2559->dev, "%s: GPIO %d request error\n",__func__, pTAS2559->ldo_enable_gpio);
			goto err;
		}
		//gpio_direction_output(pTAS2559->ldo_enable_gpio, 1);
		SPK_LDO_GPIO = pTAS2559->ldo_enable_gpio;
		SPK_LDO_ENABLE(1);
		reboot_notifier.notifier_call = amp_halt;
		panic_notifier.notifier_call = amp_halt;
		atomic_notifier_chain_register(&panic_notifier_list, &panic_notifier);
		register_reboot_notifier(&reboot_notifier);
		
	}
	
	ext_clk = clk_get(pTAS2559->dev, "ext_clk");
	if (IS_ERR(ext_clk)) 
	{
		dev_err(pTAS2559->dev, "%s: clk get %s failed\n",__func__, "ext_clk");
		goto err;
	}
	pTAS2559->ext_clk = ext_clk;

	pTAS2559->read = tas2559_dev_read;
	pTAS2559->write = tas2559_dev_write;
	pTAS2559->bulk_read = tas2559_dev_bulk_read;
	pTAS2559->bulk_write = tas2559_dev_bulk_write;
	pTAS2559->update_bits = tas2559_dev_update_bits;
	pTAS2559->set_config = tas2559_set_config;
	pTAS2559->set_calibration = tas2559_set_calibration;
	

	mutex_init(&pTAS2559->dev_lock);

	msleep(1);
	
	pTAS2559->mpFirmware = devm_kzalloc(&pClient->dev, sizeof(struct TFirmware), GFP_KERNEL);

	if (!pTAS2559->mpFirmware) {
		dev_err(&pClient->dev, "mpFirmware ENOMEM\n");
		nResult = -ENOMEM;
		goto err;
	}

	pTAS2559->mpCalFirmware = devm_kzalloc(&pClient->dev, sizeof(struct TFirmware), GFP_KERNEL);

	if (!pTAS2559->mpCalFirmware) {
		dev_err(&pClient->dev, "mpCalFirmware ENOMEM\n");
		nResult = -ENOMEM;
		goto err;
	}

#ifdef CONFIG_TAS2559_CODEC
	mutex_init(&pTAS2559->codec_lock);
	tas2559_register_codec(pTAS2559);
#endif

#ifdef CONFIG_TAS2559_MISC
	mutex_init(&pTAS2559->file_lock);
	tas2559_register_misc(pTAS2559);
#endif

if(pTAS2559->tune_id == 0x2)	
	transmit_registers(pTAS2559,registers,(sizeof(registers) / 2));
else
	transmit_registers(pTAS2559,registers_ps,(sizeof(registers_ps) / 2));


err:

	return nResult;
}

static int tas5766_i2c_remove(struct i2c_client *pClient)
{
	struct tas2559_priv *pTAS2559 = i2c_get_clientdata(pClient);

	dev_info(pTAS2559->dev, "%s\n", __func__);

	if(pTAS2559->ext_clk)
		clk_put(pTAS2559->ext_clk);

	if (pTAS2559->ldo_enable_gpio != 0 && gpio_is_valid(pTAS2559->ldo_enable_gpio)){
		gpio_free(pTAS2559->ldo_enable_gpio);
		pTAS2559->ldo_enable_gpio = 0;
	}

	unregister_reboot_notifier(&reboot_notifier);
	SPK_LDO_ENABLE(0);

#ifdef CONFIG_TAS2559_CODEC
	tas2559_deregister_codec(pTAS2559);
	mutex_destroy(&pTAS2559->codec_lock);
#endif

#ifdef CONFIG_TAS2559_MISC
	tas2559_deregister_misc(pTAS2559);
	mutex_destroy(&pTAS2559->file_lock);
#endif

	mutex_destroy(&pTAS2559->dev_lock);
	return 0;
}


static const struct i2c_device_id tas5766_i2c_id[] = {
	{"tas5766", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tas5766_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id tas5766_of_match[] = {
	{.compatible = "ti,tas5766"},
	{},
};

MODULE_DEVICE_TABLE(of, tas5766_of_match);
#endif

static struct i2c_driver tas5766_i2c_driver = {
	.driver = {
		.name = "tas5766",
		.owner = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(tas5766_of_match),
#endif
	},
	.probe = tas5766_i2c_probe,
	.remove = tas5766_i2c_remove,
	.id_table = tas5766_i2c_id,
};

module_i2c_driver(tas5766_i2c_driver);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS5766 Stereo I2C Smart Amplifier driver");
MODULE_LICENSE("GPL v2");

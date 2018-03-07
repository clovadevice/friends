

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h>

#include "ledanim.h"
#include "clova_modelname.h"


int get_clova_modelname(char * modelname, int len)
{	
	char * start, *end;
	int modelname_length;

	start = strstr(saved_command_line, "androidboot.model=");
	if(start == NULL){
		printk(KERN_ERR "not found androidboot.model in saved_command_line\n");
		return -EINVAL;
	}

    start += strlen("androidboot.model=");
	end = strchr(start, ' ');

    if(end == NULL){
		return -EINVAL;		
	}

	modelname_length = (int)end - (int)start; 
	if(modelname_length > len)
		modelname_length = len;

	strncpy(modelname, start, modelname_length);
	return 0;
}

EXPORT_SYMBOL(get_clova_modelname);



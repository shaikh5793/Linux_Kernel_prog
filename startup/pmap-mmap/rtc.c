#include <linux/module.h>
#include <linux/printk.h>
#include <linux/io.h>

#define  SECOND         0x00  //Second       (00..59) 
#define  MINUTE         0x02  //Minute       (00..59)
#define  HOUR           0x04  //Hour         (00..23) 
#define  DAY_IN_WEEK    0x06  //Day of week  (01..07)
#define  DAY            0x07  //Day          (01..31) 
#define  MONTH          0x08  //Month        (01..12) 
#define  YEAR           0x09 // Year         (00..99) 

#define  ADDRESS_REG       0x70
#define  DATA_REG          0x71
#define  ADDRESS_REG_MASK  0xe0

static unsigned char get_rtc(unsigned char addr)
{
	outb_p(addr, ADDRESS_REG);
	return inb_p(DATA_REG);
}

static void set_rtc(unsigned char data, unsigned char addr)
{
	outb_p(addr, ADDRESS_REG);
	outb_p(data, DATA_REG);
}

static int __init rtc_init(void)
{
	pr_info("rtc module loaded\n");	
	pr_info("second %x", get_rtc(SECOND));
	pr_info("minute %x", get_rtc(MINUTE));
	pr_info("hour %x", get_rtc(HOUR));
	pr_info("day %x", get_rtc(DAY));
	pr_info("month %x", get_rtc(MONTH));
	pr_info("year %x\n", get_rtc(YEAR));

	return 0;
		
}

static void __exit rtc_exit(void)
{
	pr_info("rtc module unloaded\n");
}

module_init(rtc_init);
module_exit(rtc_exit);

MODULE_LICENSE("GPL");


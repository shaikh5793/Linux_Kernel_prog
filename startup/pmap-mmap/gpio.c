#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>

#define GPIO1_BASE_ADDR 0x4804C000
#define GPIO_OE_OFFSET 0x134
#define GPIO_DATAOUT_OFFSET 0x13C
#define GPIO_DATAIN_OFFSET 0x138
#define GPIO_SIZE 0x200  // Size of the GPIO register space

#define GPIO_PIN 30

// Pointer to the base of the GPIO register set
static void __iomem *gpio_base;

static int __init my_gpio_init(void)
{
    unsigned int gpio_pin_mask = 1 << GPIO_PIN;
    unsigned int reg_val;

    // Map the entire GPIO register set
    gpio_base = ioremap(GPIO1_BASE_ADDR, GPIO_SIZE);
    if (!gpio_base) {
        printk(KERN_ERR "Unable to map GPIO registers\n");
        return -ENOMEM;
    }

    // Initialize pointers to specific registers within the mapped space
    void __iomem *gpio_oe_addr = gpio_base + GPIO_OE_OFFSET;
    void __iomem *gpio_dataout_addr = gpio_base + GPIO_DATAOUT_OFFSET;
    void __iomem *gpio_datain_addr = gpio_base + GPIO_DATAIN_OFFSET;

    // Set GPIO direction to output and write a value
    reg_val = ioread32(gpio_oe_addr);
    iowrite32(reg_val & ~gpio_pin_mask, gpio_oe_addr);  // Set as output
    iowrite32(ioread32(gpio_dataout_addr) | gpio_pin_mask, gpio_dataout_addr);

    reg_val = ioread32(gpio_oe_addr);
    printk(KERN_INFO "GPIO pin %d is configured as %s\n", GPIO_PIN, (reg_val & gpio_pin_mask) ? "input" : "output");

    printk(KERN_INFO "GPIO direct register access driver loaded\n");
    return 0;
}

static void __exit my_gpio_exit(void)
{
    iounmap(gpio_base);
    printk(KERN_INFO "GPIO direct register access driver unloaded\n");
}

module_init(my_gpio_init);
module_exit(my_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Techveda");
MODULE_DESCRIPTION("BeagleBone Black GPIO driver with comprehensive status reporting");


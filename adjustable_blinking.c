#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#define LED_GPIO 4

MODULE_LICENSE("GPL");

#define DEVICE_NAME "led03"
#define CLASS_NAME "ledclass"

struct led_dev {
    struct cdev cdev;
};

struct file_operations fops = {
    .owner = THIS_MODULE
};

int led_on = 0;
int blinking_on = 0;
int delay = 100;

static int init_led_module(void);
static void exit_led_module(void);
static ssize_t led_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t led_show(struct device *dev, struct device_attribute *attr, char *buf);
void led_blink_delayed_function(struct work_struct *work);
long validateNewDelay(long newDelay);
int getOnOff(const char *buf);
int copyDelayToBuf(int delay, char* buf);

dev_t led_dev_number;
struct device *led_device;
struct class *led_class;
struct led_dev *my_dev;

static DEVICE_ATTR(led_attr, S_IRUSR | S_IWUSR, led_show, led_store);

static struct workqueue_struct *my_wq;

struct my_delayed_work {
	struct delayed_work delayed_work;
};

struct my_delayed_work *delayed_work;

static int init_led_module(void) {
    printk(KERN_DEBUG "Init led module\n");

    my_wq = alloc_workqueue("my_wq", 0, 4);
    if(my_wq) {
		delayed_work = (struct my_delayed_work *)kzalloc(sizeof(struct my_delayed_work),GFP_KERNEL);
		if(delayed_work) {
			INIT_DELAYED_WORK((struct delayed_work *)delayed_work, &led_blink_delayed_function);
        } else {
            goto work;
        }
    } else {
        return -ENODEV;
    }

    if(alloc_chrdev_region(&led_dev_number, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_ALERT "Can't reserve dev_t\n");
        return -ENODEV;
    }
    my_dev = (struct led_dev *)kmalloc(sizeof(struct led_dev), GFP_KERNEL);
    if(my_dev == NULL) {
        printk(KERN_ALERT "Can't reserve memory\n");
        goto chrdev;
    }
    led_class = class_create(THIS_MODULE, CLASS_NAME);
    led_device = device_create(led_class, NULL, led_dev_number, NULL, DEVICE_NAME);
    if(device_create_file(led_device, &dev_attr_led_attr) < 0) {
        printk(KERN_ALERT "Can't create device file for sysfs\n");
        goto class;
    }
    cdev_init(&my_dev->cdev, &fops);
    if(cdev_add(&my_dev->cdev, led_dev_number, 1) < 0 ) {
        printk(KERN_ALERT "Can't add cdev");
        goto class;
    }
    if(!gpio_is_valid(LED_GPIO)) {
        printk(KERN_ALERT "Chosen gpio %d is not valid\n", LED_GPIO);
        goto cdev;
    }
    gpio_request(LED_GPIO, DEVICE_NAME);
    gpio_direction_output(LED_GPIO, led_on);
    return 0;

cdev:
    cdev_del(&my_dev->cdev);

class:
    device_destroy(led_class, led_dev_number);
    class_destroy(led_class);
    kfree(my_dev);

chrdev:
    kfree(delayed_work);
    unregister_chrdev_region(led_dev_number, 1);

work:
    flush_workqueue(my_wq);
    destroy_workqueue(my_wq);
    return -ENODEV;
}

static void exit_led_module(void) {
    printk(KERN_DEBUG "Exit led module\n");
    gpio_set_value(LED_GPIO, 0);
    gpio_free(LED_GPIO);
    cdev_del(&my_dev->cdev);
    device_destroy(led_class, led_dev_number);
    class_destroy(led_class);
    kfree(my_dev);
    unregister_chrdev_region(led_dev_number, 1);
    flush_workqueue(my_wq);
    destroy_workqueue(my_wq);
    kfree(delayed_work);
}

long validateNewDelay(long newDelay) {
    if(newDelay >= 50 && newDelay <= 1000) {
        return newDelay;
    } else {
        return delay;
    }
}

int getOnOff(const char *buf) {
    int ret = strncmp("on", buf, 2);
    if(ret) {
        ret = strncmp("off", buf, 3);
        if(ret) {
            printk(KERN_DEBUG "invalid %s, returning -1\n", buf);
            return -1;
        } else {
            printk(KERN_DEBUG "blinking is to be set off, returning 0\n");
            return 0;
        }
    } else {
        printk(KERN_DEBUG "blinking is to be set on, returning 1\n");
        return 1;
    }
}

static ssize_t led_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    long newDelay = 0;
    int newBlinkingOn = 0;
    printk(KERN_DEBUG "led_store() - Led store called, buf %s\n", buf);
    if(kstrtol(buf, 10, &newDelay)) {
        printk(KERN_DEBUG "led_store() - buf contains no number, checking string\n");
        newBlinkingOn = getOnOff(buf);
        if(newBlinkingOn != -1) {
            blinking_on = newBlinkingOn;
            if(blinking_on == 1 ) {
                queue_delayed_work(my_wq, (struct delayed_work *)delayed_work,delay);
		        printk(KERN_DEBUG "led_blink_delayed_function() - Submitted delayed work at %ld jiffies\n",jiffies);

            } else {
                printk(KERN_DEBUG "led_store() - blinking off\n");
            }
        }
    } else {
        printk(KERN_DEBUG "led_store() - buf contains valid number %ld, using it as a delay\n", newDelay);
        delay = validateNewDelay(newDelay)/10;
    }

    
    return count;
}

void led_blink_delayed_function(struct work_struct *work) {
    printk(KERN_DEBUG "led_blink_delayed_function()\n");
    if(led_on == 0 ) {
        printk(KERN_DEBUG "led_blink_delayed_function() - setting led_on to 1\n");
        led_on = 1;
    } else {
        printk(KERN_DEBUG "led_blink_delayed_function() - setting led_on to 0\n");
        led_on = 0;
    }
    if(blinking_on == 1) {
        queue_delayed_work(my_wq, (struct delayed_work *)delayed_work,delay);
		printk(KERN_DEBUG "led_blink_delayed_function() - Submitted delayed work at %ld jiffies\n",jiffies);
    } else {
        printk(KERN_DEBUG "led_blink_delayed_function() - turning led off\n");
        gpio_set_value(LED_GPIO, 0);
        return;
    }
    printk(KERN_DEBUG "led_blink_function() - turning led on\n");
    gpio_set_value(LED_GPIO, led_on);
}


int copyDelayToBuf(int delay, char* buf) {
    sprintf(buf, "%d\n", delay);
    return strlen(buf) + 1;
}

static ssize_t led_show(struct device *dev, struct device_attribute *att, char *buf) {
    int len;
    printk("led_show() - Led show called\n");
    len = copyDelayToBuf(delay, buf);
    return len;
}

module_init(init_led_module);
module_exit(exit_led_module);


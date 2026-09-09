#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>

#define GPIO_BASE 905
#define MIO_ECHO  4
#define GPIO_ECHO (GPIO_BASE + MIO_ECHO)

#define ECHO_DEVICE_NAME "echo_driver"

#define ECHO_TIMEOUT_MS 40

static dev_t dev_num;
static struct cdev echo_cdev;
static struct class *echo_class;
static int echo_irq;

static ktime_t t_rise;
static bool have_rise;
static s64 pulse_width_us;
static bool measurement_ready;
static DECLARE_WAIT_QUEUE_HEAD(wq);

static irqreturn_t echo_isr(int irq, void *dev_id)
{
    int level = gpio_get_value(GPIO_ECHO);
    ktime_t now = ktime_get();

    if (level == 1) {
         t_rise = now;
        have_rise = true;
    } else {
        if (have_rise) {
            pulse_width_us = ktime_us_delta(now, t_rise);
            have_rise = false;
            measurement_ready = true;
            wake_up_interruptible(&wq);
        }
    }

    return IRQ_HANDLED;
}

static int echo_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "echo_driver: open\n");
    return 0;
}

static int echo_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "echo_driver: release\n");
    return 0;
}

static ssize_t echo_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    long ret;
    if (len < sizeof(pulse_width_us)) { return -EINVAL; }

    measurement_ready = false;
    ret = wait_event_interruptible_timeout(wq, measurement_ready, msecs_to_jiffies(ECHO_TIMEOUT_MS));
    if (ret == 0) { return -ETIMEDOUT; }
    if (ret < 0) { return ret; }

    if (copy_to_user(buf, &pulse_width_us, sizeof(pulse_width_us))) { return -EFAULT; }

    return sizeof(pulse_width_us);
}

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = echo_open,
    .release = echo_release,
    .read    = echo_read,
};

static int __init echo_driver_init(void)
{
    int ret;
    ret = gpio_request(GPIO_ECHO, "echo_pin");
    if (ret) {
        printk(KERN_ERR "echo_driver: gpio_request(%d) 실패, ret=%d\n", GPIO_ECHO, ret);
        return ret;
    }
    gpio_direction_input(GPIO_ECHO);

    echo_irq = gpio_to_irq(GPIO_ECHO);
    ret = request_irq(echo_irq, echo_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "echo_irq", NULL);
    if (ret) {
        printk(KERN_ERR "echo_driver: request_irq 실패, ret=%d\n", ret);
        goto err_free_gpio;
    }

    ret = alloc_chrdev_region(&dev_num, 0, 1, ECHO_DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "echo_driver: 장치 번호 할당 실패\n");
        goto err_free_irq;
    }

    cdev_init(&echo_cdev, &fops);
    ret = cdev_add(&echo_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "echo_driver: 캐릭터 장치 추가 실패\n");
        goto err_unregister_chrdev;
    }

    echo_class = class_create(THIS_MODULE, ECHO_DEVICE_NAME);
    if (IS_ERR(echo_class)) {
        printk(KERN_ERR "echo_driver: class 생성 실패\n");
        ret = PTR_ERR(echo_class);
        goto err_cdev_del;
    }

    device_create(echo_class, NULL, dev_num, NULL, ECHO_DEVICE_NAME);

    printk(KERN_INFO "echo_driver: 초기화 완료\n");
    return 0;

err_cdev_del:
    cdev_del(&echo_cdev);
err_unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
err_free_irq:
    free_irq(echo_irq, NULL);
err_free_gpio:
    gpio_free(GPIO_ECHO);
    return ret;
}

static void __exit echo_driver_exit(void)
{
    device_destroy(echo_class, dev_num);
    class_destroy(echo_class);
    cdev_del(&echo_cdev);
    unregister_chrdev_region(dev_num, 1);

    free_irq(echo_irq, NULL);
    gpio_free(GPIO_ECHO);

    printk(KERN_INFO "echo_driver: 종료\n");
}

module_init(echo_driver_init);
module_exit(echo_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YGC");
MODULE_DESCRIPTION("Boat ultrasonic echo pulse-width driver (blocking read, PS MIO GPIO)");
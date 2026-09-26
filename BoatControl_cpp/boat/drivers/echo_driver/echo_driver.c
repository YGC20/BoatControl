#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>

/*      핀 설정     */

static int gpio_base = 905;
module_param(gpio_base, int, 0644);
MODULE_PARM_DESC(gpio_base, 
    "PS GPIO base (zynq_gpio chip base, boot 후 /sys/kernel/debug/gpio로 확인)");

#define MIO_ECHO     14 // JF9
#define MIO_TRIGGER  15 // JF10

#define GPIO_ECHO    (gpio_base + MIO_ECHO)
#define GPIO_TRIGGER (gpio_base + MIO_TRIGGER)

/*******************/

#define ECHO_DEVICE_NAME "echo_driver"

#define ECHO_TIMEOUT_MS 40
#define TRIGGER_PULSE_US 10

static dev_t dev_num;
static struct cdev echo_cdev;
static struct class *echo_class;
static int echo_irq;

static ktime_t t_rise;
static bool have_rise;
static s64 pulse_width_us;
static bool measurement_ready;
static DECLARE_WAIT_QUEUE_HEAD(wq);

static spinlock_t echo_lock;

static irqreturn_t echo_isr(int irq, void *dev_id)
{
    spin_lock(&echo_lock);
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

    spin_unlock(&echo_lock);
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

/*
 * write()를 트리거 신호로 사용한다. 유저스페이스에서 넘긴 데이터 내용은
 * 보지 않고, write가 호출됐다는 사실 자체를 "trigger 핀을 TRIGGER_PULSE_US
 * 동안 High로 올렸다가 내려라"는 명령으로 취급한다.
 * (예전엔 유저스페이스가 libgpiod로 /dev/gpiochip0을 직접 열어 이 핀을
 *  토글했는데, libgpiod 2.x는 커널 GPIO uAPI v2(커널 5.10+)가 있어야
 *  동작해서 구형 커널을 쓰는 PetaLinux 2017.4에서는 쓸 수 없었다.
 *  그래서 motor_driver와 동일한 방식으로 커널 모듈이 핀을 직접 소유하게
 *  옮겼다.)
 */
static ssize_t echo_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    gpio_set_value(GPIO_TRIGGER, 1);
    udelay(TRIGGER_PULSE_US);
    gpio_set_value(GPIO_TRIGGER, 0);
    return len;
}

static ssize_t echo_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    unsigned long flags;
    s64 copy_pwu;
    long ret;

    if (len < sizeof(pulse_width_us)) { return -EINVAL; }

    measurement_ready = false;
    ret = wait_event_interruptible_timeout(wq, measurement_ready, msecs_to_jiffies(ECHO_TIMEOUT_MS));
    if (ret == 0) { return -ETIMEDOUT; }
    if (ret < 0) { return ret; }

    spin_lock_irqsave(&echo_lock, flags);
    copy_pwu = pulse_width_us;
    spin_unlock_irqrestore(&echo_lock, flags);

    if (copy_to_user(buf, &copy_pwu, sizeof(copy_pwu))) { return -EFAULT; }

    return sizeof(copy_pwu);
}

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = echo_open,
    .release = echo_release,
    .write   = echo_write,
    .read    = echo_read,
};

static int __init echo_driver_init(void)
{
    spin_lock_init(&echo_lock);

    int ret;
    ret = gpio_request(GPIO_ECHO, "echo_pin");
    if (ret) {
        printk(KERN_ERR "echo_driver: gpio_request(%d) 실패, ret=%d\n", GPIO_ECHO, ret);
        return ret;
    }
    gpio_direction_input(GPIO_ECHO);

    ret = gpio_request(GPIO_TRIGGER, "echo_trigger");
    if (ret) {
        printk(KERN_ERR "echo_driver: gpio_request(trigger=%d) 실패, ret=%d\n", GPIO_TRIGGER, ret);
        goto err_free_echo;
    }
    gpio_direction_output(GPIO_TRIGGER, 0);

    echo_irq = gpio_to_irq(GPIO_ECHO);
    ret = request_irq(echo_irq, echo_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "echo_irq", NULL);
    if (ret) {
        printk(KERN_ERR "echo_driver: request_irq 실패, ret=%d\n", ret);
        goto err_free_trigger;
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

    printk(KERN_INFO "echo_driver: 초기화 완료 (trigger=GPIO%d, echo=GPIO%d)\n", GPIO_TRIGGER, GPIO_ECHO);
    return 0;

err_cdev_del:
    cdev_del(&echo_cdev);
err_unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
err_free_irq:
    free_irq(echo_irq, NULL);
err_free_trigger:
    gpio_free(GPIO_TRIGGER);
err_free_echo:
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
    gpio_free(GPIO_TRIGGER);
    gpio_free(GPIO_ECHO);

    printk(KERN_INFO "echo_driver: 종료\n");
}

module_init(echo_driver_init);
module_exit(echo_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YGC");
MODULE_DESCRIPTION("Boat ultrasonic sensor driver (trigger output + echo pulse-width input, PS MIO GPIO)");

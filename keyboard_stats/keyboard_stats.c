#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/time.h>
#include <asm/io.h>

MODULE_LICENSE("GPL");

#define DATA_REG        0x60    /* I/O port for keyboard data */
#define SCANCODE_MASK   0x7f
#define STATUS_MASK     0x80

#define PRINT_INTERVAL 10L
#define NANOS_PER_SEC (1000L * 1000L * 1000L)

struct KeyboardStatsData
{
    size_t pressCount;
    spinlock_t lock;
    struct task_struct* printThread;
};

static struct KeyboardStatsData keyboardStatsData;

static irqreturn_t statsHandler(int irq, void *dev_id)
{
    char scancode;

    scancode = inb(DATA_REG);
    // key pressed
    if ((scancode & STATUS_MASK) == 0)
    {
        spin_lock(&keyboardStatsData.lock);
        ++keyboardStatsData.pressCount;
        spin_unlock(&keyboardStatsData.lock);
    }

    return IRQ_HANDLED;
}

static int printStats(void* data)
{
    struct timespec lastPrint;
    getnstimeofday(&lastPrint);
    spin_lock(&keyboardStatsData.lock);
    keyboardStatsData.pressCount = 0;
    spin_unlock(&keyboardStatsData.lock);

    struct timespec ts;
    while (!kthread_should_stop())
    {
        msleep_interruptible(100);
        getnstimeofday(&ts);
        long nanos = (ts.tv_sec - lastPrint.tv_sec) * NANOS_PER_SEC + ts.tv_nsec - lastPrint.tv_nsec;
        if (nanos > PRINT_INTERVAL * NANOS_PER_SEC)
        {
            spin_lock(&keyboardStatsData.lock);
            printk(KERN_INFO "Symbols pressed in %ldms : %ld\n", 1000 * nanos / NANOS_PER_SEC, keyboardStatsData.pressCount);
            keyboardStatsData.pressCount = 0;
            spin_unlock(&keyboardStatsData.lock);
            lastPrint = ts;
        }
    }
    return 0;
}

static int __init init(void)
{
    // Keyboard IRQ number is 1
    int res = request_irq(1, statsHandler, IRQF_SHARED, "keyboard_stats", (void *)&keyboardStatsData);
    if (res)
    {
        printk(KERN_ALERT "Could not register irq : %d\n", res);
        return res;
    }
    spin_lock_init(&keyboardStatsData.lock);
    keyboardStatsData.printThread = kthread_run(&printStats, NULL, "printStats");
    return 0;
}

static void __exit exit(void)
{
    kthread_stop(keyboardStatsData.printThread);
    // Keyboard IRQ number is 1
    free_irq(1, (void *)&keyboardStatsData);
}

module_init(init);
module_exit(exit);

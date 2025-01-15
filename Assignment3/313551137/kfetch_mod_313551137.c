#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/cpufreq.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include "kfetch.h"

#define DEVICE_NAME "kfetch"
#define CLASS_NAME "kfetch_class"

static DEFINE_MUTEX(kfetch_mutex);
static int major_number;
static struct class *kfetch_class;
static struct device *kfetch_device;
//static char *kfetch_buffer;
static int current_mask = KFETCH_FULL_INFO;

// Logo
static const char *logo[] = {
    "        .-.        ",
    "       (.. |       ",
    "       <>  |       ",
    "      / --- \\      ",
    "     ( |   | )     ",
    "   |\\_)___(_//|    ",
    "  <__)------(__>   "
};

// Function declarations
static int get_hostname(char *buf);
static int get_kernel_release(char *buf);
static int get_cpu_info(char *model, char *cores);
static int get_memory_info(char *buf);
static int get_process_count(char *buf);
static int get_uptime(char *buf);

// Device operations
static int kfetch_open(struct inode *inode, struct file *file)
{
    // mutex_lock 會等待直到獲得鎖
    mutex_lock(&kfetch_mutex);
    return 0;
}

static int kfetch_release(struct inode *inode, struct file *file)
{
    // 釋放互斥鎖
    mutex_unlock(&kfetch_mutex);
    return 0;
}
//將 logo 和系統資訊並排顯示
static ssize_t kfetch_read(struct file *filp, char __user *buffer,
                          size_t length, loff_t *offset)
{
    char *temp_buf; // 臨時緩衝區
    int len = 0; // 當前寫入長度
    char hostname[65];
    char separator[65]; // 分隔線緩衝區
    char space[65];
    int i;
    // 分配臨時緩衝區，使用 GFP_KERNEL 表示這是普通的內核記憶體分配
    // kzalloc 會將分配的記憶體初始化為0
    temp_buf = kzalloc(KFETCH_BUF_SIZE, GFP_KERNEL);
    if (!temp_buf)
        return -ENOMEM;  // 記憶體分配失敗則返回錯誤

    memset(space, ' ', strlen(logo[0]));
    space[strlen(logo[0])] = '\0';
    // Get hostname並寫入緩衝區
    get_hostname(hostname);
    len += snprintf(temp_buf + len, KFETCH_BUF_SIZE - len, "%s  %s\n", space, hostname);
    
    // 創建與主機名等長的分隔線
    memset(separator, '-', strlen(hostname));
    separator[strlen(hostname)] = '\0';
    len += snprintf(temp_buf + len, KFETCH_BUF_SIZE - len, "%s  %s\n", space, separator);

    // Add logo and information side by side
    for (i = 0; i < 7; i++) {
        len += snprintf(temp_buf + len, KFETCH_BUF_SIZE - len, "%s", logo[i]);
        
        switch(i) {
            case 0: // 0: 顯示內核版本
                if (current_mask & KFETCH_RELEASE) {
                    char release[65];
                    get_kernel_release(release);
                    len += snprintf(temp_buf + len, KFETCH_BUF_SIZE - len, "  Kernel: %s", release);
                }
                break;
            case 1: // 1: 顯示CPU model name
                if (current_mask & KFETCH_CPU_MODEL) {
                    char model[100], cores[65];
                    get_cpu_info(model, cores);
                    len += snprintf(temp_buf + len, KFETCH_BUF_SIZE - len, "  CPU: %s", model);
                }
                break;
            case 2: // 2: 顯示The number of CPU cores
                if (current_mask & KFETCH_NUM_CPUS) {
                    char model[100], cores[65];
                    get_cpu_info(model, cores);
                    len += snprintf(temp_buf + len, KFETCH_BUF_SIZE - len, "  CPUs: %s", cores);
                }
                break;
            case 3: // 3: 顯示Mem
                if (current_mask & KFETCH_MEM) {
                    char mem[65];
                    get_memory_info(mem);
                    len += snprintf(temp_buf + len, KFETCH_BUF_SIZE - len, "  Mem: %s", mem);
                }
                break;
            case 4: // 4: 顯示Procs
                if (current_mask & KFETCH_NUM_PROCS) {
                    char procs[65];
                    get_process_count(procs);
                    len += snprintf(temp_buf + len, KFETCH_BUF_SIZE - len, "  Procs: %s", procs);
                }
                break;
            case 5: // 5: 顯示Uptime
                if (current_mask & KFETCH_UPTIME) {
                    char uptime[65];
                    get_uptime(uptime);
                    len += snprintf(temp_buf + len, KFETCH_BUF_SIZE - len, "  Uptime: %s", uptime);
                }
                break;
        }
        // 每行結尾加上換行符
        len += snprintf(temp_buf + len, KFETCH_BUF_SIZE - len, "\n");
    }

    if (copy_to_user(buffer, temp_buf, len)) {
        kfree(temp_buf);
        return -EFAULT;
    }

    kfree(temp_buf);
    return len;
}

static ssize_t kfetch_write(struct file *filp, const char __user *buffer,
                           size_t length, loff_t *offset)
{
    int new_mask;
    // 確保寫入的長度正確
    if (length != sizeof(int))
        return -EINVAL;
    // 從用戶空間複製資料到 kernel 空間
    if (copy_from_user(&new_mask, buffer, length))
        return -EFAULT;
    // 更新顯示遮罩
    current_mask = new_mask;
    return length;
}

static const struct file_operations kfetch_fops = {
    .owner = THIS_MODULE,  // 表示這個模組擁有這些操作
    .open = kfetch_open,   // 打開設備時調用
    .release = kfetch_release, // 關閉設備時調用
    .read = kfetch_read,   // 讀取設備時調用
    .write = kfetch_write,  // 寫入設備時調用
};

// Helper functions implementation
static int get_hostname(char *buf)
{
    // utsname() 函數返回系統的各種標識信息
    struct new_utsname *uts;
    uts = utsname();
    return snprintf(buf, 65, "%s", uts->nodename);
}

static int get_kernel_release(char *buf)
{
    struct new_utsname *uts;
    uts = utsname();
    // release 包含了內核版本信息
    return snprintf(buf, 65, "%s", uts->release);
}

static int get_cpu_info(char *model, char *cores)
{
    // cpu_data(0) 獲取第一個 CPU 的資訊
    struct cpuinfo_x86 *c = &cpu_data(0);
    // 獲取在線和總計的 CPU 數量
    int online_cpus = num_online_cpus();
    int total_cpus = num_present_cpus();
    // 將資訊寫入提供的緩衝區
    snprintf(model, 100, "%s", c->x86_model_id);
    snprintf(cores, 65, "%d / %d", online_cpus, total_cpus);
    return 0;
}

static int get_memory_info(char *buf)
{
    struct sysinfo si;
    unsigned long free_mb, total_mb;
    // 獲取系統資訊
    si_meminfo(&si);
    // 將頁數轉換為 MB
    free_mb = si.freeram >> (20 - PAGE_SHIFT);
    total_mb = si.totalram >> (20 - PAGE_SHIFT);
    
    return snprintf(buf, 65, "%lu / %lu MB", free_mb, total_mb);
}

static int get_process_count(char *buf)
{
    struct task_struct *task;
    int count = 0;
    // 使用 RCU（Read-Copy-Update）機制保護進程列表的讀取
    rcu_read_lock();
    // for_each_process 是一個巨集，用於遍歷所有進程
    for_each_process(task) {
        count++;
    }
    rcu_read_unlock();
    
    return snprintf(buf, 65, "%d", count);
}

static int get_uptime(char *buf)
{
    struct timespec64 uptime;
    unsigned long minutes;
    
    ktime_get_boottime_ts64(&uptime);
    minutes = uptime.tv_sec / 60;
    
    return snprintf(buf, 65, "%lu minutes", minutes);
}
//模組的初始化
static int __init kfetch_init(void)
{
    // 註冊字符設備，0表示動態分配主設備號
    major_number = register_chrdev(0, DEVICE_NAME, &kfetch_fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Failed to register a major number\n");
        return major_number;
    }

    // 創建設備類別
    kfetch_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(kfetch_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(kfetch_class);
    }

    // 創建設備節點
    kfetch_device = device_create(kfetch_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(kfetch_device)) {
        class_destroy(kfetch_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(kfetch_device);
    }

    printk(KERN_INFO "kfetch: module loaded\n");
    return 0;
}
//清理函數
static void __exit kfetch_exit(void)
{
    // 清理過程是初始化的反向操作
    device_destroy(kfetch_class, MKDEV(major_number, 0)); // 移除設備節點
    class_unregister(kfetch_class);                       // 取消註冊設備類別
    class_destroy(kfetch_class);                         // 銷毀設備類別
    unregister_chrdev(major_number, DEVICE_NAME);        // 取消註冊字符設備
    printk(KERN_INFO "kfetch: module unloaded\n");       // 輸出清理完成訊息
}

module_init(kfetch_init); // 告訴 kernel 載入模組時要調用 kfetch_init
module_exit(kfetch_exit); // 告訴 kernel 卸載模組時要調用 kfetch_exit

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Liu-HSIANG Kuan");
MODULE_DESCRIPTION("System Information Fetching Module");
MODULE_VERSION("0.1");
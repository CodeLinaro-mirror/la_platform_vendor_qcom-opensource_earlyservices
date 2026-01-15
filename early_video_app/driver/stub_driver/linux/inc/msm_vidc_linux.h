/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __MSM_VIDC_LINUX_H__
#define __MSM_VIDC_LINUX_H__

#include <linux\types.h>
#include <linux\videodev2.h>
#include <linux\completion.h>
#include <string.h> /* for strnstr */
#include <linux/interrupt.h>
#include <stdio.h> /* for snprintf in _dump */
#include "..\..\config\waipio_video.h"

#ifdef MSM_VIDC_EMPTY_BRACE
#undef MSM_VIDC_EMPTY_BRACE
#endif
#define MSM_VIDC_EMPTY_BRACE

#define LOG_MSG_SIZE 256
#define __init
#define __exit
#define MODULE_DEVICE_TABLE(type, name)
#define MODULE_LICENSE(...)
#define MODULE_SOFTDEP(...)

/* todo:chinmays: Pickup
* msm-5.4/include/uapi/asm-generic/errno.h
* msm-5.4/include/uapi/asm-generic/errno-base.h
* EBADR used in v4l2 and vb2
*/
#define ENODEV 19
#define EBADR	53 
#define ENOTSUPP	524
#define MAX_ERRNO	4095
#define EBADHANDLE	521	/* Illegal NFS file handle */

#define USEC_PER_SEC	1000000L
#define NSEC_PER_USEC	1000LL
#define NSEC_PER_MSEC	1000000LL
#define MSEC_PER_SEC	1000L
#define GFP_ATOMIC 0

#define min_t(type, a, b) min(a, b)
#define max_t(type, a, b) max(a, b)
#define clamp_t(type, val, lo, hi) min_t(type, max_t(type, val, lo), hi)
#define ALIGN(x, align) (((x) + ((align)-1)) & ~((align)-1))
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
#define IS_ALIGNED(p, bytes) ( (((size_t)p) & (bytes-1))==0 )

#define MEMREMAP_WC (1 << 2) /* From: msm-5.4/include/linux/io.h */

#define module_param_cb(name, ops, arg, perm)
#define DEFINE_RATELIMIT_STATE(name, interval_init, burst_init)	int name = 0
#define __ratelimit(x) (*(x))

#define PAGE_SIZE 4096
typedef long long loff_t;

#ifndef _VA_LIST_DEFINED
#define _VA_LIST_DEFINED
#ifdef _M_CEE_PURE
typedef System::ArgIterator va_list;
#else
typedef char* va_list;
#endif
#endif

typedef irqreturn_t(*irq_handler_t)(int, void*);

struct file* file_dec_g;
struct file* file_enc_g;

struct reset_control {
	int placeholder;
};

struct scatterlist {
	unsigned long	page_link;
	unsigned int	offset;
	unsigned int	length;
	dma_addr_t	dma_address;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
	unsigned int	dma_length;
#endif
};

struct sg_table {
	struct scatterlist* sgl;
};

struct ve2_queue {
	struct list_head    done_list;
	struct v4l2_event* event_val;
};

struct video_event_queue {
	struct ve2_queue ve2_eventq;
	struct mutex  lock;
};

struct v4l2_fh {
	struct list_head	list;
	struct video_device* vdev;
	struct v4l2_ctrl_handler* ctrl_handler;
	enum v4l2_priority	prio;
	wait_queue_head_t	wait;
	struct mutex		subscribe_lock;
	struct list_head	subscribed;
	struct list_head	available;
	unsigned int		navailable;
	u32			        sequence;
};

struct inode {
	unsigned long       i_ino;
	void               *i_private;
};

struct dentry {
	struct inode       *d_inode;
};

struct file {
	struct video_device* vdev;
	void* private_data;
	struct video_event_queue video_event;
	u32 domain;
	struct inode* f_inode;
};

struct clk {
	int placeholder;
};

enum vidc_domain {
	VIDC_DECODE = 0,
	VIDC_ENCODE,
};

struct file_operations {
	int (*open) (struct inode *, struct file *);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	int (*release) (struct inode *, struct file *);
};

#define cprint(msg,...) print_log(1, "msm_vidc:" msg, ##__VA_ARGS__)
#define pr_info(msg, ...) print_log(1, msg, ##__VA_ARGS__)
#define pr_warn(msg, ...) print_log(1, msg, ##__VA_ARGS__)
#define WARN print_log
#define strlcpy(dest,src,size) strncpy_s(dest,size,src,(size) ? (size) - 1 : 0)
#define strlcat(dest,src,size) strncat_s(dest,size,src,(size) ? (size) - 1 : 0)
#if _MSC_VER < 1700
#define snprintf(fmt,size,...) sprintf_s(fmt,size,##__VA_ARGS__)
#endif
#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-MAX_ERRNO)
#define scnprintf sprintf_s

int of_device_is_compatible(const struct device_node* device,
	const char* name);
const char* dev_name(const struct device* dev);
void __iomem* devm_ioremap_nocache(struct device* dev,
	resource_size_t offset, resource_size_t size);
void devm_iounmap(struct device* dev, void __iomem* addr);
void* dev_get_drvdata(const struct device* dev);
void dev_set_drvdata(struct device* dev, void* data);
struct video_device* video_devdata(struct file* file);
void* video_drvdata(struct file* file);
void video_set_drvdata(struct video_device* vdev, void* data);
void usleep_range(u32 min, u32 max);
void disable_irq_nosync(unsigned int irq);
u32 writel_relaxed(u32 value, u8* base_addr);
u32 readl_relaxed(u8* addr);
extern void print_log(unsigned int level, const char* str_msg, ...);
int request_irq(unsigned int irq, irq_handler_t hdrl, unsigned long flags,
	const char* name, void* device);
void enable_irq(unsigned int irq);
void subsystem_put(void* subsystem);
void* subsystem_get_with_fwname(const char* name, const char* fw_name);
void clk_disable_unprepare(struct clk* clk);
int clk_prepare_enable(struct clk* clk);
int clk_set_flags(struct clk* clk, unsigned long flags);
long clk_round_rate(struct clk* clk, unsigned long rate);
bool __clk_is_enabled(struct clk* clk);
void clk_put(struct clk* clk);
struct clk* clk_get(struct device* dev, const char* id);
int clk_set_rate(struct clk* clk, unsigned long rate);
int reset_control_assert(struct reset_control* rstc);
int reset_control_deassert(struct reset_control* rstc);
struct reset_control* devm_reset_control_get(struct device* dev, const char* id);
int qcom_scm_mem_protect_video_var(u32 cp_start, u32 cp_size,
	u32 cp_nonpixel_start, u32 cp_nonpixel_size);
int qcom_scm_set_remote_state(u32 state, u32 id);
void mb();
void wmb();
void rmb();
unsigned long msecs_to_jiffies(const unsigned int m);
int platform_driver_register(struct platform_driver* drv);
int platform_driver_unregister(struct platform_driver* drv);
int video_register_device(struct video_device* vdev,
	enum vfl_devnode_type type, int nr);
void video_unregister_device(struct video_device* vdev);
int of_platform_populate(struct device_node* root,
	const struct of_device_id* matches,
	const struct of_dev_auxdata* lookup,
	struct device* parent);
int v4l2_device_register(struct device* dev, struct v4l2_device* v4l2_dev);
void v4l2_device_unregister(struct v4l2_device* v4l2_dev);
struct dma_buf* dma_buf_get(int fd);
void dma_buf_put(struct dma_buf* dmabuf);
struct dma_buf_attachment* dma_buf_attach(struct dma_buf* dmabuf,
	struct device* dev);
struct sg_table* dma_buf_map_attachment(struct dma_buf_attachment* attach,
	enum dma_data_direction direction);
void dma_buf_unmap_attachment(struct dma_buf_attachment* attach,
	struct sg_table* sg_table,
	enum dma_data_direction direction);
void dma_buf_detach(struct dma_buf* dmabuf, struct dma_buf_attachment* attach);
struct dma_buf* ion_alloc(size_t len, unsigned int heap_id_mask,
	unsigned int flags);
int dma_buf_begin_cpu_access(struct dma_buf* dmabuf,
	enum dma_data_direction direction);
int dma_buf_end_cpu_access(struct dma_buf* dmabuf,
	enum dma_data_direction direction);
void* dma_buf_vmap(struct dma_buf* dmabuf);
void dma_buf_vunmap(struct dma_buf* dmabuf, void* vaddr);
struct inode *file_inode(struct file *f);
void clear_bit(int nr, u32* addr);
void kref_init(struct kref* kref);
void kref_get(struct kref* kref);
int kref_put(struct kref* kref, void(*release)(struct kref* kref));
int kref_get_unless_zero(struct kref* kref);
int hex_dump_to_buffer(const void* buf, size_t len, int rowsize, int groupsize,
	char* linebuf, size_t linebuflen, bool ascii);
int mutex_is_locked(struct mutex* lock);
int copy_to_user(void* to, const void* from, unsigned long n);
int copy_from_user(void* to, const void* from, unsigned long n);
void spin_lock_irqsave(spinlock_t* lock, int flags);
void spin_unlock_irqrestore(spinlock_t* lock, int flags);
int dma_set_max_seg_size(struct device* dev, unsigned int size);
void dma_set_seg_boundary(struct device* dev, unsigned long mask);
char* strnstr(const char* haystack, const char* needle, size_t len);
void v4l2_fh_init(struct v4l2_fh* fh, struct video_device* vdev);
struct resource* platform_get_resource(struct platform_device* dev,
	unsigned int type, unsigned int num);
struct timeval ns_to_timeval(const s64 nsec);
void sysfs_remove_group(struct kobject* kobj,
	const struct attribute_group* grp);
void* free_irq(unsigned int irq, void* dev_id);
int irq_signal();
void atomic_inc(atomic_t* val);
int atomic_read(atomic_t* val);
void atomic_dec(atomic_t* val);
void atomic_set(atomic_t* val, int set_val);
void* cmalloc(size_t size);
int msm_v4l2_dqevent(int driver_handle, struct v4l2_event* e);
void debugfs_remove_recursive(struct dentry* dentry);

static inline long IS_ERR_OR_NULL(const void* ptr)
{
	return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}

static inline bool IS_ERR(const void* ptr)
{
	return IS_ERR_VALUE((unsigned long)ptr);
}

static inline void* ERR_PTR(long error)
{
	return (void*)error;
}

static inline long PTR_ERR(const void* ptr)
{
	return (long)ptr;
}

static inline int sysfs_create_group(struct kobject* kobj,
	const struct attribute_group* grp)
{
	return 0;
}

static inline resource_size_t resource_size(const struct resource* res)
{
	return 0x500000; // FW size 5MB
}

static inline u64 ktime_get_ns()
{
	return 0;
}

static inline u64 div_u64(u64 dividend, u32 divisor)
{
	return dividend / divisor;
}

static inline void dev_coredumpv(struct device *dev, void *data,
				 size_t datalen, gfp_t gfp)
{
	vfree(data);
}

static inline int kstrtouint(const char *s, unsigned int base, unsigned int *res)
{
	return atoi(s);
}

static inline int kstrtoul(const char *s, unsigned int base, unsigned long *res)
{
	return atoi(s);
}

static inline int simple_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;
	return 0;
}

static inline int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	int i;

	i = snprintf(buf, size, fmt, args);

	if (i < size)
		return i;
	if (size != 0)
		return size - 1;
	return 0;
}

static inline void debugfs_remove_recursive(struct dentry *dentry)
{
	return 0;
}

static inline ssize_t simple_read_from_buffer(void __user *to, size_t count,
	loff_t *ppos, const void *from, size_t available)
{
	return 0;
}

static inline struct dentry *debugfs_create_dir(const char *name,
	struct dentry *parent)
{
	return NULL;
}

static inline void debugfs_create_u32(const char *name, umode_t mode,
	struct dentry *parent, u32 *value)
{
	return 0;
}

static inline struct dentry *debugfs_create_bool(const char *name,
	umode_t mode, struct dentry *parent, bool *value)
{
	return NULL;
}

static inline struct dentry *debugfs_create_file(const char *name,
	umode_t mode, struct dentry *parent,
	void *data, const struct file_operations *fops)
{
	return NULL;
}

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#endif

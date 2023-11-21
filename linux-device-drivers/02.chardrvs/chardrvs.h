#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/cdev.h>

#define CHARDRVS_IOC_MAGIC	'c'
#define CHARDRVS_IOC_MAX_NR	(3)
#define CHARDRVS_IOCSETNRUSERS		_IOW(CHARDRVS_IOC_MAGIC, 1, int *)
#define CHARDRVS_IOCGETNRUSERS		_IOR(CHARDRVS_IOC_MAGIC, 2, int *)
#define CHARDRVS_IOCQUERYAVAILSIZE	_IOR(CHARDRVS_IOC_MAGIC, 3, int *)

#define CHARDRVS_DBG

#define DRIVER_NAME			"chardrvs"
#define DRIVER_CLASS		"chardrvsclass"
#define BASE_MINORS			(0)
#define NR_MINORS			(1)
#define DEFAULT_FIFO_SIZE	(16)
#define DEFAULT_NR_USERS	(5)

struct chardrvs_priv_dev {
	dev_t dev_nr;
	struct class *new_class;
	struct cdev new_cdevice;
	struct kfifo myfifo;
	struct mutex r_f_lock;
	struct mutex w_f_lock;
#ifdef CHARDRVS_DBG
	struct proc_dir_entry *proc_file;
	struct dentry *dbg_dir;
	struct dentry *dbg_file;
#endif
	/**
	 * General lock for concurrent access
	 * to our device structure elements
	 */
	struct mutex lock;

	/**
	 * ioctl, procfs info use
	 */
	unsigned int  usrs_cnt;
	unsigned int avail;
	atomic_t ref_cntr;

	/**
	 * sleep, poll, select use
	 */
	 wait_queue_head_t wq_f;
};

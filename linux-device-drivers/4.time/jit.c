/**
 * jit.c -- the just-in-time module
 *
 * Copyright (C) 2001,2003 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001,2003 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 * jit.c,v 1.16 2004/09/26 07:02:43 gregkh Exp
 */

/**
 * Only adjust kernel APIs and data types for v5.19
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <asm/hardirq.h>

/**
 * This module is a silly one: it only embeds short code fragments
 * that show how time delays can be handled in the kernel.
 */

static int delay = HZ;
module_param(delay, int, 0);
static int tdelay = 10;
module_param(tdelay, int, 0);

MODULE_AUTHOR("Alessandro Rubini");
MODULE_LICENSE("Dual BSD/GPL");

enum jit_files {
	JIT_BUSY = 0,
	JIT_SCHED,
	JIT_QUEUE,
	JIT_SCHEDTO
};

int jit_fn_show(struct seq_file *m, void *v)
{
	unsigned long j0, j1;
	wait_queue_head_t wait;
	long data = (long)m->private;

	init_waitqueue_head(&wait);
	j0 = jiffies;
	j1 = j0 + delay;

	switch (data) {
	case JIT_BUSY:
		while (time_before(jiffies, j1))
			cpu_relax();
		break;
	case JIT_SCHED:
		while (time_before(jiffies, j1))
			schedule();
		break;
	case JIT_QUEUE:
		wait_event_interruptible_timeout(wait, 0, delay);
		break;
	case JIT_SCHEDTO:
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(delay);
		break;
	}
	j1 = jiffies;

	seq_printf(m, "%9li %9li\n", j0, j1);
	return 0;
}

static int jit_fn_open(struct inode *inode, struct file *file)
{
	return single_open(file, jit_fn_show, pde_data(inode));
}

static struct proc_ops jit_fn_fops = {
	.proc_open		= jit_fn_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};

int jit_currentime_show(struct seq_file *m, void *v)
{
	unsigned long j1;
	u64 j2;

	j1 = jiffies;
	j2 = get_jiffies_64();

	/**
	 * Scope only to avoid C90 warning
	 * forbids mixed declarations and code in C
	 */
	{
		struct timespec64 tv1;
		struct timespec64 tv2;
		ktime_get_real_ts64(&tv1);
		ktime_get_coarse_real_ts64(&tv2);
		seq_printf(m, "0x%08lx 0x%016Lx %10i.%09i\n"
			"%40i.%09i\n",
			j1, j2,
			(int) tv1.tv_sec, (int) tv1.tv_nsec,
			(int) tv2.tv_sec, (int) tv2.tv_nsec);
	}

	return 0;
}

static int jit_currentime_open(struct inode *inode, struct file *file)
{
	return single_open(file, jit_currentime_show, NULL);
}

static struct proc_ops jit_currentime_fops = {
	.proc_open	= jit_currentime_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

struct jit_data {
	struct timer_list timer;
	struct tasklet_struct tlet;
	struct seq_file *m;
	int hi;
	wait_queue_head_t wait;
	unsigned long prevjiffies;
	int loops;
};
#define JIT_ASYNC_LOOPS 5

void jit_timer_fn(struct timer_list *t)
{
	struct jit_data *data = from_timer(data, t, timer);
	unsigned long j = jiffies;
	seq_printf(data->m, "%9li  %3li     %i    %6i   %i   %s\n",
			     j, j - data->prevjiffies, in_interrupt() ? 1 : 0,
			     current->pid, smp_processor_id(), current->comm);

	if (--data->loops) {
		data->timer.expires += tdelay;
		data->prevjiffies = j;
		add_timer(&data->timer);
	} else {
		wake_up_interruptible(&data->wait);
	}
}

int jit_timer_show(struct seq_file *m, void *v)
{
	struct jit_data *data;
	unsigned long j = jiffies;

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	init_waitqueue_head(&data->wait);

	seq_puts(m, "   time   delta  inirq    pid   cpu command\n");
	seq_printf(m, "%9li  %3li     %i    %6i   %i   %s\n",
			j, 0L, in_interrupt() ? 1 : 0,
			current->pid, smp_processor_id(), current->comm);

	data->prevjiffies = j;
	data->m = m;
	data->loops = JIT_ASYNC_LOOPS;

	timer_setup(&data->timer, jit_timer_fn, 0);
	data->timer.expires = j + tdelay;
	add_timer(&data->timer);

	wait_event_interruptible(data->wait, !data->loops);
	if (signal_pending(current))
		return -ERESTARTSYS;
	kfree(data);
	return 0;
}

static int jit_timer_open(struct inode *inode, struct file *file)
{
	return single_open(file, jit_timer_show, NULL);
}

static struct proc_ops jit_timer_fops = {
	.proc_open		= jit_timer_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};

void jit_tasklet_fn(unsigned long arg)
{
	struct jit_data *data = (struct jit_data *)arg;
	unsigned long j = jiffies;
	seq_printf(data->m, "%9li  %3li     %i    %6i   %i   %s\n",
			     j, j - data->prevjiffies, in_interrupt() ? 1 : 0,
			     current->pid, smp_processor_id(), current->comm);

	if (--data->loops) {
		data->prevjiffies = j;
		if (data->hi)
			tasklet_hi_schedule(&data->tlet);
		else
			tasklet_schedule(&data->tlet);
	} else {
		wake_up_interruptible(&data->wait);
	}
}

int jit_tasklet_show(struct seq_file *m, void *v)
{
	struct jit_data *data;
	unsigned long j = jiffies;
	long hi = (long)m->private;

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	init_waitqueue_head(&data->wait);

	seq_puts(m, "   time   delta  inirq    pid   cpu command\n");
	seq_printf(m, "%9li  %3li     %i    %6i   %i   %s\n",
			j, 0L, in_interrupt() ? 1 : 0,
			current->pid, smp_processor_id(), current->comm);

	data->prevjiffies = j;
	data->m = m;
	data->loops = JIT_ASYNC_LOOPS;

	tasklet_init(&data->tlet, jit_tasklet_fn, (unsigned long)data);
	data->hi = hi;
	if (hi)
		tasklet_hi_schedule(&data->tlet);
	else
		tasklet_schedule(&data->tlet);

	wait_event_interruptible(data->wait, !data->loops);

	if (signal_pending(current))
		return -ERESTARTSYS;
	kfree(data);
	return 0;
}

static int jit_tasklet_open(struct inode *inode, struct file *file)
{
	return single_open(file, jit_tasklet_show, pde_data(inode));
}

static struct proc_ops jit_tasklet_fops = {
	.proc_open	= jit_tasklet_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

int __init jit_init(void)
{
	proc_create_data("currentime", 0, NULL, &jit_currentime_fops, NULL);

	proc_create_data("jitbusy", 0, NULL, &jit_fn_fops, (void *)JIT_BUSY);
	proc_create_data("jitsched", 0, NULL, &jit_fn_fops, (void *)JIT_SCHED);
	proc_create_data("jitqueue", 0, NULL, &jit_fn_fops, (void *)JIT_QUEUE);
	proc_create_data("jitschedto", 0, NULL, &jit_fn_fops, (void *)JIT_SCHEDTO);

	proc_create_data("jitimer", 0, NULL, &jit_timer_fops, NULL);
	proc_create_data("jitasklet", 0, NULL, &jit_tasklet_fops, NULL);
	proc_create_data("jitasklethi", 0, NULL, &jit_tasklet_fops, (void *)1);

	return 0;
}

void __exit jit_cleanup(void)
{
	remove_proc_entry("currentime", NULL);
	remove_proc_entry("jitbusy", NULL);
	remove_proc_entry("jitsched", NULL);
	remove_proc_entry("jitqueue", NULL);
	remove_proc_entry("jitschedto", NULL);

	remove_proc_entry("jitimer", NULL);
	remove_proc_entry("jitasklet", NULL);
	remove_proc_entry("jitasklethi", NULL);
}

module_init(jit_init);
module_exit(jit_cleanup);

/*
 * Compatible with newer kernels
 * 5.15 -> 6.2...
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>

#define KERN_SECTOR_SIZE    (512)
#define NR_SECTORS          (2048)
#define MEDIA_SECTOR_SIZE   (512)
#define NR_HW_QUEUES        (1)
#define QUEUE_DEPTH         (64)
#define BLK_DEVICE_NAME     "ramdisk_dev"
#define DISK_NAME           "ramdisk_blkdev"
#define NR_DISK_MINORS      (2)

MODULE_AUTHOR("Arabic Linux Community");
MODULE_DESCRIPTION("ramdisk_blk ldd");
MODULE_LICENSE("GPL");

struct blk_priv_dev {
    int major;
    sector_t capacity;

    /* Tags
    * In order to indicate which request has been completed, every request is
    * identified by an integer, ranging from 0 to the dispatch queue size. This tag
    * is generated by the block layer and later reused by the device driver, removing
    * the need to create a redundant identifier. When a request is completed in the
    * driver, the tag is sent back to the block layer to notify it of the finalization.
    */
    struct blk_mq_tag_set tag_set;
    struct gendisk *gdisk;
    u8 *data;
    struct block_device *blkdev;
};

static struct blk_priv_dev *blk_prv = NULL;
static struct blk_mq_ops mq_ops;

static int ramdisk_blkdev_open(struct block_device *dev, fmode_t mode)
{
    dev_info(&dev->bd_device, "ramdisk_blkdev_open...\n");
    blk_prv->blkdev = dev;

    return 0;
}

static void ramdisk_blkdev_release(struct gendisk *gdisk, fmode_t mode)
{
    pr_info("ramdisk_blkdev_release...\n");
    return;
}

int ramdisk_blkdev_ioctl(struct block_device *bdev, fmode_t mode,
	unsigned cmd, unsigned long arg)
{
	return -ENOTTY;
}

static struct block_device_operations blkdev_ops = {
    .owner = THIS_MODULE,
    .open = ramdisk_blkdev_open,
    .release = ramdisk_blkdev_release,
    .ioctl = ramdisk_blkdev_ioctl
};

static int setup_rmablkdev(struct blk_priv_dev *blk_prv, int major)
{
    int ret;
    int len;
    char diskname[] = DISK_NAME;

    blk_prv->data = kzalloc(NR_SECTORS * MEDIA_SECTOR_SIZE, GFP_KERNEL);
    if (!blk_prv->data) {
        ret = -ENOMEM;
        goto err_alloc;
    }
    
    blk_prv->tag_set.ops = &mq_ops;
    blk_prv->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    blk_prv->tag_set.nr_hw_queues = NR_HW_QUEUES;
    blk_prv->tag_set.queue_depth = QUEUE_DEPTH;
    blk_prv->tag_set.numa_node = NUMA_NO_NODE;
    ret = blk_mq_alloc_tag_set(&blk_prv->tag_set);
    if(ret)
    	goto err_alloc_tag;
	
    blk_prv->gdisk = blk_mq_alloc_disk(&blk_prv->tag_set, blk_prv);
    if(IS_ERR(blk_prv->gdisk)) {
        ret = PTR_ERR(blk_prv->gdisk);
        goto err_mq_alloc;
    }
    blk_prv->gdisk->state = GD_NEED_PART_SCAN;
    blk_prv->gdisk->major = major;
    blk_prv->gdisk->minors = NR_DISK_MINORS;
    blk_prv->gdisk->first_minor = 0;
    blk_prv->gdisk->fops = &blkdev_ops;
    blk_prv->gdisk->private_data = blk_prv;
    len = sprintf(blk_prv->gdisk->disk_name, diskname);
    blk_prv->capacity = NR_SECTORS *(MEDIA_SECTOR_SIZE/KERN_SECTOR_SIZE);
    set_capacity(blk_prv->gdisk, blk_prv->capacity);
    ret = add_disk(blk_prv->gdisk);
    if (ret)
        goto err_add_disk;

    return 0;

err_add_disk:
    del_gendisk(blk_prv->gdisk);
    //put_disk(blk_prv->gdisk);
err_mq_alloc:
	blk_mq_free_tag_set(&blk_prv->tag_set);
err_alloc_tag:
	kfree(blk_prv->data);
err_alloc:
    unregister_blkdev(major, BLK_DEVICE_NAME);
    kfree(blk_prv);
    return ret;

}

static int request_handle(struct request *rq)
{
    struct bio_vec bvec;
    struct req_iterator iter;
    struct blk_priv_dev *dev = rq->q->queuedata;
    loff_t pos = blk_rq_pos(rq) << SECTOR_SHIFT;
    loff_t dev_size = NR_SECTORS * MEDIA_SECTOR_SIZE;
    bool oper;

    rq_for_each_segment(bvec, rq, iter) {
        unsigned long b_len = bvec.bv_len;
        void* b_buf = page_address(bvec.bv_page) + bvec.bv_offset;

        if ((pos + b_len) > dev_size)
            b_len = (unsigned long)(dev_size - pos);
        oper = rq_data_dir(rq);
        if (oper == WRITE)
        	memcpy(dev->data + pos, b_buf, b_len);
        else
	        memcpy(b_buf, dev->data + pos, b_len);

        pos += b_len;
        pr_info("ramdisk_dev: request_handle %s nr of bytes %lu\n",
            oper ? "write" : "read", b_len);
    }
    return 0;
}

static blk_status_t ramdisk_blkdev_queue_rq(struct blk_mq_hw_ctx *hctx,
	const struct blk_mq_queue_data* mq)
{
    blk_status_t status = BLK_STS_OK;
    struct request *rq = mq->rq;

    blk_mq_start_request(rq);
    if (request_handle(rq))
        status = BLK_STS_IOERR;
    blk_mq_end_request(rq, status);

    return status;
}

static struct blk_mq_ops mq_ops = {
    .queue_rq = ramdisk_blkdev_queue_rq
};

static int __init ramdisk_blkdev_init(void)
{
    pr_info("ramdisk_blkdev_init...");
    blk_prv = kzalloc(sizeof(struct blk_priv_dev), GFP_KERNEL);
    blk_prv->major = register_blkdev(0, BLK_DEVICE_NAME);
    if(!blk_prv) {
        unregister_blkdev(blk_prv->major, BLK_DEVICE_NAME);
        return -ENOMEM;
    }
    
    return setup_rmablkdev(blk_prv, blk_prv->major);
}

static void __exit ramdisk_blkdev_exit(void)
{
    if(blk_prv) {
        if(blk_prv->gdisk)
            del_gendisk(blk_prv->gdisk);
    	blk_mq_free_tag_set(&blk_prv->tag_set);
        //put_disk(blk_prv->gdisk);
        if(bdev_is_partition(blk_prv->blkdev))
    	    blkdev_put(blk_prv->blkdev, 0);
    	kfree(blk_prv->data);
    	unregister_blkdev(blk_prv->major, BLK_DEVICE_NAME);
    	kfree(blk_prv);
    }
}

module_init(ramdisk_blkdev_init);
module_exit(ramdisk_blkdev_exit);

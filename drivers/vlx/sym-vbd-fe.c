/*
 ****************************************************************
 *
 * Copyright (C) 2002-2010 VirtualLogix
 *
 ****************************************************************
 */


#include <nk/nkern.h>

#define	RDISK_RING_TYPE		0x44495348	/* DISK */
#define	VBD_BE_RING_TYPE	0x56424442	/* VBDB */
#define	VBD_FE_RING_TYPE	0x56424446	/* VBDF */

typedef nku16_f RDiskDevId;
typedef nku32_f RDiskSector;
typedef nku64_f RDiskCookie;
typedef nku8_f  RDiskOp;
typedef nku8_f  RDiskStatus;
typedef nku8_f  RDiskCount;

#define RDISK_OP_INVALID   0
#define RDISK_OP_PROBE     1
#define RDISK_OP_READ      2
#define RDISK_OP_WRITE     3

    //
    // Remote Disk request header.
    // Buffers follow just behind this header.
    //    
typedef struct RDiskReqHeader {
    RDiskSector sector[2];	/* sector [0] - low; [1] - high */
    RDiskCookie cookie;		/* cookie to put in the response descriptor */
    RDiskDevId  devid;		/* device ID */
    RDiskOp     op;		/* operation */
    RDiskCount  count;		/* number of buffers which follows */
} __attribute__ ((packed)) RDiskReqHeader ;

//
// The buffer address is in fact a page physical address with the first and
// last sector numbers incorporated as less significant bits ([5-9] and [0-4]).
// Note that the buffer address type is the physcial address type and it
// is architecture specific.
//
typedef NkPhAddr RDiskBuffer;
#define	RDISK_FIRST_BUF(req) \
	((RDiskBuffer*)(((nku8_f*)(req)) + sizeof(RDiskReqHeader)))

#define	RDISK_SECT_SIZE_BITS	9	/* sector is 512 bytes   */
#define	RDISK_SECT_SIZE		(1 << RDISK_SECT_SIZE_BITS)

#define	RDISK_SECT_NUM_BITS	5	/* sector number is 0..31 */
#define	RDISK_SECT_NUM_MASK	((1 << RDISK_SECT_NUM_BITS) - 1)

#define	RDISK_BUFFER(paddr, start, end) \
	((paddr) | ((start) << RDISK_SECT_NUM_BITS) | (end))

#define	RDISK_BUF_SSECT(buff) \
	(((buff) >> RDISK_SECT_NUM_BITS) & RDISK_SECT_NUM_MASK)
#define	RDISK_BUF_ESECT(buff) \
	((buff) & RDISK_SECT_NUM_MASK)
#define	RDISK_BUF_SECTS(buff) \
	(RDISK_BUF_ESECT(buff) - RDISK_BUF_SSECT(buff) + 1)

#define	RDISK_BUF_SOFF(buff) (RDISK_BUF_SSECT(buff) << RDISK_SECT_SIZE_BITS)
#define	RDISK_BUF_EOFF(buff) (RDISK_BUF_ESECT(buff) << RDISK_SECT_SIZE_BITS)
#define	RDISK_BUF_SIZE(buff) (RDISK_BUF_SECTS(buff) << RDISK_SECT_SIZE_BITS)

#define	RDISK_BUF_PAGE(buff) ((buff) & ~((1 << (RDISK_SECT_NUM_BITS*2)) - 1))
#define	RDISK_BUF_BASE(buff) (RDISK_BUF_PAGE(buff) + RDISK_BUF_SOFF(buff))

    //
    // Response descriptor
    //
typedef struct RDiskResp {
    RDiskCookie cookie;		// cookie copied from request
    RDiskOp     op;		// operation code copied from request
    RDiskStatus status;		// operation status
} __attribute__ ((packed)) RDiskResp;

#define RDISK_STATUS_OK	     0
#define RDISK_STATUS_ERROR   0xff

    //
    // Probing record
    //
typedef nku16_f RDiskInfo;

typedef struct RDiskProbe {
    RDiskDevId  devid;    // device ID
    RDiskInfo   info;     // device type & flags
    RDiskSector size[2];  // size in sectors ([0] - low; [1] - high)
} __attribute__ ((packed)) RDiskProbe;


    //
    // Types below match ide_xxx in Linux ide.h
    //
#define RDISK_TYPE_FLOPPY  0x00
#define RDISK_TYPE_TAPE    0x01
#define RDISK_TYPE_CDROM   0x05
#define RDISK_TYPE_OPTICAL 0x07
#define RDISK_TYPE_DISK    0x20 

#define RDISK_TYPE_MASK    0x3f
#define RDISK_TYPE(x)      ((x) & RDISK_TYPE_MASK) 

#define RDISK_FLAG_RO      0x40
#define RDISK_FLAG_VIRT    0x80

#define KERN_WARNING "Warning "
#define KERN_ALERT   "Alert"

#define P(...) Kern::Printf(__VA_ARGS__)
#define WTRACE(_f, _a...) P( KERN_WARNING "VBD-FE: " _f, ## _a )
#define ETRACE(_f, _a...) P( KERN_ALERT   "VBD-FE: " _f, ## _a )

#define TRACE(_f, _a...)  P( KERN_ALERT   "VBD-FE: " _f, ## _a )
#define DTRACE(_f, _a...) \
	do { P(KERN_ALERT "%s: " _f, __func__, ## _a);} while (0)
#define XTRACE(_f, _a...) DTRACE (_f,  ## _a)
#define VBD_CATCHIF(cond,action)	if (cond) action;
#define VBD_ASSERT(c)	do {if (!(c)) BUG();} while (0)

typedef struct {
    int   partn_shift;
    int   devs_per_major;
    char* name;
} vbd_type_info_t;

   /*
    * We have one of these per vbd, whether ide, scsi or 'other'.  They
    * hang in private_data off the gendisk structure. We may end up
    * putting all kinds of interesting stuff here :-)
    */
typedef struct {
    int               major;
    int               usage;
    vbd_type_info_t*  type;
    int               index;
} vbd_major_info_t;

struct vbd_t;

typedef struct vbd_disk_info {
    int                     xd_device;
    vbd_major_info_t*       mi;
    struct gendisk*         gd;
   int		            device;
    struct vbd_disk_info*   next;
} vbd_disk_info_t;

    /*
     * For convenience we distinguish between ide, scsi and 'other' (i.e.
     * potentially combinations of the two) in the naming scheme and in a few 
     * other places (like default readahead, etc).
     */

#define VBD_NUM_IDE_MAJORS 	10
#define VBD_NUM_SCSI_MAJORS	9
#define VBD_NUM_VBD_MAJORS	1
#define VBD_NUM_FD_MAJORS	1
#ifdef MMC_BLOCK_MAJOR
#define VBD_NUM_MMC_MAJORS	1
#else
#define VBD_NUM_MMC_MAJORS	0
#endif

static vbd_type_info_t vbd_ide_type = {
    .partn_shift = 6,
	 // XXXcl todo blksize_size[major]  = 1024;
    .devs_per_major = 2,
	// XXXcl todo read_ahead[major] = 8; /* from drivers/ide/ide-probe.c */
    .name = "hd"
};

static vbd_type_info_t vbd_scsi_type = {
    .partn_shift = 4,
	// XXXcl todo blksize_size[major]  = 1024; /* XXX 512; */
    .devs_per_major = 16,
	// XXXcl todo read_ahead[major]    = 0; /* XXX 8; -- guessing */
    .name = "sd"
};

static vbd_type_info_t vbd_vbd_type = {
    .partn_shift = 4,
	// XXXcl todo blksize_size[major]  = 512;
	// XXXcl todo read_ahead[major]    = 8;
    .name = "xvd"
};

static vbd_type_info_t vbd_fd_type = {
    .partn_shift = 0,
    .devs_per_major = 1,
    .name = "fd"
};

#ifdef MMC_BLOCK_MAJOR
static vbd_type_info_t vbd_mmc_type = {
    .partn_shift = 3,
    .devs_per_major = 256 >> 3,
    .name = "mmc"
};
#endif

    /* XXXcl handle cciss after finding out why it's "hacked" in */

static vbd_major_info_t* vbd_major_info[VBD_NUM_IDE_MAJORS +
					VBD_NUM_SCSI_MAJORS +
					VBD_NUM_VBD_MAJORS +
					VBD_NUM_FD_MAJORS +
					VBD_NUM_MMC_MAJORS];


#define VBD_MAX_VBDS 64	    /* Information about our VBDs. */

#define VBD_MAJOR_XEN(dev)	((dev)>>8)
#define VBD_MINOR_XEN(dev)	((dev) & 0xff)

#define VBD_BLKIF_STATE_CLOSED       0
#define VBD_BLKIF_STATE_DISCONNECTED 1
#define VBD_BLKIF_STATE_CONNECTED    2

typedef struct vbd_t {
    u32         	  dsize; 	// ring descriptor size
    u32         	  imask; 	// ring index mask
    u32        		  rsize; 	// ring size
    int         	  nvbds; 	// number of VBDS
    RDiskProbe*           info;  	// VBDS info
    u32                   state; 	// state
    vbd_disk_info_t*      vbds;	 	//
    NkDevDesc*            vdev;  	//
    volatile NkDevRing*   ring;  	// ring descriptor
    u8*   		  rbase;  	// ring base
    u32                   iresp; 	// Response consumer for comms ring
    u32                   ireq; 	// Private request producer
    NkXIrq                xirq;	 	// producer cross IRQf
    NkXIrqId              xid;	 	// cross IRQ ID
    spinlock_t            lock;		// i/o lock
    int                   rsp_valid;	// probe response valid
    RDiskResp 		  rsp;		// probe response
    struct vbd_t*         next;		// next VBD
} vbd_t;

static struct task_struct* vbd_th_desc;		// thread descriptor
static struct semaphore	   vbd_th_sem;		// thread semaphore
static volatile int	   vbd_th_abort;	// thread abort
static NkXIrqId            vbd_xid;		// SYSCONF XIRQ ID
static vbd_t*              vbds;		// VBDs
static volatile int 	   vbd_wait_flag;	// wait flag
static RDiskDevId	   vbd_wait_id;		// wait disk ID

#define	VBD_BLKIF_DESC_OFF(vbd, x) (((x) & (vbd)->imask) * (vbd)->dsize)

#define	VBD_BLKIF_RING_REQ(vbd, x) \
	((RDiskReqHeader*)((vbd)->rbase + VBD_BLKIF_DESC_OFF(vbd, x)))

#define	VBD_BLKIF_RING_RESP(vbd, x) \
	((RDiskResp*)(((vbd)->rbase + VBD_BLKIF_DESC_OFF(vbd, x))))

#define	VBD_BLKIF_MAX_SEGMENTS_PER_REQUEST(vbd)	\
	(((vbd)->dsize - sizeof(RDiskReqHeader)) / sizeof(RDiskBuffer))

/* We plug the I/O ring if the driver is suspended or if the ring is full. */

#define VBD_BLKIF_RING_FULL(vbd) \
	 ((((vbd)->ireq - (vbd)->iresp) == (vbd)->rsize) || \
	 ((vbd)->state != VBD_BLKIF_STATE_CONNECTED))

static int  vbd_blkif_open    (struct block_device *bdev, fmode_t fmode);
static int  vbd_blkif_release (struct gendisk *gd, fmode_t fmode);
static int  vbd_blkif_ioctl   (struct block_device *bdev, fmode_t fmode,
			       unsigned command, unsigned long argument);

static int  vbd_blkif_getgeo  (struct block_device *bdev,
			       struct hd_geometry *geo);

static void vbd_do_blkif_request (struct request_queue *rq);

static struct block_device_operations vbd_block_fops = {
    .owner   = THIS_MODULE,
    .open    = vbd_blkif_open,
    .release = vbd_blkif_release,
    .ioctl   = vbd_blkif_ioctl,
    .getgeo  = vbd_blkif_getgeo
};

   static void*
kzalloc (size_t size, unsigned flags)
{
    void* ptr = kmalloc (size, flags);
    if (ptr) {
	memset (ptr, 0, size);
    }
    return ptr;
}

    static vbd_t*
vbd_alloc (void)
{
    return kzalloc(sizeof(vbd_t), GFP_KERNEL);
}

    static void
vbd_free (vbd_t* vbd)
{
    kfree(vbd);
}

    static vbd_t*
vbd_find (NkDevDesc* vdev)
{
    vbd_t* vbd = vbds;
    while (vbd) {
	if (vbd->vdev == vdev) {
	    return vbd;
	}
	vbd = vbd->next;
    }
    return 0;
}

    static inline void
vbd_flush_requests (vbd_t* vbd)
{
    XTRACE("irq (%d, %d)\n", vbd->ring->cxirq, vbd->vdev->dev_owner);

    wmb(); /* Ensure that the frontend can see the requests. */
    vbd->ring->ireq = vbd->ireq;
    nkops.nk_xirq_trigger(vbd->ring->cxirq, vbd->vdev->dev_owner);
}

    static void
vbd_blkif_probe_send (vbd_t* vbd, NkPhAddr paddr)
{
    unsigned long   flags;
    RDiskReqHeader* rreq;

    spin_lock_irqsave(&vbd->lock, flags);

    while ((vbd->ireq - vbd->iresp) == vbd->imask) {
       spin_unlock_irqrestore(&vbd->lock, flags);
       set_current_state(TASK_INTERRUPTIBLE);
       schedule_timeout(1);
       spin_lock_irqsave(&vbd->lock, flags);
    }

    rreq = VBD_BLKIF_RING_REQ(vbd, vbd->ireq);
    rreq->op        = RDISK_OP_PROBE;
    rreq->sector[0] = 0;
    rreq->sector[1] = 0;
    rreq->devid     = 0;
    rreq->cookie    = 0;
    rreq->count     = 1;
    RDISK_FIRST_BUF(rreq)[0] = RDISK_BUFFER(paddr, 0, 7);

    vbd->ireq++;
    vbd_flush_requests(vbd);

    spin_unlock_irqrestore(&vbd->lock, flags);
}

    static void
vbd_blkif_probe_wait (vbd_t* vbd)
{
    while (!vbd->rsp_valid) {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1);
    }
}

    static int
vbd_get_vbd_info (vbd_t* vbd, RDiskProbe* disk_info)
{
    RDiskProbe* buf = (RDiskProbe*)__get_free_page(GFP_KERNEL);
    RDiskStatus status;
    int         nr;

    if (vbd->rsp_valid) {
	BUG();
    }
    vbd_blkif_probe_send(vbd, virt_to_phys(buf));
    vbd_blkif_probe_wait(vbd);
    status = vbd->rsp.status;
    vbd->rsp_valid = 0;
    if (status == RDISK_STATUS_ERROR) {
        ETRACE("Could not probe disks (%d)\n", status);
        return -1;
    }
    if ((nr = status) > VBD_MAX_VBDS) {
	 nr = VBD_MAX_VBDS;
    }
    TRACE("%d virtual disk(s) detected\n", nr);
    memcpy(disk_info, buf, nr * sizeof(RDiskProbe));
    free_page((unsigned long)buf);
    return nr;
}

    static vbd_major_info_t*
vbd_get_major_info (int xd_device, int *minor)
{
	int mi_idx, new_major;
	int xd_major = VBD_MAJOR_XEN(xd_device);
	int xd_minor = VBD_MINOR_XEN(xd_device);

	*minor = xd_minor;

	switch (xd_major) {
	case IDE0_MAJOR: mi_idx = 0; new_major = IDE0_MAJOR; break;
	case IDE1_MAJOR: mi_idx = 1; new_major = IDE1_MAJOR; break;
	case IDE2_MAJOR: mi_idx = 2; new_major = IDE2_MAJOR; break;
	case IDE3_MAJOR: mi_idx = 3; new_major = IDE3_MAJOR; break;
	case IDE4_MAJOR: mi_idx = 4; new_major = IDE4_MAJOR; break;
	case IDE5_MAJOR: mi_idx = 5; new_major = IDE5_MAJOR; break;
	case IDE6_MAJOR: mi_idx = 6; new_major = IDE6_MAJOR; break;
	case IDE7_MAJOR: mi_idx = 7; new_major = IDE7_MAJOR; break;
	case IDE8_MAJOR: mi_idx = 8; new_major = IDE8_MAJOR; break;
	case IDE9_MAJOR: mi_idx = 9; new_major = IDE9_MAJOR; break;
	case SCSI_DISK0_MAJOR: mi_idx = 10; new_major = SCSI_DISK0_MAJOR;break;
	case SCSI_DISK1_MAJOR ... SCSI_DISK7_MAJOR:
		mi_idx = 11 + xd_major - SCSI_DISK1_MAJOR;
		new_major = SCSI_DISK1_MAJOR + xd_major - SCSI_DISK1_MAJOR;
		break;
	case SCSI_CDROM_MAJOR: mi_idx = 18; new_major = SCSI_CDROM_MAJOR;
		break;
	case FLOPPY_MAJOR: mi_idx = 19; new_major = FLOPPY_MAJOR; break;
	case MMC_BLOCK_MAJOR: mi_idx = 20; new_major = MMC_BLOCK_MAJOR; break;
	default: mi_idx = 21; new_major = 0;/* XXXcl notyet */ break;
	}

	if (vbd_major_info[mi_idx])
		return vbd_major_info[mi_idx];

	vbd_major_info[mi_idx] = kzalloc(sizeof(vbd_major_info_t), GFP_KERNEL);
	if (vbd_major_info[mi_idx] == NULL)
		return NULL;

	switch (mi_idx) {
	case 0 ... (VBD_NUM_IDE_MAJORS - 1):
		vbd_major_info[mi_idx]->type = &vbd_ide_type;
		vbd_major_info[mi_idx]->index = mi_idx;
		break;
	case VBD_NUM_IDE_MAJORS ...
	    (VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS - 1):
		vbd_major_info[mi_idx]->type = &vbd_scsi_type;
		vbd_major_info[mi_idx]->index = mi_idx - VBD_NUM_IDE_MAJORS;
		break;
	case (VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS) ...
		(VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS +
		VBD_NUM_FD_MAJORS - 1):
		vbd_major_info[mi_idx]->type = &vbd_fd_type;
		vbd_major_info[mi_idx]->index = mi_idx -
			(VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS);
		break;
	case VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS + VBD_NUM_FD_MAJORS ...
		(VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS + VBD_NUM_FD_MAJORS +
		VBD_NUM_MMC_MAJORS - 1):
		vbd_major_info[mi_idx]->type = &vbd_mmc_type;
		vbd_major_info[mi_idx]->index = mi_idx -
			(VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS +
			 VBD_NUM_FD_MAJORS);
		break;
	case (VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS + VBD_NUM_FD_MAJORS +
		VBD_NUM_MMC_MAJORS) ...
		(VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS + VBD_NUM_FD_MAJORS +
		 VBD_NUM_MMC_MAJORS + VBD_NUM_VBD_MAJORS - 1):
		vbd_major_info[mi_idx]->type = &vbd_vbd_type;
		vbd_major_info[mi_idx]->index = mi_idx -
			(VBD_NUM_IDE_MAJORS + VBD_NUM_SCSI_MAJORS +
			 VBD_NUM_FD_MAJORS + VBD_NUM_MMC_MAJORS);
		break;
	}
	vbd_major_info[mi_idx]->major = new_major;

	if (register_blkdev(vbd_major_info[mi_idx]->major,
			    vbd_major_info[mi_idx]->type->name)) {
		ETRACE("can't get major %d with name %s\n",
		       vbd_major_info[mi_idx]->major,
		       vbd_major_info[mi_idx]->type->name);
		goto out;
	}

	return vbd_major_info[mi_idx];

 out:
	kfree(vbd_major_info[mi_idx]);
	vbd_major_info[mi_idx] = NULL;
	return NULL;
}

    static struct gendisk*
vbd_get_gendisk (vbd_t* vbd, vbd_major_info_t* mi,
		 int xd_minor, RDiskProbe* xd)
{
	struct gendisk*        gd;
	vbd_disk_info_t*       di;
	int                    part;

	di = kzalloc (sizeof (vbd_disk_info_t), GFP_KERNEL);
	if (di == NULL) {
		return NULL;
	}
	    /*
	     * Construct an appropriate gendisk structure.
	     */
	part = xd_minor & ((1 << mi->type->partn_shift) - 1);
	if (part) {
	        /*
		 * VBD partitions are not expected to contain sub-partitions.
		 */
	    DTRACE("xd_minor=%d -> alloc_disk(%d)\n", xd_minor, 1);
	    gd = alloc_disk(1);
	} else {
	        /*
		 * VBD disks can contain sub-partitions.
		 */
	    DTRACE("xd_minor=%d -> alloc_disk(%d)\n",
		   xd_minor, 1 << mi->type->partn_shift);
	    gd = alloc_disk(1 << mi->type->partn_shift);
	}
	if (gd == NULL)
		goto out;

	gd->major = mi->major;
	gd->first_minor = xd_minor;
	gd->fops = &vbd_block_fops;
	gd->private_data = di;
	if (part) {
	    if (mi->major == MMC_BLOCK_MAJOR)
	        sprintf(gd->disk_name, "mmcblk%dp%d",
			xd_minor >> mi->type->partn_shift, part);
	    else
	    sprintf(gd->disk_name, "%s%c%d",
		    mi->type->name,
		    'a' + (mi->index << 1) +
		    (xd_minor >> mi->type->partn_shift),
		    part);
	} else {
	    /* Floppy disk special naming rules */
	    if (!strcmp(mi->type->name, "fd"))
	        sprintf(gd->disk_name, "%s%c", mi->type->name, '0');
	    else if (mi->major == MMC_BLOCK_MAJOR)
	        sprintf(gd->disk_name, "mmcblk%d",
			xd_minor >> mi->type->partn_shift);
	    else
	        sprintf(gd->disk_name, "%s%c",
			mi->type->name,
			'a' + (mi->index << 1) +
			(xd_minor >> mi->type->partn_shift));
	}

	if (xd->size[0]) {
	    set_capacity(gd, xd->size[0]);	// VG_FIXME: size[1] ???
	} else {
	    set_capacity(gd, 0xffffffffL);
  	}

	DTRACE("%s: gd=0x%p, di=0x%p, major=%d first minor=%d\n",
	       gd->disk_name, gd, di, gd->major, gd->first_minor);

	gd->queue = blk_init_queue(vbd_do_blkif_request, &vbd->lock);
	if (gd->queue == NULL)
		goto out;

	gd->queue->queuedata = vbd;

	elevator_init(gd->queue, "noop");
		/*
		 * Turn off barking 'headactive' mode. We dequeue
		 * buffer heads as soon as we pass them to back-end
		 * driver.
		 */
	blk_queue_logical_block_size(gd->queue, 512);

	    /* limit max hw read size to 128 (255 loopback limitation) */
	blk_queue_max_sectors(gd->queue, 128);

	blk_queue_segment_boundary(gd->queue, PAGE_SIZE - 1);
	blk_queue_max_segment_size(gd->queue, PAGE_SIZE);

	blk_queue_max_phys_segments(gd->queue,
		    VBD_BLKIF_MAX_SEGMENTS_PER_REQUEST(vbd));
	blk_queue_max_hw_segments(gd->queue,
		    VBD_BLKIF_MAX_SEGMENTS_PER_REQUEST(vbd));

	blk_queue_dma_alignment(gd->queue, 511);

	di->mi        = mi;
	di->xd_device = xd->devid;
	di->gd        = gd;
	di->device    = MKDEV(mi->major, xd_minor);
	di->next      = vbd->vbds;
	vbd->vbds     = di;

	add_disk(gd);

	return gd;

 out:
	if (gd)
		del_gendisk(gd);
	kfree(di);
	return NULL;
}

/*
 * vbd_init_device - initialize a VBD device
 * @disk:              a RDiskProbe describing the VBD
 *
 * Takes a RDiskProbe * that describes a VBD the domain has access to.
 * Performs appropriate initialization and registration of the device.
 *
 * Care needs to be taken when making re-entrant calls to ensure that
 * corruption does not occur.  Also, devices that are in use should not have
 * their details updated.  This is the caller's responsibility.
 */
    static int
vbd_init_device (vbd_t* vbd, RDiskProbe* xd)
{
	struct block_device *bd;
	struct gendisk *gd;
	vbd_major_info_t *mi;
	int device;
	int minor;

	int err = -ENOMEM;

	XTRACE("\n");

	mi = vbd_get_major_info(xd->devid, &minor);
	if (mi == NULL)
		return -EPERM;

	device = MKDEV(mi->major, minor);

	if ((bd = bdget(device)) == NULL)
		return -EPERM;

	/*
	 * Update of partition info, and check of usage count, is protected
	 * by the per-block-device semaphore.
	 */
	gd = vbd_get_gendisk(vbd, mi, minor, xd);
	if (!gd || !mi) {
		err = -EPERM;
		goto out;
	}

	mi->usage++;

	if (xd->info & RDISK_FLAG_RO) {
		set_disk_ro(gd, 1); 
	}

	/* Some final fix-ups depending on the device type */
	switch (RDISK_TYPE(xd->info)) { 
	case RDISK_TYPE_CDROM:
		gd->flags |= GENHD_FL_REMOVABLE | GENHD_FL_CD; 
		/* FALLTHROUGH */
	case RDISK_TYPE_FLOPPY: 
	case RDISK_TYPE_TAPE:
		gd->flags |= GENHD_FL_REMOVABLE; 
		break; 

	case RDISK_TYPE_DISK:
		break; 

	default:
		ETRACE("unknown device type %d\n", RDISK_TYPE(xd->info)); 
		break; 
	}
	if (vbd_wait_flag && (vbd_wait_id == xd->devid)) {
	    vbd_wait_flag = 0;
 	}

	err = 0;
 out:
	bdput(bd);    
	return err;
}

    static void
vbd_destroy_device (vbd_disk_info_t* di)
{
	struct block_device* bd;
	vbd_major_info_t* mi = di->mi;

	XTRACE("\n");

	bd = bdget(di->device);
	if (bd) {
	    if (di->gd) {
	        if (di->gd->queue) {
	           blk_cleanup_queue(di->gd->queue);
	        }
	        del_gendisk(di->gd);
	        put_disk(di->gd);
   	    }
	    bdput(bd);
	}

	if (!(--(mi->usage))) {
	    unregister_blkdev(mi->major, mi->type->name);
	    kfree(mi);
  	}

	kfree(di);
}

/*
 * Set up all the linux device goop for the virtual block devices
 * (vbd's) that we know about. Note that although from the backend
 * driver's p.o.v. VBDs are addressed simply an opaque 16-bit device
 * number, the domain creation tools conventionally allocate these
 * numbers to correspond to those used by 'real' linux -- this is just
 * for convenience as it means e.g. that the same /etc/fstab can be
 * used when booting with or without Xen.
 */
    static int
vbd_init (vbd_t* vbd)
{
	XTRACE("\n");

	memset(vbd_major_info, 0, sizeof(vbd_major_info));

	vbd->info = kmalloc(VBD_MAX_VBDS * sizeof(RDiskProbe), GFP_KERNEL);
	if (!vbd->info) {
	    ETRACE("kmalloc(%lld) failure\n",
		   (u64)(VBD_MAX_VBDS * sizeof(RDiskProbe)));
	    return 0;
	}
	vbd->nvbds = vbd_get_vbd_info (vbd, vbd->info);

	if (vbd->nvbds < 0) {
                DTRACE("get_vbd_info failed\n");
		kfree(vbd->info);
		vbd->info  = NULL;
		vbd->nvbds = 0;
	} else {
	    int i;

	    for (i = 0; i < vbd->nvbds; i++) {
		int err = vbd_init_device(vbd, vbd->info + i);
		if (err) {
		    ETRACE("device (%d,%d) creation failure %d\n",
			   VBD_MAJOR_XEN(vbd->info[i].devid),
			   VBD_MINOR_XEN(vbd->info[i].devid), err);
		} else {
		    DTRACE("device (%d,%d) created\n",
			   VBD_MAJOR_XEN(vbd->info[i].devid),
			   VBD_MINOR_XEN(vbd->info[i].devid));
		}
	    }
	}

	return 0;
}

    static void
vbd_down (vbd_t* vbd)
{
    XTRACE("\n");

    while (vbd->vbds) {
	vbd_disk_info_t* di = vbd->vbds;
	vbd->vbds = di->next;
	vbd_destroy_device(di);
    }
    kfree(vbd->info);
}

    static void
vbd_update (void)
{
    XTRACE("\n");
}

    /* "kick" means "resume" here */
    /* Only called from the function below */

    static inline void
vbd_kick_pending_request_queue (struct request_queue* rq)
{
    if (rq && test_bit(QUEUE_FLAG_STOPPED, &rq->queue_flags)) {
        blk_start_queue(rq);
	        /* XXXcl call to request_fn should not be needed but
	         * we get stuck without...  needs investigating
	         */
        rq->request_fn(rq);
    }
}

    /* called from interrupt handler only, under vbd->lock */

    static void
vbd_kick_pending_request_queues (vbd_t* vbd)
{
    vbd_disk_info_t* di = vbd->vbds;
    while (di) {
	if (di->gd) {
	    vbd_kick_pending_request_queue(di->gd->queue);
	}
	di = di->next;
    }
}

    static int
vbd_blkif_open (struct block_device *bdev, fmode_t fmode)
{
    struct gendisk *gd = bdev->bd_disk;
    vbd_disk_info_t *di = (vbd_disk_info_t*) gd->private_data;

    DTRACE("gd=0x%p, di=0x%p", gd, di);

    /* Update of usage count is protected by per-device semaphore. */
    di->mi->usage++;
    
    return 0;
}

    static int
vbd_blkif_release (struct gendisk *gd, fmode_t fmode)
{
    vbd_disk_info_t* di = (vbd_disk_info_t*) gd->private_data;
       /*
        * When usage drops to zero it may allow more VBD updates to occur.
        * Update of usage count is protected by a per-device semaphore.
        */
    if (--di->mi->usage == 0) {
        vbd_update();
    }
    return 0;
}

    /*
     *  Implement the HDIO_GETGEO ioctl: the HDIO_GETGEO
     *  value is not passed to the common vbd_blkif_ioctl() above
     *  on 2.6.15 and maybe other versions.
     *  We always return the full disk geometry, even if a
     *  partition has been opened. Only "start" changes.
     *  This is how the ioctl behaves on both 2.4 and 2.6
     *  Linuxes. This makes me wonder if the implementation
     *  on 2.4 in vbd_blkif_ioctl() is correct.
     */

    static int
vbd_blkif_getgeo (struct block_device* bdev, struct hd_geometry* geo)
{
    struct gendisk* gd = bdev->bd_disk;
    sector_t sectors = get_capacity (gd);

	/* geo->start was set by caller blkdev_ioctl() in block/ioctl.c */
    if (gd->major == MMC_BLOCK_MAJOR) {
	geo->heads     = 4;
	geo->sectors   = 16;	/* per track */
    } else
    {
	geo->heads     = 0xff;
	geo->sectors   = 0x3f;	/* per track */
    }
	/*
	 *  get_capacity() returns an unsigned 64-bit sector_t.
	 *  Performing a division by heads*sectors generates an
	 *  unresolved call to __udivdi3(). Given that capacity
	 *  was earlier set as a 32 bit integer, we could safely
	 *  typecast it to 32 bits here. But we use sector_div()
	 *  for universality. It returns the remainder and modifies
	 *  its first parameter to be the result.
	 */
    sector_div(sectors, geo->heads * geo->sectors);
    geo->cylinders = sectors;
    return 0;
}

    /*
     * vbd_blkif_queue_request
     *
     * request block io 
     * Called from vbd_do_blkif_request() only.
     * 
     * id: for guest use only.
     * operation: RDISK_OP_{READ,WRITE,PROBE}
     * buffer: buffer to read/write into. this should be a
     *   virtual address in the guest os.
     */
    static int
vbd_blkif_queue_request (vbd_t* vbd, struct request *req)
{
	/* blk_rq_pos() is an inline function returning req->__sector */
    const sector_t	req_sector = blk_rq_pos(req);
    struct gendisk*	gd = req->rq_disk;
    vbd_disk_info_t*	di = (vbd_disk_info_t*) gd->private_data;
    struct req_iterator iter;
    struct bio_vec*     bvec;
    struct page*        page;  
    unsigned long       paddr;
    unsigned int        fsect, lsect, pfsect, plsect;
    RDiskReqHeader*     rreq;
    u32		        count;

    if (unlikely(vbd->state != VBD_BLKIF_STATE_CONNECTED)) {
        return 1;
    }

    /* Fill out a communications ring structure. */
    rreq = VBD_BLKIF_RING_REQ(vbd, vbd->ireq);

    rreq->cookie    = (unsigned long)req;
    rreq->op        = (rq_data_dir(req) ? RDISK_OP_WRITE
					: RDISK_OP_READ);
    XTRACE("%c %0llx\n", (rq_data_dir(req) ? 'w' : 'r'),
	   (unsigned long long) req_sector);

    rreq->sector[0] = (RDiskSector)req_sector;
    rreq->sector[1] = (RDiskSector)(((u64)req_sector) >> 32);
    rreq->devid     = di->xd_device;

    page   = 0;
    count  = 0;
    paddr  = 0;
    plsect = 0;
    pfsect = 0;
    rq_for_each_segment(bvec, req, iter) {
            fsect = bvec->bv_offset >> 9;
            lsect = fsect + (bvec->bv_len >> 9) - 1;
            if (unlikely(lsect > 7)) {
                BUG();
	    }
	    if ((page == bvec->bv_page) && (fsect == (plsect+1))) {
		plsect = lsect;
                RDISK_FIRST_BUF(rreq)[count-1] =
				RDISK_BUFFER(paddr, pfsect, plsect);

	    } else {
		if (unlikely(count==VBD_BLKIF_MAX_SEGMENTS_PER_REQUEST(vbd))) {
		    BUG();
	        }
		page   = bvec->bv_page;
                paddr  = page_to_phys(page);
		pfsect = fsect;
		plsect = lsect;
                RDISK_FIRST_BUF(rreq)[count++] =
				RDISK_BUFFER(paddr, pfsect, plsect);
	    }
    }

    rreq->count = (RDiskCount)count;

    vbd->ireq++;

    return 0;
}

/*
 * Linux 2.6 code
 * vbd_do_blkif_request
 *  read a block; request is in a request queue
 * On Linux 2.6 it is called from the kernel only.
 */
    static void
vbd_do_blkif_request (struct request_queue *rq)
{
    struct request* req;
    int             queued;
    vbd_t*          vbd = rq->queuedata;

    DTRACE("entered, vbd=0x%p\n", vbd);

    queued = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
    while ((req = blk_peek_request(rq)) != NULL)
#else
    while ((req = elv_next_request(rq)) != NULL)
#endif
    {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
        if (!blk_fs_request(req)) {
            end_request(req, 0);
            continue;
        }
#endif

	if (VBD_BLKIF_RING_FULL(vbd)) {
            blk_stop_queue(rq);
            break;
        }

	blk_start_request(req);

	if (!blk_fs_request(req)) {
	    __blk_end_request_all(req, -EIO);
	    continue;
	}
	DTRACE("%p: cmd %p, sec %llx, (%u/%u) buffer:%p [%s]\n",
	       req, req->cmd, (u64)blk_rq_pos(req),
	       blk_rq_cur_sectors(req), blk_rq_sectors(req),
	       req->buffer,
	       rq_data_dir(req) ? "write" : "read");
	if (vbd_blkif_queue_request(vbd, req)) {
	    ETRACE("Badness in %s at %s:%d <<<\n",
		   __FUNCTION__, __FILE__, __LINE__);

            blk_stop_queue(rq);
            break;
        }
        queued++;
    }

    if (queued != 0)
	vbd_flush_requests(vbd);
}

    static void
vbd_blkif_int (void* cookie, NkXIrq xirq)
{
    u32 i, rp;
    unsigned long  flags; 
    vbd_t*         vbd = cookie;

    (void) xirq;
    XTRACE("\n");
    spin_lock_irqsave(&vbd->lock, flags);
    if (unlikely(vbd->state == VBD_BLKIF_STATE_CLOSED)) {
        spin_unlock_irqrestore(&vbd->lock, flags);
	return;
    }
    rp = vbd->ring->iresp;
    rmb(); /* Ensure we see queued responses up to 'rp'. */

    for (i = vbd->iresp; i != rp; i++) {
	RDiskResp*      bret = VBD_BLKIF_RING_RESP(vbd, i);
	struct request* req  = (struct request*)(unsigned long)bret->cookie;

	switch (bret->op) {
	    case RDISK_OP_READ:
	    case RDISK_OP_WRITE: {
		int error = (bret->status != RDISK_STATUS_OK);

		VBD_CATCHIF (error,
			 ETRACE("Bad return from blkdev data request: %x\n",
				bret->status));

		    /* __blk_end_request() is no more GPL */
		__blk_end_request(req, error ? -EIO : 0, blk_rq_bytes(req));

		break;
	    }
	    case RDISK_OP_PROBE:
		XTRACE("response to probe received\n");
		memcpy(&vbd->rsp, bret, sizeof(*bret));
		vbd->rsp_valid = 1;
		break;

	    default:
		 BUG();
	}
    }
    vbd->iresp = i;
    vbd_kick_pending_request_queues(vbd);
    spin_unlock_irqrestore(&vbd->lock, flags);
}

    static void
vbd_disconnect (vbd_t* vbd)
{
    if (vbd->xid) {
	nkops.nk_xirq_detach(vbd->xid);
    }
}

    static int
vbd_connect (vbd_t* vbd)
{
    NkDevRing* ring = (NkDevRing*)nkops.nk_ptov(vbd->vdev->dev_header); 

    vbd->dsize = ring->dsize;
    vbd->imask = ring->imask;
    vbd->rsize = ring->imask + 1;
    vbd->iresp = ring->iresp;
    vbd->ireq  = ring->ireq;
    vbd->ring  = ring; 
    vbd->lock  = SPIN_LOCK_UNLOCKED;

    if (ring->pxirq) {
	vbd->xirq = ring->pxirq;
    } else {
        vbd->xirq = nkops.nk_xirq_alloc(1);	
        if (!vbd->xirq) {
	    ETRACE("cross IRQ allocation failure\n");
	    return 0;
	}
    }
    vbd->rbase = nkops.nk_ptov(ring->base);
    vbd->xid = nkops.nk_xirq_attach(vbd->xirq, vbd_blkif_int, vbd);
    if (!vbd->xid) {
	ETRACE("XIRQ %d attach failure\n", vbd->xirq);
	vbd_disconnect(vbd);
	return 0;
    }
    vbd->state = VBD_BLKIF_STATE_CONNECTED;
    ring->pxirq = vbd->xirq;
    nkops.nk_xirq_trigger(NK_XIRQ_SYSCONF, vbd->vdev->dev_owner);
    return 1;
}

    static void
vbd_create (NkDevDesc* vdev)
{
    vbd_t* vbd;
    if (vbd_find(vdev)) {
	return;
    }
    vbd = vbd_alloc();
    if (!vbd) {
	return;
    }
    vbd->vdev = vdev;
    if (!vbd_connect(vbd)) {
	vbd_free(vbd);
	return;
    }
    vbd_init(vbd);
    vbd->next = vbds;
    vbds      = vbd;
}

    static void
vbd_lookup (void)
{
    NkOsId     myid = nkops.nk_id_get();
    NkPhAddr   pdev;
    NkDevDesc* ldev = 0;

	//
	// search for a local ring (a king of loop-back VBD)
	// 
    pdev = 0;
    while ((pdev = nkops.nk_dev_lookup_by_type(NK_DEV_ID_RING, pdev))) {
	NkDevDesc* vdev = (NkDevDesc*)nkops.nk_ptov(pdev);
	NkDevRing* ring = (NkDevRing*)nkops.nk_ptov(vdev->dev_header); 
	if ((vdev->dev_owner == myid) &&
	    (ring->type == RDISK_RING_TYPE) &&
	    (ring->pid == myid)) {
	    ldev = vdev;
	}
    }
    if (ldev) {
	vbd_create(ldev);
    }
	//
	// search for remote rings
	//
    pdev = 0;
    while ((pdev = nkops.nk_dev_lookup_by_type(NK_DEV_ID_RING, pdev))) {
	NkDevDesc* vdev = (NkDevDesc*)nkops.nk_ptov(pdev);
	NkDevRing* ring = (NkDevRing*)nkops.nk_ptov(vdev->dev_header); 
	if ((vdev->dev_owner != myid) &&
	    (ring->type == RDISK_RING_TYPE) &&
	    (ring->pid  == myid)) {
	    vbd_create(vdev);
	}
    }
}

    static int
vbd_thread (void* cookie)
{
    int res;

    (void) cookie;
    XTRACE("started\n");

    while (!vbd_th_abort) {
	vbd_lookup();
	res = down_interruptible(&vbd_th_sem);
    }

    while (!kthread_should_stop()) {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1);
    }
    XTRACE("exited\n");
    return 0;
}

    static void
vbd_sysconf (void* cookie, NkXIrq xirq)
{
    (void) cookie;
    (void) xirq;
    up(&vbd_th_sem);
}

    static int __init
vbd_wait (char* start)
{
    long  vmajor;
    long  vminor;
    char* end;

    if (*start++ != '(') {
	return 0;
    }
    vmajor = simple_strtoul(start, &end, 0);
    if ((end == start) || (*end != ',')) {
	return 0;
    }
    start = end+1;

    vminor = simple_strtoul(start, &end, 0);
    if ((end == start) || (*end != ')')) {
	return 0;
    }
    vbd_wait_flag = 1;
    vbd_wait_id   = (vmajor << 8) | vminor;
    return 1;
}


__setup("vdisk-wait=", vbd_wait);

    int 
vbd_module_init (void)
{
    init_MUTEX_LOCKED(&vbd_th_sem);

    vbd_xid = nkops.nk_xirq_attach(NK_XIRQ_SYSCONF, vbd_sysconf, 0);
    if (!vbd_xid) {
	ETRACE("XIRQ %d attach failure\n", NK_XIRQ_SYSCONF);
	return 1;
    }

    vbd_th_desc = kthread_run(vbd_thread, 0, "vbd-fe");
    if (IS_ERR(vbd_th_desc)) {
	nkops.nk_xirq_detach(vbd_xid);
	ETRACE("kthread_run(vbd_thread) failure\n");
	return 1;
    }

    TRACE("module loaded (%s)\n", "vlx-vbd-fe.c 1.76 10/01/09");

    while (vbd_wait_flag) {	// waiting for the virtual disk
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1);
    }
    return 0;
}

    void
vbd_module_exit (void)
{
    if (vbd_xid) {
        nkops.nk_xirq_detach(vbd_xid);
    }

    vbd_th_abort = 1;
    up(&vbd_th_sem);
    if (vbd_th_desc && !IS_ERR(vbd_th_desc)) {
   	kthread_stop(vbd_th_desc);
    }

    while (vbds) {
	vbd_t* next = vbds->next;
	vbd_down(vbds);
	vbd_disconnect(vbds);
	vbd_free(vbds);
	vbds = next;
    }
}


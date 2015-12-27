/*
 ****************************************************************
 *
 *  Component: VLX Performance Monitoring
 *             Linux user interface
 *
 *  Copyright (C) 2011, Red Bend Ltd.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  You should have received a copy of the GNU General Public License Version 2
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *  Contributor(s):
 *    Chi Dat Truong (chidat.truong@redbend.com)
 *
 ****************************************************************
 */

#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <nk/nkperfmon.h>
#include <vlx/perfmon.h>

#define ERR        "VLX Performance Analysis: error -- "
#define TRACE(format, args...) printk("[%s:%d] " format " \n", __FUNCTION__ , __LINE__ , ## args )

#define PMON_TAMPON_SIZE          4096
#define PMON_VERSION_NUMBER       "1.0"
#define PMON_BUFFER_SIZE          64*1024   /* Default buffer size is 64KB */

MODULE_DESCRIPTION("VLX Performance monitoring module");
MODULE_AUTHOR("Chi Dat TRUONG <chidat.truong@redbend.com>");
MODULE_LICENSE("GPL");

/*
 * Module loading parameters
 */
static int pmon_buffer_size = PMON_BUFFER_SIZE;
module_param(pmon_buffer_size, int, 0444);
MODULE_PARM_DESC(pmon_buffer_size, " integer\n\t\t"
        "size in bytes for each allocated log buffer (default = 1MB)");

static int pmon_one_shot = 0;
module_param(pmon_one_shot, int, 0444);
MODULE_PARM_DESC(pmon_one_shot, " integer\n\t\t"
        "one shot test");

typedef struct NkPMonBufferDesc {
	unsigned long   vmem_order;        // memory size order
	void*           vmem_start;        // start address (virtual)
	NkPhAddr        pmem_start;        // start address (physical)
} NkPMonBufferDesc;

/*
 * Support only 2 buffers for each cpu
 */
#define MAX_BUFFER    2

#define	PMON_MAX_CPUS	4

static NkPMonBufferDesc pmon_buffers[PMON_MAX_CPUS][MAX_BUFFER];
static NkPMonBufferDesc pmon_stats_buffers[PMON_MAX_CPUS][MAX_BUFFER];
static PmonSysInfo pmon_sysinfo;
static int current_buffer_index[PMON_MAX_CPUS];
static int current_stats_buffer_index[PMON_MAX_CPUS];
static char* ascii_tampon = NULL;
static char* raw_tampon = NULL;
static char* stats_tampon = NULL;
static char* sysinfo_tampon = NULL;

#define	PMON_CONTROL(cmd, cpu, arg) \
	os_ctx->pmonops.control(os_ctx, PMON_CONTROL_SET(cmd,cpu), arg)

static int max_phys_cpus;

/*
 * Allocate physical contiguous memory pages
 */
	static int
_perfmon_alloc_buffers (void)
{
	int i,j;

	for (j=0; j<PMON_MAX_CPUS; j++) {
		for (i=0; i<MAX_BUFFER; i++) {
			// Allocate buffer for logging
			memset(&pmon_buffers[j][i], 0, sizeof(NkPMonBufferDesc));
			pmon_buffers[j][i].vmem_order = get_order(pmon_buffer_size);
			pmon_buffers[j][i].vmem_start = (void*)__get_free_pages(GFP_KERNEL, pmon_buffers[j][i].vmem_order);
			if (!pmon_buffers[j][i].vmem_start) {
				printk(ERR "__get_free_pages(%lx) failed for buffer %d\n", pmon_buffers[j][i].vmem_order, i);
				return -1;
			}
			memset(pmon_buffers[j][i].vmem_start, 0, pmon_buffer_size);
			pmon_buffers[j][i].pmem_start = nkops.nk_vtop(pmon_buffers[j][i].vmem_start);

			// Allocate buffer for cpu statistic
			memset(&pmon_stats_buffers[j][i], 0, sizeof(NkPMonBufferDesc));
			pmon_stats_buffers[j][i].vmem_order = get_order(sizeof(NkPmonCpuStats));
			pmon_stats_buffers[j][i].vmem_start = (void*)__get_free_pages(GFP_KERNEL, pmon_stats_buffers[j][i].vmem_order);
			if (!pmon_stats_buffers[j][i].vmem_start) {
				printk(ERR "__get_free_pages(%lx) failed for buffer %d\n", pmon_stats_buffers[j][i].vmem_order, i);
				return -1;
			}
			memset(pmon_stats_buffers[j][i].vmem_start, 0, sizeof(NkPmonCpuStats));
			pmon_stats_buffers[j][i].pmem_start = nkops.nk_vtop(pmon_stats_buffers[j][i].vmem_start);

		}
	}

	if ((ascii_tampon = kmalloc(PMON_TAMPON_SIZE, GFP_KERNEL)) == NULL) return -1;
	if ((raw_tampon = kmalloc(PMON_TAMPON_SIZE, GFP_KERNEL)) == NULL) return -1;
	if ((stats_tampon = kmalloc(PMON_TAMPON_SIZE, GFP_KERNEL)) == NULL) return -1;
	if ((sysinfo_tampon = kmalloc(PMON_TAMPON_SIZE, GFP_KERNEL)) == NULL) return -1;

	return 0;
}

/*
 * Free pages
 */
	static void
_perfmon_free_buffers (void)
{
	int i,j;

	if (ascii_tampon) kfree(ascii_tampon);
	if (raw_tampon) kfree(raw_tampon);
	if (stats_tampon) kfree(stats_tampon);
	if (sysinfo_tampon) kfree(sysinfo_tampon);

	for (j=0; j<PMON_MAX_CPUS; j++) {
		for (i=0; i<MAX_BUFFER; i++) {
			if (pmon_buffers[j][i].vmem_start) {
				free_pages((unsigned long)pmon_buffers[j][i].vmem_start, pmon_buffers[j][i].vmem_order);
				pmon_buffers[j][i].vmem_start = 0;
			}
			if (pmon_stats_buffers[j][i].vmem_start) {
				free_pages((unsigned long)pmon_stats_buffers[j][i].vmem_start, pmon_stats_buffers[j][i].vmem_order);
				pmon_stats_buffers[j][i].vmem_start = 0;
			}
		}
	}
}

/*
 * Initialize data and start the performance monitoring operation
 */

	static int
_perfmon_init_data (void)
{
	int i;
	int j;
	int err=0;
	NkPmonBuffer* buf;

	// Initialize index
	for (j=0; j < max_phys_cpus; j++) {
		current_buffer_index[j] = 0;
		for (i=0; i<MAX_BUFFER; i++) {
			buf = (NkPmonBuffer*) pmon_buffers[j][i].vmem_start;
			buf->length = PMON_GET_RECORD_LENGTH(pmon_buffer_size);
		}
	}

	// Set version
	strncpy(pmon_sysinfo.version, PMON_VERSION_NUMBER,
		sizeof pmon_sysinfo.version - 1);

	// Set timer information
	pmon_sysinfo.timer.freq = (unsigned long) os_ctx->pmonops.get_freq(os_ctx);
	// Set os information
	pmon_sysinfo.last_os_id = os_ctx->lastid;
	// Set the maximum of record elements
	pmon_sysinfo.max_records = PMON_GET_RECORD_LENGTH(pmon_buffer_size);

	for (j=0; j < max_phys_cpus; j++) {
	    err += PMON_CONTROL(PMON_CPUSTATS_START , j,
	     pmon_stats_buffers[j][current_stats_buffer_index[j]].pmem_start);
	}

	return err;
}

/*
 * Stop the performance monitoring operation and clean up
 */
	static void
_perfmon_prepare_exit(void)
{
	int cpu;
	for (cpu=0; cpu < max_phys_cpus; cpu++) {
	    PMON_CONTROL(PMON_CPUSTATS_START, cpu, 0);
	    PMON_CONTROL(PMON_STOP, cpu, 0);
	}
}

	static int
_perfmon_proc_open (struct inode* inode,
               struct file*  file)
{
	return 0;
}

	static int
_perfmon_proc_release (struct inode* inode,
                  struct file*  file)
{
	return 0;
}

	static loff_t
_perfmon_proc_lseek (struct file* file,
                loff_t       off,
                int          whence)
{
	loff_t new;

	switch (whence) {
		case 0:         new = off; break;
		case 1:         new = file->f_pos + off; break;
		case 2:         new = 1 + off; break;
		default: return -EINVAL;
	}

	if (new) {
		return -EINVAL;
	}

	return (file->f_pos = new);
}

#if 0
	static ssize_t
_perfmon_proc_read (struct file* file,
               char*        buf,
               size_t       count,
               loff_t*      ppos)
{
	return 0;
}
#endif

	static ssize_t
_perfmon_proc_write (struct file* file,
                const char*  buf,
                size_t       size,
                loff_t*      ppos)
{
	if (*ppos || !size) {
		return 0;
	}
	return size;
}

	static ssize_t
_perfmon_prepare_ascii_header(char* buf, int size, NkPmonBuffer* log, int cpu)
{
	ssize_t count = 0;

	// We expect the buffer is sized enough
	// for filling header information
	count += sprintf(buf+count, "========================================\n");
	count += sprintf(buf+count, " Time Stamp\t\t State\t\t cookie\n");
	count += sprintf(buf+count, " ----------\t\t -----\t\t ------\n");

	if (count > size) return -EFAULT;
	return count;
}

/*
 * Read current log buffer
 */
	static ssize_t
_perfmon_asciicurr_proc_read (struct file* file,
                              char*        buf,
                              size_t       count,
                              loff_t*      ppos)
{
	struct proc_dir_entry* dp;
	int                    cpu;
	ssize_t                copy_size;
	int                    record_size;
	NkPmonBuffer*          log;
	static int             i;

	if (count < 0) {
		return -EINVAL;
	} else {
		if (count > PMON_TAMPON_SIZE) count = PMON_TAMPON_SIZE;
	}

	dp = PDE(file->f_dentry->d_inode);
	// Get cpu number from parents in path /proc/nk/cpu/#/perfmon/ascii/curr
	cpu = simple_strtoul(dp->parent->parent->parent->name, 0, 10);
	log = (NkPmonBuffer*) pmon_buffers[cpu][current_buffer_index[cpu]].vmem_start;
	copy_size = 0;

	// read header first
	if (*ppos == 0) {
		copy_size = _perfmon_prepare_ascii_header(ascii_tampon, PMON_TAMPON_SIZE, log, cpu);
		// mark the start position of the raw buffer for the next
		// if the buffer is overflowed we start with the last position.
		// Actually the first and the last are always separated by one record
		i = (log->first > log->last ? log->last : log-> first);
	}
	else { // read next
		if (i == log->last) {
			return 0;
		}
		return 0;
	}
	do {
		record_size = copy_size;
		copy_size += sprintf(ascii_tampon+copy_size, "0x%016llx\t",
		                     log->data[i].stamp);
		copy_size += sprintf(ascii_tampon+copy_size, "0x%08x\t0x%08x\n",
                             log->data[i].state, log->data[i].cookie);
		record_size = copy_size - record_size;
		i++;
		if (i >= log->length) i = 0;  // circular buffer
		// if we cannot fill another record information => return the results
		if (copy_size+record_size > count) {
			break;
		}
	} while(i != log->last);

	if (copy_to_user(buf, ascii_tampon, copy_size)) {
		return -EFAULT;
	} else {
		*ppos += copy_size;
	}
	return copy_size;
}

/*
 * Read the previous log buffer
 */
	static ssize_t
_perfmon_asciiprev_proc_read (struct file* file,
                              char*        buf,
                              size_t       count,
                              loff_t*      ppos)
{
	struct proc_dir_entry* dp;
	int                    cpu;
	ssize_t                copy_size;
	int                    record_size;
	NkPmonBuffer*          log;
	static int             i;

	if (count < 0) {
		return -EINVAL;
	} else {
		if (count > PMON_TAMPON_SIZE) count = PMON_TAMPON_SIZE;
	}

	dp = PDE(file->f_dentry->d_inode);
	// Get cpu number from parents in path /proc/nk/cpu/#/perfmon/ascii/prev
	cpu = simple_strtoul(dp->parent->parent->parent->name, 0, 10);
	log = (NkPmonBuffer*) pmon_buffers[cpu][1 - current_buffer_index[cpu]].vmem_start;
	copy_size = 0;

	// read header first
	if (*ppos == 0) {
		copy_size = _perfmon_prepare_ascii_header(ascii_tampon, PMON_TAMPON_SIZE, log, cpu);
		// mark the start position of the raw buffer for the next
		// if the buffer is overflowed we start with the last position.
		// Actually the first and the last are always separated by one record
		i = (log->first > log->last ? log->last : log-> first);
	}
	else { // read next
		if (i == log->last) {
			return 0;
		}
	}
	do {
		record_size = copy_size;
		copy_size += sprintf(ascii_tampon+copy_size, "0x%016llx\t",
		                     log->data[i].stamp);
		copy_size += sprintf(ascii_tampon+copy_size, "0x%08x\t0x%08x\n",
                             log->data[i].state, log->data[i].cookie);
		record_size = copy_size - record_size;
		i++;
		if (i >= log->length) i = 0;  // circular buffer
		// if we cannot fill another record information => return the results
		if (copy_size+record_size > count) {
			break;
		}
	} while(i != log->last);

	if (copy_to_user(buf, ascii_tampon, copy_size)) {
		return -EFAULT;
	} else {
		*ppos += copy_size;
	}
	return copy_size;
}

/*
 * Read the CPU Performance
 */
	static ssize_t
_perfmon_asciicpustats_proc_read (struct file* file,
                                  char*        buf,
                                  size_t       count,
                                  loff_t*      ppos)
{
	struct proc_dir_entry* dp;
	int                    cpu;
	ssize_t                copy_size;
	ssize_t                size;
	unsigned long long     period;
	int                    id;
	PmonCpuStats*          pmon_stats;

	if (count < 0) {
		return -EINVAL;
	} else {
		if (count > PMON_TAMPON_SIZE) count = PMON_TAMPON_SIZE;
	}

	dp = PDE(file->f_dentry->d_inode);
	// Get cpu number from parents in path /proc/nk/cpu/#/perfmon/ascii/cpustats
	cpu = simple_strtoul(dp->parent->parent->parent->name, 0, 10);
	copy_size = 0;

	if (*ppos == 0) {
		int old_index = current_stats_buffer_index[cpu];
		// switch index to use next buffer
		current_stats_buffer_index[cpu] = 1 - old_index;
		// clean buffer
		memset(pmon_stats_buffers[cpu][current_stats_buffer_index[cpu]].vmem_start, 0, sizeof(PmonCpuStats));
		// start new mesure
		PMON_CONTROL(PMON_CPUSTATS_START, cpu,
	 pmon_stats_buffers[cpu][current_stats_buffer_index[cpu]].pmem_start);
		// copy information of last buffer to user
		pmon_stats = pmon_stats_buffers[cpu][old_index].vmem_start;
		size = 0;

		period = pmon_stats->laststamp - pmon_stats->startstamp;
		size += sprintf(stats_tampon+size, "\n");
		size += sprintf(stats_tampon+size, "CPU Consumption Statistics\n");
		size += sprintf(stats_tampon+size, "PERIOD:\t%llu\nIDLE :\t%llu\n", period, pmon_stats->cpustats[0]);
		size += sprintf(stats_tampon+size, "\n");

		for (id = 1; id <= pmon_sysinfo.last_os_id; id++) {
			size += sprintf(stats_tampon+size, "OS %02d:\t%llu\n", id, pmon_stats->cpustats[id]);
		}

		copy_size = strlen(stats_tampon);
		if (copy_size > count) copy_size = count;
		if (copy_to_user(buf, stats_tampon, copy_size)) {
			return -EFAULT;
		} else {
			*ppos += copy_size;
		}
	}
	else {
		copy_size = strlen(stats_tampon);
		if (*ppos > copy_size) {
			return 0;
		}
		copy_size -= *ppos;
		if (copy_size > count) copy_size = count;
		if (copy_to_user(buf, stats_tampon, copy_size)) {
			return -EFAULT;
		} else {
			*ppos += copy_size;
		}
	}
	return copy_size;
}

/*
 *
 */
	static ssize_t
_perfmon_asciisysinfo_proc_read (struct file* file,
                                 char*        buf,
                                 size_t       count,
                                 loff_t*      ppos)
{
	struct proc_dir_entry* dp;
	int                    cpu;
	ssize_t                copy_size;
	ssize_t                size;

	if (count < 0) {
		return -EINVAL;
	} else {
		if (count > PMON_TAMPON_SIZE) count = PMON_TAMPON_SIZE;
	}

	dp = PDE(file->f_dentry->d_inode);
	// Get cpu number from parents in path /proc/nk/cpu/#/perfmon/ascii/cpustats
	cpu = simple_strtoul(dp->parent->parent->parent->name, 0, 10);
	copy_size = 0;

	if (*ppos == 0) {
		size = 0;
		size += sprintf(sysinfo_tampon+size, "\n");
		size += sprintf(sysinfo_tampon+size, "VLX Performance Analysis v%s\n", pmon_sysinfo.version);
		size += sprintf(sysinfo_tampon+size, "Timer frequency : %ld Hertz\n", pmon_sysinfo.timer.freq);
		size += sprintf(sysinfo_tampon+size, "Number of OSes  : %ld\n", pmon_sysinfo.last_os_id);
		size += sprintf(sysinfo_tampon+size, "Recording log uses double buffers of %d bytes, ", pmon_buffer_size);
		size += sprintf(sysinfo_tampon+size, "each containing %ld records\n", pmon_sysinfo.max_records);
		size += sprintf(sysinfo_tampon+size, "\n");
		copy_size = strlen(sysinfo_tampon);
		if (copy_size > count) copy_size = count;
		if (copy_to_user(buf, sysinfo_tampon, copy_size)) {
			return -EFAULT;
		} else {
			*ppos += copy_size;
		}
	}
	else {
		copy_size = strlen(sysinfo_tampon);
		if (*ppos > copy_size) {
			return 0;
		}
		copy_size -= *ppos;
		if (copy_size > count) copy_size = count;
		if (copy_to_user(buf, sysinfo_tampon, copy_size)) {
			return -EFAULT;
		} else {
			*ppos += copy_size;
		}
	}
	return copy_size;
}

	static ssize_t
_perfmon_rawcurr_proc_read (struct file* file,
                            char*        buf,
                            size_t       count,
                            loff_t*      ppos)
{
	struct proc_dir_entry* dp;
	int                     cpu;
	int                     copy_elts;
	ssize_t                 copy_size;
	NkPmonBuffer*           log;
	static int              stop_read;
	static int              i;

	if (count < 0) {
		return -EINVAL;
	}

	dp = PDE(file->f_dentry->d_inode);
	// Get cpu number from parents in path /proc/nk/cpu/#/perfmon/raw/curr
	cpu = simple_strtoul(dp->parent->parent->parent->name, 0, 10);
	log = (NkPmonBuffer*) pmon_buffers[cpu][current_buffer_index[cpu]].vmem_start;
	copy_size = 0;

	if (*ppos == 0) {
		// mark the start position of the raw buffer for the next
		// if the buffer is overflowed we start with the last position.
		// Actually the first and the last are always separated by one record
		i = (log->first > log->last ? log->last : log-> first);
		stop_read = 0;
	}
	// read entire raw buffer
	if (stop_read && i == log->last) return 0;
	// The reader try to attempt the last position
	// Note: the last position moves since we are in the current buffer
	else if (i < log->last) {
		copy_elts = log->last - i;
	} else {  // (i > log->last)
		copy_elts = log->length - i;
	}

	if (copy_elts > count/sizeof(NkPmonRecord)) {
		copy_elts = count/sizeof(NkPmonRecord);
	}
	copy_size = copy_elts * sizeof(NkPmonRecord);
	if (copy_to_user(buf, &log->data[i], copy_size)) {
		return -EFAULT;
	} else {
		*ppos += copy_size;
		i += copy_elts;
	}
	if (i >= log->length) i = 0;
	stop_read = 1;

	return copy_size;
}

	static ssize_t
_perfmon_rawprev_proc_read (struct file* file,
                            char*        buf,
                            size_t       count,
                            loff_t*      ppos)
{
	struct proc_dir_entry* dp;
	int                    cpu;
	NkPmonBuffer*          log;
	unsigned long          _ppos;
	char*                  dest;
	int                    data_size;
	ssize_t                copy_size;

	if (count < 0) {
		return -EINVAL;
	}

	dp = PDE(file->f_dentry->d_inode);
	// Get cpu number from parents in path /proc/nk/cpu/#/perfmon/raw/prev
	cpu = simple_strtoul(dp->parent->parent->parent->name, 0, 10);
	log = (NkPmonBuffer*) pmon_buffers[cpu][1 - current_buffer_index[cpu]].vmem_start;
	copy_size = 0;

	_ppos = *ppos;
	// calcutate the size of data recorded
	data_size = log->last - log->first;  // calculate the relative length of the buffer

	// Case the raw buffer is already overflowed
	if (data_size < 0) {
		// End of buffer reached
		if (_ppos >= (log->length * sizeof(NkPmonRecord))) return 0;

		// Calculate the superior segment count of the raw buffer
		//   [- - - - - - BUFFER - - - - - - - - ]
		//   0    ^              ^ <--data_size--> length
		//      last           first
		// Note: the last and first positions are always separated by one record
		// because of the circular buffer
		data_size = (log->length - log->last) * sizeof(NkPmonRecord);  // calculate data size in bytes

		if (_ppos < data_size) {
			//data_size = rcount;
			copy_size = (count > (data_size - _ppos) ? (data_size - _ppos) : count);
			dest = (char*) &log->data[log->last];
		}

		// We are in the inferior segment
		// Calcutate the inferior segment count of the raw buffer
		//   [- - - - - - - - - BUFFER - - -]
		//   0<--data_size-->^           ^  length
		//                  last       first
		else {
			// match the offset _ppos=0 to the start position of the raw buffer
			_ppos = _ppos - data_size;
			data_size = log->last * sizeof(NkPmonRecord);   // recalculate the data size of the segment
			copy_size = (count > (data_size - _ppos) ? (data_size - _ppos) : count);
			dest = (char*) &log->data[0];
		}
	}
	// Case the raw buffer is not full
	else {
		if (data_size==0) data_size = log->length;
		data_size = data_size * sizeof(NkPmonRecord);  // caculate data size in bytes
		if (_ppos >= data_size) return 0;  // end of buffer reached
		dest = (char*) &log->data[log->first];
		copy_size = (count > (data_size - _ppos) ? (data_size - _ppos) : count);
	}

	// Copy the data to user space
	if (copy_to_user(buf, dest + _ppos, copy_size)) {
		return -EFAULT;
	} else {
		*ppos += copy_size;
	}
	return copy_size;
}

/*
 * Read the CPU Performance data
 */
	static ssize_t
_perfmon_rawcpustats_proc_read (struct file* file,
                                char*        buf,
                                size_t       count,
                                loff_t*      ppos)
{
	struct proc_dir_entry* dp;
	int                    cpu;
	ssize_t                copy_size;
	int                    old_index;

	if (count < 0) {
		return -EINVAL;
	}

	dp = PDE(file->f_dentry->d_inode);
	// Get cpu number from parents in path /proc/nk/cpu/#/perfmon/raw/cpustats
	cpu = simple_strtoul(dp->parent->parent->parent->name, 0, 10);
	copy_size = sizeof(PmonCpuStats);

	if (*ppos == 0) {
		old_index = current_stats_buffer_index[cpu];
		// switch index to use next buffer
		current_stats_buffer_index[cpu] = 1 - current_stats_buffer_index[cpu];
		// clean new buffer
		memset(pmon_stats_buffers[cpu][current_stats_buffer_index[cpu]].vmem_start, 0, sizeof(PmonCpuStats));
		// start new mesure
		PMON_CONTROL(PMON_CPUSTATS_START, cpu,
	 pmon_stats_buffers[cpu][current_stats_buffer_index[cpu]].pmem_start);
		if (count < copy_size) {
			copy_size = count;
		}
		// copy information of last buffer to user
		if (copy_to_user(buf, pmon_stats_buffers[cpu][old_index].vmem_start, copy_size)) {
			return -EFAULT;
		} else {
			*ppos += copy_size;
		}
	}
	else {
		old_index = 1 - current_stats_buffer_index[cpu];
		if (*ppos > copy_size) {
			return 0;
		}
		copy_size -= *ppos;
		if (count < copy_size) {
			copy_size = count;
		}
		if (copy_to_user(buf, pmon_stats_buffers[cpu][old_index].vmem_start, copy_size)) {
			return -EFAULT;
		} else {
			*ppos += copy_size;
		}
	}
	return copy_size;
}

/*
 * Read the system information in raw data
 */
	static ssize_t
_perfmon_rawsysinfo_proc_read (struct file* file,
                               char*        buf,
                               size_t       count,
                               loff_t*      ppos)
{
	char* info = (char*) &pmon_sysinfo;
	int copy_size = sizeof(PmonSysInfo);

	if (count < 0) {
		return -EINVAL;
	}

	if (*ppos > copy_size) {
		return 0;
	}
	else {
		copy_size -= *ppos;
	}

	if (count < copy_size) {
		copy_size = count;
	}
	if (copy_to_user(buf, info + *ppos, copy_size)) {
		return -EFAULT;
	} else {
		*ppos += copy_size;
	}
	return copy_size;
}


/*
 * Control the recording buffer
 *
 */
	static ssize_t
_perfmon_control_proc_write (struct file* file,
                             const char*  buf,
                             size_t       size,
                             loff_t*      ppos)
{
	struct proc_dir_entry* dp;
	char cmd[30];
	int err = 0;
	int cpu;

	dp = PDE(file->f_dentry->d_inode);
	// Get cpu number from parents in path /proc/nk/cpu/#/perfmon/raw/prev
	cpu = simple_strtoul(dp->parent->parent->parent->name, 0, 10);
	memset(cmd, 0, 30);
	if (copy_from_user(cmd, buf, (size < 30 ? size : 30))) {
		return -EFAULT;
	}

	if (strstr(cmd, CMD_SWITCH)!=NULL) {
		current_buffer_index[cpu] = 1 - current_buffer_index[cpu];
		// continue the monitoring with new buffer
		if (pmon_one_shot)
			PMON_CONTROL(PMON_START_ONE_SHOT, cpu, 
		     pmon_buffers[cpu][current_buffer_index[cpu]].pmem_start);
		else
			err += PMON_CONTROL(PMON_START, cpu, pmon_buffers[cpu][current_buffer_index[cpu]].pmem_start);
	} else if (strstr(cmd, CMD_ONESHOT_START)!=NULL) {
		pmon_one_shot = 1;
		err += PMON_CONTROL(PMON_START_ONE_SHOT, cpu, pmon_buffers[cpu][current_buffer_index[cpu]].pmem_start);
	} else if (strstr(cmd, CMD_STATS_START)!=NULL) {
		err += PMON_CONTROL(PMON_CPUSTATS_START, cpu,
	pmon_stats_buffers[cpu][current_stats_buffer_index[cpu]].pmem_start);
	} else if (strstr(cmd, CMD_START)!=NULL) {
		pmon_one_shot = 0;
		err += PMON_CONTROL(PMON_START, cpu, pmon_buffers[cpu][current_buffer_index[cpu]].pmem_start);
	} else if (strstr(cmd, CMD_STOP)!=NULL) {
		err += PMON_CONTROL(PMON_STOP, cpu, pmon_buffers[cpu][current_buffer_index[cpu]].pmem_start);
	}
	else printk(ERR "Unknown command\n");
	if (err) printk(ERR "Command failed\n");
	return size;
}

/*
 * When reading from control interface we obtain the status
 * of each service
 */
	static ssize_t
_perfmon_control_proc_read (struct file* file,
                            char*        buf,
                            size_t       count,
                            loff_t*      ppos)
{
	NkPmonBuffer* log;
	int status[PMON_SUPPORTED_SERVICE];
	char* info = (char*) status;
	int copy_size = sizeof(int) * PMON_SUPPORTED_SERVICE;
	struct proc_dir_entry* dp;
	int cpu;

	if (count < 0) {
		return -EINVAL;
	}

	if (*ppos > copy_size) {
		return 0;
	}
	else {
		copy_size -= *ppos;
	}

	if (count < copy_size) {
		copy_size = count;
	}

	dp = PDE(file->f_dentry->d_inode);
	// Get cpu number from parents in path /proc/nk/cpu/#/perfmon/raw/prev
	cpu = simple_strtoul(dp->parent->parent->parent->name, 0, 10);

	log = (NkPmonBuffer*) pmon_buffers[cpu][current_buffer_index[cpu]].vmem_start;

	status[0] = 1;  // CPU performance service is alway enabled

	if (log->first == log->last) status[1] = 0;
	else status[1] = 1;

	if (copy_to_user(buf, info + *ppos, copy_size)) {
		return -EFAULT;
	} else {
		*ppos += copy_size;
	}
	return copy_size;
}

static struct file_operations _nkcpu_pmoncontrol_proc_fops = {
	open:    _perfmon_proc_open,
	release: _perfmon_proc_release,
	llseek:  _perfmon_proc_lseek,
	read:    _perfmon_control_proc_read,
	write:   _perfmon_control_proc_write,
};

static struct file_operations _nkcpu_pmonasciicurr_proc_fops = {
	open:    _perfmon_proc_open,
	release: _perfmon_proc_release,
	llseek:  _perfmon_proc_lseek,
	read:    _perfmon_asciicurr_proc_read,
	write:   _perfmon_proc_write,
};

static struct file_operations _nkcpu_pmonasciiprev_proc_fops = {
	open:    _perfmon_proc_open,
	release: _perfmon_proc_release,
	llseek:  _perfmon_proc_lseek,
	read:    _perfmon_asciiprev_proc_read,
	write:   _perfmon_proc_write,
};


static struct file_operations _nkcpu_pmonasciicpustats_proc_fops = {
	open:    _perfmon_proc_open,
	release: _perfmon_proc_release,
	llseek:  _perfmon_proc_lseek,
	read:    _perfmon_asciicpustats_proc_read,
	write:   _perfmon_proc_write,
};

static struct file_operations _nkcpu_pmonasciisysinfo_proc_fops = {
	open:    _perfmon_proc_open,
	release: _perfmon_proc_release,
	llseek:  _perfmon_proc_lseek,
	read:    _perfmon_asciisysinfo_proc_read,
	write:   _perfmon_proc_write,
};

static struct file_operations _nkcpu_pmonrawcurr_proc_fops = {
	open:    _perfmon_proc_open,
	release: _perfmon_proc_release,
	llseek:  _perfmon_proc_lseek,
	read:    _perfmon_rawcurr_proc_read,
	write:   _perfmon_proc_write,
};

static struct file_operations _nkcpu_pmonrawprev_proc_fops = {
	open:    _perfmon_proc_open,
	release: _perfmon_proc_release,
	llseek:  _perfmon_proc_lseek,
	read:    _perfmon_rawprev_proc_read,
	write:   _perfmon_proc_write,
};

static struct file_operations _nkcpu_pmonrawcpustats_proc_fops = {
	open:    _perfmon_proc_open,
	release: _perfmon_proc_release,
	llseek:  _perfmon_proc_lseek,
	read:    _perfmon_rawcpustats_proc_read,
	write:   _perfmon_proc_write,
};

static struct file_operations _nkcpu_pmonrawsysinfo_proc_fops = {
	open:    _perfmon_proc_open,
	release: _perfmon_proc_release,
	llseek:  _perfmon_proc_lseek,
	read:    _perfmon_rawsysinfo_proc_read,
	write:   _perfmon_proc_write,
};

typedef struct ProcData {
	struct proc_dir_entry* cpu;                    /* /proc/nk/cpu      */
	struct proc_dir_entry* cpunum[PMON_MAX_CPUS];  /* /proc/nk/cpu/#    */
	struct proc_dir_entry* cpu_pmon[PMON_MAX_CPUS]; /* /proc/nk/cpu/#/pmon/... */
	struct proc_dir_entry* cpu_pmon_ascii[PMON_MAX_CPUS];
	struct proc_dir_entry* cpu_pmon_raw[PMON_MAX_CPUS];
} ProcData;

static ProcData proc_data;

static const char num_dir_names[32][3] = {
"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
"10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
"20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
"30", "31"
};


	static int
_nk_proc_create (struct proc_dir_entry* parent,
                 const char* name,
                 struct file_operations* fops,
                 void* data)
{
	struct proc_dir_entry* file;
	file = create_proc_entry(name, (S_IFREG|S_IRUGO|S_IWUSR), parent);
	if (!file) {
		printk(ERR "create_proc_entry(%s) failed\n", name);
		return -1;
	}
	file->data      = data;
	file->size      = 0;
	file->proc_fops = fops;
	return 0;
}

/*
 * Create /proc/nk/cpu/#/pmon/... sub-tree
 */
	static int
_nk_perfmon_tree_create (void)
{
	int cpu;
	int error = 0;

		/*
		 * Create /proc/nk/cpu directory
		 */
	proc_data.cpu = proc_mkdir("nk/cpu", NULL);
	if (!proc_data.cpu) {
		printk(ERR "proc_mkdir(/proc/nk/cpu) failed\n");
		return -1;
	}
	for (cpu = 0 ; cpu < max_phys_cpus ; cpu++) {
		/*
		 * Create /proc/nk/cpu/# directory
		 */
		proc_data.cpunum[cpu] = proc_mkdir(num_dir_names[cpu], proc_data.cpu);
		if (!proc_data.cpunum[cpu]) {
			printk(ERR "proc_mkdir(/proc/nk/cpu/%d) failed\n", cpu);
			// call clean to remove created entry before exit
			return -1;
		}

		/*
		 * Create /proc/nk/cpu/#/pmon directory
		 */
		proc_data.cpu_pmon[cpu] = proc_mkdir("pmon", proc_data.cpunum[cpu]);
		if (!proc_data.cpu_pmon[cpu]) {
			printk(ERR "proc_mkdir(/proc/nk/cpu/%d/pmon) failed\n", cpu);
			return -1;
		}

		/*
		 * Create /proc/nk/cpu/#/pmon/ascii/[curr, prev, cpustats, sysinfo]
		 */
		proc_data.cpu_pmon_ascii[cpu] = proc_mkdir("ascii", proc_data.cpu_pmon[cpu]);
		if (!proc_data.cpu_pmon_ascii[cpu]) {
			printk(ERR "proc_mkdir(/proc/nk/cpu/%d/pmon/ascii) failed\n", cpu);
			return -1;
		}
		error += _nk_proc_create(proc_data.cpu_pmon_ascii[cpu], "curr",  &_nkcpu_pmonasciicurr_proc_fops, (void*)cpu);
		error += _nk_proc_create(proc_data.cpu_pmon_ascii[cpu], "prev",  &_nkcpu_pmonasciiprev_proc_fops, (void*)cpu);
		error += _nk_proc_create(proc_data.cpu_pmon_ascii[cpu], "cpustats",  &_nkcpu_pmonasciicpustats_proc_fops, (void*)cpu);
		error += _nk_proc_create(proc_data.cpu_pmon_ascii[cpu], "sysinfo",  &_nkcpu_pmonasciisysinfo_proc_fops, (void*)cpu);

		/*
		 * Create /proc/nk/cpu/#/pmon/raw/[curr, prev, cpustats, sysinfo]
		 */
		proc_data.cpu_pmon_raw[cpu] = proc_mkdir("raw", proc_data.cpu_pmon[cpu]);
		if (!proc_data.cpu_pmon_ascii[cpu]) {
			printk(ERR "proc_mkdir(/proc/nk/cpu/%d/pmon/raw) failed\n", cpu);
			return -1;
		}
		error += _nk_proc_create(proc_data.cpu_pmon_raw[cpu], "curr",  &_nkcpu_pmonrawcurr_proc_fops, (void*)cpu);
		error += _nk_proc_create(proc_data.cpu_pmon_raw[cpu], "prev",  &_nkcpu_pmonrawprev_proc_fops, (void*)cpu);
		error += _nk_proc_create(proc_data.cpu_pmon_raw[cpu], "cpustats",  &_nkcpu_pmonrawcpustats_proc_fops, (void*)cpu);
		error += _nk_proc_create(proc_data.cpu_pmon_raw[cpu], "sysinfo",  &_nkcpu_pmonrawsysinfo_proc_fops, (void*)cpu);

		/*
		 * Create /proc/nk/cpu/#/pmon/switch
		 */
		error += _nk_proc_create(proc_data.cpu_pmon[cpu], "control",  &_nkcpu_pmoncontrol_proc_fops, (void*)cpu);
	}
	return error;
}

/*
 * Remove /proc/nk/cpu/#/pmon/... sub-tree
 */
	static void
_nk_perfmon_tree_remove (void)
{
	int cpu;

		/*
		 * Clean /proc/nk/cpu/#/pmon/... sub-tree(s)
		 */
	for (cpu = 0 ; cpu < max_phys_cpus ; cpu++) {
		if (proc_data.cpu_pmon_ascii[cpu]) {
			remove_proc_entry("curr", proc_data.cpu_pmon_ascii[cpu]);
			remove_proc_entry("prev", proc_data.cpu_pmon_ascii[cpu]);
			remove_proc_entry("cpustats", proc_data.cpu_pmon_ascii[cpu]);
			remove_proc_entry("sysinfo", proc_data.cpu_pmon_ascii[cpu]);
			remove_proc_entry("ascii", proc_data.cpu_pmon[cpu]);
		}

		if (proc_data.cpu_pmon_raw[cpu]) {
			remove_proc_entry("curr", proc_data.cpu_pmon_raw[cpu]);
			remove_proc_entry("prev", proc_data.cpu_pmon_raw[cpu]);
			remove_proc_entry("cpustats", proc_data.cpu_pmon_raw[cpu]);
			remove_proc_entry("sysinfo", proc_data.cpu_pmon_raw[cpu]);
			remove_proc_entry("raw", proc_data.cpu_pmon[cpu]);
		}

		if (proc_data.cpu_pmon[cpu]) {
			remove_proc_entry("control", proc_data.cpu_pmon[cpu]);
			remove_proc_entry("pmon", proc_data.cpunum[cpu]);
		}

		/*
		 * Remove /proc/nk/cpu/#
		 */
		remove_proc_entry(num_dir_names[cpu], proc_data.cpu);
	}
	remove_proc_entry("nk/cpu", NULL);
}

	static void
_perfmon_module_exit (void)
{
	/*
	 * Remove all /proc/nk/cpu
	 */
	_nk_perfmon_tree_remove();

	_perfmon_prepare_exit();
	_perfmon_free_buffers();
}

    static int
max_phys_cpus_get (void)
{
    int cpu = 0;
    while (!PMON_CONTROL(PMON_NOP, cpu, 0)) cpu++;
    if (cpu > PMON_MAX_CPUS) {
	cpu = PMON_MAX_CPUS;
    } 
    return cpu;
}

	static int
_perfmon_module_init (void)
{
	max_phys_cpus = max_phys_cpus_get();

	//TRACE("MAX PHYS CPUS=%d", max_phys_cpus);
	if (max_phys_cpus <= 0) {
		printk(ERR "Did not find any physical CPUs\n");
		return -EINVAL;
	}
	if (_nk_perfmon_tree_create() < 0) {
		printk(ERR "Creating tree failed\n");
		_nk_perfmon_tree_remove();
		return -1;
	}
	/*
	 * Initialization
	 */
	if (pmon_buffer_size < PMON_BUFFER_SIZE) {
		pmon_buffer_size = PMON_BUFFER_SIZE;
	}

	if (_perfmon_alloc_buffers() < 0) {
		printk(ERR "Allocating memory failed\n");
		_perfmon_module_exit();
		return -1;
	}

	if (_perfmon_init_data() < 0) {
		printk(ERR "Initializing memory failed\n");
		_perfmon_module_exit();
		return -1;
	}

	printk("\nVLX Performance Analysis v%s\n", pmon_sysinfo.version);
	return 0;
}

module_init(_perfmon_module_init);
module_exit(_perfmon_module_exit);


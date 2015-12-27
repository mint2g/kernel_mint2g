/*******************************************************************************
 * Source file : beken_fm_ctl.c
 * Description : beken FM Receiver driver for linux.
 * Date        : 11/27/2012
 *
 * Copyright (C) 2012 Bekencorp
*
********************************************************************************
* Revison
  2012-11-27  initial version
*******************************************************************************/

/* seek direction */
#define BEKEN_SEEK_DIR_UP          0
#define BEKEN_SEEK_DIR_DOWN        1


/** The following define the IOCTL command values via the ioctl macros */
#define	BEKEN_FM_IOCTL_BASE     'R'
#define	BEKEN_FM_IOCTL_ENABLE		 _IOW(BEKEN_FM_IOCTL_BASE, 0, int)
#define BEKEN_FM_IOCTL_GET_ENABLE  _IOW(BEKEN_FM_IOCTL_BASE, 1, int)
#define BEKEN_FM_IOCTL_SET_TUNE    _IOW(BEKEN_FM_IOCTL_BASE, 2, int)
#define BEKEN_FM_IOCTL_GET_FREQ    _IOW(BEKEN_FM_IOCTL_BASE, 3, int)
#define BEKEN_FM_IOCTL_SEARCH      _IOW(BEKEN_FM_IOCTL_BASE, 4, int[4])
#define BEKEN_FM_IOCTL_STOP_SEARCH _IOW(BEKEN_FM_IOCTL_BASE, 5, int)
#define BEKEN_FM_IOCTL_MUTE        _IOW(BEKEN_FM_IOCTL_BASE, 6, int)
#define BEKEN_FM_IOCTL_SET_VOLUME  _IOW(BEKEN_FM_IOCTL_BASE, 7, int)
#define BEKEN_FM_IOCTL_GET_VOLUME  _IOW(BEKEN_FM_IOCTL_BASE, 8, int)
#define BEKEN_FM_IOCTL_GET_STATUS  _IOW(BEKEN_FM_IOCTL_BASE, 9, int[2])



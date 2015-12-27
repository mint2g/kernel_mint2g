/*
 ****************************************************************
 *
 *  Component: VLX nano-kernel boot interface
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
 *  #ident  "@(#)env.h 1.6     07/02/01 Red Bend"
 * 
 ****************************************************************
 */

#ifndef _NK_BOOT_ENV_H
#define _NK_BOOT_ENV_H

#define ENV_MAGIC 0xa3889a8a

typedef struct EnvDesc {
    int	    envMagic;
    int	    envChecksum;
    int	    envDataOffset;
    int	    envSize;
    int	    envMaxSize;
    char*   envPtr;
} EnvDesc;

#endif

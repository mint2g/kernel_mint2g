/*
 ****************************************************************
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
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *
 ****************************************************************
 */

#ifndef ASMARM_NKSMP_H
#define ASMARM_NKSMP_H

#include <asm/nkern.h>

struct cpumask;

#define hard_smp_processor_id()	(VCPU()->vcpuid)

#endif

/*
 ****************************************************************
 *
 *  Component: VLX space allocator
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
 *  #ident  "@(#)nkspc.c 1.1     07/10/18 Red Bend"
 *
 *  Contributor(s):
 *    Guennadi Maslov (guennadi.maslov@redbend.com)
 *    Vladimir Grouzdev (vladimir.grouzdev@redbend.com)
 *    Eric Lescouet (eric.lescouet@redbend.com)
 *
 ****************************************************************
 */

#include <nk/nkern.h>

#define	VCHUNK(d,sz)	((NkSpcChunk_##sz*)((d)->chunk))

/*
 * round up value to next 2^n boundary
 */
#define ROUND_UP(sz)						\
    static inline nku##sz##_f					\
round_up_##sz(const nku##sz##_f val, const nku##sz##_f pow2)	\
{								\
    return((val + (pow2 - 1)) & ~(pow2 - 1));			\
}

ROUND_UP(32)
ROUND_UP(64)

     /*
      * Initialize descriptor to empty (non-existent) space
      */
#define SPC_INIT(sz)				\
    void					\
nk_spc_init_##sz(NkSpcDesc_##sz* d)		\
{						\
    NkSpcChunk_##sz* vchunk = VCHUNK(d,sz);	\
						\
    d->numChunks    = 1;			\
    vchunk[0].start = 0;			\
    vchunk[0].size  = 0;			\
    vchunk[0].tag   = NK_SPC_NONEXISTENT;	\
}

SPC_INIT(32)
SPC_INIT(64)

/*
 *  remove chunk number n from the space descriptor
 */
#define DELETE_CHUNK(sz)				\
    static void					\
delete_chunk_##sz(NkSpcDesc_##sz* d, int n)		\
{						\
    NkSpcChunk_##sz*	vchunk = VCHUNK(d,sz);	\
    int			i;			\
						\
    d->numChunks -= 1;				\
    for (i = n ; i < d->numChunks ; i++) {	\
	vchunk[i].start = vchunk[i+1].start;	\
	vchunk[i].size  = vchunk[i+1].size;	\
	vchunk[i].tag   = vchunk[i+1].tag;	\
    }						\
}

DELETE_CHUNK(32)
DELETE_CHUNK(64)

/*
 *  Insert a chunk into the space descriptor at the n-th position.
 */
#define INSERT_CHUNK(sz)				\
    static void					\
insert_chunk_##sz (NkSpcDesc_##sz*	d,		\
	       int		n,		\
	       nku##sz##_f	lo,		\
	       nku##sz##_f	hi,		\
	       NkSpcTag		tag)		\
{						\
    NkSpcChunk_##sz* vchunk = VCHUNK(d,sz);	\
    int         i;				\
						\
    for (i = d->numChunks ; i > n ; i--) {	\
        vchunk[i].start = vchunk[i-1].start;	\
	vchunk[i].size  = vchunk[i-1].size;	\
	vchunk[i].tag   = vchunk[i-1].tag;	\
    }						\
    d->numChunks   += 1;			\
    vchunk[n].start = lo;			\
    vchunk[n].size  = hi - lo + 1;		\
    vchunk[n].tag   = tag;			\
}

INSERT_CHUNK(32)
INSERT_CHUNK(64)

/*
 *  Find a chunk intersection with a memory range.
 */
#define FIND_SECT(sz)				\
    static void					\
findSect_##sz (NkSpcDesc_##sz*	d,		\
	       int		i,		\
	       nku##sz##_f	memLo,		\
	       nku##sz##_f	memHi,		\
	       nku##sz##_f*	sectLo,		\
	       nku##sz##_f*	sectHi)		\
{						\
    NkSpcChunk_##sz*	vchunk = VCHUNK(d,sz);	\
    nku##sz##_f		lo;			\
    nku##sz##_f		hi;			\
						\
    lo = vchunk[i].start;			\
    hi = lo + vchunk[i].size - 1;		\
						\
    if (memLo > lo) lo = memLo;			\
    if (memHi < hi) hi = memHi;			\
						\
    *sectLo = lo;				\
    *sectHi = hi;				\
}

FIND_SECT(32)
FIND_SECT(64)

/*
 *  Put an intersection into the memory descriptor.
 *  (returns inserted chunk index).
 */
#define PUT_SECT(sz)				\
    static int					\
putSect_##sz (NkSpcDesc_##sz*	d,		\
	      int		i,		\
	      nku##sz##_f	sectLo,		\
	      nku##sz##_f	sectHi,		\
	      NkSpcTag		tag)		\
{						\
    NkSpcChunk_##sz*	vchunk = VCHUNK(d,sz);	\
    nku##sz##_f		chunkLo;		\
    nku##sz##_f		chunkHi;		\
    NkSpcTag		chunkFl;		\
    /*						\
     * remove current chunk from the memory	\
     * descriptor				\
     */						\
    chunkLo = vchunk[i].start;			\
    chunkHi = chunkLo + vchunk[i].size - 1;	\
    chunkFl = vchunk[i].tag;			\
    delete_chunk_##sz(d, i);			\
    /*						\
     * if remaining left part is not empty,	\
     * put it back to the memory descriptor,	\
     * (left merge is impossible);		\
     * otherwise if left merge is possible,	\
     * remove the left chunk and left expand	\
     * the intersection.			\
     */							\
    if (sectLo != chunkLo) {				\
	insert_chunk_##sz(d, i, chunkLo, sectLo-1, chunkFl);\
	i += 1;						\
    } else if (i > 0 && (tag == vchunk[i-1].tag)) {	\
	i -= 1;						\
	sectLo = vchunk[i].start;			\
	delete_chunk_##sz(d, i);			\
    }						\
    /*						\
     * if remaining right part is not empty put	\
     * it back to the memory descriptor,	\
     * (right merge is impossible);		\
     * otherwise if right merge is possible,	\
     * remove the right chunk and right expand	\
     * the intersection.			\
     */						\
    if (sectHi != chunkHi) {					\
	insert_chunk_##sz(d, i, sectHi+1, chunkHi, chunkFl);	\
    } else if (i < d->numChunks && (tag == vchunk[i].tag)) {	\
	sectHi = vchunk[i].start + vchunk[i].size - 1;		\
	delete_chunk_##sz(d, i);				\
    }						\
    /*						\
     * finally insert the intersection		\
     */						\
    insert_chunk_##sz(d, i, sectLo, sectHi, tag);\
    return i;					\
}

PUT_SECT(32)
PUT_SECT(64)

/*
 *  Allocate a chunk of physical memory in the desired memory range
 *  (returns non-zero if allocation fails)
 */
#define CALLOC_ANY(sz)				\
    static int					\
callocAny_##sz(NkSpcDesc_##sz*	d,		\
	       nku##sz##_f*	addr,		\
	       nku##sz##_f	size,		\
	       NkSpcTag		tag,		\
	       nku##sz##_f	bnd,		\
	       nku##sz##_f	memLo,		\
	       nku##sz##_f	memHi)		\
{						\
    NkSpcChunk_##sz*	vchunk = VCHUNK(d,sz);	\
    int			i;			\
    nku##sz##_f		lo;			\
    nku##sz##_f		hi;			\
    nku##sz##_f		l;			\
    nku##sz##_f		h;			\
						\
    for (i = 0 ; i < d->numChunks ; i++) {	\
	if (vchunk[i].tag != NK_SPC_FREE) {	\
	    continue;				\
	}					\
	    /*						\
	     * find an intersection [lo,hi] with a	\
	     * current chunk				\
	     */						\
	findSect_##sz(d, i, memLo, memHi, &lo, &hi);	\
	    /*						\
	     * calculate a segment [l,h] of given size	\
	     */						\
	l = round_up_##sz(lo, bnd);			\
	h = l + size - 1;				\
	    /*						\
	     * if the intersection [lo,hi] is not empty	\
	     * and [l,h] fits in [lo,hi]		\
	     */						\
	if (lo <= l && l <= h && h <= hi) {		\
	        /*					\
		 * put the allocated part of the	\
		 * intersection into the memory		\
		 * descriptor				\
		 */					\
	    i = putSect_##sz(d, i, l, h, tag);		\
	        /*					\
		 * successful allocation: the allocated	\
		 * chunk starts at lo			\
		 */					\
	    *addr = lo;					\
	    return 0;					\
	}						\
    }							\
        /*			\
	 * allocation fails	\
	 */			\
    return 1;			\
}

CALLOC_ANY(32)
CALLOC_ANY(64)

/*
 *  Tag the desired address range.
 */
#define SPC_TAG(sz)						\
    void							\
nk_spc_tag_##sz(NkSpcDesc_##sz*	d,				\
		nku##sz##_f	paddr,				\
		nku##sz##_f	psize,				\
		NkSpcTag	tag)				\
{								\
    NkSpcChunk_##sz*	vchunk = VCHUNK(d,sz);			\
    int			i;					\
    nku##sz##_f		lo;					\
    nku##sz##_f		hi;					\
								\
    for (i = 0 ; i < d->numChunks ; i++) {			\
            /*							\
	     * don't re-tag allocated chunks			\
	     */							\
	if (NK_SPC_STATE(vchunk[i].tag) == NK_SPC_ALLOCATED) {	\
	    continue;						\
	}							\
	if (NK_SPC_STATE(vchunk[i].tag) == NK_SPC_RESERVED) {	\
	    continue;						\
	}							\
	    /*							\
	     * skip chunks with the same tag			\
	     */							\
	if (vchunk[i].tag == tag) {				\
	    continue;						\
	}							\
            /*							\
	     * find an intersection [lo, hi] with the current chunk\
	     */							\
        findSect_##sz(d, i, paddr, paddr + psize - 1, &lo, &hi);\
	    /*							\
	     * if the intersection [lo, hi] is not empty	\
	     */							\
	if (lo <= hi) {						\
	        /*						\
	         * re-tag the intersection			\
	         */						\
	    i = putSect_##sz(d, i, lo, hi, tag);		\
	}							\
    }								\
}

SPC_TAG(32)
SPC_TAG(64)

/*
 * As different from tagging, here verify that the
 * memory range exists and is allocated.
 */
#define SPC_FREE(sz)				\
    void					\
nk_spc_free_##sz (NkSpcDesc_##sz* d,		\
		  nku##sz##_f	  paddr,	\
		  nku##sz##_f	  psize)	\
{						\
    int		i;				\
    nku##sz##_f	lo, hi;				\
						\
    for (i = 0; i < d->numChunks; i++) {	\
	    /*							\
	     * find an intersection [lo,hi] with a current chunk\
	     */							\
	findSect_##sz(d, i, paddr, paddr + psize - 1, &lo, &hi);\
	    /*							\
	     * if the intersection [lo,hi] is not empty	\
	     */						\
	if (lo <= hi) {					\
	        /*					\
		 * tag the intersection as free		\
		 */					\
	    i = putSect_##sz(d, i, lo, hi, NK_SPC_FREE);\
	}						\
    }							\
}

SPC_FREE(32)
SPC_FREE(64)

#define ALLOC_ANY(sz)				\
    int						\
nk_spc_alloc_any_##sz (NkSpcDesc_##sz*	d,	\
		       nku##sz##_f*	paddr,	\
		       nku##sz##_f	psize,	\
		       nku##sz##_f	align,	\
		       NkSpcTag		owner)	\
{						\
    int err = callocAny_##sz(d, paddr, psize,				\
			     NK_SPC_TAG(NK_SPC_ALLOCATED, owner),	\
			     align, 0, -1);				\
    return !err;				\
}

ALLOC_ANY(32)
ALLOC_ANY(64)

#define ALLOC_WITHIN_RANGE(sz)				\
    int							\
nk_spc_alloc_within_range_##sz (NkSpcDesc_##sz* d,	\
				nku##sz##_f*	paddr,	\
				nku##sz##_f	psize,	\
				nku##sz##_f	align,	\
				nku##sz##_f	lo,	\
				nku##sz##_f	hi,	\
				NkSpcTag	owner)	\
{							\
    int err = callocAny_##sz(d, paddr, psize,				\
			     NK_SPC_TAG(NK_SPC_ALLOCATED, owner),	\
			     align, lo, hi);				\
    return !err;							\
}

ALLOC_WITHIN_RANGE(32)
ALLOC_WITHIN_RANGE(64)

#define SPC_ALLOC(sz)				\
    int 					\
nk_spc_alloc_##sz (NkSpcDesc_##sz*	d,	\
		   nku##sz##_f		paddr,	\
		   nku##sz##_f		psize,	\
		   NkSpcTag		owner)	\
{						\
    nku##sz##_f addr;				\
    int         err;				\
								\
    err = callocAny_##sz(d, &addr, psize,			\
			 NK_SPC_TAG(NK_SPC_ALLOCATED, owner),	\
			 1, paddr, paddr + psize - 1);		\
    return !err;						\
}

SPC_ALLOC(32)
SPC_ALLOC(64)

/*
 *  Free all memory of the given tag.
 */
#define SPC_RELEASE(sz)					\
    void						\
nk_spc_release_##sz (NkSpcDesc_##sz* d, NkSpcTag tag)	\
{							\
    NkSpcChunk_##sz*	vchunk = VCHUNK(d,sz);		\
    int         	i;				\
							\
    for (i = 0; i < d->numChunks; i++) {				\
        if ((NK_SPC_OWNER(vchunk[i].tag) == NK_SPC_OWNER(tag)) &&	\
	    (NK_SPC_STATE(vchunk[i].tag) & NK_SPC_STATE(tag))) {	\
	    nk_spc_free_##sz(d, vchunk[i].start, vchunk[i].size);	\
	    i = 0;							\
	}								\
    }									\
}

SPC_RELEASE(32)
SPC_RELEASE(64)

/*
 *  Dump memory layout.
 */
    static char*
nk_spc_state (NkSpcTag tag)
{
    char* c;
    switch (NK_SPC_STATE(tag)) {
	case NK_SPC_FREE:        c = "F    "; break;
	case NK_SPC_ALLOCATED:   c = " A   "; break;
	case NK_SPC_RESERVED:    c = "   R "; break;
	case NK_SPC_NONEXISTENT: c = "    N"; break;
	default:                 c = "     "; break;
    }
    return c;
}

    static char
nk_spc_owner (NkSpcTag tag)
{
    return ('0' + NK_SPC_OWNER(tag));
}

#define SPC_DUMP(sz)				\
    void					\
nk_spc_dump_##sz (NkSpcDesc_##sz* d)		\
{						\
    NkSpcChunk_##sz* vchunk = VCHUNK(d,sz);	\
    int              i;				\
						\
    printnk("#chunk: %d\n", d->numChunks);			\
    for (i = 0; i < d->numChunks; i++) {			\
	printnk("%c:  0x%08x  0x%08x  %s  [0x%08x]\n",		\
	        nk_spc_owner(vchunk[i].tag), vchunk[i].start,	\
	        vchunk[i].size, nk_spc_state(vchunk[i].tag),	\
	        vchunk[i].tag);		     			\
    }								\
}

SPC_DUMP(32)
SPC_DUMP(64)

#define SPC_SIZE(sz)				\
    nku##sz##_f					\
nk_spc_size_##sz (NkSpcDesc_##sz* d)		\
{						\
    NkSpcChunk_##sz*	vchunk = VCHUNK(d,sz);	\
    nku##sz##_f		size   = 0;		\
    int			i;			\
						\
    for (i = 0 ; i < d->numChunks ; i++) {				\
        if (NK_SPC_STATE(vchunk[i].tag) != NK_SPC_NONEXISTENT) {	\
	    nku##sz##_f limit = vchunk[i].start + vchunk[i].size;	\
									\
	    if (limit > size) {						\
		size = limit;						\
	    }								\
	}								\
    }									\
    return size;							\
}

SPC_SIZE(32)
SPC_SIZE(64)


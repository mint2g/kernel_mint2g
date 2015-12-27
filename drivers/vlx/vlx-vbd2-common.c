/*
 ****************************************************************
 *
 *  Component: VLX Virtual Block Device v.2 common code
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
 *    Adam Mirowski (adam.mirowski@redbend.com)
 *
 ****************************************************************
 */

#ifdef VBD_LINK_MAX_SEGS_PER_REQ

    static unsigned __init
vbd_get_msb_pos (unsigned long value)
{
    unsigned bitnb;

    for (bitnb = sizeof (value) * 8; bitnb;) {
	--bitnb;
	if (value & (1 << bitnb)) return bitnb;
    }
    return 0;
}

    /*
     * Each link can have a custom communications configuration,
     * depending on the vlink parameter indicated by the "start"
     * string. To store the resulting various vmq_xx_config_t we
     * use the private data of the vmq_link_t (vbd_link_t), which
     * must therefore be allocated here.
     * All the other initializations of vbd_link_t are however
     * performed later, by vbd_link_init(), to not overload this
     * function.
     */

    static const vmq_xx_config_t* __init
vbd_cb_get_xx_config (vmq_link_t* link2, const char* start)
{
    long msg_count = VBD_LINK_DEFAULT_MSG_COUNT;
    long segs_per_req_max = VBD_LINK_MAX_SEGS_PER_REQ;
    vbd_link_t* vbd = (vbd_link_t*) kzalloc (sizeof *vbd, GFP_KERNEL);
    _Bool double_buffering = 0;

    VBD_ASSERT (!VBD_LINK (link2));
    if (!vbd) {
	ETRACE ("out of memory for link\n");
	return NULL;
    }
    VBD_LINK (link2) = vbd;
	/*
	 * vdev=(vbd2,<linkid>|[<elems>][,[<segs_per_req_max>][,[db][,be]]])
	 */

	/*
	 * Backend  gets start == rx_s_info.
	 * Frontend gets start == tx_s_info.
	 * So the frontend does not see the ",be" component
	 * in the "start" configuration parameter.
	 * Instead, it must look into vmq_link_rx_s_info(link)
	 * string to find it.
	 */
    if (!start || start == vmq_link_tx_s_info (link2)) {
	const char* rx_s_info = vmq_link_rx_s_info (link2);

	DTRACE ("frontend start %p rx_s_info %p (%s)\n", start, rx_s_info,
		rx_s_info ? rx_s_info : "NULL");
	if (rx_s_info && strstr (rx_s_info, ",be")) {
	    return VMQ_XX_CONFIG_IGNORE_VLINK;
	}
    }
    if (start) {
	DTRACE ("Using params '%s'\n", start);
	if (*start && *start != ',') {
	    char* end;

	    msg_count = simple_strtoul (start, &end, 0);
	    if (end == start)
		return (vmq_xx_config_t*) vbd_vlink_syntax (end);
	    start = end;
	    DTRACE ("original msg_count %ld\n", msg_count);
	    if (msg_count > 0) {
		    /*
		     * Round down to highest power of 2.
		     * nkops.nk_mask2bit() scans from LSB,
		     * so it is not suitable here.
		     */
		msg_count = 1UL << vbd_get_msb_pos (msg_count);
	    } else {
		msg_count = VBD_LINK_DEFAULT_MSG_COUNT;
	    }
	}
	if (*start == ',') {
	    ++start;
	    if (*start && *start != ',') {
		char* end;

		segs_per_req_max = simple_strtoul (start, &end, 0);
		if (end == start)
		    return (vmq_xx_config_t*) vbd_vlink_syntax (end);
		start = end;
		DTRACE ("original segs_per_req_max %ld\n", segs_per_req_max);
		if (segs_per_req_max <= 0 ||
		    segs_per_req_max > VBD_LINK_MAX_SEGS_PER_REQ) {
		    segs_per_req_max = VBD_LINK_MAX_SEGS_PER_REQ;
		}
	    }
	}
	if (*start == ',') {
	    ++start;
	    if (start[0] == 'd' && start[1] == 'b') {
		start += 2;
		double_buffering = 1;
		DTRACE ("double_buffering on\n");
	    }
	}
	if (*start == ',') {
	    ++start;
	    if (start[0] == 'b' && start[1] == 'e') {
		start += 2;
		DTRACE ("backend side of vlink\n");
	    }
	}
	if (*start) {
	    return (vmq_xx_config_t*) vbd_vlink_syntax (start);
	}
    }
    DTRACE ("final msg_count %ld segs_per_req_max %ld\n",
	    msg_count, segs_per_req_max);
    vbd->segs_per_req_max    = segs_per_req_max;
    vbd->xx_config.msg_count = msg_count;
    vbd->xx_config.msg_max   = sizeof (vbd2_req_header_t) +
			       sizeof (vbd2_buffer_t) * segs_per_req_max;
    if (vbd->xx_config.msg_max < sizeof (vbd2_probe_link_t)) {
	DTRACE ("msg_max too small for probing (%d), upped to %d\n",
		vbd->xx_config.msg_max, sizeof (vbd2_probe_link_t));
	vbd->xx_config.msg_max = sizeof (vbd2_probe_link_t);
    }
    if (double_buffering) {
	vbd->xx_config.data_count = msg_count;
	vbd->xx_config.data_max   = PAGE_SIZE * segs_per_req_max;
    }
    return &vbd->xx_config;
}

#endif

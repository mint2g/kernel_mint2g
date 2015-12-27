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
 *    Gilles Maigne (gilles.maigne@redbend.com)
 *
 ****************************************************************
 */

/*! \file vuart.h
 *  \brief Provide virtual UART driver interface
 */ 

#ifndef VUART_H
#define VUART_H 

#ifdef NU_OSWARE
#include <osware/osware.h>
#endif

/*
 * forward declaration for type
 */
typedef struct NkDevVuart NkDevVuart;


/*! 
 * This define the vuart type
 */
typedef NkDevVuart*	NkVuart;


/*! 
 * Here are the virtual UART events.
 */
typedef enum {

	/*! 
	 * TX_READY is sent when alloc failed and a slot have been freed so 
	 * transmission can be restarted.
	 */
    NK_VUART_TX_READY			= 0x0,

	/*!
	 * RX_READY indicates a buffer has been received
	 */
    NK_VUART_RX_READY			= 0x1,

	/*!
	 * Indicates that remote site has opened the device
	 */
    NK_VUART_PEER_OPEN			= 0x2,

	/*!
	 * Indicates that remote site has closed the device
	 */
    NK_VUART_PEER_CLOSE			= 0x3,

	/*!
	 * Indicates that modem signal has changed
	 */
    NK_VUART_MODEM_EVENT		= 0x4,

	/*!
	 * Indicates that remote site has closed the device
	 */
    NK_VUART_BREAK_EVENT		= 0x5,

	/*!
	 * Indicates that remote site has closed the device
	 */
    NK_VUART_CONFIG_EVENT		= 0x6,

	/*!
	 * Indicate internal configuration
	 */
    NK_VUART_INTERNAL_EVENT		= 0x7,


} NkVuartEvent;


#define     NK_VUART_EVENT_MAX			0x8


typedef enum {
    NK_VUART_SIGNAL_CTS = 0x1,
    NK_VUART_SIGNAL_DCD = 0x2,
    NK_VUART_SIGNAL_DSR = 0x4,
    NK_VUART_SIGNAL_DTR = 0x8,
    NK_VUART_SIGNAL_RTS = 0x10
} NkVuartModemSignal;


    /*!
     * This structure define the buffer descriptors for both reception and
     * emission.
     */
typedef struct {
	/*!
	 * The size of each ring descriptor
	 */
    nku16_f	bufSize;

	/*!
	 * This is the number of ring descriptor for the emission buffer
	 * It should be a power of two.
	 */
    nku8_f	emitRingCnt;
    
} NkVuartConfig;


/*!
 * This is a pointer to a ring descriptor containing information on emitted
 * data.
 */
typedef struct {
    void*	descaddr;
} NkVuartBufDesc;


/*!
 * Call-back function prototype for virtual UART event .
 * The 'NkVuartEventHandler' is called by the virtual UART interrupt handler 
 * on occurrence of an 'NkVuartEvent' event. 
 *
 * So an event handler should not use API which are not allowed by the 
 * underlying operating system within interrupt handlers. 
 *
 * Note The virtual UART software protect an event handler against re-entrance.
 * - The first parameter (vuart) is the UART identifier
 * - The second parameter (cookie) is an opaque data. This opaque has been 
 *   provided when the event has been subscribed 
 *   (see the vuart_event_subscribe routine).
 * - The third parameter is an additional parameter used to transmit an event
 *  related information.
 */
typedef void (*NkVuartEventHandler) (NkVuart	vuart, 
				     void*	cookie, long param);


/*!
 * NkVuartParam describes configuration parameter of an uart :
 *       - baud rate (bits [ 0 - 19  ])
 *       - parity    (bits [ 20 - 21 ])
 *       - number of stop bits (bits [ 28 - 30 ] )
 *       - number of bits in a character (bits [ 24 - 28 ])
 */

typedef long NkVuartParam;
#define NK_VUART_PARITY_NONE 0
#define NK_VUART_PARITY_EVEN 1
#define NK_VUART_PARITY_ODD  2

#define NK_VUART_STOPBIT_1
#define NK_VUART_STOPBIT_2

#define vuart_param_get_parity(p)	( ((p) >> 20) & 0x3 )
#define vuart_param_get_bitnum(p)	( ((p) >> 24) & 0xf )
#define vuart_param_get_stop(p)		( ((p) >> 28) & 0x3 )
#define vuart_param_get_baudrate(p)	( (p) & 0xfffff )

#define vuart_param_init(p, speed, parity, stop, bitnum) \
    (p) = ((speed) | ((parity) << 20) | ((bitnum) << 24) | ((stop) << 28))


    /*!
     * This routine creates an UART device endpoint.
     * To be operationnal, the two operating system involved in 
     * communication must create their own uart end point.
     *
     * \param name : specifies a name associated to the UART.
     * \param config : The parameter config specifies the size and 
     *                 the number of buffer used for transmission and 
     *                 parameter for buffer descriptor.
     * \return In case of success this routines returns an handle 
     *	on the created uart. Otherwise 0 is returned in case of failure.
     */
NkVuart		vuart_create(char* name, 
			     NkVuartConfig* config); 


    /*!
     * This routine deletes the local endpoint of an UART device. 
     * To be completely deleted the two endpoint must be deleted : 
     * To do so vuart_delete must be invoked by the two operating 
     * system involved in a communication.
     * 
     * \param vuart : specifies the handle to the vuart end point.
     * \return In case of success this routines returns 0
     *	Otherwise otherwise a negative eror code is returned.
     */
int		vuart_delete(NkVuart vuart);


    /*!
     *  open the virtual device
     *      This routines does nothing except to tell the remote site 
     *      that the device is ready to be used.
     * \param vuart : the virtual UART virtual address
     * \return 0 : at the moment this routine is always successful.
     */

int		vuart_open(NkVuart uartid);

    /*!
     * This routine allocates a transmission buffer. The allocated buffer 
     * should be used for further transmission. 
     * If the allocation request can not be fulfilled  the caller must wait 
     * for the release of transmission buffers to retry the buffer allocation. 
     * The release of transmission buffer is notified by the NK_VUART_TX_READY 
     * event.
     * Notes :
     *
     * - This mechanism implicitly implements flow control. If flow control is 
     *   not desired (for application which performs logging) a new memory 
     *   allocation routine should be added to the API, this allocation routines 
     *   should always perform successful memory allocation.
     * - The rationale for this API is that flow control is not implementable 
     *   with RTS/CTS signals. Mainly because unlike real UART the two endpoint 
     *   are not running concurrently, so a primary OS can overrun the FIFO 
     *   without letting a chance to the secondary OS to stop the transmission
     *
     *
     * \return In case of success, the routine returns 0 and initializes 
     *   the buffer descriptor bufdesc. The buffer descriptor bufdesc is 
     *   an opaque type . A set of access routines (described below) 
     *    allows to get the content of buffer descriptor.
     *  \return In case of failure this routine returns negative error code.
     */
int		vuart_alloc(NkVuart uartid, NkVuartBufDesc* d);

 


   /*!
     * transmit a buffer previously allocated by vuart_alloc
     *
     * \param vuart  :  pointer to the virtual uart 
     * \param bufdesc :  points to a valid buffer descriptor 
     * \param size    :  contains the effective size of the transmitted data.
     *
     * \return 0 in case of success and a negative error code in case of 
     *		 error.
     */
int		vuart_transmit(NkVuart uartid, NkVuartBufDesc* d, int size);



    /*!
     * This routine polls data from the virtual UART device . 
     * Once, the incoming buffer has been processed, it should be freed 
     * by calling the vuart_free routine.
     *
     * This routine is non blocking so if nothing is ready to be 
     * polled it returns a negative error code. In addition this routine
     * can be called from an event handler.
     *
     * Vuart_poll does not extract the buffer from the FIFO. 
     * So while the buffer is not freed, vuart_poll will return the 
     * same buf descriptor.
     *
     * \param vuart : the virtual uart device handle
     * \param bufdesc : is a pointer to a buffer descriptor 
     *                 (by reference argument). 
     * \return  In case of success the buffer descriptor is initialized
     *          and the effective size of the buffer is returned.
     *          In case of failure a negative error code is returned
     */

int		vuart_poll(NkVuart uartid, NkVuartBufDesc* bufdesc);


    /*!
     * This routine receives data from the virtual UART device . 
     * Once, the incoming buffer has been processed, it should be freed 
     * by calling the vuart_free routine.
     *
     * This routine is non blocking so if nothing is ready to be 
     * received it returns a negative error code. In addition this routine
     * can be called from an event handler.
     *
     * Vuart_receive extract the buffer from the FIFO. 
     *
     * \param vuart : the virtual uart device handle
     * \param bufdesc : is a pointer to a buffer descriptor 
     *                 (by reference argument). 
     * \return  In case of success the buffer descriptor is initialized
     *          and the effective size of the buffer is returned.
     *          In case of failure a negative error code is returned
     */

int		vuart_receive(NkVuart uartid, NkVuartBufDesc* bufdesc);


    /*!
     * free the buffer associated to the desc. 
     * The descriptor must have been initialized by a previous call 
     * to the vuart_receive routine.
     * \param vuart : The virtual uart device handle
     * \param bufdesc : A pointer to a buffer descriptor previously initialized
     *                  by the vuart_receive.
     * \return : 0 in case of success or a negative value if an error occurred.
     */
int		vuart_free(NkVuart vuart, NkVuartBufDesc* desc);


    /*!
     * This function closes the virtual UART device and notify the remote 
     * site of the close. Note that after a call to close event handler 
     * are still connected. 
     * Note that the last close causes the freeing of all transmitted and 
     * received message (but without any notification).
     * \param	vuart : The virtual UART handle.
     * \return  0 if the call is successful or a negative error code 
     *            in case of failure.
     */
int		vuart_close(NkVuart uartid);


    /*!
     * This function requests to be notified in the occurrence of one of 
     * the virtual UART event (described above). 
     * 
     * \param event : specifies the event the caller is interested in.
     *
     * \param hdl   : It is the routine invoked when the event event occurs. 
     *         This handler will be further described later. If hdl is NULL
     *         then the event is unsubscribed. 
     *
     * \param cookie : It is an opaque data which is provided to the event 
     *         handler when this one is invoked. Typically this parameter 
     *         can contain a pointer to a data structure of the application.
     * returns 0 in case of success
     * otherwise returns a negative error code.
     */
int		vuart_event_subscribe(NkVuart vuart, NkVuartEvent event, 
				      NkVuartEventHandler hdl, void* cookie);



    /*!
     * This routine is used to get a pointer to the buffer 
     *	      associated to a buffer descriptor and the real size of the buffer
     * \param desc : the buffer descriptor
     * \param psize : a pointer where is stored the size of the buffer.
     * \return A pointer to the transmitted/received buffer.
     */

char*		vuart_buffer(NkVuartBufDesc* d, int* size);



    /*!
     * This routine attach a site to a virtual UART device. 
     * This routines is used in conjunction of nkops.nk_dev_lookup_by_type
     * to get the virtual address of a device.
     * \param addr : the physical address of the uart
     * \param name : return the name of the device
     * \param remoteSite : return the id of the remote site
     * \return 0 in case of failure otherwise an handle on the 
     *	       virtual UART .
     */
NkVuart		vuart_attach(NkPhAddr addr, char** name, NkOsId* remoteSite);


    /*!
     *  Return the name of virtual UART device.
     * \param vuart : the virtual uart handle
     * \return the device name.
     */
char*		vuart_name(NkVuart vuart);



    /*!
     * This routine set a break event. It results in the sending of 
     *       NK_VUART_BREAK_EVENT to the remote site.
     * \param vuart : The virtual uart device handle
     * \return	0 in case of success 
     *		or a negative error code in case of failure.
     */
int		vuart_set_break(NkVuart vuart);



    /*!
     * This routine clear a break event. If a break signal has been set, it 
     *     results in the sending of 
     *     NK_VUART_BREAK_EVENT to the remote site.
     * 
     * \param vuart : The virtual uart device handle
     * \return 0 in case of success or 
     *		 a negative error code in case of failure.
     */
int		vuart_clear_break(NkVuart vuart);


    /*!
     * Set UART signal. Only CTS/RTS/DCD causes a remote event 
     * generation. 
     * \param vuart : The virtual uart device handle
     * \param f     : new signal parameter.
     * \return  0 always successful but locking is wrong at the moment.
     */
int		vuart_set_ctrl(NkVuart vuart, NkVuartModemSignal f);


    /*!
     * This routine returns the value of UART modem signals.
     * \param vuart : the virtual uart
     * \param sig   : point to an integer which will contain signals
     * \return  0  :
     *		always successful but locking is wrong at the moment.
     */
int		vuart_get_ctrl(NkVuart vuart, NkVuartModemSignal* f);

#endif


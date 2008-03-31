/*
 * Copyright (C) 2008 Google, Inc.
 *
 * Based on, but no longer compatible with, the original
 * OpenBinder.org binder driver interface, which is:
 *
 * Copyright (c) 2005 Palmsource, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_BINDER_H
#define _LINUX_BINDER_H

#define B_PACK_CHARS(c1, c2, c3, c4) \
	((((c1)<<24)) | (((c2)<<16)) | (((c3)<<8)) | (c4))
#define B_TYPE_LARGE 0x85

enum {
	BINDER_TYPE_BINDER	= B_PACK_CHARS('s', 'b', '*', B_TYPE_LARGE),
	BINDER_TYPE_WEAK_BINDER	= B_PACK_CHARS('w', 'b', '*', B_TYPE_LARGE),
	BINDER_TYPE_HANDLE	= B_PACK_CHARS('s', 'h', '*', B_TYPE_LARGE),
	BINDER_TYPE_WEAK_HANDLE	= B_PACK_CHARS('w', 'h', '*', B_TYPE_LARGE),
	BINDER_TYPE_FD		= B_PACK_CHARS('f', 'd', '*', B_TYPE_LARGE),
};

enum {
	FLAT_BINDER_FLAG_PRIORITY_MASK = 0xff,
	FLAT_BINDER_FLAG_ACCEPTS_FDS = 0x100,
};

/*
 * This is the flattened representation of a Binder object for transfer
 * between processes.  The 'offsets' supplied as part of a binder transaction
 * contains offsets into the data where these structures occur.  The Binder
 * driver takes care of re-writing the structure type and data as it moves
 * between processes.
 */
typedef struct flat_binder_object
{
	/* 8 bytes for large_flat_header. */
	unsigned long		type;
	unsigned long		flags;

	/* 8 bytes of data. */
	union {
		void		*binder;	/* local object */
		signed long	handle;		/* remote object */
	};

	/* extra data associated with local object */
	void			*cookie;
} flat_binder_object_t;

/*
 * On 64-bit platforms where user code may run in 32-bits the driver must
 * translate the buffer (and local binder) addresses apropriately.
 */

typedef struct binder_write_read {
	signed long	write_size;	/* bytes to write */
	signed long	write_consumed;	/* bytes consumed by driver (for ERESTARTSYS) */
	unsigned long	write_buffer;
	signed long	read_size;	/* bytes to read */
	signed long	read_consumed;	/* bytes consumed by driver (for ERESTARTSYS) */
	unsigned long	read_buffer;
} binder_write_read_t;

/* Use with BINDER_VERSION, driver fills in fields. */
typedef struct binder_version {
	/* driver protocol version -- increment with incompatible change */
	signed long	protocol_version;
} binder_version_t;

/* This is the current protocol version. */
#define BINDER_CURRENT_PROTOCOL_VERSION 6

#define BINDER_IOC_MAGIC 'b'
#define BINDER_WRITE_READ	_IOWR(BINDER_IOC_MAGIC, 1, binder_write_read_t)
#define	BINDER_SET_IDLE_TIMEOUT	_IOW(BINDER_IOC_MAGIC, 3, bigtime_t)
#define	BINDER_SET_MAX_THREADS	_IOW(BINDER_IOC_MAGIC, 5, size_t)
#define	BINDER_SET_IDLE_PRIORITY	_IOW(BINDER_IOC_MAGIC, 6, int)
#define	BINDER_SET_CONTEXT_MGR	_IOW(BINDER_IOC_MAGIC, 7, int)
#define	BINDER_THREAD_EXIT	_IOW(BINDER_IOC_MAGIC, 8, int)
#define BINDER_VERSION _IOWR(BINDER_IOC_MAGIC, 9, binder_version_t)
#define BINDER_IOC_MAXNR 9

/*
 * NOTE: Two special error codes you should check for when calling
 * in to the driver are:
 *
 * EINTR -- The operation has been interupted.  This should be
 * handled by retrying the ioctl() until a different error code
 * is returned.
 *
 * ECONNREFUSED -- The driver is no longer accepting operations
 * from your process.  That is, the process is being destroyed.
 * You should handle this by exiting from your process.  Note
 * that once this error code is returned, all further calls to
 * the driver from any thread will return this same code.
 */

typedef int64_t bigtime_t;

enum transaction_flags {
	TF_ONE_WAY	= 0x01,	/* this is a one-way call: asynchronous, with no return */
	TF_ROOT_OBJECT	= 0x04,	/* contents are the component's root object */
	TF_STATUS_CODE	= 0x08,	/* contents are a 32-bit status code */
	TF_ACCEPT_FDS	= 0x10,	/* allow replies with file descriptors */
};

typedef struct binder_transaction_data
{
	/* The first two are only used for bcTRANSACTION and brTRANSACTION,
	 * identifying the target and contents of the transaction.
	 */
	union {
		unsigned long	handle;	/* target descriptor of command transaction */
		void		*ptr;	/* target descriptor of return transaction */
	} target;
	void			*cookie;	/* target object cookie */
	unsigned int		code;		/* transaction command */

	/* General information about the transaction. */
	unsigned int	flags;
	pid_t		sender_pid;
	uid_t		sender_euid;
	size_t		data_size;	/* number of bytes of data */
	size_t		offsets_size;	/* number of bytes of flat_binder_object offsets */

	/* If this transaction is inline, the data immediately
	 * follows here; otherwise, it ends with a pointer to
	 * the data buffer.
	 */
	union {
		struct {
			const void	*buffer;	/* transaction data */
			const void	*offsets;	/* offsets to flat_binder_object structs */
		} ptr;
		uint8_t	buf[8];
	} data;
} binder_transaction_data_t;

typedef struct binder_wakeup_time
{
	bigtime_t time;
	int priority;
} binder_wakeup_time_t;

enum BinderDriverReturnProtocol {
	brERROR = -1,
	/*
	 * int: error code
	 */

	brOK = 0,
	/* No parameters! */

	brTRANSACTION = 3,
	brREPLY = 4,
	/*
	 * binder_transaction_data: the received command.
	 */

	brACQUIRE_RESULT = 5,
	/*
	 * not currently supported
	 * int: 0 if the last bcATTEMPT_ACQUIRE was not successful.
	 * Else the remote object has acquired a primary reference.
	 */

	brDEAD_REPLY = 6,
	/*
	 * The target of the last transaction (either a bcTRANSACTION or
	 * a bcATTEMPT_ACQUIRE) is no longer with us.  No parameters.
	 */

	brTRANSACTION_COMPLETE = 7,
	/*
	 * No parameters... always refers to the last transaction requested
	 * (including replies).  Note that this will be sent even for
	 * asynchronous transactions.
	 */

	brINCREFS = 8,
	brACQUIRE = 9,
	brRELEASE = 10,
	brDECREFS = 11,
	/*
	 * void *:	ptr to binder
	 * void *: cookie for binder
	 */

	brATTEMPT_ACQUIRE = 12,
	/*
	 * not currently supported
	 * int:	priority
	 * void *: ptr to binder
	 * void *: cookie for binder
	 */

	brNOOP = 14,
	/*
	 * No parameters.  Do nothing and examine the next command.  It exists
	 * primarily so that we can replace it with a brSPAWN_LOOPER command.
	 */

	brSPAWN_LOOPER = 15,
	/*
	 * No parameters.  The driver has determined that a process has no threads
	 * waiting to service incomming transactions.  When a process receives this
	 * command, it must spawn a new service thread and register it via
	 * bcENTER_LOOPER.
	 */

	brFINISHED = 16,
	/*
	 * not currently supported
	 * stop threadpool thread
	 */

	brDEAD_BINDER = 17,
	/*
	 * void *: cookie
	 */
	brCLEAR_DEATH_NOTIFICATION_DONE = 18,
	/*
	 * void *: cookie
	 */

	brFAILED_REPLY = 19,
	/*
	 * The the last transaction (either a bcTRANSACTION or
	 * a bcATTEMPT_ACQUIRE) failed (e.g. out of memory).  No parameters.
	 */
};

enum BinderDriverCommandProtocol {
	bcTRANSACTION = 1,
	bcREPLY = 2,
	/*
	 * binder_transaction_data: the sent command.
	 */

	bcACQUIRE_RESULT = 3,
	/*
	 * not currently supported
	 * int:  0 if the last brATTEMPT_ACQUIRE was not successful.
	 * Else you have acquired a primary reference on the object.
	 */

	bcFREE_BUFFER = 4,
	/*
	 * void *: ptr to transaction data received on a read
	 */

	bcINCREFS = 6,
	bcACQUIRE = 7,
	bcRELEASE = 8,
	bcDECREFS = 9,
	/*
	 * int:	descriptor
	 */

	bcINCREFS_DONE = 10,
	bcACQUIRE_DONE = 11,
	/*
	 * void *: ptr to binder
	 * void *: cookie for binder
	 */

	bcATTEMPT_ACQUIRE = 12,
	/*
	 * not currently supported
	 * int: priority
	 * int: descriptor
	 */

	bcREGISTER_LOOPER = 15,
	/*
	 * No parameters.
	 * Register a spawned looper thread with the device.  This must be
	 * called by the function that is supplied in bcSET_THREAD_ENTRY as
	 * part of its initialization with the binder.
	 */

	bcENTER_LOOPER = 16,
	bcEXIT_LOOPER = 17,
	/*
	 * No parameters.
	 * These two commands are sent as an application-level thread
	 * enters and exits the binder loop, respectively.  They are
	 * used so the binder can have an accurate count of the number
	 * of looping threads it has available.
	 */

	bcREQUEST_DEATH_NOTIFICATION = 21,
	/*
	 * void *: ptr to binder
	 * void *: cookie
	 */

	bcCLEAR_DEATH_NOTIFICATION = 22,
	/*
	 * void *: ptr to binder
	 * void *: cookie
	 */

	bcDEAD_BINDER_DONE = 23
	/*
	 * void *: cookie
	 */
};

#endif /* _LINUX_BINDER_H */


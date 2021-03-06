/*
 * brickd
 * Copyright (C) 2012-2013 Matthias Bolte <matthias@tinkerforge.com>
 *
 * usb_transfer.c: libusb transfer specific functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <libusb.h>

#include "usb_transfer.h"

#include "log.h"
#include "stack.h"
#include "usb.h"

#define LOG_CATEGORY LOG_CATEGORY_USB

static void LIBUSB_CALL usb_transfer_wrapper(struct libusb_transfer *handle) {
	USBTransfer *transfer = handle->user_data;

	if (!transfer->submitted) {
		log_error("%s transfer %p returned from %s, but was not submitted before",
		          usb_transfer_get_type_name(transfer->type, 1), transfer,
		          transfer->stack->base.name);

		return;
	}

	transfer->submitted = 0;
	transfer->completed = 1;

	if (handle->status == LIBUSB_TRANSFER_CANCELLED) {
		log_debug("%s transfer %p for %s was cancelled",
		          usb_transfer_get_type_name(transfer->type, 1), transfer,
		          transfer->stack->base.name);

		return;
	} else if (handle->status == LIBUSB_TRANSFER_NO_DEVICE) {
		log_debug("%s transfer %p for %s was aborted, device got disconnected",
		          usb_transfer_get_type_name(transfer->type, 1), transfer,
		          transfer->stack->base.name);

		return;
	} else if (handle->status == LIBUSB_TRANSFER_STALL) {
		log_debug("%s transfer %p for %s got stalled",
		          usb_transfer_get_type_name(transfer->type, 1), transfer,
		          transfer->stack->base.name);

		return;
	} else if (handle->status != LIBUSB_TRANSFER_COMPLETED) {
		log_warn("%s transfer %p returned with an error from %s: %s (%d)",
		         usb_transfer_get_type_name(transfer->type, 1), transfer,
		         transfer->stack->base.name,
		         get_libusb_transfer_status_name(transfer->handle->status),
		         transfer->handle->status);
	} else {
		log_debug("%s transfer %p returned successfully from %s",
		         usb_transfer_get_type_name(transfer->type, 1), transfer,
		         transfer->stack->base.name);

		if (transfer->function != NULL) {
			transfer->function(transfer);
		}
	}

	if (transfer->type == USB_TRANSFER_TYPE_READ) {
		usb_transfer_submit(transfer);
	}
}

const char *usb_transfer_get_type_name(USBTransferType type, int upper) {
	switch (type) {
	case USB_TRANSFER_TYPE_READ:
		return upper ? "Read" : "read";

	case USB_TRANSFER_TYPE_WRITE:
		return upper ? "Write" : "write";

	default:
		return upper ? "<Unknown>" : "<unknown>";
	}
}

int usb_transfer_create(USBTransfer *transfer, USBStack *stack,
                        USBTransferType type, USBTransferFunction function) {
	transfer->stack = stack;
	transfer->type = type;
	transfer->submitted = 0;
	transfer->completed = 0;
	transfer->function = function;
	transfer->handle = libusb_alloc_transfer(0);

	if (transfer->handle == NULL) {
		log_error("Could not allocate libusb %s transfer for %s",
		          usb_transfer_get_type_name(transfer->type, 0),
		          stack->base.name);

		return -1;
	}

	return 0;
}

void usb_transfer_destroy(USBTransfer *transfer) {
	struct timeval tv;
	time_t start;
	time_t now;
	int rc;

	log_debug("Destroying %s transfer %p for %s",
	          usb_transfer_get_type_name(transfer->type, 0), transfer,
	          transfer->stack->base.name);

	if (transfer->submitted) {
		transfer->completed = 0;

		rc = libusb_cancel_transfer(transfer->handle);

		if (rc < 0) {
			log_warn("Could not cancel pending %s transfer %p for %s: %s (%d)",
			         usb_transfer_get_type_name(transfer->type, 0), transfer,
			         transfer->stack->base.name,
			         get_libusb_error_name(rc), rc);
		} else {
			tv.tv_sec = 0;
			tv.tv_usec = 0;

			start = time(NULL);
			now = start;

			// FIXME: don't wait 1sec per transfer
			while (!transfer->completed && now >= start && now < start + 1) {
				rc = libusb_handle_events_timeout(transfer->stack->context, &tv);

				if (rc < 0) {
					log_error("Could not handle USB events: %s (%d)",
					          get_libusb_error_name(rc), rc);
				}

				now = time(NULL);
			}

			if (!transfer->completed) {
				log_warn("Attempt to cancel pending %s transfer %p for %s timed out",
				         usb_transfer_get_type_name(transfer->type, 0), transfer,
				         transfer->stack->base.name);
			}
		}
	}

	if (!transfer->submitted) {
		libusb_free_transfer(transfer->handle);
	} else {
		log_warn("Leaking pending %s transfer %p for %s",
		         usb_transfer_get_type_name(transfer->type, 0), transfer,
		         transfer->stack->base.name);
	}
}

int usb_transfer_submit(USBTransfer *transfer) {
	uint8_t end_point;
	int length;
	int rc;

	if (transfer->submitted) {
		log_error("%s transfer %p is already submitted for %s",
		          usb_transfer_get_type_name(transfer->type, 1), transfer,
		          transfer->stack->base.name);

		return -1;
	}

	switch (transfer->type) {
	case USB_TRANSFER_TYPE_READ:
		end_point = LIBUSB_ENDPOINT_IN + USB_ENDPOINT_IN;
		length = sizeof(Packet);

		break;

	case USB_TRANSFER_TYPE_WRITE:
		end_point = LIBUSB_ENDPOINT_OUT + USB_ENDPOINT_OUT;
		length = transfer->packet.header.length;

		break;

	default:
		log_error("Transfer for %s has invalid type",
		          transfer->stack->base.name);

		return -1;
	}

	transfer->submitted = 1;

	libusb_fill_bulk_transfer(transfer->handle,
	                          transfer->stack->device_handle,
	                          end_point,
	                          (unsigned char *)&transfer->packet,
	                          length,
	                          usb_transfer_wrapper,
	                          transfer,
	                          0);

	rc = libusb_submit_transfer(transfer->handle);

	if (rc < 0) {
		log_error("Could not submit %s transfer %p to %s: %s (%d)",
		          usb_transfer_get_type_name(transfer->type, 0), transfer,
		          transfer->stack->base.name,
		          get_libusb_error_name(rc), rc);

		transfer->submitted = 0;

		return -1;
	}

	log_debug("Submitted %s transfer %p for %u bytes to %s",
	          usb_transfer_get_type_name(transfer->type, 0), transfer, length,
	          transfer->stack->base.name);

	return 0;
}

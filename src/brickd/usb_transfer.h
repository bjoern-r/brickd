/*
 * brickd
 * Copyright (C) 2012-2013 Matthias Bolte <matthias@tinkerforge.com>
 *
 * usb_transfer.h: libusb transfer specific functions
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

#ifndef BRICKD_USB_TRANSFER_H
#define BRICKD_USB_TRANSFER_H

#include <libusb.h>

#include "packet.h"
#include "usb_stack.h"

typedef enum {
	USB_TRANSFER_TYPE_READ = 0,
	USB_TRANSFER_TYPE_WRITE
} USBTransferType;

typedef struct _USBTransfer USBTransfer;

typedef void (*USBTransferFunction)(USBTransfer *transfer);

struct _USBTransfer {
	USBStack *stack;
	USBTransferType type;
	int submitted;
	int completed;
	USBTransferFunction function;
	struct libusb_transfer *handle;
	Packet packet;
};

const char *usb_transfer_get_type_name(USBTransferType type, int upper);

int usb_transfer_create(USBTransfer *transfer, USBStack *stack,
                        USBTransferType type, USBTransferFunction function);
void usb_transfer_destroy(USBTransfer *transfer);

int usb_transfer_submit(USBTransfer *transfer);

#endif // BRICKD_USB_TRANSFER_H

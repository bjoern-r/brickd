/*
 * brickd
 * Copyright (C) 2013 Matthias Bolte <matthias@tinkerforge.com>
 *
 * usb_stack.h: USB stack specific functions
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

#ifndef BRICKD_USB_STACK_H
#define BRICKD_USB_STACK_H

#include <libusb.h>

#include "stack.h"
#include "utils.h"

typedef struct {
	Stack base;

	uint8_t bus_number;
	uint8_t device_address;
	libusb_context *context;
	libusb_device *device;
	struct libusb_device_descriptor device_descriptor;
	libusb_device_handle *device_handle;
	Array read_transfers;
	Array write_transfers;
	Array write_queue;
	int connected;
} USBStack;

int usb_stack_create(USBStack *stack, uint8_t bus_number, uint8_t device_address);
void usb_stack_destroy(USBStack *stack);

#endif // BRICKD_USB_STACK_H

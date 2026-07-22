/*
 * hw/usb_core.h - USB Core (HCD, hubs, endpoints)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _HW_USB_CORE_H
#define _HW_USB_CORE_H

#include <kernel/types.h>

#define USB_CLASS_HID 3
#define USB_CLASS_MASS_STORAGE 8
#define USB_CLASS_HUB 9

#define USB_ENDPOINT_TYPE_CONTROL 0
#define USB_ENDPOINT_TYPE_ISOCHRONOUS 1
#define USB_ENDPOINT_TYPE_BULK 2
#define USB_ENDPOINT_TYPE_INTERRUPT 3

#define USB_REQUEST_GET_STATUS 0
#define USB_REQUEST_SET_ADDRESS 5
#define USB_REQUEST_GET_DESCRIPTOR 6
#define USB_REQUEST_SET_CONFIGURATION 9

#define USB_DESCRIPTOR_TYPE_DEVICE 1
#define USB_DESCRIPTOR_TYPE_CONFIGURATION 2
#define USB_DESCRIPTOR_TYPE_STRING 3
#define USB_DESCRIPTOR_TYPE_INTERFACE 4
#define USB_DESCRIPTOR_TYPE_ENDPOINT 5

struct usb_device_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t usb_version;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t max_packet_size;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version;
    uint8_t manufacturer_string;
    uint8_t product_string;
    uint8_t serial_number_string;
    uint8_t num_configurations;
} __attribute__((packed));

struct usb_endpoint_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t endpoint_address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed));

void usb_init(void);
int usb_enumerate_devices(void);
int usb_read(struct usb_device* dev, uint8_t endpoint, void* buffer, size_t size);
int usb_write(struct usb_device* dev, uint8_t endpoint, const void* buffer, size_t size);

#endif /* _HW_USB_CORE_H */

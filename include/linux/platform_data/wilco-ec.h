/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ChromeOS Wilco Embedded Controller
 *
 * Copyright 2018 Google LLC
 */

#ifndef WILCO_EC_H
#define WILCO_EC_H

#include <linux/device.h>
#include <linux/kernel.h>

/* Message flags for using the mailbox() interface */
#define WILCO_EC_FLAG_NO_RESPONSE	BIT(0) /* EC does not respond */
#define WILCO_EC_FLAG_EXTENDED_DATA	BIT(1) /* EC returns 256 data bytes */
#define WILCO_EC_FLAG_RAW_REQUEST	BIT(2) /* Do not trim request data */
#define WILCO_EC_FLAG_RAW_RESPONSE	BIT(3) /* Do not trim response data */
#define WILCO_EC_FLAG_RAW		(WILCO_EC_FLAG_RAW_REQUEST | \
					 WILCO_EC_FLAG_RAW_RESPONSE)

/* Normal commands have a maximum 32 bytes of data */
#define EC_MAILBOX_DATA_SIZE		32
/* Extended commands have 256 bytes of response data */
#define EC_MAILBOX_DATA_SIZE_EXTENDED	256

/**
 * struct wilco_ec_device - Wilco Embedded Controller handle.
 * @dev: Device handle.
 * @mailbox_lock: Mutex to ensure one mailbox command at a time.
 * @io_command: I/O port for mailbox command.  Provided by ACPI.
 * @io_data: I/O port for mailbox data.  Provided by ACPI.
 * @io_packet: I/O port for mailbox packet data.  Provided by ACPI.
 * @data_buffer: Buffer used for EC communication.  The same buffer
 *               is used to hold the request and the response.
 * @data_size: Size of the data buffer used for EC communication.
 * @debugfs_pdev: The child platform_device used by the debugfs sub-driver.
 * @rtc_pdev: The child platform_device used by the RTC sub-driver.
 */
struct wilco_ec_device {
	struct device *dev;
	struct mutex mailbox_lock;
	struct resource *io_command;
	struct resource *io_data;
	struct resource *io_packet;
	void *data_buffer;
	size_t data_size;
	struct platform_device *debugfs_pdev;
	struct platform_device *rtc_pdev;
};

/**
 * struct wilco_ec_request - Mailbox request message format.
 * @struct_version: Should be %EC_MAILBOX_PROTO_VERSION
 * @checksum: Sum of all bytes must be 0.
 * @mailbox_id: Mailbox identifier, specifies the command set.
 * @mailbox_version: Mailbox interface version %EC_MAILBOX_VERSION
 * @reserved: Set to zero.
 * @data_size: Length of request, data + last 2 bytes of the header.
 * @command: Mailbox command code, unique for each mailbox_id set.
 * @reserved_raw: Set to zero for most commands, but is used by
 *                some command types and for raw commands.
 */
struct wilco_ec_request {
	u8 struct_version;
	u8 checksum;
	u16 mailbox_id;
	u8 mailbox_version;
	u8 reserved;
	u16 data_size;
	u8 command;
	u8 reserved_raw;
} __packed;

/**
 * struct wilco_ec_response - Mailbox response message format.
 * @struct_version: Should be %EC_MAILBOX_PROTO_VERSION
 * @checksum: Sum of all bytes must be 0.
 * @result: Result code from the EC.  Non-zero indicates an error.
 * @data_size: Length of the response data buffer.
 * @reserved: Set to zero.
 * @mbox0: EC returned data at offset 0 is unused (always 0) so this byte
 *         is treated as part of the header instead of the data.
 * @data: Response data buffer.  Max size is %EC_MAILBOX_DATA_SIZE_EXTENDED.
 */
struct wilco_ec_response {
	u8 struct_version;
	u8 checksum;
	u16 result;
	u16 data_size;
	u8 reserved[2];
	u8 mbox0;
	u8 data[0];
} __packed;

/**
 * enum wilco_ec_msg_type - Message type to select a set of command codes.
 * @WILCO_EC_MSG_LEGACY: Legacy EC messages for standard EC behavior.
 * @WILCO_EC_MSG_PROPERTY: Get/Set/Sync EC controlled NVRAM property.
 * @WILCO_EC_MSG_TELEMETRY_SHORT: 32 bytes of telemetry data provided by the EC.
 * @WILCO_EC_MSG_TELEMETRY_LONG: 256 bytes of telemetry data provided by the EC.
 */
enum wilco_ec_msg_type {
	WILCO_EC_MSG_LEGACY = 0x00f0,
	WILCO_EC_MSG_PROPERTY = 0x00f2,
	WILCO_EC_MSG_TELEMETRY_SHORT = 0x00f5,
	WILCO_EC_MSG_TELEMETRY_LONG = 0x00f6,
};

/**
 * struct wilco_ec_message - Request and response message.
 * @type: Mailbox message type.
 * @flags: Message flags, e.g. %WILCO_EC_FLAG_NO_RESPONSE.
 * @command: Mailbox command code.
 * @result: Result code from the EC.  Non-zero indicates an error.
 * @request_size: Number of bytes to send to the EC.
 * @request_data: Buffer containing the request data.
 * @response_size: Number of bytes expected from the EC.
 *                 This is 32 by default and 256 if the flag
 *                 is set for %WILCO_EC_FLAG_EXTENDED_DATA
 * @response_data: Buffer containing the response data, should be
 *                 response_size bytes and allocated by caller.
 */
struct wilco_ec_message {
	enum wilco_ec_msg_type type;
	u8 flags;
	u8 command;
	u8 result;
	size_t request_size;
	void *request_data;
	size_t response_size;
	void *response_data;
};

/**
 * wilco_ec_mailbox() - Send request to the EC and receive the response.
 * @ec: Wilco EC device.
 * @msg: Wilco EC message.
 *
 * Return: Number of bytes received or negative error code on failure.
 */
int wilco_ec_mailbox(struct wilco_ec_device *ec, struct wilco_ec_message *msg);

#endif /* WILCO_EC_H */
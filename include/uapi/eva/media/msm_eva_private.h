/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef __MSM_EVA_PRIVATE_H__
#define __MSM_EVA_PRIVATE_H__

#include <linux/types.h>
#include <drm/drm.h>

/* buffer type */
#define EVA_KMD_BUFTYPE_INPUT			0x00000001
#define EVA_KMD_BUFTYPE_OUTPUT			0x00000002

#define SW_DBG_BUF_SIZE				5242880
#define SW_DBG_UMD_KMD_SIZE			1048576
#define SW_DBG_CMD_Q_IDX            SW_DBG_UMD_KMD_SIZE
#define SW_DBG_MSG_Q_IDX            (SW_DBG_UMD_KMD_SIZE * 2)
#define SW_DBG_DSP_CMD_Q_IDX        (SW_DBG_UMD_KMD_SIZE * 3)
#define SW_DBG_DSP_MSG_Q_IDX        (SW_DBG_UMD_KMD_SIZE * 4)
#define EVA_SW_DBG_BUF_UMD_OFFSET	(SW_DBG_UMD_KMD_SIZE / 2)
#define EVA_SW_DBG_OFFLINE_DUMP_IDX	(EVA_SW_DBG_BUF_UMD_OFFSET / 2)
#define EVA_SW_DBG_KMD_OFFLINE_DUMP_IDX	EVA_SW_DBG_OFFLINE_DUMP_IDX
#define EVA_SW_DBG_UMD_OFFLINE_DUMP_IDX	(EVA_SW_DBG_BUF_UMD_OFFSET + EVA_SW_DBG_OFFLINE_DUMP_IDX)

/**
 * struct eva_kmd_session_info - session information
 * @session_id:    current session id
 */
struct eva_kmd_session_info {
	__u32 session_id;
	__u32 reserved[10];
};

/**
 * struct eva_kmd_buffer - buffer information to be registered
 * @index:         index of buffer
 * @type:          buffer type
 * @fd:            file descriptor of buffer
 * @size:          allocated size of buffer
 * @offset:        offset in fd from where usable data starts
 * @pixelformat:   fourcc format
 * @flags:         buffer flags
 */
struct eva_kmd_buffer {
	__u32 index;
	__u32 type;
	__u32 fd;
	__u32 size;
	__u32 offset;
	__u32 pixelformat;
	__u32 flags;
	__u32 reserved[5];
};

/**
 * struct eva_kmd_send_cmd - sending generic HFI command
 * @cmd_address_fd:   file descriptor of cmd_address
 * @cmd_size:         allocated size of buffer
 */
struct eva_kmd_send_cmd {
	__u32 cmd_address_fd;
	__u32 cmd_size;
	__u32 reserved[10];
};

/**
 * struct eva_kmd_client_data - store generic client
 *                              data
 * @transactionid:  transaction id
 * @client_data1:   client data to be used during callback
 * @client_data2:   client data to be used during callback
 */
struct eva_kmd_client_data {
	__u32 transactionid;
	__u32 client_data1;
	__u32 client_data2;
};

/**
 * Structures and macros for KMD arg data
 */

#define	MAX_HFI_PKT_SIZE	600

struct eva_kmd_hfi_packet {
	__u32 pkt_data[MAX_HFI_PKT_SIZE];
	void *oob_buf;
};

#define EVA_KMD_PROP_HFI_VERSION	1
#define EVA_KMD_PROP_SESSION_TYPE	2
#define EVA_KMD_PROP_SESSION_KERNELMASK	3
#define EVA_KMD_PROP_SESSION_PRIORITY	4
#define EVA_KMD_PROP_SESSION_SECURITY	5
#define EVA_KMD_PROP_SESSION_ERROR	9
#define EVA_KMD_PROP_SESSION_STATE	10
#define EVA_KMD_PROP_SESSION_LATENCY	13
#define EVA_KMD_PROP_PKT_CONCURRENCY	14

#define EVA_KMD_PROP_PWR_FDU	0x10
#define EVA_KMD_PROP_PWR_ICA	0x11
#define EVA_KMD_PROP_PWR_OD	0x12
#define EVA_KMD_PROP_PWR_MPU	0x13
#define EVA_KMD_PROP_PWR_FW	0x14
#define EVA_KMD_PROP_PWR_DDR	0x15
#define EVA_KMD_PROP_PWR_SYSCACHE	0x16
#define EVA_KMD_PROP_PWR_FDU_OP	0x17
#define EVA_KMD_PROP_PWR_ICA_OP	0x18
#define EVA_KMD_PROP_PWR_OD_OP	0x19
#define EVA_KMD_PROP_PWR_MPU_OP	0x1A
#define EVA_KMD_PROP_PWR_FW_OP	0x1B
#define EVA_KMD_PROP_PWR_DDR_OP	0x1C
#define EVA_KMD_PROP_PWR_SYSCACHE_OP	0x1D
#define EVA_KMD_PROP_PWR_FPS_FDU	0x1E
#define EVA_KMD_PROP_PWR_FPS_MPU	0x1F
#define EVA_KMD_PROP_PWR_FPS_OD	0x20
#define EVA_KMD_PROP_PWR_FPS_ICA	0x21

#define EVA_KMD_PROP_PWR_VADL 0x22
#define EVA_KMD_PROP_PWR_VADL_OP 0x23
#define EVA_KMD_PROP_PWR_FPS_VADL 0x24

#define EVA_KMD_PROP_PWR_TOF 0x25
#define EVA_KMD_PROP_PWR_TOF_OP 0x26
#define EVA_KMD_PROP_PWR_FPS_TOF 0x27

#define EVA_KMD_PROP_PWR_RGE 0x28
#define EVA_KMD_PROP_PWR_RGE_OP 0x29
#define EVA_KMD_PROP_PWR_FPS_RGE 0x2A

#define EVA_KMD_PROP_PWR_XRA 0x2B
#define EVA_KMD_PROP_PWR_XRA_OP 0x2C
#define EVA_KMD_PROP_PWR_FPS_XRA 0x2D

#define EVA_KMD_PROP_PWR_LSR 0x2E
#define EVA_KMD_PROP_PWR_LSR_OP 0x2F
#define EVA_KMD_PROP_PWR_FPS_LSR 0x30

#define EVA_KMD_PROP_SET_NAME 0x31

#define EVA_KMD_PROP_SOC_HW_VERSION     0x32

#define MAX_KMD_PROP_NUM_PER_PACKET		64
#define MAX_KMD_PROP_TYPE	(EVA_KMD_PROP_PWR_FPS_ICA + 1)

struct eva_kmd_sys_property {
	__u32 prop_type;
	__u32 data;
};

struct eva_kmd_sys_properties {
	__u32 prop_num;
	struct eva_kmd_sys_property prop_data[MAX_KMD_PROP_NUM_PER_PACKET];
};

#define SESSION_CREATE	1
#define SESSION_DELETE	2
#define SESSION_START	3
#define SESSION_STOP	4
#define SESSION_INFO	5

struct eva_kmd_session_control {
	__u32 ctrl_type;
	__u32 ctrl_data[8];
};


/**
 * struct eva_kmd_arg
 *
 * @type:          command type
 * @buf_offset:    offset to buffer list in the command
 * @buf_num:       number of buffers in the command
 * @session:       session information
 * @req_power:     power information
 * @regbuf:        buffer to be registered
 * @unregbuf:      buffer to be unregistered
 * @send_cmd:      sending generic HFI command

 * @hfi_pkt:       HFI packet created by user library
 * @sys_properties System properties read or set by user library
 * @hfi_fence_pkt: HFI fence packet created by user library
 */
struct eva_kmd_arg {
	__u32 type;
	__u32 buf_offset;
	__u32 buf_num;
	union eva_data_t {
		struct eva_kmd_session_info session;
		struct eva_kmd_buffer regbuf;
		struct eva_kmd_buffer unregbuf;
		struct eva_kmd_send_cmd send_cmd;
		struct eva_kmd_hfi_packet hfi_pkt;
		struct eva_kmd_sys_properties sys_properties;
		struct eva_kmd_session_control session_ctrl;
		__u64 frame_id;
	} data;
};

/*
 * DRM ioctl command indices (relative to DRM_COMMAND_BASE).
 * These are the indices passed to DRM_IOCTL_DEF_DRV().
 */
#define DRM_EVA_GET_SESSION_INFO	0x00
#define DRM_EVA_UPDATE_POWER		0x01
#define DRM_EVA_SEND_CMD_PKT		0x02
#define DRM_EVA_RECEIVE_MSG_PKT		0x03
#define DRM_EVA_SET_SYS_PROPERTY	0x04
#define DRM_EVA_GET_SYS_PROPERTY	0x05
#define DRM_EVA_SESSION_CONTROL		0x06
#define DRM_EVA_FLUSH_ALL		0x07
#define DRM_EVA_FLUSH_FRAME		0x08

/*
 * Userspace-facing DRM ioctl numbers.
 * All use struct eva_kmd_arg as the data structure.
 */
#define DRM_IOCTL_EVA_GET_SESSION_INFO   DRM_IOWR(DRM_COMMAND_BASE + DRM_EVA_GET_SESSION_INFO,   struct eva_kmd_arg)
#define DRM_IOCTL_EVA_UPDATE_POWER       DRM_IOWR(DRM_COMMAND_BASE + DRM_EVA_UPDATE_POWER,       struct eva_kmd_arg)
#define DRM_IOCTL_EVA_SEND_CMD_PKT       DRM_IOWR(DRM_COMMAND_BASE + DRM_EVA_SEND_CMD_PKT,       struct eva_kmd_arg)
#define DRM_IOCTL_EVA_RECEIVE_MSG_PKT    DRM_IOWR(DRM_COMMAND_BASE + DRM_EVA_RECEIVE_MSG_PKT,    struct eva_kmd_arg)
#define DRM_IOCTL_EVA_SET_SYS_PROPERTY   DRM_IOWR(DRM_COMMAND_BASE + DRM_EVA_SET_SYS_PROPERTY,   struct eva_kmd_arg)
#define DRM_IOCTL_EVA_GET_SYS_PROPERTY   DRM_IOWR(DRM_COMMAND_BASE + DRM_EVA_GET_SYS_PROPERTY,   struct eva_kmd_arg)
#define DRM_IOCTL_EVA_SESSION_CONTROL    DRM_IOWR(DRM_COMMAND_BASE + DRM_EVA_SESSION_CONTROL,    struct eva_kmd_arg)
#define DRM_IOCTL_EVA_FLUSH_ALL          DRM_IOWR(DRM_COMMAND_BASE + DRM_EVA_FLUSH_ALL,          struct eva_kmd_arg)
#define DRM_IOCTL_EVA_FLUSH_FRAME        DRM_IOWR(DRM_COMMAND_BASE + DRM_EVA_FLUSH_FRAME,        struct eva_kmd_arg)
#endif

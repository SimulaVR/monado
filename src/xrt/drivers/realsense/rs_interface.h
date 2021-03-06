// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Interface to RealSense devices.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup drv_rs
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


/*!
 * @defgroup drv_rs Intel RealSense driver
 * @ingroup drv
 *
 * @brief Driver for the SLAM-capable Intel Realsense devices.
 */

/*!
 * Create a RelaseSense 6DOF tracker device.
 *
 * @ingroup drv_rs
 */
struct xrt_device *
rs_6dof_create(void);

/*!
 * Create a auto prober for rs devices.
 *
 * @ingroup drv_rs
 */
struct xrt_auto_prober *
rs_create_auto_prober(void);

/*!
 * @dir drivers/realsense
 *
 * @brief @ref drv_rs files.
 */


#ifdef __cplusplus
}
#endif

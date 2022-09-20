// Copyright 2020, Collabora, Ltd.
// Copyright 2020, Moses Turner.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief SimulaVR driver code.
 * @author Moses Turner <moses@collabora.com>
 * @ingroup drv_svr
 */

#include "math/m_mathinclude.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "svr_interface.h"

#include "xrt/xrt_defines.h"
#include "xrt/xrt_device.h"

#include "math/m_api.h"
#include "math/m_space.h"
#include "math/m_vec2.h"

#include "os/os_time.h"
#include "os/os_threading.h"


#include "util/u_var.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_time.h"
#include "util/u_json.h"
#include "util/u_misc.h"
#include "util/u_logging.h"
#include "util/u_distortion_mesh.h"


DEBUG_GET_ONCE_LOG_OPTION(svr_log, "SIMULA_LOG", U_LOGGING_INFO)

#define SVR_TRACE(d, ...) U_LOG_XDEV_IFL_T(&d->base, d->log_level, __VA_ARGS__)
#define SVR_DEBUG(d, ...) U_LOG_XDEV_IFL_D(&d->base, d->log_level, __VA_ARGS__)
#define SVR_INFO(d, ...) U_LOG_XDEV_IFL_I(&d->base, d->log_level, __VA_ARGS__)
#define SVR_WARN(d, ...) U_LOG_XDEV_IFL_W(&d->base, d->log_level, __VA_ARGS__)
#define SVR_ERROR(d, ...) U_LOG_XDEV_IFL_E(&d->base, d->log_level, __VA_ARGS__)

struct svr_hmd
{
	struct xrt_device base;

	struct svr_two_displays_distortion distortion;

	enum u_logging_level log_level;
};

static inline struct svr_hmd *
svr_hmd(struct xrt_device *xdev)
{
	return (struct svr_hmd *)xdev;
}

static void
svr_hmd_destroy(struct xrt_device *xdev)
{
	struct svr_hmd *ns = svr_hmd(xdev);

	// Remove the variable tracking.
	u_var_remove_root(ns);

	u_device_free(&ns->base);
}
//
static void
svr_hmd_update_inputs(struct xrt_device *xdev)
{}

static void
svr_hmd_get_tracked_pose(struct xrt_device *xdev,
                         enum xrt_input_name name,
                         uint64_t at_timestamp_ns,
                         struct xrt_space_relation *out_relation)
{
	struct svr_hmd *ns = svr_hmd(xdev);

	if (name != XRT_INPUT_GENERIC_HEAD_POSE) {
		SVR_ERROR(ns, "unknown input name");
		return;
	}


	out_relation->angular_velocity = (struct xrt_vec3)XRT_VEC3_ZERO;
	out_relation->linear_velocity = (struct xrt_vec3)XRT_VEC3_ZERO;
	out_relation->pose =
	    (struct xrt_pose)XRT_POSE_IDENTITY; // This is so that tracking overrides/multi driver just transforms us by
	                                        // the tracker + offset from the tracker.
	out_relation->relation_flags = XRT_SPACE_RELATION_BITMASK_ALL;
}

#define DEG_TO_RAD(DEG) (DEG * M_PI / 180.)

static void
svr_hmd_get_view_poses(struct xrt_device *xdev,
                       const struct xrt_vec3 *default_eye_relation,
                       uint64_t at_timestamp_ns,
                       uint32_t view_count,
                       struct xrt_space_relation *out_head_relation,
                       struct xrt_fov *out_fovs,
                       struct xrt_pose *out_poses)
{
	//!@todo: default_eye_relation inherits from the env var OXR_DEBUG_IPD_MM / oxr_session.c
	// probably needs a lot more attention

	u_device_get_view_poses(xdev, default_eye_relation, at_timestamp_ns, view_count, out_head_relation, out_fovs,
	                        out_poses);



	//!@todo you may need to invert this - I can't test locally
	float turn_vals[2] = {5.0, -5.0};
	for (uint32_t i = 0; i < view_count && i < 2; i++) {
		struct xrt_vec3 y_up = (struct xrt_vec3)XRT_VEC3_UNIT_Y;
		math_quat_from_angle_vector(DEG_TO_RAD(turn_vals[i]), &y_up, &out_poses[i].orientation);
	}
}

//!@todo: remove hard-coding and move to u_distortion_mesh
bool
svr_mesh_calc(struct xrt_device *xdev, int view, float u, float v, struct xrt_uv_triplet *result)
{
	struct svr_hmd *svr = svr_hmd(xdev);

	struct svr_one_display_distortion *dist = &svr->distortion.views[view];

	struct svr_display_distortion_polynomial_values *distortion_channels[3] = {&dist->red, &dist->green,
	                                                                           &dist->blue};


	// Somewhere at the program (constants definition)
	/* Display size in mm */
	// note for people expecting everything to be in meters: no, really, this is millimeters and we don't need a
	// scaling factor
	// float _DispDimsX = 51.7752;
	// float _DispDimsY = 51.7752;
	/* Half of the horizontal field of view (in radians) fovH/2 */
	float _FoVh_2 = dist->half_fov;
	/* Field of view aspect ratio (fovH/fovV), equals to 1 if fovH = fovV */
	float _aspect = 1.0f;


	// Results r/g/b.
	struct xrt_vec2 tc[3] = {{0, 0}, {0, 0}, {0, 0}};

	// Dear compiler, please vectorize.
	for (int i = 0; i < 3; i++) {

		// Just before applying the polynomial (maybe before the loop or at the beginning of it)
		// Denormalization: conversion from uv texture coordinates (origin at bottom left corner) to mm display
		// coordinates
		struct xrt_vec2 XoYo = {0, 0}; // Assuming (0,0) at the center of the display: -DispDimsX/2 <= XoYo.x <=
		                               // DispDimsX/2; -DispDimsY <= XoYo.y <= DispDimsY
		XoYo.x = dist->display_size_mm.x * (u - 0.5f);
		XoYo.y = dist->display_size_mm.y * (v - 0.5f);

		struct xrt_vec2 tanH_tanV = {
		    0, 0}; // Resulting angular coordinates (tan(H), tan(V)) of input image corresponding to the
		           // coordinates of the input texture whose color will be sampled

		float r2 = m_vec2_dot(XoYo, XoYo);
		float r = sqrtf(r2);

		// 9 degree polynomial (only odd coefficients)
		struct svr_display_distortion_polynomial_values *vals = distortion_channels[i];
		float k1 = vals->k1;
		float k3 = vals->k3;
		float k5 = vals->k5;
		float k7 = vals->k7;
		float k9 = vals->k9;

		float k = r * (k1 + r2 * (k3 + r2 * (k5 + r2 * (k7 + r2 * k9))));

		// Avoid problems when r = 0
		if (r > 0) {
			tanH_tanV.x = (k * XoYo.x) / r;
			tanH_tanV.y = (k * XoYo.y) / r;
		} else {
			tanH_tanV.x = 0;
			tanH_tanV.y = 0;
		}

		// Normalization: Transformation from angular coordinates (tan(H), tan(V)) of input image to tc
		// (normalized coordinates with origin at the bottom left corner)
		tc[i].x = (tanH_tanV.x + tanf(_FoVh_2)) / (2 * tanf(_FoVh_2));
		tc[i].y = ((tanH_tanV.y + tanf(_FoVh_2) / _aspect) / (2 * tanf(_FoVh_2))) * _aspect;

		// SVR_TRACE(svr, "Distortion %f %f -> %i %f %f", u, v, i, tc[i].x, tc[i].y);
	}
	result->r = tc[0];
	result->g = tc[1];
	result->b = tc[2];


	return true;
}


/*
 *
 * Create function.
 *
 */

struct xrt_device *
svr_hmd_create(struct svr_two_displays_distortion *distortion)
{
	enum u_device_alloc_flags flags =
	    (enum u_device_alloc_flags)(U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE);
	struct svr_hmd *svr = U_DEVICE_ALLOCATE(struct svr_hmd, flags, 1, 0);

	// Slow copy. Could refcount it but who cares, this runs once.
	svr->distortion = *distortion;

	svr->log_level = debug_get_log_option_svr_log();



	svr->base.update_inputs = svr_hmd_update_inputs;
	svr->base.get_tracked_pose = svr_hmd_get_tracked_pose;
	svr->base.get_view_poses = svr_hmd_get_view_poses;
	svr->base.destroy = svr_hmd_destroy;
	svr->base.name = XRT_DEVICE_GENERIC_HMD;

	// Sorta a lie, we have to do this to make the state tracker happy. (Should multi.c override these?)
	svr->base.orientation_tracking_supported = true;
	svr->base.position_tracking_supported = true;

	svr->base.device_type = XRT_DEVICE_TYPE_HMD;

	svr->base.hmd->screens[0].nominal_frame_interval_ns = (uint64_t)time_s_to_ns(1.0f / 90.0f);


	// Print name.
	snprintf(svr->base.str, XRT_DEVICE_NAME_LEN, "SimulaVR HMD");
	snprintf(svr->base.serial, XRT_DEVICE_NAME_LEN, "0001");
	// Setup input.
	svr->base.inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;

	struct u_extents_2d exts;

	// one screen is 2448px wide, but there are two of them.
	exts.w_pixels = 2448 * 2;
	// Both screens are 2448px tall
	exts.h_pixels = 2448;

	u_extents_2d_split_side_by_side(&svr->base, &exts);
	// rotate leye by 180
	svr->base.hmd->views[0].rot = u_device_rotation_180;

	for (int view = 0; view < 2; view++) {
		svr->base.hmd->distortion.fov[view].angle_left = -svr->distortion.views[view].half_fov;
		svr->base.hmd->distortion.fov[view].angle_right = svr->distortion.views[view].half_fov;
		svr->base.hmd->distortion.fov[view].angle_up = svr->distortion.views[view].half_fov;
		svr->base.hmd->distortion.fov[view].angle_down = -svr->distortion.views[view].half_fov;
	}

	u_distortion_mesh_set_none(&svr->base);
	svr->base.hmd->distortion.models = XRT_DISTORTION_MODEL_COMPUTE;
	svr->base.hmd->distortion.preferred = XRT_DISTORTION_MODEL_COMPUTE;
	svr->base.compute_distortion = svr_mesh_calc;

	// Setup variable tracker.
	u_var_add_root(svr, "Simula HMD", true);
	svr->base.orientation_tracking_supported = true;
	svr->base.device_type = XRT_DEVICE_TYPE_HMD;

	size_t idx = 0;

	//!@todo these should be true for the final product iirc but possibly not for the demo unit
	svr->base.hmd->blend_modes[idx++] = XRT_BLEND_MODE_ADDITIVE;
	svr->base.hmd->blend_modes[idx++] = XRT_BLEND_MODE_OPAQUE;
	svr->base.hmd->blend_modes[idx++] = XRT_BLEND_MODE_ALPHA_BLEND;

	svr->base.hmd->blend_mode_count = idx;

	uint64_t start;
	uint64_t end;

	start = os_monotonic_get_ns();
	u_distortion_mesh_fill_in_compute(&svr->base);
	end = os_monotonic_get_ns();

	float diff = (end - start);
	diff /= U_TIME_1MS_IN_NS;

	SVR_DEBUG(svr, "Filling mesh took %f ms", diff);


	return &svr->base;
}

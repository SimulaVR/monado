# Copyright 2019-2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

add_subdirectory(bindings)

set(ANDROID_SOURCE_FILES
	android/android_ahardwarebuffer_allocator.c
	android/android_ahardwarebuffer_allocator.h
	android/android_custom_surface.cpp
	android/android_custom_surface.h
	android/android_globals.cpp
	android/android_globals.h
	android/android_load_class.cpp
	android/android_load_class.hpp
	android/org.freedesktop.monado.auxiliary.cpp
	android/org.freedesktop.monado.auxiliary.hpp
	android/org.freedesktop.monado.auxiliary.impl.hpp
	)

set(MATH_SOURCE_FILES
	math/m_api.h
	math/m_base.cpp
	math/m_eigen_interop.hpp
	math/m_filter_fifo.c
	math/m_filter_fifo.h
	math/m_filter_one_euro.c
	math/m_filter_one_euro.h
	math/m_hash.cpp
	math/m_imu_3dof.c
	math/m_imu_3dof.h
	math/m_imu_pre.c
	math/m_imu_pre.h
	math/m_optics.c
	math/m_permutation.c
	math/m_permutation.h
	math/m_predict.c
	math/m_predict.h
	math/m_quatexpmap.cpp
	math/m_space.cpp
	math/m_space.h
	math/m_vec2.h
	math/m_vec3.h
	)

set(OGL_SOURCE_FILES
	ogl/ogl_documentation.h
	)
if(XRT_HAVE_OPENGL OR XRT_HAVE_OPENGLES)
	list(APPEND OGL_SOURCE_FILES
		ogl/ogl_api.c
		ogl/ogl_api.h
		ogl/ogl_helpers.c
		ogl/ogl_helpers.h
		)
endif()
if(XRT_HAVE_EGL)
	list(APPEND OGL_SOURCE_FILES
		ogl/egl_api.c
		ogl/egl_api.h
		)
endif()

set(OS_SOURCE_FILES
	os/os_ble.h
	os/os_documentation.h
	os/os_hid.h
	os/os_hid_hidraw.c
	os/os_threading.h
	)
if(XRT_HAVE_DBUS)
	list(APPEND OS_SOURCE_FILES
		os/os_ble_dbus.c
		)
endif()

set(GSTREAMER_SOURCE_FILES
	gstreamer/gst_internal.h
	gstreamer/gst_sink.h
	gstreamer/gst_sink.c
	gstreamer/gst_pipeline.h
	gstreamer/gst_pipeline.c
	)

set(TRACKING_SOURCE_FILES
	tracking/t_data_utils.c
	tracking/t_imu_fusion.hpp
	tracking/t_imu.cpp
	tracking/t_imu.h
	tracking/t_lowpass_vector.hpp
	tracking/t_lowpass.hpp
	tracking/t_tracking.h
	)
if(XRT_HAVE_OPENCV)
	list(APPEND TRACKING_SOURCE_FILES
		tracking/t_calibration_opencv.hpp
		tracking/t_calibration.cpp
		tracking/t_convert.cpp
		tracking/t_debug_hsv_filter.cpp
		tracking/t_debug_hsv_picker.cpp
		tracking/t_debug_hsv_viewer.cpp
		tracking/t_file.cpp
		tracking/t_frame_cv_mat_wrapper.cpp
		tracking/t_frame_cv_mat_wrapper.hpp
		tracking/t_fusion.hpp
		tracking/t_helper_debug_sink.hpp
		tracking/t_hsv_filter.c
		tracking/t_kalman.cpp
		tracking/t_tracker_psmv_fusion.hpp
		tracking/t_tracker_psmv.cpp
		tracking/t_tracker_psvr.cpp
		tracking/t_tracker_hand.cpp
		)
endif()

set(UTIL_SOURCE_FILES
	util/u_bitwise.c
	util/u_bitwise.h
	util/u_debug.c
	util/u_debug.h
	util/u_device.c
	util/u_device.h
	util/u_distortion.c
	util/u_distortion.h
	util/u_distortion_mesh.c
	util/u_distortion_mesh.h
	util/u_documentation.h
	util/u_file.c
	util/u_file.cpp
	util/u_file.h
	util/u_format.c
	util/u_format.h
	util/u_frame.c
	util/u_frame.h
	util/u_generic_callbacks.hpp
	util/u_git_tag.h
	util/u_hand_tracking.c
	util/u_hand_tracking.h
	util/u_handles.c
	util/u_handles.h
	util/u_hashmap.cpp
	util/u_hashmap.h
	util/u_hashset.cpp
	util/u_hashset.h
	util/u_json.c
	util/u_json.h
	util/u_logging.c
	util/u_logging.h
	util/u_misc.c
	util/u_misc.h
	util/u_sink.h
	util/u_sink_converter.c
	util/u_sink_deinterleaver.c
	util/u_sink_queue.c
	util/u_sink_quirk.c
	util/u_sink_split.c
	util/u_time.cpp
	util/u_time.h
	util/u_timing.h
	util/u_timing_fake.c
	util/u_timing_frame.c
	util/u_timing_render.c
	util/u_trace_marker.c
	util/u_trace_marker.h
	util/u_var.cpp
	util/u_var.h
	util/u_config_json.c
	util/u_config_json.h
	util/u_verify.h
	util/u_process.c
	util/u_process.h
	)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/util/u_git_tag.c.in" "${CMAKE_CURRENT_BINARY_DIR}/u_git_tag.c" @ONLY)
list(APPEND UTIL_SOURCE_FILES "${CMAKE_CURRENT_BINARY_DIR}/u_git_tag.c")

set(VK_SOURCE_FILES
	vk/vk_documentation.h
	vk/vk_helpers.c
	vk/vk_helpers.h
	vk/vk_image_allocator.c
	vk/vk_image_allocator.h
	)

# Common includes
add_library(aux-includes INTERFACE)
target_include_directories(aux-includes INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(aux-includes INTERFACE xrt-interfaces)

if(XRT_HAVE_OPENGL OR XRT_HAVE_OPENGLES)
	# OpenGL library.
	add_library(aux_ogl STATIC ${OGL_SOURCE_FILES})
	target_link_libraries(aux_ogl PUBLIC aux-includes)
	# for GLAD
	target_link_libraries(aux_ogl PUBLIC xrt-external-glad)
	if(XRT_HAVE_OPENGLES)
		target_link_libraries(aux_ogl PUBLIC EGL::EGL)
	endif()
endif()

# OS library.
add_library(aux_os STATIC ${OS_SOURCE_FILES})
target_link_libraries(aux_os PUBLIC aux-includes xrt-pthreads)
if(XRT_HAVE_DBUS)
	target_link_libraries(aux_os PRIVATE ${DBUS_LIBRARIES})
	target_include_directories(aux_os SYSTEM
		PRIVATE
		${DBUS_INCLUDE_DIRS}
		)
endif()


# Math library.
add_library(aux_math STATIC ${MATH_SOURCE_FILES})
target_link_libraries(aux_math PUBLIC aux-includes)

# Math files has extra include(s).
target_include_directories(aux_math SYSTEM
	PRIVATE ${EIGEN3_INCLUDE_DIR}
	)

# Util library.
add_library(aux_util STATIC ${UTIL_SOURCE_FILES})
target_link_libraries(aux_util PUBLIC aux-includes xrt-pthreads aux_generated_bindings)
# for u_device
target_link_libraries(aux_util PUBLIC aux_math)
if(XRT_HAVE_JPEG)
	target_link_libraries(aux_util PRIVATE ${JPEG_LIBRARIES})
	target_include_directories(aux_util PRIVATE ${JPEG_INCLUDE_DIRS})
endif()
# for cJSON
if(XRT_HAVE_SYSTEM_CJSON)
	target_link_libraries(aux_util PUBLIC cJSON::cJSON)
	target_compile_definitions(aux_util PRIVATE XRT_HAVE_SYSTEM_CJSON)
else()
	target_link_libraries(aux_util PUBLIC xrt-external-cjson)
endif()
# For u_trace_marker
if(XRT_HAVE_PERCETTO)
	target_link_libraries(aux_util PRIVATE ${PERCETTO_LINK_LIBRARIES})
	target_include_directories(aux_util PUBLIC ${PERCETTO_INCLUDE_DIRS})
endif()
if(XRT_HAVE_LIBBSD)
	target_include_directories(aux_util SYSTEM PRIVATE ${LIBBSD_INCLUDE_DIRS})
	target_link_libraries(aux_util PUBLIC ${LIBBSD_LIBRARIES})
endif()
if(ANDROID)
	target_link_libraries(aux_util PUBLIC ${ANDROID_LOG_LIBRARY})
endif()

# GStreamer library.
if(XRT_HAVE_GST)
	add_library(aux_gstreamer STATIC ${GSTREAMER_SOURCE_FILES})
	target_link_libraries(aux_gstreamer PUBLIC
		aux-includes
		)
	target_link_libraries(aux_gstreamer PRIVATE
		xrt-interfaces
		aux_math
		aux_os
		${GST_LIBRARIES}
		)
	target_include_directories(aux_gstreamer PRIVATE
		${GST_INCLUDE_DIRS}
		)
endif()

# Tracking library.
add_library(aux_tracking STATIC ${TRACKING_SOURCE_FILES})
target_link_libraries(aux_tracking PUBLIC aux-includes PRIVATE aux_math aux_util)

# Tracking files have extra includes.
target_include_directories(aux_tracking SYSTEM
	PRIVATE
	${EIGEN3_INCLUDE_DIR}
	)
target_link_libraries(aux_tracking PRIVATE
	xrt-external-flexkalman
	xrt-external-hungarian
	)
if(XRT_HAVE_OPENCV)
	target_include_directories(aux_tracking SYSTEM
		PRIVATE
		${OpenCV_INCLUDE_DIRS}
		)
	target_link_libraries(aux_tracking PUBLIC ${OpenCV_LIBRARIES})
endif()

if (XRT_BUILD_DRIVER_VIVE OR XRT_BUILD_DRIVER_SURVIVE)
	set(VIVE_CONFIG_SOURCE_FILES
		vive/vive_config.h
		vive/vive_config.c
		)
	add_library(aux_vive STATIC ${VIVE_CONFIG_SOURCE_FILES})
	target_link_libraries(aux_vive PRIVATE xrt-interfaces aux_util aux_math aux_tracking xrt-external-cjson)
	target_link_libraries(aux_vive PRIVATE ${ZLIB_LIBRARIES})
	target_include_directories(aux_vive PRIVATE ${ZLIB_INCLUDE_DIRS})
endif()

if(XRT_HAVE_VULKAN)
	# Vulkan library.
	add_library(aux_vk STATIC ${VK_SOURCE_FILES})
	target_link_libraries(aux_vk PUBLIC aux_os aux_util)
	target_link_libraries(aux_vk PUBLIC Vulkan::Vulkan)
	target_include_directories(aux_vk PUBLIC ${Vulkan_INCLUDE_DIR})
	if(ANDROID)
		target_link_libraries(aux_vk PUBLIC ${ANDROID_LIBRARY})
	endif()
endif()


if(ANDROID)
	add_library(aux_android STATIC ${ANDROID_SOURCE_FILES})
	target_link_libraries(aux_android
		PUBLIC aux_util
		PRIVATE ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY} xrt-external-jni-wrap xrt-external-jnipp
		)
	target_link_libraries(aux_vk PUBLIC aux_android)
endif()


// Copyright 2019-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Microsoft Windows window code.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup comp_main
 */

#include <stdlib.h>
#include <string.h>
#include "xrt/xrt_compiler.h"
#include "main/comp_window.h"
#include "util/u_misc.h"


/*
 *
 * Private structs.
 *
 */

/*!
 * A Microsoft Windows window.
 *
 * @implements comp_target_swapchain
 */
struct comp_window_mswin
{
	struct comp_target_swapchain base;

	HINSTANCE instance;
	HWND window;


	bool fullscreen_requested;
	bool should_exit;
};

static WCHAR szWindowClass[] = L"Monado";
static WCHAR szWindowData[] = L"MonadoWindow";

/*
 *
 * Functions.
 *
 */

static void
draw_window(HWND hWnd, struct comp_window_mswin *cwm)
{
	ValidateRect(hWnd, NULL);
}

static LRESULT CALLBACK
WndProc(HWND hWnd, unsigned int message, WPARAM wParam, LPARAM lParam)
{
	struct comp_window_mswin *cwm = GetPropW(hWnd, szWindowData);
	if (!cwm) {
		// This is before we've set up our window, or for some other helper window...
		// We might want to handle messages differently in here.
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	switch (message) {
	case WM_PAINT: draw_window(hWnd, cwm); return 0;
	case WM_CLOSE: cwm->should_exit = true; return 0;
	case WM_DESTROY:
		// Post a quit message and return.
		cwm->should_exit = true;
		PostQuitMessage(0);
		return 0;
	default: return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}


static inline struct vk_bundle *
get_vk(struct comp_window_mswin *cwm)
{
	return &cwm->base.base.c->vk;
}

static void
comp_window_mswin_destroy(struct comp_target *ct)
{
	struct comp_window_mswin *cwm = (struct comp_window_mswin *)ct;

	comp_target_swapchain_cleanup(&cwm->base);

	//! @todo

	free(ct);
}

static void
comp_window_mswin_update_window_title(struct comp_target *ct, const char *title)
{
	struct comp_window_mswin *cwm = (struct comp_window_mswin *)ct;
	//! @todo
}

static void
comp_window_mswin_fullscreen(struct comp_window_mswin *w)
{
	//! @todo
}

static VkResult
comp_window_mswin_create_surface(struct comp_window_mswin *w, VkSurfaceKHR *vk_surface)
{
	struct vk_bundle *vk = get_vk(w);
	VkResult ret;
	VkWin32SurfaceCreateInfoKHR surface_info = {
	    .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
	    .hinstance = w->instance,
	    .hwnd = w->window,
	};

	ret = vk->vkCreateWin32SurfaceKHR(vk->instance, &surface_info, NULL, vk_surface);
	if (ret != VK_SUCCESS) {
		COMP_ERROR(w->base.base.c, "vkCreateWin32SurfaceKHR: %s", vk_result_string(ret));
		return ret;
	}

	return VK_SUCCESS;
}

static bool
comp_window_mswin_init_swapchain(struct comp_target *ct, uint32_t width, uint32_t height)
{
	struct comp_window_mswin *cwm = (struct comp_window_mswin *)ct;
	VkResult ret;

	ret = comp_window_mswin_create_surface(cwm, &cwm->base.surface.handle);
	if (ret != VK_SUCCESS) {
		COMP_ERROR(ct->c, "Failed to create surface '%s'!", vk_result_string(ret));
		return false;
	}

	//! @todo

	return true;
}


static void
comp_window_mswin_flush(struct comp_target *ct)
{
	struct comp_window_mswin *cwm = (struct comp_window_mswin *)ct;
	// force handling messages.
	MSG msg;
	while (PeekMessageW(&msg, cwm->window, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}


static bool
comp_window_mswin_init(struct comp_target *ct)
{
	struct comp_window_mswin *cwm = (struct comp_window_mswin *)ct;
	cwm->instance = GetModuleHandle(NULL);

	WNDCLASSEXW wcex;
	U_ZERO(&wcex);
	wcex.cbSize = sizeof(WNDCLASSEXW);
	wcex.style = CS_HREDRAW | CS_VREDRAW;

	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = cwm->instance;
	wcex.lpszClassName = szWindowClass;
//! @todo icon
#if 0
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SAMPLEGUI));
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_SAMPLEGUI);
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
#endif
	RegisterClassExW(&wcex);

	cwm->window = CreateWindowW(szWindowClass, L"Monado (Windowed)", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
	                            CW_USEDEFAULT, 0, NULL, NULL, cwm->instance, NULL);

	SetPropW(cwm->window, szWindowData, cwm);
	ShowWindow(cwm->window, SW_SHOWDEFAULT);
	UpdateWindow(cwm->window);
	return true;
}

static void
comp_window_mswin_configure(struct comp_window_mswin *w, int32_t width, int32_t height)
{
	if (w->base.base.c->settings.fullscreen && !w->fullscreen_requested) {
		COMP_DEBUG(w->base.base.c, "Setting full screen");
		comp_window_mswin_fullscreen(w);
		w->fullscreen_requested = true;
	}
}

struct comp_target *
comp_window_mswin_create(struct comp_compositor *c)
{
	struct comp_window_mswin *w = U_TYPED_CALLOC(struct comp_window_mswin);

	// The display timing code hasn't been tested on Windows and may be broken.
	comp_target_swapchain_init_and_set_fnptrs(&w->base, COMP_TARGET_FORCE_FAKE_DISPLAY_TIMING);

	w->base.base.name = "MS Windows";
	w->base.base.destroy = comp_window_mswin_destroy;
	w->base.base.flush = comp_window_mswin_flush;
	w->base.base.init_pre_vulkan = comp_window_mswin_init;
	w->base.base.init_post_vulkan = comp_window_mswin_init_swapchain;
	w->base.base.set_title = comp_window_mswin_update_window_title;
	w->base.base.c = c;

	return &w->base.base;
}

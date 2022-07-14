/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "egl_native_surface_interpreter.h"
#include "mir/client/client_buffer.h"
#include "mir/mir_render_surface.h"
#include "mir/graphics/frame.h"
#include "mir/log.h"
#include "sync_fence.h"
#include "android_format_conversion-inl.h"
#include <boost/throw_exception.hpp>
#include <hardware/gralloc.h>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <system/graphics.h>
#include <system/window.h>
#include <deviceinfo/deviceinfo.h>

namespace mcla = mir::client::android;
namespace mcl = mir::client;
namespace mga = mir::graphics::android;

namespace
{
static std::string get_exe_path()
{
    char link[PATH_MAX];
    const ssize_t target = readlink("/proc/self/exe", link, PATH_MAX);
    if (target)
        return std::string(link);
    else
        return std::string("");
}

std::vector<std::string> get_list_prop(const std::string& list_prop)
{
    std::vector<std::string> ret;
    std::string tmp;
    std::istringstream stream(list_prop);
    while (std::getline(stream, tmp, *","))
    {
        ret.push_back(tmp);
    }
    return ret;
}

static bool decide_egl_sync()
{
    // This configuration is determined by a comma-separated list of paths to programs.
    const auto exe = get_exe_path();
    if (exe.empty())
        return false;

    // First look whether to explicitly allow EGL flushing mode.
    DeviceInfo device_info(DeviceInfo::PrintMode::None);
    auto setting = device_info.get("MirAndroidPlatformClientEglFlush", "");

    // Check for the value being either "all", or a matching entry in the list.
    auto values = get_list_prop(setting);
    if (setting == "all")
        return true;
    if (std::find(values.begin(), values.end(), exe) != values.end())
        return true;

    // Now check whether someone explicitly wants classic fencing.
    setting = device_info.get("MirAndroidPlatformClientFenceSync", "");
    if (setting == "all")
        return true;
    values = get_list_prop(setting);
    return (std::find(values.begin(), values.end(), exe) != values.end());
}
}

mcla::EGLNativeSurfaceInterpreter::EGLNativeSurfaceInterpreter(EGLNativeSurface* surface)
    : surface(surface),
      driver_pixel_format(-1),
      sync_ops(std::make_shared<mga::RealSyncFileOps>()),
      hardware_bits(GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER),
      software_bits(GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_HW_COMPOSER |
                    GRALLOC_USAGE_HW_TEXTURE),
      driver_usage_bits(0),
      last_buffer_age(0),
      use_egl_sync(decide_egl_sync())
{
}

std::shared_ptr<mga::NativeBuffer> mcla::EGLNativeSurfaceInterpreter::driver_requests_buffer(int fence)
{
    std::unique_lock<decltype(mutex)> lk(mutex);

    if (cancelled_buffers.size() != 0)
    {
        auto buffer_to_driver = cancelled_buffers.front();
        cancelled_buffers.pop();
        buffer_to_driver->reset_fence();
        buffer_to_driver->update_usage(fence, mga::BufferAccess::write);
        return buffer_to_driver;
    }
    else
    {
        acquire_surface();
        auto buffer = surface->get_current_buffer();
        last_buffer_age = buffer->age();
        auto buffer_to_driver = mga::to_native_buffer_checked(buffer->native_buffer_handle());

        ANativeWindowBuffer* anwb = buffer_to_driver->anwb();
        anwb->format = driver_pixel_format;

        queue_tracker.push(buffer_to_driver);
        return buffer_to_driver;
    }
}

void mcla::EGLNativeSurfaceInterpreter::driver_returns_buffer(ANativeWindowBuffer*, int fence_fd)
{
    std::unique_lock<decltype(mutex)> lk(mutex);

    if (!surface)
        return;

    auto done_buffer = queue_tracker.front();
    auto native_buffer = mga::to_native_buffer_checked(done_buffer);
    native_buffer->wait_for_unlock_by_gpu();

    // In EGL syncing mode we explicitly force the GL driver to flush the command buffer
    // as is supposed to be the case with eglSwapBuffers, as per specification.
    // This mode is intended for devices with driver bugs which don't have Android's EGL implementation
    // flush properly, as is the case with the Adreno 615 on the Pixel 3a.
    if (use_egl_sync && fence_fd >= 0)
    {
        EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint attribs[] = {
            EGL_SYNC_NATIVE_FENCE_FD_ANDROID, dup(fence_fd),
            EGL_NONE
        };
        close(fence_fd);

        EGLSyncKHR sync = egl.eglCreateSyncKHR(dpy, EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
        if (sync != EGL_NO_SYNC_KHR)
        {
            const EGLint state = egl.eglClientWaitSyncKHR(dpy, sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, EGL_FOREVER);
            if (state != EGL_CONDITION_SATISFIED_KHR)
            {
                // Worst case this is going to flicker
            }
        }
        egl.eglDestroySyncKHR(dpy, sync);
    }
    else if (!use_egl_sync && fence_fd >= 0)
    {
        using namespace std::chrono;
        mga::SyncFence sync_fence(sync_ops, mir::Fd(fence_fd));
        sync_fence.wait_for(milliseconds::max());
    }

    surface->swap_buffers_sync();
    queue_tracker.pop();
}

void mcla::EGLNativeSurfaceInterpreter::driver_cancels_buffer(ANativeWindowBuffer*, int)
{
    std::unique_lock<decltype(mutex)> lk(mutex);

    auto cancelled_buffer = queue_tracker.front();
    auto native_buffer = mga::to_native_buffer_checked(cancelled_buffer);
    native_buffer->wait_for_unlock_by_gpu();
    cancelled_buffer->reset_fence();
    cancelled_buffers.push(cancelled_buffer);
    queue_tracker.pop();
}

void mcla::EGLNativeSurfaceInterpreter::lock_buffer(ANativeWindowBuffer*)
{
    std::unique_lock<decltype(mutex)> lk(mutex);

    auto buffer = queue_tracker.front();
    auto native_buffer = mga::to_native_buffer_checked(buffer);
    native_buffer->lock_for_gpu();
}

void mcla::EGLNativeSurfaceInterpreter::dispatch_driver_request_format(int format)
{
    /*
     * Here, we use the hack to "lock" the format to the first one set by
     * Android's libEGL at the EGL surface's creation time, which is the one
     * chosen at the Mir window creation time, the one Mir server always
     * acknowledge and acted upon. Some Android EGL implementation change this
     * later, resulting in incompatibility between Mir client and server. By
     * locking the format this way, the client will still render in the old
     * format (rendering code hornor the setting here).
     * TODO: find a way to communicate the format change back to the server. I
     * believe there must be a good reason to change the rendering format
     * (maybe for performance reason?).
     */
    if (driver_pixel_format == -1 || driver_pixel_format == 0 || format == 0)
        driver_pixel_format = format;
}

void mcla::EGLNativeSurfaceInterpreter::dispatch_driver_usage_bits(uint64_t usage)
{
    uint64_t default_bits = 0;
    if (!surface || surface->get_parameters().buffer_usage == mir_buffer_usage_hardware)
        default_bits = hardware_bits;
    else
        default_bits = software_bits;
    driver_usage_bits = (usage | default_bits);

    // There is seemingly no way to pass preferences to newly created buffers,
    // so leave it as an exercise for the future.
}

int mcla::EGLNativeSurfaceInterpreter::driver_requests_info(int key) const
{
    switch (key)
    {
    case NATIVE_WINDOW_WIDTH:
    case NATIVE_WINDOW_DEFAULT_WIDTH:
        if (!surface && requested_size.is_set())
        {
            return requested_size.value().width.as_int();
        }
        else
        {
            acquire_surface();
            return surface->get_parameters().width;
        }
    case NATIVE_WINDOW_HEIGHT:
    case NATIVE_WINDOW_DEFAULT_HEIGHT:
        if (!surface && requested_size.is_set())
        {
            return requested_size.value().height.as_int();
        }
        else
        {
            acquire_surface();
            return surface->get_parameters().height;
        }
    case NATIVE_WINDOW_FORMAT:
        return driver_pixel_format;
    case NATIVE_WINDOW_TRANSFORM_HINT:
        return 0;
    case NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS:
        return 2;
    case NATIVE_WINDOW_CONCRETE_TYPE:
        return NATIVE_WINDOW_SURFACE;
    case NATIVE_WINDOW_CONSUMER_USAGE_BITS: {
        uint64_t default_bits = 0;
        if (!surface || surface->get_parameters().buffer_usage == mir_buffer_usage_hardware)
            default_bits = hardware_bits;
        else
            default_bits = software_bits;
        return driver_usage_bits | default_bits;
    }
    case NATIVE_WINDOW_DEFAULT_DATASPACE:
        return HAL_DATASPACE_UNKNOWN;
    case NATIVE_WINDOW_BUFFER_AGE:
        return last_buffer_age;
    case NATIVE_WINDOW_IS_VALID:
        // true
        return 1;
    case NATIVE_WINDOW_MAX_BUFFER_COUNT:
        // The default maximum count of BufferQueue items.
        // See android::BufferQueueDefs::NUM_BUFFER_SLOTS.
        return 64;
    case NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER:
        return 1;
    default:
        std::stringstream sstream;
        sstream << "driver requested unsupported query. key: " << key;
        throw std::runtime_error(sstream.str());
    }
}

void mcla::EGLNativeSurfaceInterpreter::sync_to_display(bool should_sync)
{
    if (surface)
        surface->request_and_wait_for_configure(mir_window_attrib_swapinterval, should_sync);
}

void mcla::EGLNativeSurfaceInterpreter::dispatch_driver_request_buffer_count(unsigned int count)
{
    if (surface)
        surface->set_buffer_cache_size(count);
    else
        cache_count = count;
}

void mcla::EGLNativeSurfaceInterpreter::dispatch_driver_request_damage(geometry::Rectangles /*areas*/)
{
    if (!surface)
        return;

    // This currently doesn't seem to have a way to forcefully redraw a surface using the damage region.
}

void mcla::EGLNativeSurfaceInterpreter::dispatch_driver_request_buffer_size(geometry::Size size)
{
    if (surface)
    {
        auto params = surface->get_parameters();
        if (geometry::Size{params.width, params.height} == size)
            return;
        surface->set_size(size);
    }
    else
    {
        requested_size = size;
    }
}

void mcla::EGLNativeSurfaceInterpreter::set_surface(EGLNativeSurface* s)
{
    surface = s;
    if (surface)
    {
        if (requested_size.is_set())
            surface->set_size(requested_size.value());
        if (cache_count.is_set())
            surface->set_buffer_cache_size(cache_count.value());
    }
}

void mcla::EGLNativeSurfaceInterpreter::acquire_surface() const
{
    if (surface)
        return;

    if (!native_key)
        throw std::runtime_error("no id to access MirRenderSurface");

    auto rs = mcl::render_surface_lookup(native_key);
    if (!rs)
        throw std::runtime_error("no MirRenderSurface found");
    auto size = rs->size();
    // kludge - the side effect of this function will pass the MirNativeSurface into
    rs->get_buffer_stream(size.width.as_int(), size.height.as_int(), mga::to_mir_format(driver_pixel_format), mir_buffer_usage_hardware);

    if (!surface)
        throw std::runtime_error("no EGLNativeSurface received from mirclient library");
}

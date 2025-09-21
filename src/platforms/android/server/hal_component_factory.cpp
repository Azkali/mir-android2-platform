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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "device_quirks.h"
#include "hal_component_factory.h"
#include "real_hwc2_wrapper.h"
#include "display_buffer.h"
#include "display_device.h"
#include "framebuffers.h"
#include "hybris_gralloc_impl.h"
#include "hwc_report.h"
#include "hwc_configuration.h"
#include "hwc_layers.h"
#include "hwc_device.h"
#include "graphic_buffer_allocator.h"
#include "cmdstream_sync_factory.h"
#include "android_format_conversion-inl.h"

#include <boost/throw_exception.hpp>
#include <dlfcn.h>
#include <stdexcept>

#define MIR_LOG_COMPONENT "android/server"
#include "mir/log.h"

namespace mg = mir::graphics;
namespace mga = mir::graphics::android;
namespace geom = mir::geometry;

mga::HalComponentFactory::HalComponentFactory(
    std::shared_ptr<HwcReport> const& hwc_report,
    std::shared_ptr<mga::DeviceQuirks> const& quirks)
    : hwc_report(hwc_report),
      num_framebuffers{quirks->num_framebuffers()},
      working_egl_sync(quirks->working_egl_sync())
{
    // Always use HWC2
    hwc_wrapper = std::make_shared<mga::RealHwc2Wrapper>(hwc_report);
    hwc_report->set_version();

    start_fake_surfaceflinger();
    auto hybris_gralloc = std::make_shared<mga::HybrisGrallocImpl>();
    command_stream_sync_factory = create_command_stream_sync_factory();
    buffer_allocator = std::make_shared<mga::GraphicBufferAllocator>(
        hybris_gralloc, command_stream_sync_factory, quirks);
}

std::unique_ptr<mg::CommandStreamSync> mga::HalComponentFactory::create_command_stream_sync()
{
    return command_stream_sync_factory->create_command_stream_sync();
}

std::unique_ptr<mga::CommandStreamSyncFactory> mga::HalComponentFactory::create_command_stream_sync_factory()
{
    if (!working_egl_sync)
        return std::make_unique<mga::NullCommandStreamSyncFactory>();

    try
    {
        return std::make_unique<mga::EGLSyncFactory>();
    }
    catch (std::runtime_error&)
    {
        return std::make_unique<mga::NullCommandStreamSyncFactory>();
    }
}

std::unique_ptr<mga::FramebufferBundle> mga::HalComponentFactory::create_framebuffers(mg::DisplayConfigurationOutput const& config)
{
    return std::unique_ptr<mga::FramebufferBundle>(new mga::Framebuffers(
        *buffer_allocator,
        config.modes[config.current_mode_index].size,
        config.current_format,
        num_framebuffers));
}

std::unique_ptr<mga::LayerList> mga::HalComponentFactory::create_layer_list()
{
    geom::Displacement offset{0,0};
    // HWC2 uses FloatSourceCrop
    return std::unique_ptr<mga::LayerList>(
        new mga::LayerList(std::make_shared<mga::FloatSourceCrop>(), {}, offset));
}

std::unique_ptr<mga::DisplayDevice> mga::HalComponentFactory::create_display_device()
{
    hwc_report->report_hwc_version();

    return std::unique_ptr<mga::DisplayDevice>(
        new mga::HwcDevice20(hwc_wrapper));
}

std::unique_ptr<mga::HwcConfiguration> mga::HalComponentFactory::create_hwc_configuration()
{
    // HWC2 uses HwcPowerModeControl
    return std::unique_ptr<mga::HwcConfiguration>(new mga::HwcPowerModeControl(hwc_wrapper));
}

std::shared_ptr<mg::GraphicBufferAllocator> mga::HalComponentFactory::the_buffer_allocator()
{
    return buffer_allocator;
}

extern "C" void *android_dlopen(const char *filename, int flags);
extern "C" void *android_dlsym(void *handle, const char *symbol);

void mga::HalComponentFactory::start_fake_surfaceflinger()
{
    // Adapted from mer-hybris/qt5-qpa-hwcomposer plugin
    void *libminisf;
    void (*startMiniSurfaceFlinger)(void) = NULL;

    // A reason for calling this method here is to initialize the binder
    // thread pool such that services started from for example the
    // hwcomposer plugin don't get stuck.
    // Another is to have the SurfaceFlinger service in the same process
    // as hwcomposer, on some devices this could improve performance.

    libminisf = android_dlopen("libminisf.so", RTLD_LAZY);

    if (libminisf) {
        startMiniSurfaceFlinger = (void(*)(void))android_dlsym(libminisf, "startMiniSurfaceFlinger");
    }

    if (startMiniSurfaceFlinger) {
        startMiniSurfaceFlinger();
        mir::log_info("Started fake SurfaceFlinger service");
    } else {
        mir::log_info("Device does not have libminisf or it is incompatible, not starting fake SurfaceFlinger service");
    }
}

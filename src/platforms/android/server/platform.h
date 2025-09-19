/*
 * Copyright © 2012 Canonical Ltd.
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

#ifndef MIR_GRAPHICS_ANDROID_PLATFORM_H_
#define MIR_GRAPHICS_ANDROID_PLATFORM_H_

#include "mir/graphics/platform.h"
#include "device_quirks.h"
#include "overlay_optimization.h"
#include "mir/graphics/display.h"
#include "mir/renderer/gl/context.h"
#include <EGL/egl.h>

namespace mir
{
namespace graphics
{
class DisplayReport;
namespace android
{
class GraphicBufferAllocator;
class FramebufferFactory;
class DisplayComponentFactory;
class CommandStreamSyncFactory;
class NativeWindowReport;
class HybrisGrallocImpl;


class RenderingPlatform : public graphics::RenderingPlatform
{
public:
    RenderingPlatform(
        std::shared_ptr<HybrisGrallocImpl> const& hybris_gralloc,
        std::shared_ptr<CommandStreamSyncFactory> const& sync_factory,
        std::shared_ptr<DeviceQuirks> const& quirks);
    ~RenderingPlatform() override;

    mir::UniqueModulePtr<graphics::GraphicBufferAllocator> create_buffer_allocator(graphics::Display const& output) override;

    auto maybe_create_provider(graphics::RenderingProvider::Tag const& type_tag)
        -> std::shared_ptr<graphics::RenderingProvider> override;

private:
    std::shared_ptr<HybrisGrallocImpl> const hybris_gralloc;
    std::shared_ptr<CommandStreamSyncFactory> const sync_factory;
    std::shared_ptr<DeviceQuirks> const quirks;
    EGLDisplay const dpy;
    std::shared_ptr<renderer::gl::Context> const ctx;
};

class HWCDisplayProvider : public graphics::DisplayProvider
{
public:
    class Tag : public graphics::DisplayProvider::Tag
    {
    };

    explicit HWCDisplayProvider(std::shared_ptr<DisplayComponentFactory> const& display_buffer_builder);

    auto on_this_sink(graphics::DisplaySink& sink) const -> bool;

private:
    std::shared_ptr<DisplayComponentFactory> const display_buffer_builder;
};

class DisplayPlatform : public graphics::DisplayPlatform
{
public:
    DisplayPlatform(
        std::shared_ptr<graphics::GraphicBufferAllocator> const& buffer_allocator,
        std::shared_ptr<DisplayComponentFactory> const& display_buffer_builder,
        std::shared_ptr<DisplayReport> const& display_report,
        std::shared_ptr<NativeWindowReport> const& native_window_report,
        OverlayOptimization overlay_option,
        std::shared_ptr<DeviceQuirks> const& quirks);

    UniqueModulePtr<Display> create_display(
        std::shared_ptr<graphics::DisplayConfigurationPolicy> const&,
        std::shared_ptr<graphics::GLConfig> const& /*gl_config*/) override;

protected:
    auto maybe_create_provider(DisplayProvider::Tag const& type_tag)
        -> std::shared_ptr<DisplayProvider> override;

private:
    std::shared_ptr<graphics::GraphicBufferAllocator> const buffer_allocator;
    std::shared_ptr<DisplayComponentFactory> const display_buffer_builder;
    std::shared_ptr<DisplayReport> const display_report;
    std::shared_ptr<DeviceQuirks> const quirks;
    std::shared_ptr<NativeWindowReport> const native_window_report;
    OverlayOptimization const overlay_option;
    std::shared_ptr<HWCDisplayProvider> const hwc_display_provider;
};

}
}
}
#endif /* MIR_GRAPHICS_ANDROID_PLATFORM_H_ */

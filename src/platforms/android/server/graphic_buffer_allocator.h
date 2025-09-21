/*
 * Copyright © 2017 The UBports project.
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
 * Authored by:
 *   Kevin DuBois <kevin.dubois@canonical.com>
 *   Marius Gripsgard <marius@ubports.com>
 */

#ifndef MIR_PLATFORM_ANDROID_GRAPHIC_BUFFER_ALLOCATOR_H_
#define MIR_PLATFORM_ANDROID_GRAPHIC_BUFFER_ALLOCATOR_H_

#include <cstddef>  // to fix missing #includes in graphics.h from hardware.h
#include <hardware/hardware.h>
#include "mir_toolkit/mir_native_buffer.h"

#include "mir/graphics/buffer_properties.h"
#include "mir/graphics/graphic_buffer_allocator.h"
#include "mir/graphics/egl_wayland_allocator.h"
#include "mir/graphics/display_sink.h"
#include "mir/graphics/gl_config.h"
#include "mir/renderer/gl/gl_surface.h"
#include "mir/graphics/egl_context_executor.h"

#include <hybris_gralloc.h>

#include <EGL/egl.h>

namespace mir
{
namespace renderer
{
namespace gl
{
class Context;
}
}
namespace graphics
{

class Display;
class EGLExtensions;

namespace android
{

class Gralloc;
class DeviceQuirks;
class CommandStreamSyncFactory;
class HybrisRegistarDevice;

class GraphicBufferAllocator :
  public graphics::GraphicBufferAllocator
{
public:
    GraphicBufferAllocator(
        std::shared_ptr<HybrisGralloc> const& hybris_gralloc,
        std::shared_ptr<CommandStreamSyncFactory> const& cmdstream_sync_factory,
        std::shared_ptr<DeviceQuirks> const& quirks);

    std::vector<MirPixelFormat> supported_pixel_formats() override;
    std::shared_ptr<graphics::Buffer> alloc_software_buffer(geometry::Size, MirPixelFormat) override;
    void bind_display(wl_display* display, std::shared_ptr<Executor> wayland_executor) override;
    void unbind_display(wl_display* display) override;
    std::shared_ptr<graphics::Buffer> buffer_from_resource(
        wl_resource* buffer,
        std::function<void()>&& on_consumed,
        std::function<void()>&& on_release) override;
    auto buffer_from_shm(
        std::shared_ptr<renderer::software::RWMappableBuffer> shm_data,
        std::function<void()>&& on_consumed,
        std::function<void()>&& on_release) -> std::shared_ptr<graphics::Buffer> override;

    std::shared_ptr<graphics::Buffer> alloc_framebuffer(
        geometry::Size sz, MirPixelFormat pf);

    void set_ctx(graphics::Display const& output);
private:
    std::shared_ptr<HybrisGralloc> const hybris_gralloc;
    std::shared_ptr<EGLExtensions> const egl_extensions;
    std::shared_ptr<Gralloc> alloc_device;
    std::shared_ptr<CommandStreamSyncFactory> const cmdstream_sync_factory;
    std::shared_ptr<DeviceQuirks> const quirks;

    // WaylandTexBuffer
    std::shared_ptr<renderer::gl::Context> ctx;
    std::shared_ptr<Executor> wayland_executor;
};

class GLRenderingProvider : public graphics::GLRenderingProvider
{
public:
    GLRenderingProvider(std::shared_ptr<mir::renderer::gl::Context> ctx);
    ~GLRenderingProvider();

    auto as_texture(std::shared_ptr<graphics::Buffer> buffer) -> std::shared_ptr<graphics::gl::Texture> override;

    auto suitability_for_allocator(std::shared_ptr<graphics::GraphicBufferAllocator> const& target) -> graphics::probe::Result override;

    auto suitability_for_display(graphics::DisplaySink& sink) -> graphics::probe::Result override;

    auto make_framebuffer_provider(graphics::DisplaySink& sink) -> std::unique_ptr<graphics::RenderingProvider::FramebufferProvider> override;

    auto surface_for_sink(
        graphics::DisplaySink& sink,
        graphics::GLConfig const& gl_config) -> std::unique_ptr<graphics::gl::OutputSurface> override;

private:
    EGLDisplay dpy;
    std::shared_ptr<common::EGLContextExecutor> const egl_delegate;
    std::shared_ptr<renderer::gl::Context> const ctx;
};

}
}
}
#endif /* MIR_PLATFORM_ANDROID_GRAPHIC_BUFFER_ALLOCATOR_H_ */

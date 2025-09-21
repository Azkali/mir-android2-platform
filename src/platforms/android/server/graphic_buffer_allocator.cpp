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

#include <mir/version.h>
#include "mir/graphics/platform.h"
#include "mir/graphics/egl_extensions.h"
#include "mir/graphics/egl_error.h"
#include "mir/graphics/buffer_properties.h"
#include "mir/graphics/gl_config.h"
#include "mir/graphics/display_report.h"
#include "mir/raii.h"
#include "mir/graphics/display.h"
#include "mir/renderer/gl/context.h"
#include "mir/graphics/program_factory.h"
#include "mir/graphics/program.h"
#include "mir/executor.h"
#include "cmdstream_sync_factory.h"
#include "sync_fence.h"
#include "ref_counted_native_buffer.h"
#include "graphic_buffer_allocator.h"
#include "gralloc_module.h"
#include "gralloc_buffer.h"
#include "device_quirks.h"
#include "egl_sync_fence.h"
#include "android_format_conversion-inl.h"
#include "gl_context.h"
#include "mir/graphics/egl_wayland_allocator.h"
#include "shm_buffer.h"
#include "display_buffer.h"
#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_errno.hpp>

#include <wayland-server.h>

#include <stdexcept>

#define MIR_LOG_COMPONENT "android-buffer-allocator"
#include <mir/log.h>

namespace mg  = mir::graphics;
namespace mga = mir::graphics::android;
namespace mgc = mg::common;
namespace geom = mir::geometry;

// Local stub implementations for GL context creation
namespace
{
class StubGLConfig : public mg::GLConfig
{
public:
    int depth_buffer_bits() const override { return 0; }
    int stencil_buffer_bits() const override { return 0; }
};

class StubDisplayReport : public mg::DisplayReport
{
public:
    void report_successful_setup_of_native_resources() override {}
    void report_successful_egl_make_current_on_construction() override {}
    void report_successful_egl_buffer_swap_on_construction() override {}
    void report_successful_drm_mode_set_crtc_on_construction() override {}
    void report_successful_display_construction() override {}
    void report_drm_master_failure(int) override {}
    void report_vt_switch_away_failure() override {}
    void report_vt_switch_back_failure() override {}
    void report_egl_configuration(EGLDisplay, EGLConfig) override {}
    void report_vsync(unsigned int, mg::Frame const&) override {}
};
}

std::unique_ptr<mir::renderer::gl::Context> context_for_output(mg::Display const& /*output*/)
{
    // Since Display no longer provides ContextSource in Mir 2.x,
    // create a PbufferGLContext with default parameters for texture operations
    static StubGLConfig stub_gl_config;
    static StubDisplayReport stub_display_report;

    return std::make_unique<mga::PbufferGLContext>(stub_gl_config, stub_display_report);
}

mga::GraphicBufferAllocator::GraphicBufferAllocator(
    std::shared_ptr<HybrisGralloc> const& hybris_gralloc,
    std::shared_ptr<CommandStreamSyncFactory> const& cmdstream_sync_factory,
    std::shared_ptr<DeviceQuirks> const& quirks)
    : hybris_gralloc(hybris_gralloc),
    egl_extensions(std::make_shared<mg::EGLExtensions>()),
    alloc_device(std::make_shared<mga::GrallocModule>(hybris_gralloc, cmdstream_sync_factory, quirks, egl_extensions)),
    cmdstream_sync_factory(cmdstream_sync_factory),
    quirks(quirks)
{
}

void mga::GraphicBufferAllocator::set_ctx(mg::Display const& output) {
  ctx = context_for_output(output);
}


std::shared_ptr<mg::Buffer> mga::GraphicBufferAllocator::alloc_framebuffer(
    geometry::Size size, MirPixelFormat pf)
{
    auto gralloc_buffer = alloc_device->alloc_buffer(
        size,
        mga::to_android_format(pf),
        quirks->fb_gralloc_bits());

    return gralloc_buffer;
}

std::vector<MirPixelFormat> mga::GraphicBufferAllocator::supported_pixel_formats()
{
    static std::vector<MirPixelFormat> const pixel_formats{
        mir_pixel_format_abgr_8888,
        mir_pixel_format_xbgr_8888,
        mir_pixel_format_rgb_888,
        mir_pixel_format_rgb_565
    };

    return pixel_formats;
}

std::shared_ptr<mg::Buffer> mga::GraphicBufferAllocator::alloc_software_buffer(
    geometry::Size size, MirPixelFormat format)
{
    auto gralloc_buffer = alloc_device->alloc_buffer(
        size,
        mga::to_android_format(format),
        mga::convert_to_android_usage(mg::BufferUsage::software));

    return gralloc_buffer;
}


namespace
{
GLuint get_tex_id()
{
    GLuint tex;
    glGenTextures(1, &tex);
    return tex;
}

geom::Size get_wl_buffer_size(wl_resource* buffer, mg::EGLExtensions::WaylandExtensions const& ext)
{
    EGLint width, height;

    auto dpy = eglGetCurrentDisplay();
    if (ext.eglQueryWaylandBufferWL(dpy, buffer, EGL_WIDTH, &width) == EGL_FALSE)
    {
        BOOST_THROW_EXCEPTION(mg::egl_error("Failed to query WaylandAllocator buffer width"));
    }
    if (ext.eglQueryWaylandBufferWL(dpy, buffer, EGL_HEIGHT, &height) == EGL_FALSE)
    {
        BOOST_THROW_EXCEPTION(mg::egl_error("Failed to query WaylandAllocator buffer height"));
    }

    return geom::Size{width, height};
}

mg::gl::Texture::Layout get_texture_layout(
    wl_resource* resource,
    mg::EGLExtensions::WaylandExtensions const& ext)
{
    EGLint inverted;
    auto dpy = eglGetCurrentDisplay();

    if (ext.eglQueryWaylandBufferWL(dpy, resource, EGL_WAYLAND_Y_INVERTED_WL, &inverted) == EGL_FALSE)
    {
        // EGL_WAYLAND_Y_INVERTED_WL is unsupported; the default is that the texture is in standard
        // GL texture layout
        return mg::gl::Texture::Layout::GL;
    }
    if (inverted)
    {
        // It has the standard y-decreases-with-row layout of GL textures
        return mg::gl::Texture::Layout::GL;
    }
    else
    {
        // It has y-increases-with-row layout.
        return mg::gl::Texture::Layout::TopRowFirst;
    }
}

EGLint get_wl_egl_format(wl_resource* resource, mg::EGLExtensions::WaylandExtensions const& ext)
{
    EGLint format;
    auto dpy = eglGetCurrentDisplay();

    if (ext.eglQueryWaylandBufferWL(dpy, resource, EGL_TEXTURE_FORMAT, &format) == EGL_FALSE)
    {
        BOOST_THROW_EXCEPTION(mg::egl_error("Failed to query Wayland buffer format"));
    }
    return format;
}

class WaylandTexBuffer :
    public mg::BufferBasic,
    public mg::NativeBufferBase,
    public mg::gl::Texture
{
public:
    // Note: Must be called with a current EGL context
    WaylandTexBuffer(
        std::shared_ptr<mir::renderer::gl::Context> ctx,
        wl_resource* buffer,
        mg::EGLExtensions const& extensions,
        std::function<void()>&& on_consumed,
        std::function<void()>&& on_release,
        std::shared_ptr<mir::Executor> wayland_executor)
        : ctx{std::move(ctx)},
          tex{get_tex_id()},
          on_consumed{std::move(on_consumed)},
          on_release{std::move(on_release)},
          size_{get_wl_buffer_size(buffer, extensions.wayland(eglGetCurrentDisplay()))},
          layout_{get_texture_layout(buffer, extensions.wayland(eglGetCurrentDisplay()))},
          egl_format{get_wl_egl_format(buffer, extensions.wayland(eglGetCurrentDisplay()))},
          wayland_executor{std::move(wayland_executor)}
    {
        eglBindAPI(MIR_SERVER_EGL_OPENGL_API);

        const EGLint image_attrs[] =
            {
                EGL_WAYLAND_PLANE_WL, 0,
                EGL_NONE
            };

        auto egl_image = extensions.base(eglGetCurrentDisplay()).eglCreateImageKHR(
            eglGetCurrentDisplay(),
            EGL_NO_CONTEXT,
            EGL_WAYLAND_BUFFER_WL,
            buffer,
            image_attrs);

        if (egl_image == EGL_NO_IMAGE_KHR)
            BOOST_THROW_EXCEPTION(mg::egl_error("Failed to create EGLImage"));

        glBindTexture(GL_TEXTURE_2D, tex);
        extensions.base(eglGetCurrentDisplay()).glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // tex is now an EGLImage sibling, so we can free the EGLImage without
        // freeing the backing data.
        extensions.base(eglGetCurrentDisplay()).eglDestroyImageKHR(eglGetCurrentDisplay(), egl_image);
    }

    ~WaylandTexBuffer()
    {
        wayland_executor->spawn(
            [context = ctx, tex = tex]()
            {
                context->make_current();

                glDeleteTextures(1, &tex);

                context->release_current();
            });

        on_release();
    }


    mir::geometry::Size size() const override
    {
        return size_;
    }

    MirPixelFormat pixel_format() const override
    {
        /* TODO: These are lies, but the only piece of information external code uses
         * out of the MirPixelFormat is whether or not the buffer has an alpha channel.
         */
        switch(egl_format)
        {
            case EGL_TEXTURE_RGB:
                return mir_pixel_format_xrgb_8888;
            case EGL_TEXTURE_RGBA:
                return mir_pixel_format_argb_8888;
            case EGL_TEXTURE_EXTERNAL_WL:
                // Unspecified whether it has an alpha channel; say it does.
                return mir_pixel_format_argb_8888;
            case EGL_TEXTURE_Y_U_V_WL:
            case EGL_TEXTURE_Y_UV_WL:
                // These are just absolutely not RGB at all!
                // But they're defined to not have an alpha channel, so xrgb it is!
                return mir_pixel_format_xrgb_8888;
            case EGL_TEXTURE_Y_XUXV_WL:
                // This is a planar format, but *does* have alpha.
                return mir_pixel_format_argb_8888;
            default:
                // We've covered all possibilities above
                BOOST_THROW_EXCEPTION((std::logic_error{"Unexpected texture format!"}));
        }
    }

    NativeBufferBase* native_buffer_base() override
    {
        return this;
    }

    mir::graphics::gl::Program const& shader(mir::graphics::gl::ProgramFactory& cache) const override
    {
        char const* extension_fragment = "";
        char const* fragment_fragment =
            "uniform sampler2D tex;\n"
            "vec4 sample_to_rgba(in vec2 texcoord)\n"
            "{\n"
            "    return texture2D(tex, texcoord);\n"
            "}\n";

        /*
         * Note that the following change happens in Mir 1.8.0. However, it identifies
         * itself incorrectly as 1.7.2. Luckily 1.7.2 doesn't exist, so it should be
         * safe to check for this.
         */
#   if MIR_SERVER_VERSION >= MIR_VERSION_NUMBER(1, 7, 2)
        static int shader_id = 0;
        return cache.compile_fragment_shader(
            &shader_id,
            extension_fragment,
            fragment_fragment);
#   else
        static auto const shader = cache.compile_fragment_shader(
            extension_fragment,
            fragment_fragment);

        return *shader;
#   endif
    }

    Layout layout() const override
    {
        return layout_;
    }

    void bind() override
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        on_consumed();
        on_consumed = [](){};
    }

    void add_syncpoint() override
    {
    }
private:
    std::shared_ptr<mir::renderer::gl::Context> const ctx;
    GLuint const tex;

    std::function<void()> on_consumed;
    std::function<void()> const on_release;

    geom::Size const size_;
    Layout const layout_;
    EGLint const egl_format;

    std::shared_ptr<mir::Executor> const wayland_executor;
    std::mutex mutable content_lock;
};
}

void mga::GraphicBufferAllocator::bind_display(wl_display* display, std::shared_ptr<Executor> wayland_executor)
{
    auto context_guard = mir::raii::paired_calls(
      [this]() { ctx->make_current(); },
      [this]() { ctx->release_current(); });
    auto dpy = eglGetCurrentDisplay();

    if (dpy == EGL_NO_DISPLAY)
        BOOST_THROW_EXCEPTION((std::logic_error{"WaylandAllocator::bind_display called without an active EGL Display"}));

    try {
        auto const& wayland_ext = egl_extensions->wayland(dpy);
        if (wayland_ext.eglBindWaylandDisplayWL(dpy, display) == EGL_FALSE)
        {
            BOOST_THROW_EXCEPTION(mg::egl_error("Failed to bind Wayland EGL display"));
        }
    } catch (...) {
        mir::log_warning("No EGL_WL_bind_wayland_display support");
        return;
    }
        mir::log_info("Bound WaylandAllocator display");

    this->wayland_executor = std::move(wayland_executor);
}

std::shared_ptr<mg::Buffer> mga::GraphicBufferAllocator::buffer_from_resource(
    wl_resource* buffer,
    std::function<void()>&& on_consumed,
    std::function<void()>&& on_release)
{
    auto context_guard = mir::raii::paired_calls(
        [this]() { ctx->make_current(); },
        [this]() { ctx->release_current(); });

    // Create an EGL context executor for the current context
    auto egl_delegate = std::make_shared<mgc::EGLContextExecutor>(ctx->make_share_context());

    // Use the generic Wayland buffer implementation
    return mg::wayland::buffer_from_resource(
        buffer,
        std::move(on_consumed),
        std::move(on_release),
        *egl_extensions,
        egl_delegate);
}

auto mga::GraphicBufferAllocator::buffer_from_shm(
    std::shared_ptr<renderer::software::RWMappableBuffer> shm_data,
    std::function<void()>&& on_consumed,
    std::function<void()>&& on_release) -> std::shared_ptr<graphics::Buffer>
{
    // TODO: Implement proper SHM buffer support for Android platform
    mir::log_warning("Android platform: SHM buffer creation attempted");

    return std::make_shared<mgc::NotifyingMappableBackedShmBuffer>(
        std::move(shm_data),
        std::move(on_consumed),
        std::move(on_release));
}

void mga::GraphicBufferAllocator::unbind_display(wl_display* /*display*/)
{
    // TODO: Implement unbind_display for Mir 2.x
    // This should clean up any Wayland display-specific resources
    // For now, this is a no-op to allow compilation
}

// Android GLRenderingProvider implementation - simple approach like eglstream-kms
mga::GLRenderingProvider::GLRenderingProvider(std::shared_ptr<mir::renderer::gl::Context> ctx):
  ctx{ctx},
  egl_delegate{std::make_shared<mgc::EGLContextExecutor>(ctx->make_share_context())}
{
}

mga::GLRenderingProvider::~GLRenderingProvider()
{
}

auto mga::GLRenderingProvider::as_texture(std::shared_ptr<mg::Buffer> buffer)
    -> std::shared_ptr<mg::gl::Texture>
{
    std::shared_ptr<NativeBufferBase> native_buffer{buffer, buffer->native_buffer_base()};
    if (auto our_texture = std::dynamic_pointer_cast<mg::gl::Texture>(std::move(native_buffer))) {
        return our_texture;
    } else if (auto shm = std::dynamic_pointer_cast<mgc::ShmBuffer>(native_buffer)) {
        return shm->texture_for_provider(egl_delegate, this);
    }

    return std::dynamic_pointer_cast<mg::gl::Texture>(std::move(native_buffer));
}

auto mga::GLRenderingProvider::surface_for_sink(
    mg::DisplaySink& sink,
    mg::GLConfig const& /*gl_config*/) -> std::unique_ptr<mg::gl::OutputSurface>
{
    // Check if the sink is an Android DisplaySink
    if (auto android_sink = dynamic_cast<mga::DisplaySink*>(&sink))
    {
        // Return the DisplaySink itself since it inherits from OutputSurface
        return std::unique_ptr<gl::OutputSurface>(android_sink);
    }
    BOOST_THROW_EXCEPTION((std::runtime_error{"DisplayInterfaceProvider does not support any viable output interface"}));
}

auto mga::GLRenderingProvider::suitability_for_allocator(std::shared_ptr<mg::GraphicBufferAllocator> const& target)
    -> mg::probe::Result
{
    // Android GLRenderingProvider works best with Android GraphicBufferAllocator
    if (dynamic_cast<mga::GraphicBufferAllocator*>(target.get()))
    {
        return mg::probe::best;
    }
    return mg::probe::unsupported;
}

auto mga::GLRenderingProvider::suitability_for_display(mg::DisplaySink& /*sink*/) -> mg::probe::Result
{
    // Android platform supports GL rendering on most display sinks
    return mg::probe::supported;
}

auto mga::GLRenderingProvider::make_framebuffer_provider(mg::DisplaySink& /*sink*/)
    -> std::unique_ptr<mg::RenderingProvider::FramebufferProvider>
{
    // Android platform provides framebuffers for HWC composition
    class AndroidFramebufferProvider : public mg::RenderingProvider::FramebufferProvider
    {
    public:
        auto buffer_to_framebuffer(std::shared_ptr<mg::Buffer> buffer) -> std::unique_ptr<mg::Framebuffer> override
        {
            // For Android platform, we can create a simple framebuffer wrapper
            // that makes the Android Buffer look like a Framebuffer for HWC composition
            if (auto android_buffer = std::dynamic_pointer_cast<GrallocBuffer>(buffer))
            {
                return std::make_unique<AndroidFramebuffer>(android_buffer);
            }

            // Return nullptr for non-Android buffers
            return {};
        }

    private:
        // Simple wrapper class that makes Android Buffer look like a Framebuffer
        class AndroidFramebuffer : public mg::Framebuffer
        {
        public:
            AndroidFramebuffer(std::shared_ptr<GrallocBuffer> buffer)
                : buffer{std::move(buffer)}
            {
            }

            auto size() const -> geometry::Size override
            {
                return buffer->size();
            }

            // Get the underlying Android buffer for HWC composition
            std::shared_ptr<GrallocBuffer> get_buffer() const { return buffer; }

        private:
            std::shared_ptr<GrallocBuffer> const buffer;
        };
    };

    return std::make_unique<AndroidFramebufferProvider>();
}


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
 * Authored by:
 *   Kevin DuBois <kevin.dubois@canonical.com>
 */

#include <mir/version.h>
#include "mir/graphics/egl_extensions.h"
#include "mir/graphics/egl_error.h"
#include "mir/graphics/program.h"
#include "mir/graphics/program_factory.h"
#include "sync_fence.h"
#include "android_format_conversion-inl.h"
#include "gralloc_buffer.h"
#include "command_stream_sync.h"

#include <system/window.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;
namespace mrs=mir::renderer::software;
namespace gl=mir::graphics::gl;
namespace android=mga;

template <typename T>
class mga::GrallocBuffer::Mapping : public mir::renderer::software::Mapping<T>
{
public:
    Mapping(std::shared_ptr<HybrisGralloc> const& gralloc, ANativeWindowBuffer* anwb, int usage):
    gralloc{gralloc},
    anwb_{anwb}
    {
        int width = size().width.as_uint32_t();
        int height = size().height.as_uint32_t();
        int stride = stride_.as_uint32_t();
        int format = format_;

        void* ptr = nullptr;
        void* vaddr;
        int rc = gralloc->lock(anwb_->handle, usage, 0, 0, width, height, vaddr);
        ptr = vaddr;
        if (rc != 0)
        {
            BOOST_THROW_EXCEPTION(std::runtime_error("Failed to lock gralloc buffer"));
        }

        data_ptr = static_cast<T*>(ptr);
        size_ = geom::Size{width, height};
        stride_ = geom::Stride{stride};
        format_ = static_cast<MirPixelFormat>(format);
    }

    ~Mapping()
    {
        if (gralloc && anwb_)
        {
            gralloc->unlock(anwb_->handle);
        }
    }

    auto format() const -> MirPixelFormat override
    {
        return format_;
    }

    auto stride() const -> geom::Stride override
    {
        return stride_;
    }

    auto size() const -> geom::Size override
    {
        return size_;
    }

    auto data() -> T* override
    {
        return data_ptr;
    }

    auto len() const -> size_t override
    {
        return size_.height.as_uint32_t() * stride_.as_uint32_t();
    }

private:
    std::shared_ptr<HybrisGralloc> gralloc;
    ANativeWindowBuffer* anwb_;
    T* data_ptr;
    geom::Size size_;
    geom::Stride stride_;
    MirPixelFormat format_;
};

mga::GrallocBuffer::GrallocBuffer(
    std::shared_ptr<HybrisGralloc> const& hybris_gralloc,
    std::shared_ptr<ANativeWindowBuffer> const& anwb,
    std::shared_ptr<CommandStreamSync> const& cmdstream_sync,
    std::shared_ptr<Fence> const& fence,
    BufferAccess access,
    std::shared_ptr<EGLExtensions> const& extensions) :
    hybris_gralloc(hybris_gralloc),
    native_window_buffer(anwb),
    cmdstream_sync(cmdstream_sync),
    fence_(fence),
    access(access),
    egl_extensions(extensions)
{
}

mga::GrallocBuffer::~GrallocBuffer()
{
    if (texture_id != 0)
    {
        glDeleteTextures(1, &texture_id);
    }
}

geom::Size mga::GrallocBuffer::size() const
{
    return geom::Size{native_window_buffer->width, native_window_buffer->height};
}

geom::Stride mga::GrallocBuffer::stride() const
{
    return geom::Stride{native_window_buffer->stride};
}

MirPixelFormat mga::GrallocBuffer::format() const
{
    return mga::to_mir_format(native_window_buffer->format);
}

mg::NativeBufferBase* mga::GrallocBuffer::native_buffer_base()
{
    return this;
}

// NativeBuffer interface implementation
ANativeWindowBuffer* mga::GrallocBuffer::anwb() const
{
    return native_window_buffer.get();
}

buffer_handle_t mga::GrallocBuffer::handle() const
{
    return native_window_buffer->handle;
}

android::NativeFence mga::GrallocBuffer::copy_fence() const
{
    return fence_->copy_native_handle();
}

android::NativeFence mga::GrallocBuffer::fence() const
{
    return fence_->native_handle();
}

void mga::GrallocBuffer::ensure_available_for(android::BufferAccess intent)
{
    if ((access == mga::BufferAccess::read) && (intent == mga::BufferAccess::read))
        return;

    fence_->wait();
}

bool mga::GrallocBuffer::ensure_available_for(android::BufferAccess intent, std::chrono::milliseconds timeout)
{
    if ((access == mga::BufferAccess::read) && (intent == mga::BufferAccess::read))
        return true;

    return fence_->wait_for(timeout);
}

void mga::GrallocBuffer::update_usage(android::NativeFence& fence, android::BufferAccess current_usage)
{
    fence_->merge_with(fence);
    access = current_usage;
}

void mga::GrallocBuffer::reset_fence()
{
    fence_->reset_fence();
}

void mga::GrallocBuffer::lock_for_gpu()
{
    cmdstream_sync->raise();
}

void mga::GrallocBuffer::wait_for_unlock_by_gpu()
{
    using namespace std::chrono;
    cmdstream_sync->wait_for(duration_cast<nanoseconds>(seconds(2)));
}

// RWMappableBuffer interface implementation
auto mga::GrallocBuffer::map_writeable() -> std::unique_ptr<renderer::software::Mapping<unsigned char>>
{
    return std::make_unique<Mapping<unsigned char>>(hybris_gralloc, native_window_buffer.get(), GRALLOC_USAGE_SW_WRITE_OFTEN);
}

auto mga::GrallocBuffer::map_readable() -> std::unique_ptr<renderer::software::Mapping<unsigned char const>>
{
    return std::make_unique<Mapping<unsigned char const>>(hybris_gralloc, native_window_buffer.get(), GRALLOC_USAGE_SW_READ_OFTEN);
}

auto mga::GrallocBuffer::map_rw() -> std::unique_ptr<renderer::software::Mapping<unsigned char>>
{
    return std::make_unique<Mapping<unsigned char>>(hybris_gralloc, native_window_buffer.get(), GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
}

// gl::Texture interface implementation
gl::Program const& mga::GrallocBuffer::shader(gl::ProgramFactory& cache) const
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
#if MIR_SERVER_VERSION >= MIR_VERSION_NUMBER(1, 7, 2)
    static int shader_id = 0;
    return cache.compile_fragment_shader(
        &shader_id,
        extension_fragment,
        fragment_fragment);
#else
    static auto const program = cache.compile_fragment_shader(
        extension_fragment,
        fragment_fragment);

    return *program;
#endif
}

gl::Texture::Layout mga::GrallocBuffer::layout() const
{
    return gl::Texture::Layout::TopRowFirst;
}

void mga::GrallocBuffer::add_syncpoint()
{

}

GLuint mga::GrallocBuffer::tex_id() const
{
    return texture_id;
}

// Android-specific methods
void mga::GrallocBuffer::gl_bind_to_texture()
{
    std::unique_lock<std::mutex> lk(content_lock);
    do_bind(lk);
    secure_for_render(lk);
}

void mga::GrallocBuffer::upload_to_texture()
{
    std::unique_lock<std::mutex> lk(content_lock);
    do_bind(lk);
}

void mga::GrallocBuffer::secure_for_render()
{
    std::unique_lock<std::mutex> lock(content_lock);
    secure_for_render(lock);
}

void mga::GrallocBuffer::bind_for_write()
{
    upload_to_texture();
}

void mga::GrallocBuffer::commit()
{
    // post rendering step - only necessary when buffer is backed by user memory (c.f. to ShmBuffer)
}

void mga::GrallocBuffer::tex_bind()
{
    bool const needs_initialisation = texture_id == 0;
    if (needs_initialisation)
    {
        glGenTextures(1, &texture_id);
    }
    glBindTexture(GL_TEXTURE_2D, texture_id);
    if (needs_initialisation)
    {
        // The ShmBuffer *should* be immutable, so we can just upload once.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl_bind_to_texture();
    }
}

void mga::GrallocBuffer::do_bind(std::unique_lock<std::mutex> const&)
{
    ensure_available_for(mga::BufferAccess::read);

    DispContextPair current
    {
        eglGetCurrentDisplay(),
        eglGetCurrentContext()
    };

    if (current.first == EGL_NO_DISPLAY)
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("cannot bind buffer to texture without EGL context"));
    }

    static const EGLint image_attrs[] =
    {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };

    EGLImageKHR image;
    auto it = egl_image_map.find(current);
    if (it == egl_image_map.end())
    {
        image = egl_extensions->base(current.first).eglCreateImageKHR(
                    current.first, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                    native_window_buffer.get(), image_attrs);

        if (image == EGL_NO_IMAGE_KHR)
        {
            BOOST_THROW_EXCEPTION(mg::egl_error("error binding buffer to texture"));
        }
        egl_image_map[current] = image;
    }
    else /* already had it in map */
    {
        image = it->second;
    }

    egl_extensions->base(current.first).glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
}

void mga::GrallocBuffer::secure_for_render(std::unique_lock<std::mutex> const&)
{
    lock_for_gpu();
}

void mga::BindResolverTex::bind()
{
    tex_bind();
}

// Conversion functions moved from android_native_buffer.cpp
mga::GrallocBuffer* mga::to_gralloc_buffer_checked(mg::NativeBufferBase* buffer)
{
    if (auto native = dynamic_cast<mga::GrallocBuffer*>(buffer))
        return native;
    BOOST_THROW_EXCEPTION(std::invalid_argument("cannot downcast mg::NativeBufferBase to android::GrallocBuffer"));
}

std::shared_ptr<mga::GrallocBuffer> mga::to_gralloc_buffer_checked(std::shared_ptr<mg::NativeBufferBase> const& buffer)
{
    if (auto native = std::dynamic_pointer_cast<mga::GrallocBuffer>(buffer))
        return native;
    BOOST_THROW_EXCEPTION(std::invalid_argument("cannot downcast mg::NativeBufferBase to android::GrallocBuffer"));
}

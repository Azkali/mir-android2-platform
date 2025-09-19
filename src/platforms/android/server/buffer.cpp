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
#include "native_buffer.h"
#include "sync_fence.h"
#include "android_format_conversion-inl.h"
#include "buffer.h"

#include <system/window.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;
namespace mrs=mir::renderer::software;

template <typename T>
class mga::Buffer::Mapping : public mir::renderer::software::Mapping<T>
{
public:
    Mapping(std::shared_ptr<HybrisGralloc> const& gralloc, std::shared_ptr<NativeBuffer> const& buffer, int usage):
    buffer{buffer},
    gralloc{gralloc}
    {
        int width = size().width.as_uint32_t();
        int height = size().height.as_uint32_t();
        int top = 0;
        int left = 0;

        if (gralloc->lock(buffer->handle(), usage, top, left, width, height, vaddr) ||
            !vaddr)
            BOOST_THROW_EXCEPTION(std::runtime_error("error securing buffer for client cpu use"));
    }

    ~Mapping()
    {
        gralloc->unlock(buffer->handle());
    }

    auto format() const -> MirPixelFormat override
    {
        return mga::to_mir_format(buffer->anwb()->format);
    }

    auto stride() const -> geom::Stride override
    {
    return geom::Stride{buffer->anwb()->stride *
                        MIR_BYTES_PER_PIXEL(format())};
    }

    auto size() const -> geom::Size override
    {
        ANativeWindowBuffer *anwb = buffer->anwb();
        return geom::Size{anwb->width, anwb->height};
    }

    auto data() -> T* override
    {
        return (T*)vaddr;
    }

    auto len() const -> size_t override
    {
        return stride().as_uint32_t() * size().height.as_uint32_t();
    }

private:
    std::shared_ptr<NativeBuffer> buffer;
    std::shared_ptr<HybrisGralloc> gralloc;
    void *vaddr;
};

void mga::BindResolverTex::bind()
{
    tex_bind();
}


mga::Buffer::Buffer(std::shared_ptr<HybrisGralloc> const& hybris_gralloc,
    std::shared_ptr<NativeBuffer> const& buffer_handle,
    std::shared_ptr<mg::EGLExtensions> const& extensions)
    : hybris_gralloc(hybris_gralloc),
      native_buffer(buffer_handle),
      egl_extensions(extensions)
{
}

mga::Buffer::~Buffer()
{
    for(auto& it : egl_image_map)
    {
        EGLDisplay disp = it.first.first;
        egl_extensions->base(disp).eglDestroyImageKHR(disp, it.second);
    }
}

geom::Size mga::Buffer::size() const
{
    ANativeWindowBuffer *anwb = native_buffer->anwb();
    return {anwb->width, anwb->height};
}

geom::Stride mga::Buffer::stride() const
{
    ANativeWindowBuffer *anwb = native_buffer->anwb();
    return geom::Stride{anwb->stride *
                        MIR_BYTES_PER_PIXEL(format())};
}

MirPixelFormat mga::Buffer::format() const
{
    ANativeWindowBuffer *anwb = native_buffer->anwb();
    return mga::to_mir_format(anwb->format);
}

GLuint mga::Buffer::tex_id() const
{
    return texture_id;
}

void mga::Buffer::gl_bind_to_texture()
{
    std::unique_lock<std::mutex> lk(content_lock);
    do_bind(lk);
    secure_for_render(lk);
}

void mga::Buffer::upload_to_texture()
{
    std::unique_lock<std::mutex> lk(content_lock);
    do_bind(lk);
}

void mga::Buffer::bind_for_write()
{
    upload_to_texture();
}

void mga::Buffer::do_bind(std::unique_lock<std::mutex> const&)
{
    native_buffer->ensure_available_for(mga::BufferAccess::read);

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
                    native_buffer->anwb(), image_attrs);

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

mg::NativeBufferBase *mga::Buffer::native_buffer_base()
{
    return this;
}

std::shared_ptr<mga::NativeBuffer> mga::Buffer::native_buffer_handle() const
{
    std::unique_lock<std::mutex> lk(content_lock);

    auto native_resource = std::shared_ptr<mga::NativeBuffer>(
        native_buffer.get(),
        [this](NativeBuffer*)
        {
            content_lock.unlock();
        });

    //lock remains in effect until the native handle is released
    lk.release();
    return native_resource;
}

auto mga::Buffer::map_writeable() -> std::unique_ptr<mrs::Mapping<unsigned char>>
{
    native_buffer->ensure_available_for(mga::BufferAccess::write);

    return std::make_unique<Mapping<unsigned char>>(hybris_gralloc, native_buffer, GRALLOC_USAGE_SW_WRITE_OFTEN);
}

auto mga::Buffer::map_readable() -> std::unique_ptr<mrs::Mapping<unsigned char const>>
{
    native_buffer->ensure_available_for(mga::BufferAccess::read);

    return std::make_unique<Mapping<unsigned char const>>(hybris_gralloc, native_buffer, GRALLOC_USAGE_SW_READ_OFTEN);
}

auto mga::Buffer::map_rw() -> std::unique_ptr<mrs::Mapping<unsigned char>>
{
    return std::make_unique<Mapping<unsigned char>>(hybris_gralloc, native_buffer, GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
}

void mga::Buffer::secure_for_render()
{
    std::unique_lock<std::mutex> lk(content_lock);
    secure_for_render(lk);
}

void mga::Buffer::secure_for_render(std::unique_lock<std::mutex> const&)
{
    native_buffer->lock_for_gpu();
}

void mga::Buffer::commit()
{
    // post rendering step - only necessary when buffer is backed by user memory (c.f. to ShmBuffer)
}

mg::gl::Program const& mga::Buffer::shader(
    mg::gl::ProgramFactory& cache) const
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

mg::gl::Texture::Layout mga::Buffer::layout() const
{
    return Layout::GL;
}

void mga::Buffer::add_syncpoint()
{

}

void mga::Buffer::tex_bind()
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

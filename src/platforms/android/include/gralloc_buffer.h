/*
 * Copyright © 2012,2013 Canonical Ltd.
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

#ifndef MIR_GRAPHICS_ANDROID_GRALLOC_BUFFER_H_
#define MIR_GRAPHICS_ANDROID_GRALLOC_BUFFER_H_

#include "mir/graphics/buffer_basic.h"
#include "mir/renderer/sw/pixel_source.h"
#include "mir/graphics/texture.h"

#include "hybris_gralloc.h"
#include "fence.h"
#include "command_stream_sync.h"
#include "native_buffer.h"

#include <hardware/gralloc.h>
#include <system/window.h>

#include <mutex>
#include <condition_variable>
#include <map>

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

namespace mir
{
namespace graphics
{
struct EGLExtensions;
namespace android
{


/*
 * renderer::gl::TextureSource and graphics::gl::Texture both have
 * a bind() method. They need to do different things.
 *
 * Because we can't just override them based on their signature,
 * do the intermediate-base-class trick of having two proxy bases
 * which do nothing but rename bind() to something unique.
 */

class BindResolverTex : public gl::Texture
{
public:
    BindResolverTex() = default;

    void bind() override final;

protected:
    virtual void tex_bind() = 0;
};

class GrallocBuffer: public BufferBasic,
                     public NativeBufferBase,
                     public BindResolverTex,
                     public mir::renderer::software::RWMappableBuffer
{
public:
    GrallocBuffer(std::shared_ptr<HybrisGralloc> const& hybris_gralloc,
                  std::shared_ptr<ANativeWindowBuffer> const& anwb,
                  std::shared_ptr<CommandStreamSync> const& cmdstream_sync,
                  std::shared_ptr<Fence> const& fence,
                  BufferAccess access,
                  std::shared_ptr<EGLExtensions> const& extensions);
    ~GrallocBuffer();

    // BufferBasic interface
    geometry::Size size() const override;
    geometry::Stride stride() const override;
    MirPixelFormat format() const override;
    MirPixelFormat pixel_format() const override { return format(); }

    // NativeBufferBase interface
    NativeBufferBase* native_buffer_base() override;

    // NativeBuffer interface (merged from separate NativeBuffer class)
    ANativeWindowBuffer* anwb() const;
    buffer_handle_t handle() const;
    android::NativeFence copy_fence() const;
    android::NativeFence fence() const;
    void ensure_available_for(android::BufferAccess intent);
    bool ensure_available_for(android::BufferAccess intent, std::chrono::milliseconds timeout);
    void update_usage(android::NativeFence& fence, android::BufferAccess current_usage);
    void reset_fence();
    void lock_for_gpu();
    void wait_for_unlock_by_gpu();

    // RWMappableBuffer interface
    auto map_writeable() -> std::unique_ptr<renderer::software::Mapping<unsigned char>> override;
    auto map_readable() -> std::unique_ptr<renderer::software::Mapping<unsigned char const>> override;
    auto map_rw() -> std::unique_ptr<renderer::software::Mapping<unsigned char>> override;

    // gl::Texture interface
    gl::Program const& shader(gl::ProgramFactory& cache) const override;
    Layout layout() const override;
    void add_syncpoint() override;
    GLuint tex_id() const override;

    // Android-specific methods
    void gl_bind_to_texture();
    void upload_to_texture();
    void secure_for_render();
    void bind_for_write();
    void commit();

protected:
    void tex_bind() override;

private:
    void do_bind(std::unique_lock<std::mutex> const&);
    void secure_for_render(std::unique_lock<std::mutex> const&);

    std::shared_ptr<HybrisGralloc> hybris_gralloc;
    std::shared_ptr<ANativeWindowBuffer> const native_window_buffer;
    std::shared_ptr<CommandStreamSync> cmdstream_sync;
    std::shared_ptr<Fence> fence_;
    BufferAccess access;
    std::shared_ptr<EGLExtensions> egl_extensions;

    typedef std::pair<EGLDisplay, EGLContext> DispContextPair;
    std::map<DispContextPair,EGLImageKHR> egl_image_map;

    std::mutex mutable content_lock;
    GLuint texture_id{0};

    template <typename T>
    class Mapping;

    template <typename T>
    friend class Mapping;
};

// Conversion functions
GrallocBuffer* to_gralloc_buffer_checked(mir::graphics::NativeBufferBase* buffer);
std::shared_ptr<GrallocBuffer> to_gralloc_buffer_checked(std::shared_ptr<mir::graphics::NativeBufferBase> const& buffer);

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_GRALLOC_BUFFER_H_ */

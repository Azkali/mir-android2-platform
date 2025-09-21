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

#ifndef MIR_GRAPHICS_ANDROID_NATIVE_BUFFER_H_
#define MIR_GRAPHICS_ANDROID_NATIVE_BUFFER_H_

// #include "mir/graphics/native_buffer.h"  // Removed - not available in Mir 2.x
#include "mir/graphics/buffer.h"  // For NativeBufferBase in Mir 2.x
#include "fence.h"
#include <system/window.h>
#include <memory>

namespace mir
{
namespace graphics
{

namespace android
{
enum class BufferAccess
{
    read,
    write
};

class GrallocBuffer;

class NativeBuffer : public graphics::NativeBufferBase
{
public:
    virtual ~NativeBuffer() = default;

    virtual ANativeWindowBuffer* anwb() const = 0;
    virtual buffer_handle_t handle() const = 0;
    virtual android::NativeFence copy_fence() const = 0;
    virtual android::NativeFence fence() const = 0;

    virtual void ensure_available_for(android::BufferAccess intent) = 0;
    virtual bool ensure_available_for(android::BufferAccess intent, std::chrono::milliseconds timeout) = 0;
    virtual void update_usage(android::NativeFence& fence, android::BufferAccess current_usage) = 0;
    virtual void reset_fence() = 0;

    virtual void lock_for_gpu() = 0;
    virtual void wait_for_unlock_by_gpu() = 0;

protected:
    NativeBuffer() = default;
    NativeBuffer(NativeBuffer const&) = delete;
    NativeBuffer& operator=(NativeBuffer const&) = delete;
};

android::GrallocBuffer* to_gralloc_buffer_checked(graphics::NativeBufferBase* buffer);
std::shared_ptr<android::GrallocBuffer> to_gralloc_buffer_checked(
    std::shared_ptr<graphics::NativeBufferBase> const& buffer);


}
}
}

#endif /* MIR_GRAPHICS_ANDROID_NATIVE_BUFFER_H_ */

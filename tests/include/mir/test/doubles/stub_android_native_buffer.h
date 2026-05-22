/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan.griffiths@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_ANDROID_NATIVE_BUFFER_H_
#define MIR_TEST_DOUBLES_STUB_ANDROID_NATIVE_BUFFER_H_

#include "src/platforms/android/include/gralloc_buffer.h"
#include "src/platforms/android/include/hybris_gralloc.h"
#include "src/platforms/android/include/fence.h"
#include "src/platforms/android/include/command_stream_sync.h"
#include "mir/graphics/egl_extensions.h"
#include "mir/geometry/size.h"

namespace mir
{
namespace test
{
namespace doubles
{
struct StubCommandStreamSync : public graphics::CommandStreamSync
{
    void raise() override {}
    void reset() override {}
    bool wait_for(std::chrono::nanoseconds) override { return true; }
};

struct StubFence : public graphics::android::Fence
{
    void wait() override {}
    bool wait_for(std::chrono::milliseconds) override { return true; }
    void reset_fence() override {}
    void merge_with(graphics::android::NativeFence&) override {}
    graphics::android::NativeFence copy_native_handle() const override { return -1; }
    graphics::android::NativeFence native_handle() const override { return -1; }
};

struct StubHybrisGralloc : public graphics::android::HybrisGralloc
{
    int release(buffer_handle_t, bool) override { return 0; }
    int importBuffer(buffer_handle_t, buffer_handle_t&) override { return 0; }
    int allocate(int, int, int, int, buffer_handle_t&, uint32_t&) override { return 0; }
    int lock(buffer_handle_t, int, int, int, int, int, void*&) override { return 0; }
    int unlock(buffer_handle_t) override { return 0; }
};

struct StubAndroidNativeBuffer : public graphics::android::GrallocBuffer
{
    StubAndroidNativeBuffer()
        : GrallocBuffer(
            std::make_shared<StubHybrisGralloc>(),
            std::make_shared<ANativeWindowBuffer>(),
            std::make_shared<StubCommandStreamSync>(),
            std::make_shared<StubFence>(),
            graphics::android::BufferAccess::read,
            std::make_shared<graphics::EGLExtensions>())
    {
    }

    StubAndroidNativeBuffer(geometry::Size sz)
        : GrallocBuffer(
            std::make_shared<StubHybrisGralloc>(),
            std::make_shared<ANativeWindowBuffer>(),
            std::make_shared<StubCommandStreamSync>(),
            std::make_shared<StubFence>(),
            graphics::android::BufferAccess::read,
            std::make_shared<graphics::EGLExtensions>())
    {
        stub_anwb.width = sz.width.as_int();
        stub_anwb.height = sz.height.as_int();
    }

    ANativeWindowBuffer* anwb() const { return const_cast<ANativeWindowBuffer*>(&stub_anwb); }
    buffer_handle_t handle() const { return native_handle.get(); }
    graphics::android::NativeFence copy_fence() const { return -1; }
    graphics::android::NativeFence fence() const { return -1; }

    void ensure_available_for(graphics::android::BufferAccess) {}
    bool ensure_available_for(graphics::android::BufferAccess, std::chrono::milliseconds) { return true; }
    void update_usage(graphics::android::NativeFence&, graphics::android::BufferAccess) {}
    void reset_fence() {}

    void lock_for_gpu() {};
    void wait_for_unlock_by_gpu() {};

    ANativeWindowBuffer stub_anwb;
    std::unique_ptr<native_handle_t> native_handle =
        std::make_unique<native_handle_t>();
};
}
}
}

#endif /* MIR_TEST_DOUBLES_STUB_ANDROID_NATIVE_BUFFER_H_ */

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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/graphics/platform_ipc_operations.h"
#include "egl_sync_fence.h"
#include "android_native_buffer.h"
#include "sync_fence.h"
#include "gralloc_registrar.h"
#include "mir/client/client_buffer.h"
#include "android_format_conversion-inl.h"


#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace mg = mir::graphics;
namespace mga = mir::graphics::android;
namespace mcl = mir::client;
namespace mcla = mir::client::android;
namespace geom = mir::geometry;

namespace
{
struct NativeHandleDeleter
{
    NativeHandleDeleter(const std::shared_ptr<mga::HybrisGralloc>& hybris_gralloc)
        : hybris_gralloc(hybris_gralloc)
    {}

    void operator()(const native_handle_t* t)
    {
        hybris_gralloc->release(t, /* was_allocated */ false);
    }
private:
    const std::shared_ptr<mga::HybrisGralloc> hybris_gralloc;
};

}

mcla::GrallocRegistrar::GrallocRegistrar(const std::shared_ptr<mga::HybrisGralloc>& hybris_gralloc) :
    hybris_gralloc(hybris_gralloc)
{
}

namespace
{
std::shared_ptr<mga::NativeBuffer> create_native_buffer(
    std::shared_ptr<const native_handle_t> const& handle,
    std::shared_ptr<mga::Fence> const& fence,
    MirBufferPackage const& package,
    MirPixelFormat pf)
{
    auto ops = std::make_shared<mga::RealSyncFileOps>();
    auto anwb = std::shared_ptr<mga::RefCountedNativeBuffer>(
        new mga::RefCountedNativeBuffer(handle),
        [](mga::RefCountedNativeBuffer* buffer)
        {
            buffer->mir_dereference();
        });

    anwb->width = package.width;
    anwb->height = package.height;
    //note: mir uses stride in bytes, ANativeWindowBuffer needs it in pixel units. some drivers care
    //about byte-stride, they will pass stride via ANativeWindowBuffer::handle (which is opaque to us)
    anwb->stride = package.stride / MIR_BYTES_PER_PIXEL(pf);
    anwb->usage = GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER;
    anwb->format = mga::to_android_format(pf);
    anwb->handle = handle.get();

    auto sync = std::make_shared<mg::NullCommandSync>(); //no need for eglsync client side
    return std::make_shared<mga::AndroidNativeBuffer>(anwb, sync, fence, mga::BufferAccess::read);
}
}
std::shared_ptr<mga::NativeBuffer> mcla::GrallocRegistrar::register_buffer(
    MirBufferPackage& package,
    MirPixelFormat pf) const
{
    auto fence_present = package.flags & mir_buffer_flag_fenced;

    int native_handle_header_size = sizeof(native_handle_t);
    int total_size = sizeof(int) *
                     (package.fd_items + package.data_items + native_handle_header_size);
    native_handle_t* handle = static_cast<native_handle_t*>(::operator new(total_size));
    handle->version = native_handle_header_size;

    std::shared_ptr<mga::Fence> fence;
    if (fence_present)
    {
        auto ops = std::make_shared<mga::RealSyncFileOps>();
        fence = std::make_shared<mga::SyncFence>(ops, mir::Fd(package.fd[0]));
        handle->numFds  = package.fd_items - 1;
        for (auto i = 1; i < handle->numFds; i++)
            handle->data[i - 1] = package.fd[i];
    }
    else
    {
        auto ops = std::make_shared<mga::RealSyncFileOps>();
        fence = std::make_shared<mga::SyncFence>(ops, mir::Fd(mir::Fd::invalid));
        handle->numFds  = package.fd_items;
        for (auto i = 0; i < handle->numFds; i++)
            handle->data[i] = package.fd[i];
    }

    handle->numInts = package.data_items;
    for (auto i = 0; i < handle->numInts; i++)
        handle->data[handle->numFds+i] = package.data[i];

    const native_handle_t* out_handle = nullptr;
    if (hybris_gralloc->importBuffer(handle, out_handle) || !out_handle)
    {
        ::operator delete(handle);
        BOOST_THROW_EXCEPTION(std::runtime_error("error registering graphics buffer for client use\n"));
    }
    if (handle != out_handle)
    {
        native_handle_close(handle);
        ::operator delete(handle);
        handle = const_cast<native_handle_t*>(out_handle);

        // replace handle in creation package as it is going to be stored and reused
        if (fence_present)
        {
            package.fd_items = handle->numFds + 1;
            for (auto i = 1; i < handle->numFds; i++)
                package.fd[i] = handle->data[i - 1];
        }
        else
        {
            package.fd_items = handle->numFds;
            for (auto i = 0; i < handle->numFds; i++)
                package.fd[i] = handle->data[i];
        }

        package.data_items = handle->numInts;
        for (auto i = 0; i < handle->numInts; i++)
            package.data[i] = handle->data[handle->numFds+i];
    }

    NativeHandleDeleter del(hybris_gralloc);
    return create_native_buffer(std::shared_ptr<const native_handle_t>(handle, del), fence, package, pf);
}

std::shared_ptr<char> mcla::GrallocRegistrar::secure_for_cpu(
    std::shared_ptr<mga::NativeBuffer> const& handle,
    geometry::Rectangle const rect)
{
    void* vaddr;
    int usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
    int width = rect.size.width.as_uint32_t();
    int height = rect.size.height.as_uint32_t();
    int top = rect.top_left.x.as_uint32_t();
    int left = rect.top_left.y.as_uint32_t();
    if ( hybris_gralloc->lock(handle->handle(),
                              usage, top, left, width, height, vaddr) )
        BOOST_THROW_EXCEPTION(std::runtime_error("error securing buffer for client cpu use"));

    auto hybris_gr = hybris_gralloc;
    return std::shared_ptr<char>((char *) vaddr, [hybris_gr, handle](char*)
        {
            hybris_gr->unlock(handle->handle());
            //we didn't alloc region(just mapped it), so we don't delete
        });
}

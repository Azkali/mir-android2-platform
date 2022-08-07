/*
 * Copyright © 2022 UBPorts Foundation
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
 */

#include "hybris_gralloc_impl.h"

#include <hybris/gralloc/gralloc.h>

namespace mga = mir::graphics::android;

mga::HybrisGrallocImpl::HybrisGrallocImpl()
{
    // Initialize gralloc module unless it was already done by libhybris EGL platform
    hybris_gralloc_initialize(0);
}

int mga::HybrisGrallocImpl::release(buffer_handle_t handle, bool was_allocated)
{
    return hybris_gralloc_release(handle, was_allocated);
}

int mga::HybrisGrallocImpl::importBuffer(buffer_handle_t raw_handle, buffer_handle_t& out_handle)
{
    return hybris_gralloc_import_buffer(raw_handle, &out_handle);
}

int mga::HybrisGrallocImpl::allocate(int width, int height, int format, int usage, buffer_handle_t &handle, uint32_t &stride)
{
    return hybris_gralloc_allocate(width, height, format, usage, &handle, &stride);
}

int mga::HybrisGrallocImpl::lock(buffer_handle_t handle, int usage, int l, int t, int w, int h, void * &vaddr)
{
    return hybris_gralloc_lock(handle, usage, l, t, w, h, &vaddr);
}

int mga::HybrisGrallocImpl::unlock(buffer_handle_t handle)
{
    return hybris_gralloc_unlock(handle);
}

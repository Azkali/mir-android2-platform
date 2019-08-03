/*
 * Copyright © 2021 UBPorts Foundation
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

#ifndef MIR_GRAPHICS_ANDROID_HYBRISALLOCDEVICE_H_
#define MIR_GRAPHICS_ANDROID_HYBRISALLOCDEVICE_H_

#include <hybris/gralloc/gralloc.h>

namespace mir
{
namespace graphics
{
namespace android
{
class HybrisAllocDevice : public alloc_device_t
{
public:
    HybrisAllocDevice()
    {
        alloc = hook_alloc;
        free = hook_free;

        /* FIXME: it should be called only once, but gets called from somewhere else.
        hybris_gralloc_initialize(0); */
    }

    ~HybrisAllocDevice()
    {
    }

    static int hook_alloc(alloc_device_t*,
                          int width, int height, int format, int usage_flag,
                          buffer_handle_t* handle, int* stride)
    {
        uint32_t outStride = 0;
        auto ret = hybris_gralloc_allocate(width, height, format, usage_flag,
                                       handle, &outStride);
        *stride = outStride;
        return ret;
    }

    static int hook_free(alloc_device_t*, buffer_handle_t handle)
    {
        return hybris_gralloc_release(handle, 1/*was_allocated*/);
    }
};
} // namespace android
} // namespace graphics
} // namespace mir

#endif /* MIR_GRAPHICS_ANDROID_HYBRISALLOCDEVICE_H_ */

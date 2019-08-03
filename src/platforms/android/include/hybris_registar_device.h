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

#ifndef MIR_GRAPHICS_ANDROID_HYBRISREGISTARDEVICE_H_
#define MIR_GRAPHICS_ANDROID_HYBRISREGISTARDEVICE_H_

#include <hybris/gralloc/gralloc.h>

namespace mir
{
namespace graphics
{
namespace android
{
class HybrisRegistarDevice : public gralloc_module_t
{
public:
    HybrisRegistarDevice()
    {
        registerBuffer = hook_registerBuffer;
        unregisterBuffer = hook_unregisterBuffer;
        lock = hook_lock;
        unlock = hook_unlock;

        // Initialize gralloc module unless it was already done by libhybris EGL platform
        hybris_gralloc_initialize(0);
    }

    static int hook_registerBuffer(gralloc_module_t const*, buffer_handle_t handle)
    {
        return hybris_gralloc_retain(handle);
    }

    static int hook_unregisterBuffer(gralloc_module_t const*, buffer_handle_t handle)
    {
        return hybris_gralloc_release(handle, 0/*was_allocated*/);
    }

    static int hook_lock(gralloc_module_t const*, buffer_handle_t handle,
        int usage, int left, int top, int width, int height, void** vaddr)
    {
        return hybris_gralloc_lock(handle, usage, top, left, width, height, vaddr);
    }

    static int hook_unlock(gralloc_module_t const*, buffer_handle_t handle)
    {
        return hybris_gralloc_unlock(handle);
    }
};
} // namespace android
} // namespace graphics
} // namespace mir

#endif /* MIR_GRAPHICS_ANDROID_HYBRISREGISTARDEVICE_H_ */

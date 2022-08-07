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

#ifndef MIR_GRAPHICS_ANDROID_HYBRISGRALLOC_H_
#define MIR_GRAPHICS_ANDROID_HYBRISGRALLOC_H_

#include <cstdint>

/* Forward declarations. */
typedef struct native_handle native_handle_t;
typedef const native_handle_t* buffer_handle_t;

namespace mir
{
namespace graphics
{
namespace android
{

class HybrisGralloc
{
public:
    virtual ~HybrisGralloc() = default;

    /* Contains only what's used by us. */
    virtual int release(buffer_handle_t handle, bool was_allocated) = 0;
    virtual int importBuffer(buffer_handle_t raw_handle, buffer_handle_t& out_handle) = 0;
    virtual int allocate(int width, int height, int format, int usage, buffer_handle_t &handle, uint32_t &stride) = 0;
    virtual int lock(buffer_handle_t handle, int usage, int l, int t, int w, int h, void * &vaddr) = 0;
    virtual int unlock(buffer_handle_t handle) = 0;

protected:
    HybrisGralloc() = default;
    HybrisGralloc(HybrisGralloc const&) = delete;
    HybrisGralloc& operator=(HybrisGralloc const&) = delete;
};

} // namespace android
} // namespace graphics
} // namespace mir

#endif /* MIR_GRAPHICS_ANDROID_HYBRISGRALLOC_H_ */

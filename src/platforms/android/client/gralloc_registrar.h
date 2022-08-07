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

#ifndef MIR_CLIENT_ANDROID_GRALLOC_REGISTRAR_H_
#define MIR_CLIENT_ANDROID_GRALLOC_REGISTRAR_H_

#include "buffer_registrar.h"
#include "hybris_gralloc.h"

namespace mir
{
namespace client
{
namespace android
{

class NativeHandleOps
{
public:
    virtual ~NativeHandleOps() = default;
    virtual int close(const native_handle_t* h) = 0;
};

class RealNativeHandleOps : public NativeHandleOps
{
public:
    int close(const native_handle_t* h) override;
};

class GrallocRegistrar : public BufferRegistrar
{
public:
    GrallocRegistrar(
        std::shared_ptr<graphics::android::HybrisGralloc> const& hybris_gralloc,
        std::shared_ptr<NativeHandleOps> const& native_handle_ops);

    std::shared_ptr<graphics::android::NativeBuffer> register_buffer(
        MirBufferPackage& package,
        MirPixelFormat pf) const;
    std::shared_ptr<char> secure_for_cpu(
        std::shared_ptr<graphics::android::NativeBuffer> const& handle,
        geometry::Rectangle const);

private:
    std::shared_ptr<graphics::android::HybrisGralloc> hybris_gralloc;
    std::shared_ptr<NativeHandleOps> native_handle_ops;
};

}
}
}
#endif /* MIR_CLIENT_ANDROID_GRALLOC_REGISTRAR_H_ */

/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_MOCK_BUFFER_H_
#define MIR_TEST_DOUBLES_MOCK_BUFFER_H_

#include "mir/graphics/buffer_basic.h"
#include "mir/geometry/size.h"
#include "mir/graphics/buffer_id.h"
#include "mir/renderer/sw/pixel_source.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mir
{
namespace test
{
namespace doubles
{

struct MockBuffer : public graphics::Buffer, public graphics::NativeBufferBase,
    public renderer::software::WriteMappableBuffer
{
 public:
    MockBuffer()
    {
        using namespace testing;
        ON_CALL(*this, native_buffer_base())
                .WillByDefault(Return(this));
    }

    MockBuffer(geometry::Size size,
               geometry::Stride s,
               MirPixelFormat pf)
        : MockBuffer{}
    {
        using namespace testing;
        ON_CALL(*this, size())
                .WillByDefault(Return(size));
        ON_CALL(*this, stride())
                .WillByDefault(Return(s));
        ON_CALL(*this, pixel_format())
                .WillByDefault(Return(pf));

        ON_CALL(*this, id())
                .WillByDefault(Return(graphics::BufferID{4}));
        // native_buffer_handle() removed in Mir 2.x
    }

    MOCK_CONST_METHOD0(size, geometry::Size());
    MOCK_CONST_METHOD0(stride, geometry::Stride());
    MOCK_CONST_METHOD0(pixel_format, MirPixelFormat());
    // native_buffer_handle() removed in Mir 2.x - use native_buffer_base() instead

    MOCK_CONST_METHOD0(id, graphics::BufferID());

    // WriteMappableBuffer interface methods
    MOCK_METHOD0(map_writeable, std::unique_ptr<renderer::software::Mapping<unsigned char>>());

    // BufferDescriptor interface methods
    MOCK_CONST_METHOD0(format, MirPixelFormat());
    MOCK_METHOD0(native_buffer_base, graphics::NativeBufferBase*());
    MOCK_METHOD0(used_as_texture, void());
};

}
}
}

#endif // MIR_TEST_DOUBLES_MOCK_BUFFER_H_

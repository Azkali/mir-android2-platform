/*
 * Copyright © 2012-2014 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_BUFFER_H_
#define MIR_TEST_DOUBLES_STUB_BUFFER_H_

#include "mir/graphics/buffer_basic.h"
#include "mir/graphics/buffer_properties.h"
#include "mir/geometry/size.h"
#include "mir/graphics/buffer_id.h"
#include "mir/renderer/sw/pixel_source.h"
#include <vector>
#include <string.h>

#include <boost/throw_exception.hpp>
#include <exception>
#include <stdexcept>

namespace mir
{
namespace test
{
namespace doubles
{

class StubMapping : public renderer::software::Mapping<unsigned char>
{
public:
    StubMapping(std::vector<unsigned char>& data) : data_(data) {}

    virtual unsigned char* data() override { return data_.data(); }
    virtual size_t len() const override { return data_.size(); }
    virtual MirPixelFormat format() const override { return mir_pixel_format_abgr_8888; }
    virtual geometry::Stride stride() const override { return geometry::Stride{0}; }
    virtual geometry::Size size() const override { return geometry::Size{0, 0}; }

private:
    std::vector<unsigned char>& data_;
};

class StubBuffer :
    public graphics::Buffer,
    public graphics::NativeBufferBase,
    public renderer::software::WriteMappableBuffer
{
public:
    StubBuffer()
        : StubBuffer{
              nullptr,
              graphics::BufferProperties{
                  geometry::Size{},
                  mir_pixel_format_abgr_8888,
                  graphics::BufferUsage::hardware},
              geometry::Stride{}}

    {
    }

    StubBuffer(geometry::Size const& size)
        : StubBuffer{
              nullptr,
              graphics::BufferProperties{
                  size,
                  mir_pixel_format_abgr_8888,
                  graphics::BufferUsage::hardware},
              geometry::Stride{}}

    {
    }

    StubBuffer(std::shared_ptr<graphics::NativeBufferBase> const& native_buffer, geometry::Size const& size)
        : StubBuffer{
              native_buffer,
              graphics::BufferProperties{
                  size,
                  mir_pixel_format_argb_8888,
                  graphics::BufferUsage::hardware},
                  geometry::Stride{}}

    {
    }

    StubBuffer(std::shared_ptr<graphics::NativeBufferBase> const& native_buffer)
        : StubBuffer{native_buffer, {}}
    {
    }

    StubBuffer(graphics::BufferProperties const& properties)
        : StubBuffer{nullptr, properties, geometry::Stride{properties.size.width.as_int() * MIR_BYTES_PER_PIXEL(properties.format)}}
    {
    }

    StubBuffer(graphics::BufferID id)
        : native_buffer(nullptr),
          buf_size{},
          buf_pixel_format{mir_pixel_format_abgr_8888},
          buf_stride{},
          buf_id{id}
    {
    }

    StubBuffer(std::shared_ptr<graphics::NativeBufferBase> const& native_buffer,
               graphics::BufferProperties const& properties,
               geometry::Stride stride)
        : native_buffer(native_buffer),
          buf_size{properties.size},
          buf_pixel_format{properties.format},
          buf_stride{stride},
          buf_id{graphics::BufferID{1}}
    {
    }

    // Buffer interface methods
    virtual graphics::BufferID id() const override { return buf_id; }
    virtual geometry::Size size() const override { return buf_size; }
    virtual MirPixelFormat pixel_format() const override { return buf_pixel_format; }
    virtual NativeBufferBase* native_buffer_base() override { return this; }

    // WriteMappableBuffer interface methods
    virtual std::unique_ptr<renderer::software::Mapping<unsigned char>> map_writeable() override
    {
        // Return a simple mapping that writes to our internal buffer
        return std::make_unique<StubMapping>(written_pixels);
    }

    // BufferDescriptor interface methods (inherited from WriteMappableBuffer)
    virtual MirPixelFormat format() const override { return buf_pixel_format; }
    virtual geometry::Stride stride() const override { return buf_stride; }

    std::shared_ptr<graphics::NativeBufferBase> const native_buffer;
    geometry::Size const buf_size;
    MirPixelFormat const buf_pixel_format;
    geometry::Stride const buf_stride;
    graphics::BufferID const buf_id;
    std::vector<unsigned char> written_pixels;
};
}
}
}
#endif /* MIR_TEST_DOUBLES_STUB_BUFFER_H_ */

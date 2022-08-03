/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include "native_buffer.h"
#include "hardware/gralloc.h"
#include "mir/graphics/platform_ipc_operations.h"
#include "src/platforms/android/client/gralloc_registrar.h"
#include "mir/test/doubles/mock_android_native_buffer.h"
#include "mir/test/doubles/mock_hybris_gralloc.h"
#include <memory>
#include <stdexcept>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mcla = mir::client::android;
namespace geom = mir::geometry;
namespace mtd = mir::test::doubles;

class MockNativeHandleOps : public mcla::NativeHandleOps
{
public:
    MOCK_METHOD(int, close, (const native_handle_t* h), (override));
};

struct GrallocRegistrar : public ::testing::Test
{
    GrallocRegistrar() :
        mock_hybris_gralloc(std::make_shared<testing::NiceMock<mtd::MockHybrisGralloc>>()),
        mock_native_handle_ops(std::make_shared<testing::NiceMock<MockNativeHandleOps>>()),
        mock_native_buffer(std::make_shared<testing::NiceMock<mtd::MockAndroidNativeBuffer>>())
    {
        stub_package.width = width;
        stub_package.height = height;
        stub_package.stride = stride;
        stub_package.fd_items = 4;
        stub_package.data_items = 21;
        for (auto i = 0; i < stub_package.fd_items; i++)
            stub_package.fd[i] = (i*4);
        stub_package.flags = 0;
        for (auto i = 0; i < stub_package.data_items; i++)
            stub_package.data[i] = (i*3);

        using namespace testing;
        ON_CALL(*mock_hybris_gralloc, importBuffer(_, _))
            .WillByDefault(Invoke([](buffer_handle_t handle, buffer_handle_t &out_handle) {
                // native_handle_clone(), but without dup() call.
                auto new_handle = native_handle_create(handle->numFds, handle->numInts);

                auto size = sizeof(native_handle_t) +
                    ((handle->numFds + handle->numInts) * sizeof(int));
                memcpy(new_handle, handle, size);

                out_handle = new_handle;
                return 0;
        }));
    }

    uint32_t const width{41};
    uint32_t const height{43};
    uint32_t const top{0};
    uint32_t const left{1};
    uint32_t const stride{11235};
    MirPixelFormat const pf{mir_pixel_format_abgr_8888};
    geom::Rectangle const rect{geom::Point{top, left}, geom::Size{width, height}};

    std::shared_ptr<mtd::MockHybrisGralloc> const mock_hybris_gralloc;
    std::shared_ptr<MockNativeHandleOps> const mock_native_handle_ops;
    MirBufferPackage stub_package;
    std::shared_ptr<mtd::MockAndroidNativeBuffer> const mock_native_buffer;
};

TEST_F(GrallocRegistrar, client_buffer_converts_stub_package)
{
    mcla::GrallocRegistrar registrar(mock_hybris_gralloc, mock_native_handle_ops);
    auto buffer = registrar.register_buffer(stub_package, pf);

    auto handle = buffer->handle();
    ASSERT_NE(nullptr, handle);
    ASSERT_EQ(stub_package.fd_items, handle->numFds);
    ASSERT_EQ(stub_package.data_items, handle->numInts);
    for (auto i = 0; i < stub_package.fd_items; i++)
        EXPECT_EQ(stub_package.fd[i], handle->data[i]);
    for (auto i = 0; i < stub_package.data_items; i++)
        EXPECT_EQ(stub_package.data[i], handle->data[i + stub_package.fd_items]);
}

TEST_F(GrallocRegistrar, client_sets_correct_version)
{
    mcla::GrallocRegistrar registrar(mock_hybris_gralloc, mock_native_handle_ops);
    auto buffer = registrar.register_buffer(stub_package, pf);
    EXPECT_EQ(buffer->handle()->version, static_cast<int>(sizeof(native_handle_t)));
}

TEST_F(GrallocRegistrar, registrar_frees_fds)
{
    using namespace testing;

    EXPECT_CALL(*mock_hybris_gralloc, release(
            Field(&native_handle_t::numFds, Eq(stub_package.fd_items)),
            /* was_allocated */ false))
        .Times(1);

    {
        mcla::GrallocRegistrar registrar(mock_hybris_gralloc, mock_native_handle_ops);
        auto buffer = registrar.register_buffer(stub_package, pf);
    }
}


TEST_F(GrallocRegistrar, register_failure)
{
    using namespace testing;
    EXPECT_CALL(*mock_hybris_gralloc, importBuffer(_, _))
        .Times(1)
        .WillOnce(Return(-1));
    EXPECT_CALL(*mock_hybris_gralloc, release(_,_))
        .Times(0);

    mcla::GrallocRegistrar registrar(mock_hybris_gralloc, mock_native_handle_ops);
    EXPECT_THROW({
        registrar.register_buffer(stub_package, pf);
    }, std::runtime_error);
}

TEST_F(GrallocRegistrar, region_is_cleaned_up_correctly)
{
    using namespace testing;
    mcla::GrallocRegistrar registrar(mock_hybris_gralloc, mock_native_handle_ops);

    native_handle_t const* handle_alloc{nullptr};
    native_handle_t const* handle_freed{nullptr};

    Sequence seq;
    EXPECT_CALL(*mock_hybris_gralloc, lock(_,_,_,_,_,_,_))
        .InSequence(seq)
        .WillOnce(DoAll(
            SaveArg<0>(&handle_alloc),
            Return(0)));
    EXPECT_CALL(*mock_hybris_gralloc, unlock(_))
        .InSequence(seq)
        .WillOnce(DoAll(
            SaveArg<0>(&handle_freed),
            Return(0)));

    registrar.secure_for_cpu(mock_native_buffer, rect);

    EXPECT_EQ(handle_alloc, handle_freed);
}

TEST_F(GrallocRegistrar, maps_buffer_for_cpu_correctly)
{
    EXPECT_CALL(*mock_hybris_gralloc, lock(
        mock_native_buffer->handle(),
        GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN,
        top,
        left,
        width,
        height,
        testing::_))
        .Times(1);

    mcla::GrallocRegistrar registrar(mock_hybris_gralloc, mock_native_handle_ops);
    registrar.secure_for_cpu(mock_native_buffer, rect);
}

TEST_F(GrallocRegistrar, lock_failure_throws)
{
    using namespace testing;
    EXPECT_CALL(*mock_hybris_gralloc, lock(_,_,_,_,_,_,_))
        .Times(1)
        .WillOnce(Return(-1));

    mcla::GrallocRegistrar registrar(mock_hybris_gralloc, mock_native_handle_ops);

    EXPECT_THROW({
        registrar.secure_for_cpu(mock_native_buffer, rect);
    }, std::runtime_error);
}

/* unlock is called in destructor. should not throw */
TEST_F(GrallocRegistrar, unlock_failure_doesnt_throw)
{
    using namespace testing;
    EXPECT_CALL(*mock_hybris_gralloc, unlock(_))
        .Times(1)
        .WillOnce(Return(-1));
    mcla::GrallocRegistrar registrar(mock_hybris_gralloc, mock_native_handle_ops);

    auto region = registrar.secure_for_cpu(mock_native_buffer, rect);
    EXPECT_NO_THROW({
        region.reset();
    });
}

TEST_F(GrallocRegistrar, produces_valid_anwb)
{
    using namespace testing;
    mcla::GrallocRegistrar registrar(mock_hybris_gralloc, mock_native_handle_ops);

    int correct_usage = GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER;
    int32_t const expected_stride_in_pixels = static_cast<int32_t>(stride / MIR_BYTES_PER_PIXEL(pf));

    auto native_handle = registrar.register_buffer(stub_package, pf);
    ASSERT_THAT(native_handle, Ne(nullptr));
    auto anwb = native_handle->anwb();
    ASSERT_THAT(anwb, Ne(nullptr));
    EXPECT_THAT(native_handle->handle(), Ne(nullptr));
    EXPECT_THAT(native_handle->handle(), Eq(native_handle->anwb()->handle));
    EXPECT_EQ(width, static_cast<uint32_t>(anwb->width));
    EXPECT_EQ(height, static_cast<uint32_t>(anwb->height));
    EXPECT_EQ(correct_usage, anwb->usage);
    EXPECT_EQ(expected_stride_in_pixels, anwb->stride);
    ASSERT_THAT(anwb->common.incRef, Ne(nullptr));
    ASSERT_THAT(anwb->common.decRef, Ne(nullptr));
    anwb->common.incRef(&anwb->common);
    anwb->common.decRef(&anwb->common);
}

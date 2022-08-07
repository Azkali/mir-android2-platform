/*
 * Copyright © 2012 Canonical Ltd.
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

#ifndef MIR_TEST_DOUBLES_MOCK_HYBRIS_GRALLOC_H_
#define MIR_TEST_DOUBLES_MOCK_HYBRIS_GRALLOC_H_

#include "cutils/native_handle.h"

#include "hybris_gralloc.h"

#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockHybrisGralloc : public graphics::android::HybrisGralloc
{
public:
    MockHybrisGralloc()
    {
        buffer_handle = mock_generate_sane_android_handle(43, 22);
        fake_stride = 300;

        using namespace testing;
        ON_CALL(*this, allocate(_,_,_,_,_,_))
            .WillByDefault(DoAll(
                SetArgReferee<4>(buffer_handle),
                SetArgReferee<5>(fake_stride),
                Return(0)));
    }

    ~MockHybrisGralloc()
    {
        free(buffer_handle);
    }

    native_handle_t* buffer_handle;
    unsigned int fake_stride;

    MOCK_METHOD(int, release, (buffer_handle_t handle, bool was_allocated), (override));
    MOCK_METHOD(int, importBuffer, (buffer_handle_t raw_handle, buffer_handle_t& out_handle), (override));
    MOCK_METHOD(int, allocate, (int width, int height, int format, int usage, buffer_handle_t &handle, uint32_t &stride), (override));
    MOCK_METHOD(int, lock, (buffer_handle_t handle, int usage, int l, int t, int w, int h, void * &vaddr), (override));
    MOCK_METHOD(int, unlock, (buffer_handle_t handle), (override));

private:
    native_handle_t* mock_generate_sane_android_handle(int numFd, int numInt)
    {
        native_handle_t *handle;
        int total=numFd + numInt;
        int header_offset=3;

        handle = (native_handle_t*) malloc(sizeof(int) * (header_offset+ total));
        handle->version = 0x389;
        handle->numFds = numFd;
        handle->numInts = numInt;
        for(int i=0; i<total; i++)
        {
            handle->data[i] = i*3;
        }

        return handle;
    }
};

}
}
}

#endif /*  MIR_TEST_DOUBLES_MOCK_HYBRIS_GRALLOC_H_ */

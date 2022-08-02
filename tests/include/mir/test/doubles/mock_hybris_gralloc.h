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

#ifndef MIR_TEST_DOUBLES_MOCK_HYBRIS_GRALLOC_H_
#define MIR_TEST_DOUBLES_MOCK_HYBRIS_GRALLOC_H_

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
    MockHybrisGralloc() = default;

    MOCK_METHOD(int, release, (buffer_handle_t handle, bool was_allocated), (override));
    MOCK_METHOD(int, importBuffer, (buffer_handle_t raw_handle, buffer_handle_t& out_handle), (override));
    MOCK_METHOD(int, allocate, (int width, int height, int format, int usage, buffer_handle_t &handle, uint32_t &stride), (override));
    MOCK_METHOD(int, lock, (buffer_handle_t handle, int usage, int l, int t, int w, int h, void * &vaddr), (override));
    MOCK_METHOD(int, unlock, (buffer_handle_t handle), (override));
};

}
}
}

#endif /*  MIR_TEST_DOUBLES_MOCK_HYBRIS_GRALLOC_H_ */

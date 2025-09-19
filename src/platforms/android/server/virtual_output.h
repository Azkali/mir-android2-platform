/*
 * Copyright © 2016 Canonical Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 */

#ifndef MIR_GRAPHICS_ANDROID_VIRTUAL_OUTPUT_H_
#define MIR_GRAPHICS_ANDROID_VIRTUAL_OUTPUT_H_

// #include "mir/graphics/virtual_output.h"  // Removed - not available in Mir 2.x

#include <functional>

namespace mir
{
namespace graphics
{
namespace android
{

// Virtual output functionality disabled for Mir 2.x compatibility
/*
class VirtualOutput : public graphics::VirtualOutput
{
public:
    explicit VirtualOutput(std::function<void()> enable_virtual_output,
                           std::function<void()> disable_virtual_output);
    ~VirtualOutput();

    void enable() override;
    void disable() override;
private:
    std::function<void()> enable_virtual_output;
    std::function<void()> disable_virtual_output;
};
*/

}
}
}
#endif

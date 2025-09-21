/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_ANDROID_DISPLAY_BUFFER_H_
#define MIR_GRAPHICS_ANDROID_DISPLAY_BUFFER_H_

#include "configurable_display_sink.h"
#include "mir/graphics/display.h"
#include "mir/graphics/egl_resources.h"
#include "mir/gl/program_factory.h"
#include "mir/renderer/gl/gl_surface.h"
#include "display_configuration.h"
#include "gl_context.h"
#include "hwc_fallback_gl_renderer.h"
#include "overlay_optimization.h"
#include <system/window.h>
#include <vector>

namespace mir
{
namespace graphics
{
namespace android
{

class DisplayDevice;
class FramebufferBundle;
class LayerList;


class DisplaySink : public ConfigurableDisplaySink,
                    public DisplaySyncGroup,
                      public graphics::gl::OutputSurface
{
public:
    //TODO: could probably just take the HalComponentFactory to reduce the
    //      number of dependencies
    DisplaySink(
        DisplayName,
        std::unique_ptr<LayerList> layer_list,
        std::shared_ptr<FramebufferBundle> const& fb_bundle,
        std::shared_ptr<DisplayDevice> const& display_device,
        std::shared_ptr<ANativeWindow> const& native_window,
        GLContext const& shared_gl_context,
        mir::gl::ProgramFactory const& program_factory,
        glm::mat2 const& transform,
        geometry::Rectangle area,
        OverlayOptimization overlay_option);

    geometry::Rectangle view_area() const override;
    void make_current() override;
    void release_current() override;
    void swap_buffers();
    std::unique_ptr<mir::graphics::Framebuffer> commit() override {
        gl_context.swap_buffers();
        return {};
    }

    bool overlay(std::vector<DisplayElement> const& renderlist);
    void set_next_image(std::unique_ptr<mir::graphics::Framebuffer> content) override;
    void post() override;
    void for_each_display_sink(std::function<void(graphics::DisplaySink&)> const& f) override;
    std::chrono::milliseconds recommended_sleep() const override;

    void bind() override;
    geometry::Size size() const override;
    auto layout() const -> Layout override
    {
        return Layout::GL;
    }

    glm::mat2 transformation() const override;
    auto maybe_create_allocator(DisplayAllocator::Tag const& type_tag) -> DisplayAllocator* override;

    void configure(MirPowerMode power_mode, glm::mat2 const& trans, geometry::Rectangle const&) override;
    DisplayContents contents() override;
    MirPowerMode power_mode() const override;
private:
    DisplayName display_name;
    std::unique_ptr<LayerList> layer_list;
    std::shared_ptr<FramebufferBundle> const fb_bundle;
    std::shared_ptr<DisplayDevice> const display_device;
    std::shared_ptr<ANativeWindow> const native_window;
    FramebufferGLContext gl_context;
    HWCFallbackGLRenderer overlay_program;
    bool overlay_enabled;
    glm::mat2 transform;
    geometry::Rectangle area;
    MirPowerMode power_mode_;
    std::unique_ptr<mir::graphics::Framebuffer> next_framebuffer;
};

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_DISPLAY_BUFFER_H_ */

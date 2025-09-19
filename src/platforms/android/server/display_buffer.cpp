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

#include "mir/graphics/transformation.h"
#include <iostream>
#include "framebuffer_bundle.h"
#include "display_buffer.h"
#include "display_device.h"
#include "hwc_layerlist.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <algorithm>
#include <sstream>

namespace mg=mir::graphics;
namespace mgl=mir::gl;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;


mga::DisplaySink::DisplaySink(
    mga::DisplayName display_name,
    std::unique_ptr<LayerList> layer_list,
    std::shared_ptr<FramebufferBundle> const& fb_bundle,
    std::shared_ptr<DisplayDevice> const& display_device,
    std::shared_ptr<ANativeWindow> const& native_window,
    mga::GLContext const& shared_gl_context,
    mgl::ProgramFactory const& program_factory,
    glm::mat2 const& transform,
    geom::Rectangle area,
    mga::OverlayOptimization overlay_option)
    : display_name(display_name),
      layer_list(std::move(layer_list)),
      fb_bundle{fb_bundle},
      display_device{display_device},
      native_window{native_window},
      gl_context{shared_gl_context, fb_bundle, native_window},
      overlay_program{program_factory, gl_context, geom::Rectangle{{0,0},fb_bundle->fb_size()}},
      overlay_enabled{overlay_option == mga::OverlayOptimization::enabled},
      transform{transform},
      area{area},
      power_mode_{mir_power_mode_on},
      next_framebuffer{nullptr}
{
}

geom::Rectangle mga::DisplaySink::view_area() const
{
    return area;
}

void mga::DisplaySink::make_current()
{
    gl_context.make_current();
}

void mga::DisplaySink::release_current()
{
    gl_context.release_current();
}

bool mga::DisplaySink::overlay(std::vector<DisplayElement> const& renderlist)
{
    // Convert DisplayElement list to RenderableList for compatibility
    // This is a simplified conversion - may need more sophisticated handling
    RenderableList legacy_list;
    for (auto const& element : renderlist)
    {
        (void)element; // Suppress unused variable warning
        // Convert DisplayElement to Renderable
        // This is a placeholder implementation
        // TODO: Implement proper conversion
    }

    glm::mat2 static const no_transformation(1, 0, 0, 1);
    if (!overlay_enabled ||
        !display_device->compatible_renderlist(legacy_list) ||
        transform != no_transformation)
        return false;

    layer_list->update_list(legacy_list, area.top_left - geom::Point());

    bool needs_commit{false};
    for (auto& layer : *layer_list)
        needs_commit |= layer.needs_commit;

    return needs_commit;
}

void mga::DisplaySink::set_next_image(std::unique_ptr<mir::graphics::Framebuffer> content)
{
    // Store the framebuffer for use in overlay() method
    next_framebuffer = std::move(content);
}

void mga::DisplaySink::post()
{
    // The DisplaySink::post() method handles the actual HWC commit
    // This method is called directly by the compositor

    // Get the display contents for this sink
    auto contents = this->contents();

    // Create a list of display contents (HWC expects a list)
    std::list<mga::DisplayContents> contents_list;
    contents_list.push_back(contents);

    // Call the HWC commit
    display_device->commit(contents_list);

    // Reset the next framebuffer after it's been used
    if (next_framebuffer)
    {
        next_framebuffer.reset();
    }
}

void mga::DisplaySink::for_each_display_sink(std::function<void(graphics::DisplaySink&)> const& f)
{
    f(*this);
}

std::chrono::milliseconds mga::DisplaySink::recommended_sleep() const
{
    // For Android platform, return zero for now
    // TODO: implement proper frame timing based on display refresh rate
    return std::chrono::milliseconds::zero();
}

void mga::DisplaySink::swap_buffers()
{
    layer_list->update_list({}, area.top_left - geom::Point());
    //HWC 1.0 cannot call eglSwapBuffers() on the display context
    if (display_device->can_swap_buffers())
        gl_context.swap_buffers();
}

void mga::DisplaySink::bind()
{
}

glm::mat2 mga::DisplaySink::transformation() const
{
    return transform;
}

void mga::DisplaySink::configure(MirPowerMode power_mode,
                                   glm::mat2 const& trans,
                                   geom::Rectangle const& a)
{
    power_mode_ = power_mode;
    area = a;
    if (power_mode_ != mir_power_mode_on)
        display_device->content_cleared();
    transform = trans;
}

mga::DisplayContents mga::DisplaySink::contents()
{
    return mga::DisplayContents{display_name, *layer_list, area.top_left - geom::Point(), gl_context, overlay_program};
}

MirPowerMode mga::DisplaySink::power_mode() const
{
    return power_mode_;
}

mir::geometry::Size mga::DisplaySink::size() const
{
    return area.size;
}

auto mga::DisplaySink::maybe_create_allocator(DisplayAllocator::Tag const& /*type_tag*/) -> DisplayAllocator*
{
    // TODO: Implement allocator creation
    // This is a placeholder implementation
    return nullptr;
}


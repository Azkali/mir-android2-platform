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
 *   Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "platform.h"

#include "graphic_buffer_allocator.h"
#include "hybris_gralloc_impl.h"
#include "mir/graphics/platform.h"
#include "display.h"
#include "hal_component_factory.h"
#include "hwc_loggers.h"
#include "sync_fence.h"
#include "native_buffer.h"
#include "native_window_report.h"

#include "mir/graphics/buffer_id.h"
#include "mir/graphics/display_report.h"
#include "mir/gl/default_program_factory.h"
#include "mir/options/option.h"
#include "mir/abnormal_exit.h"
#include "mir/assert_module_entry_point.h"
#include "mir/libname.h"
#include "mir/udev/wrapper.h"

#include <boost/throw_exception.hpp>
#include <hybris/properties/properties.h>
#include <stdexcept>
#include <mutex>
#include <string.h>

namespace mg = mir::graphics;
namespace mga = mir::graphics::android;
namespace mf = mir::frontend;
namespace mo = mir::options;

namespace
{
char const* const hwc_log_opt = "hwc-report";
char const* const hwc_overlay_opt = "disable-overlays";
char const* const log_opt_value = "log";
char const* const off_opt_value = "off";
char const* const fb_native_window_report_opt = "report-fb-native-window";

std::shared_ptr<mga::HwcReport> make_hwc_report(mo::Option const& options)
{
    if (!options.is_set(hwc_log_opt))
        return std::make_shared<mga::NullHwcReport>();

    auto opt = options.get<std::string>(hwc_log_opt);
    if (opt == log_opt_value)
        return std::make_shared<mga::HwcFormattedLogger>();
    else if (opt == off_opt_value)
        return std::make_shared<mga::NullHwcReport>();
    else
        throw mir::AbnormalExit(
                std::string("Invalid hwc-report option: " + opt + " (valid options are: \"" +
                    off_opt_value + "\" and \"" + log_opt_value + "\")"));
}

std::shared_ptr<mga::NativeWindowReport> make_native_window_report(
    mo::Option const& options,
    std::shared_ptr<mir::logging::Logger> const& logger)
{
    if (options.is_set(fb_native_window_report_opt) &&
        options.get<std::string>(fb_native_window_report_opt) == log_opt_value)
        return std::make_shared<mga::ConsoleNativeWindowReport>(logger);
    else
        return std::make_shared<mga::NullNativeWindowReport>();
}

mga::OverlayOptimization should_use_overlay_optimization(mo::Option const& options)
{
    if (!options.is_set(hwc_overlay_opt))
        return mga::OverlayOptimization::enabled;

    if (options.get<bool>(hwc_overlay_opt))
        return mga::OverlayOptimization::disabled;
    else
        return mga::OverlayOptimization::enabled;
}
}  // namespace

namespace
{
// Local stub implementations for GL context creation
class StubGLConfig : public mg::GLConfig
{
public:
    int depth_buffer_bits() const override { return 24; }
    int stencil_buffer_bits() const override { return 8; }
};

class StubDisplayReport : public mg::DisplayReport
{
public:
    void report_successful_setup_of_native_resources() override {}
    void report_successful_egl_make_current_on_construction() override {}
    void report_successful_egl_buffer_swap_on_construction() override {}
    void report_successful_display_construction() override {}
    void report_egl_configuration(EGLDisplay, EGLConfig) override {}
    void report_vsync(unsigned int, mg::Frame const&) override {}
    void report_successful_drm_mode_set_crtc_on_construction() override {}
    void report_drm_master_failure(int) override {}
    void report_vt_switch_away_failure() override {}
    void report_vt_switch_back_failure() override {}
};

std::shared_ptr<mir::renderer::gl::Context> create_gl_context()
{
    // Create a basic GL context for Android platform
    // This is similar to what eglstream-kms does
    static StubGLConfig stub_gl_config;
    static StubDisplayReport stub_display_report;

    return std::make_shared<mga::PbufferGLContext>(stub_gl_config, stub_display_report);
}
}

mga::RenderingPlatform::RenderingPlatform(
    std::shared_ptr<mga::HybrisGrallocImpl> const& hybris_gralloc,
    std::shared_ptr<mga::CommandStreamSyncFactory> const& sync_factory,
    std::shared_ptr<mga::DeviceQuirks> const& quirks) :
    hybris_gralloc(hybris_gralloc),
    sync_factory(sync_factory),
    quirks(quirks),
    dpy(eglGetDisplay(EGL_DEFAULT_DISPLAY)),
    ctx(create_gl_context())
{
    if (dpy == EGL_NO_DISPLAY)
    {
        BOOST_THROW_EXCEPTION((std::runtime_error{"Failed to get EGL display"}));
    }

    EGLint major, minor;
    if (!eglInitialize(dpy, &major, &minor))
    {
        BOOST_THROW_EXCEPTION((std::runtime_error{"Failed to initialize EGL"}));
    }
}

mga::RenderingPlatform::~RenderingPlatform() = default;

mir::UniqueModulePtr<mg::GraphicBufferAllocator> mga::RenderingPlatform::create_buffer_allocator(mg::Display const& output)
{
    auto allocator = mir::make_module_ptr<mga::GraphicBufferAllocator>(hybris_gralloc, sync_factory, quirks);

    // Set the display context for the allocator
    allocator->set_ctx(output);
    return allocator;
}

auto mga::RenderingPlatform::maybe_create_provider(mg::RenderingProvider::Tag const& type_tag)
    -> std::shared_ptr<mg::RenderingProvider>
{
    if (dynamic_cast<mg::GLRenderingProvider::Tag const*>(&type_tag))
    {
        return std::make_shared<mga::GLRenderingProvider>(ctx);
    }
    return nullptr;
}

mga::HWCDisplayProvider::HWCDisplayProvider(
    std::shared_ptr<mga::DisplayComponentFactory> const& display_buffer_builder)
    : display_buffer_builder(display_buffer_builder)
{
}

auto mga::HWCDisplayProvider::on_this_sink(mg::DisplaySink& /*sink*/) const -> bool
{
    // For Android platform, we assume all display sinks are HWC-compatible
    // This is a simplified implementation - in a real implementation, you might
    // want to check if the sink is actually using HWC hardware
    return true;
}

mga::DisplayPlatform::DisplayPlatform(
    std::shared_ptr<mg::GraphicBufferAllocator> const& buffer_allocator,
    std::shared_ptr<mga::DisplayComponentFactory> const& display_buffer_builder,
    std::shared_ptr<mg::DisplayReport> const& display_report,
    std::shared_ptr<mga::NativeWindowReport> const& native_window_report,
    mga::OverlayOptimization overlay_option,
    std::shared_ptr<mga::DeviceQuirks> const& quirks) :
    buffer_allocator(buffer_allocator),
    display_buffer_builder(display_buffer_builder),
    display_report(display_report),
    quirks(quirks),
    native_window_report(native_window_report),
    overlay_option(overlay_option),
    hwc_display_provider(std::make_shared<mga::HWCDisplayProvider>(display_buffer_builder))
{
}

mir::UniqueModulePtr<mg::Display> mga::DisplayPlatform::create_display(
    std::shared_ptr<mg::DisplayConfigurationPolicy> const&,
    std::shared_ptr<mg::GLConfig> const& gl_config)
{
    auto const program_factory = std::make_shared<mir::gl::DefaultProgramFactory>();
    return mir::make_module_ptr<mga::Display>(
            display_buffer_builder, program_factory, gl_config, display_report, native_window_report, overlay_option);
}

mir::UniqueModulePtr<mir::graphics::DisplayPlatform> create_display_platform(
    mg::SupportedDevice const&,
    std::shared_ptr<mir::options::Option> const& options,
    std::shared_ptr<mir::EmergencyCleanupRegistry> const&,
    std::shared_ptr<mir::ConsoleServices> const&,
    std::shared_ptr<mir::graphics::DisplayReport> const& report)
{
    mir::assert_entry_point_signature<mg::CreateDisplayPlatform>(&create_display_platform);
    auto quirks = std::make_shared<mga::DeviceQuirks>(mga::PropertiesOps{}, *options);
    auto hwc_report = make_hwc_report(*options);
    auto overlay_option = should_use_overlay_optimization(*options);
    hwc_report->report_overlay_optimization(overlay_option);
    auto component_factory = std::make_shared<mga::HalComponentFactory>(
        hwc_report, quirks);

    // Use nullptr for logger since we don't have a concrete logger implementation
    std::shared_ptr<mir::logging::Logger> logger = nullptr;

    return mir::make_module_ptr<mga::DisplayPlatform>(
        component_factory->the_buffer_allocator(),
        component_factory, report,
        make_native_window_report(*options, logger),
        overlay_option, quirks);
}

auto mga::DisplayPlatform::maybe_create_provider(mg::DisplayProvider::Tag const& type_tag)
    -> std::shared_ptr<mg::DisplayProvider>
{
    if (dynamic_cast<mga::HWCDisplayProvider::Tag const*>(&type_tag))
    {
        return hwc_display_provider;
    }
    return nullptr;
}

mir::UniqueModulePtr<mir::graphics::RenderingPlatform> create_rendering_platform(
    mg::SupportedDevice const&,
    std::vector<std::shared_ptr<mg::DisplayPlatform>> const&,
    mo::Option const& options,
    mir::EmergencyCleanupRegistry&)
{
    mir::assert_entry_point_signature<mg::CreateRenderPlatform>(&create_rendering_platform);

    auto quirks = std::make_shared<mga::DeviceQuirks>(mga::PropertiesOps{}, options);

    std::shared_ptr<mga::CommandStreamSyncFactory> sync_factory;
    if (quirks->working_egl_sync())
        sync_factory = std::make_shared<mga::EGLSyncFactory>();
    else
        sync_factory = std::make_shared<mga::NullCommandStreamSyncFactory>();

    auto hybris_gralloc = std::make_shared<mga::HybrisGrallocImpl>();

    return mir::make_module_ptr<mga::RenderingPlatform>(hybris_gralloc, sync_factory, quirks);
}

void add_graphics_platform_options(
    boost::program_options::options_description& config)
{
    // Check if the options alredy has been added, if so ignore and move on
    if (config.find_nothrow(hwc_log_opt, false))
       return;
    mir::assert_entry_point_signature<mg::AddPlatformOptions>(&add_graphics_platform_options);
    config.add_options()
        (hwc_log_opt,
         boost::program_options::value<std::string>()->default_value(std::string{off_opt_value}),
         "[platform-specific] How to handle the HWC logging report. [{log,off}]")
        (fb_native_window_report_opt,
         boost::program_options::value<std::string>()->default_value(std::string{off_opt_value}),
         "[platform-specific] whether to log the EGLNativeWindowType backed by the framebuffer [{log,off}]")
        (hwc_overlay_opt,
         boost::program_options::value<bool>()->default_value(false),
         "[platform-specific] Whether to disable overlay optimizations [{on,off}]");
    mga::DeviceQuirks::add_options(config);
}

static int get_android_api_level()
{
    char propval[PROP_VALUE_MAX];

    if (::property_get("ro.build.version.sdk", propval, "") < 0)
        return -1;

    return atoi(propval);
}

auto probe_display_platform(
    std::shared_ptr<mir::ConsoleServices> const& /*console*/,
    std::shared_ptr<mir::udev::Context> const& /*udev*/,
    mir::options::Option const& /*options*/) -> std::vector<mir::graphics::SupportedDevice>
{
    mir::assert_entry_point_signature<mg::PlatformProbe>(&probe_display_platform);

    if (get_android_api_level() >= 26) { // Android 8 = API level 26.
        std::vector<mg::SupportedDevice> devices;
        devices.emplace_back(
            std::unique_ptr<mir::udev::Device>{},  // No specific udev device for Android
            mg::probe::best + 16,  // High priority
            std::any{}  // No platform data
        );
        return devices;
    } else {
        return {};
    }
}

auto probe_rendering_platform(
    std::span<std::shared_ptr<mir::graphics::DisplayPlatform>> const& /*targets*/,
    mir::ConsoleServices& /*console*/,
    std::shared_ptr<mir::udev::Context> const& /*udev*/,
    mir::options::Option const& /*options*/) -> std::vector<mir::graphics::SupportedDevice>
{
    mir::assert_entry_point_signature<mg::RenderProbe>(&probe_rendering_platform);

    if (get_android_api_level() >= 26) { // Android 8 = API level 26.
        std::vector<mg::SupportedDevice> devices;
        devices.emplace_back(
            std::unique_ptr<mir::udev::Device>{},  // No specific udev device for Android
            mg::probe::best + 16,  // High priority
            std::any{}  // No platform data
        );
        return devices;
    } else {
        return {};
    }
}

namespace
{
mir::ModuleProperties const description = {
    "ubports:android2",
    MIR_VERSION_MAJOR,
    MIR_VERSION_MINOR,
    MIR_VERSION_MICRO,
    mir::libname()
};
}  // namespace

mir::ModuleProperties const* describe_graphics_module()
{
    mir::assert_entry_point_signature<mg::DescribeModule>(&describe_graphics_module);
    return &description;
}

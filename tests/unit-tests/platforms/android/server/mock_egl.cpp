/*
 * Copyright © Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mir/test/doubles/mock_egl.h"
#include <gtest/gtest.h>

namespace mtd = mir::test::doubles;

namespace
{
mtd::MockEGL* global_mock_egl = nullptr;
}

EGLConfig configs[] =
{
    (void*)3,
    (void*)4,
    (void*)5,
};

EGLint config_attrs[] =
{
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 24,
    EGL_STENCIL_SIZE, 8,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
};

EGLint context_attrs[] =
{
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

EGLint surface_attrs[] =
{
    EGL_WIDTH, 1920,
    EGL_HEIGHT, 1080,
    EGL_NONE
};

namespace mir
{
namespace test
{
namespace doubles
{

MockEGL::MockEGL()
    : fake_egl_display((EGLDisplay)0x1)
    , fake_configs(configs)
    , fake_configs_num(3)
    , fake_egl_surface((EGLSurface)0x6)
    , fake_egl_context((EGLContext)0x3)
    , fake_egl_image((EGLImageKHR)0x4)
    , fake_visual_id(0x5)
{
    global_mock_egl = this;
}

MockEGL::~MockEGL()
{
    if (global_mock_egl == this)
        global_mock_egl = nullptr;
}

void MockEGL::provide_egl_extensions()
{
    ON_CALL(*this, eglQueryString(testing::_, EGL_EXTENSIONS))
        .WillByDefault(testing::Return("EGL_KHR_image EGL_KHR_image_base EGL_KHR_gl_texture_2D_image EGL_KHR_gl_texture_cubemap_image EGL_KHR_gl_renderbuffer_image EGL_KHR_fence_sync"));
}

void MockEGL::provide_stub_platform_buffer_swapping()
{
    ON_CALL(*this, eglSwapBuffers(testing::_, testing::_))
        .WillByDefault(testing::Return(EGL_TRUE));
}

} // namespace doubles
} // namespace test
} // namespace mir

extern "C"
{
EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglChooseConfig(dpy, attrib_list, configs, config_size, num_config);
    }
    return EGL_FALSE;
}

EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglGetConfigs(dpy, configs, config_size, num_config);
    }
    return EGL_FALSE;
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglCreateContext(dpy, config, share_context, attrib_list);
    }
    return EGL_NO_CONTEXT;
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglDestroyContext(dpy, ctx);
    }
    return EGL_FALSE;
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglCreateWindowSurface(dpy, config, win, attrib_list);
    }
    return EGL_NO_SURFACE;
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglCreatePbufferSurface(dpy, config, attrib_list);
    }
    return EGL_NO_SURFACE;
}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglDestroySurface(dpy, surface);
    }
    return EGL_FALSE;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglMakeCurrent(dpy, draw, read, ctx);
    }
    return EGL_FALSE;
}

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglGetDisplay(display_id);
    }
    return EGL_NO_DISPLAY;
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglInitialize(dpy, major, minor);
    }
    return EGL_FALSE;
}

EGLBoolean eglTerminate(EGLDisplay dpy)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglTerminate(dpy);
    }
    return EGL_FALSE;
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglSwapBuffers(dpy, surface);
    }
    return EGL_FALSE;
}

const char* eglQueryString(EGLDisplay dpy, EGLint name)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglQueryString(dpy, name);
    }
    return nullptr;
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglGetConfigAttrib(dpy, config, attribute, value);
    }
    return EGL_FALSE;
}

EGLContext eglGetCurrentContext(void)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglGetCurrentContext();
    }
    return EGL_NO_CONTEXT;
}

EGLDisplay eglGetCurrentDisplay(void)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglGetCurrentDisplay();
    }
    return EGL_NO_DISPLAY;
}

EGLSurface eglGetCurrentSurface(EGLint readdraw)
{
    if (global_mock_egl)
    {
        return global_mock_egl->eglGetCurrentSurface(readdraw);
    }
    return EGL_NO_SURFACE;
}

} // extern "C"

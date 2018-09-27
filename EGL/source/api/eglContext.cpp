/**
 * Copyright (C) 2015-2018 Think Silicon S.A. (https://think-silicon.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public v3
 * License as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

/**
 *  @file       eglContext.cpp
 *  @author     Think Silicon
 *  @date       25/07/2018
 *  @version    1.0
 *
 *  @brief      EGL Context functionality. It connects EGL to the client API
 *
 */

#include "utils/egl_defs.h"
#include "eglContext.h"
#include "eglSurface.h"

EGLContext_t::EGLContext_t(EGLenum rendering_api, const EGLint *attribList)
: mAPIContext(nullptr), mRenderingAPI(rendering_api), mAPIInterface(nullptr),
mDisplay(EGL_NO_DISPLAY), mReadSurface(EGL_NO_SURFACE), mDrawSurface(EGL_NO_SURFACE)
{
    FUN_ENTRY(EGL_LOG_TRACE);

    mClientVersion = GetClientVersionFromConfig(attribList);
}

EGLContext_t::~EGLContext_t()
{
    FUN_ENTRY(EGL_LOG_TRACE);
}

void
EGLContext_t::Release()
{
    FUN_ENTRY(EGL_LOG_TRACE);

    mAPIInterface->release_system_fbo_cb(mAPIContext);
}

EGLenum
EGLContext_t::GetClientVersionFromConfig(const EGLint *attribList)
{
    FUN_ENTRY(EGL_LOG_TRACE);

    if(!attribList || attribList[0] != EGL_CONTEXT_CLIENT_VERSION) {
        return EGL_GL_VERSION_1;
    }

    return attribList[1];
}

EGLBoolean
EGLContext_t::Create()
{
    FUN_ENTRY(EGL_LOG_DEBUG);

    rendering_api_return_e ret = RENDERING_API_init_api(mRenderingAPI, mClientVersion, &mAPIInterface);

    if(ret != RENDERING_API_INIT_SUCCESS && ret != RENDERING_API_ALREADY_INIT) {
        return EGL_FALSE;
    }

    mAPIContext = mAPIInterface->create_context_cb();

    return mAPIContext != nullptr ? EGL_TRUE : EGL_FALSE;
}

EGLBoolean
EGLContext_t::Destroy()
{
    FUN_ENTRY(EGL_LOG_DEBUG);

    if (mAPIInterface == nullptr) {
        return EGL_FALSE;
    }

    //TODO: Include Error Handling in Final implementation
    mAPIInterface->delete_context_cb(mAPIContext);

    return EGL_TRUE;
}

EGLBoolean
EGLContext_t::MakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read)
{
    FUN_ENTRY(EGL_LOG_DEBUG);

    //TODO: Include Error Handling in Final implementation
    mDisplay = dpy;

    EGLSurface_t *drawSurface = static_cast<EGLSurface_t *>(draw);
    if(drawSurface == nullptr){
        return EGL_BAD_SURFACE;
    }
    // TODO: support correct construction/destruction of pbuffers
    if (drawSurface->GetType() != EGL_WINDOW_BIT) {
        return EGL_TRUE;
    }

    if(drawSurface) {
        mDrawSurface = draw;
        mAPIInterface->set_write_surface_cb(mAPIContext, drawSurface->GetEGLSurfaceInterface());
    }

    EGLSurface_t *readSurface = static_cast<EGLSurface_t *>(read);
    if(readSurface) {
        mReadSurface = read;
        mAPIInterface->set_read_surface_cb(mAPIContext, readSurface->GetEGLSurfaceInterface());
    }

    return EGL_TRUE;
}

void
EGLContext_t::SetNextImageIndex(uint32_t index)
{
    FUN_ENTRY(EGL_LOG_TRACE);

    mAPIInterface->set_next_image_index_cb(mAPIContext, index);
}

GLPROC
EGLContext_t::GetProcAddr(const char* procname)
{
    FUN_ENTRY(EGL_LOG_TRACE);

    return mAPIInterface->get_proc_addr_cb(procname);
}

void
EGLContext_t::Finish()
{
    FUN_ENTRY(EGL_LOG_DEBUG);

    mAPIInterface->finish_cb(mAPIContext);
}

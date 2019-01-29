/*     Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#if PLATFORM_WIN32

#   ifndef GLEW_STATIC
#       define GLEW_STATIC // Must be defined to use static version of glew
#   endif
#   include "GL/glew.h"
    // Glew includes <windows.h>
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include "GL/wglew.h"
#   include <GL/GL.h>

#elif PLATFORM_LINUX

#   ifndef GLEW_STATIC
#       define GLEW_STATIC // Must be defined to use static version of glew
#   endif
#   ifndef GLEW_NO_GLU
#       define GLEW_NO_GLU
#   endif

#   include "GL/glew.h"
#   include <GL/glx.h>

// Undefine beautiful defines from GL/glx.h -> X11/Xlib.h
#   ifdef Bool
#       undef Bool
#   endif
#   ifdef True
#       undef True
#   endif
#   ifdef False
#       undef False
#   endif
#   ifdef Status
#       undef Status
#   endif
#   ifdef Success
#       undef Success
#   endif

#elif PLATFORM_MACOS

#   ifndef GLEW_STATIC
#       define GLEW_STATIC // Must be defined to use static version of glew
#   endif
#   ifndef GLEW_NO_GLU
#       define GLEW_NO_GLU
#   endif

#   include "GL/glew.h"

#elif PLATFORM_ANDROID

#   include <GLES3/gl3.h>
#   include <GLES3/gl3ext.h>
    // GLStubs must be included after GLFeatures!
#   include "GLStubsAndroid.h"

#elif PLATFORM_IOS

#   include <OpenGLES/ES3/gl.h>
#   include <OpenGLES/ES3/glext.h>
#   include "GLStubsIOS.h"

#else
#   error Unsupported platform
#endif

#include "Errors.h"

#include "PlatformDefinitions.h"
#include "RefCntAutoPtr.h"
#include "DebugUtilities.h"
#include "GLObjectWrapper.h"
#include "ValidatedCast.h"
#include "RenderDevice.h"
#include "BaseInterfacesGL.h"

#define CHECK_GL_ERROR(...)\
{                                       \
    auto err = glGetError();            \
    if( err != GL_NO_ERROR )            \
    {                                   \
        LogError<false>(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__, "\nGL Error Code: ", err); \
        UNEXPECTED("Error");            \
    }                                   \
}

#define CHECK_GL_ERROR_AND_THROW(...)\
{                                       \
    auto err = glGetError();            \
    if( err != GL_NO_ERROR )            \
        LogError<true>(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__, "\nGL Error Code: ", err); \
}

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

#pragma once

/// \file
/// Declaration of Diligent::FenceGLImpl class

#include "stl/deque.h"
#include "stl/utility.h"

#include "FenceGL.h"
#include "RenderDeviceGL.h"
#include "FenceBase.h"
#include "GLObjectWrapper.h"
#include "RenderDeviceGLImpl.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Implementation of the Diligent::IFenceGL interface
class FenceGLImpl final : public FenceBase<IFenceGL, RenderDeviceGLImpl>
{
public:
    using TFenceBase = FenceBase<IFenceGL, RenderDeviceGLImpl>;

    FenceGLImpl(IReferenceCounters* pRefCounters,
                RenderDeviceGLImpl* pDevice, 
                const FenceDesc&    Desc);
    ~FenceGLImpl();

    virtual Uint64 GetCompletedValue()override final;

    /// Resets the fence to the specified value. 
    virtual void Reset(Uint64 Value)override final;

    void AddPendingFence(GLObjectWrappers::GLSyncObj&& Fence, Uint64 Value)
    {
        m_PendingFences.emplace_back(Value, stl::move(Fence));
    }

private:
    stl::deque<stl::pair<Uint64, GLObjectWrappers::GLSyncObj> > m_PendingFences;
    volatile Uint64 m_LastCompletedFenceValue = 0;
};

}


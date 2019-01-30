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

#include "pch.h"
#include <sstream>
#include "stl/utility.h"
#include "stl/array.h"
#include "RenderPassCache.h"
#include "RenderDeviceVkImpl.h"
#include "PipelineStateVkImpl.h"

namespace Diligent
{

RenderPassCache::~RenderPassCache()
{
    auto& FBCache = m_DeviceVkImpl.GetFramebufferCache();
    for(auto it = m_Cache.begin(); it != m_Cache.end(); ++it)
    {
        FBCache.OnDestroyRenderPass(it->second);
    }
}

VkRenderPass RenderPassCache::GetRenderPass(const RenderPassCacheKey& Key)
{
    std::lock_guard<std::mutex> Lock(m_Mutex);
    auto it = m_Cache.find(Key);
    if(it == m_Cache.end())
    {
        // Do not zero-intitialize arrays
        stl::array<VkAttachmentDescription, MaxRenderTargets+1> Attachments;
        stl::array<VkAttachmentReference,   MaxRenderTargets+1> AttachmentReferences;
        VkSubpassDescription                               Subpass;
        auto RenderPassCI = PipelineStateVkImpl::GetRenderPassCreateInfo(Key.NumRenderTargets, Key.RTVFormats, Key.DSVFormat,
                                                                         Key.SampleCount, Attachments, AttachmentReferences, Subpass);
        std::stringstream PassNameSS;
        PassNameSS << "Render pass: rt count: " << Key.NumRenderTargets << "; sample count: "<< Key.SampleCount 
                   << "; DSV Format: " << GetTextureFormatAttribs(Key.DSVFormat).Name << "; RTV Formats: ";
        for(Uint32 rt = 0; rt < Key.NumRenderTargets; ++rt)
            PassNameSS << (rt > 0 ? ", " : "") << GetTextureFormatAttribs(Key.RTVFormats[rt]).Name;
        auto RenderPass = m_DeviceVkImpl.GetLogicalDevice().CreateRenderPass(RenderPassCI, PassNameSS.str().c_str());
        VERIFY_EXPR(RenderPass != VK_NULL_HANDLE);
        it = m_Cache.emplace(Key, stl::move(RenderPass)).first;
    }

    return it->second;
}

}

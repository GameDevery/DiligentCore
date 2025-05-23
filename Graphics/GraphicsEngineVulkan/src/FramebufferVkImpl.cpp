/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include "FramebufferVkImpl.hpp"

#include <vector>

#include "RenderDeviceVkImpl.hpp"
#include "RenderPassVkImpl.hpp"
#include "TextureViewVkImpl.hpp"

#include "EngineMemory.h"

namespace Diligent
{

FramebufferVkImpl::FramebufferVkImpl(IReferenceCounters*    pRefCounters,
                                     RenderDeviceVkImpl*    pDevice,
                                     const FramebufferDesc& Desc) :
    TFramebufferBase{pRefCounters, pDevice, Desc}
{
    VkFramebufferCreateInfo FramebufferCI = {};

    FramebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    FramebufferCI.pNext = nullptr;
    FramebufferCI.flags = 0;

    RenderPassVkImpl* pRenderPassVkImpl = ClassPtrCast<RenderPassVkImpl>(m_Desc.pRenderPass);
    FramebufferCI.renderPass            = pRenderPassVkImpl->GetVkRenderPass();

    FramebufferCI.attachmentCount = m_Desc.AttachmentCount;

    std::vector<VkImageView> vkImgViews(m_Desc.AttachmentCount);
    for (Uint32 i = 0; i < m_Desc.AttachmentCount; ++i)
    {
        if (ITextureView* pView = m_Desc.ppAttachments[i])
        {
            vkImgViews[i] = ClassPtrCast<TextureViewVkImpl>(pView)->GetVulkanImageView();
        }
    }
    FramebufferCI.pAttachments = vkImgViews.data();

    FramebufferCI.width  = m_Desc.Width;
    FramebufferCI.height = m_Desc.Height;
    FramebufferCI.layers = m_Desc.NumArraySlices;

    const VulkanUtilities::LogicalDevice& LogicalDevice = pDevice->GetLogicalDevice();

    m_VkFramebuffer = LogicalDevice.CreateFramebuffer(FramebufferCI, m_Desc.Name);
    if (!m_VkFramebuffer)
    {
        LOG_ERROR_AND_THROW("Failed to create Vulkan framebuffer");
    }
}

FramebufferVkImpl::~FramebufferVkImpl()
{
    m_pDevice->SafeReleaseDeviceObject(std::move(m_VkFramebuffer), ~Uint64{0});
}

} // namespace Diligent

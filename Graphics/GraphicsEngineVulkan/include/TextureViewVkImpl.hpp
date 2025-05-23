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

#pragma once

/// \file
/// Declaration of Diligent::TextureViewVkImpl class

#include "EngineVkImplTraits.hpp"
#include "TextureViewBase.hpp"
#include "VulkanUtilities/ObjectWrappers.hpp"

namespace Diligent
{

/// Texture view implementation in Vulkan backend.
class TextureViewVkImpl final : public TextureViewBase<EngineVkImplTraits>
{
public:
    using TTextureViewBase = TextureViewBase<EngineVkImplTraits>;

    TextureViewVkImpl(IReferenceCounters*                 pRefCounters,
                      RenderDeviceVkImpl*                 pDevice,
                      const TextureViewDesc&              ViewDesc,
                      ITexture*                           pTexture,
                      VulkanUtilities::ImageViewWrapper&& ImgView,
                      bool                                bIsDefaultView);
    ~TextureViewVkImpl();

    IMPLEMENT_QUERY_INTERFACE_IN_PLACE(IID_TextureViewVk, TTextureViewBase)

    /// Implementation of ITextureViewVk::GetVulkanImageView().
    virtual VkImageView DILIGENT_CALL_TYPE GetVulkanImageView() const override final { return m_ImageView; }

protected:
    /// Vulkan image view descriptor handle
    VulkanUtilities::ImageViewWrapper m_ImageView;
};

} // namespace Diligent

/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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
/// Definition of the Diligent::ITextureViewD3D12 interface

#include "../../GraphicsEngine/interface/TextureView.h"

namespace Diligent
{

// {BDFBD325-0699-4720-BC0E-BF84086EC033}
static constexpr INTERFACE_ID IID_TextureViewD3D12 =
    {0xbdfbd325, 0x699, 0x4720, {0xbc, 0xe, 0xbf, 0x84, 0x8, 0x6e, 0xc0, 0x33}};

/// Exposes Direct3D12-specific functionality of a texture view object.
class ITextureViewD3D12 : public ITextureView
{
public:
    /// Returns CPU descriptor handle of the texture view.
    virtual D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle() = 0;
};

} // namespace Diligent

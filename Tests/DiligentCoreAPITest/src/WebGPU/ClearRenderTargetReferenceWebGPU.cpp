/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

#include "WebGPU/TestingEnvironmentWebGPU.hpp"
#include "WebGPU/TestingSwapChainWebGPU.hpp"

namespace Diligent
{

namespace Testing
{

void ClearRenderTargetReferenceWebGPU(ISwapChain* pSwapChain, const float ClearColor[])
{
    auto* pEnvWebGPU              = TestingEnvironmentWebGPU::GetInstance();
    auto* pTestingSwapChainWebGPU = ClassPtrCast<TestingSwapChainWebGPU>(pSwapChain);

    auto wgpuCmdEncoder = pEnvWebGPU->CreateCommandEncoder();

    WGPURenderPassColorAttachment wgpuRenderPassColorAttachment{};
    wgpuRenderPassColorAttachment.view       = pTestingSwapChainWebGPU->GetWebGPUColorTextureView();
    wgpuRenderPassColorAttachment.loadOp     = WGPULoadOp_Clear;
    wgpuRenderPassColorAttachment.storeOp    = WGPUStoreOp_Store;
    wgpuRenderPassColorAttachment.clearValue = WGPUColor{ClearColor[0], ClearColor[1], ClearColor[2], ClearColor[3]};
    wgpuRenderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    WGPURenderPassDescriptor wgpuRenderPassDesc = {};
    wgpuRenderPassDesc.colorAttachmentCount     = 1;
    wgpuRenderPassDesc.colorAttachments         = &wgpuRenderPassColorAttachment;

    WGPURenderPassEncoder wgpuRenderPass = wgpuCommandEncoderBeginRenderPass(wgpuCmdEncoder, &wgpuRenderPassDesc);
    wgpuRenderPassEncoderEnd(wgpuRenderPass);
    pEnvWebGPU->SubmitCommandEncoder(wgpuCmdEncoder);

    wgpuRenderPassEncoderRelease(wgpuRenderPass);
    wgpuCommandEncoderRelease(wgpuCmdEncoder);
}

} // namespace Testing

} // namespace Diligent

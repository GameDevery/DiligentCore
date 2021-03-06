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
/// Declaration of Diligent::PipelineStateD3D12Impl class

#include "RenderDeviceD3D12.h"
#include "PipelineStateD3D12.h"
#include "PipelineStateBase.h"
#include "RootSignature.h"
#include "ShaderResourceLayoutD3D12.h"
#include "SRBMemoryAllocator.h"
#include "RenderDeviceD3D12Impl.h"
#include "ShaderVariableD3D12.h"

namespace Diligent
{

class FixedBlockMemoryAllocator;

/// Pipeline state object implementation in Direct3D12 backend.
class PipelineStateD3D12Impl final : public PipelineStateBase<IPipelineStateD3D12, RenderDeviceD3D12Impl>
{
public:
    using TPipelineStateBase = PipelineStateBase<IPipelineStateD3D12, RenderDeviceD3D12Impl>;

    PipelineStateD3D12Impl(IReferenceCounters* pRefCounters, RenderDeviceD3D12Impl* pDeviceD3D12, const PipelineStateDesc& PipelineDesc);
    ~PipelineStateD3D12Impl();

    virtual void QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    /// Implementation of IPipelineState::BindStaticResources() in Direct3D12 backend.
    virtual void BindStaticResources(Uint32 ShaderFlags, IResourceMapping* pResourceMapping, Uint32 Flags) override final;

    /// Implementation of IPipelineState::GetStaticVariableCount() in Direct3D12 backend.
    virtual Uint32 GetStaticVariableCount(SHADER_TYPE ShaderType) const override final;

    /// Implementation of IPipelineState::GetStaticVariableByName() in Direct3D12 backend.
    virtual IShaderResourceVariable* GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final;

    /// Implementation of IPipelineState::GetStaticVariableByIndex() in Direct3D12 backend.
    virtual IShaderResourceVariable* GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    /// Implementation of IPipelineState::CreateShaderResourceBinding() in Direct3D12 backend.
    virtual void CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding, bool InitStaticResources) override final;

    /// Implementation of IPipelineState::IsCompatibleWith() in Direct3D12 backend.
    virtual bool IsCompatibleWith(const IPipelineState* pPSO) const override final;

    /// Implementation of IPipelineStateD3D12::GetD3D12PipelineState().
    virtual ID3D12PipelineState* GetD3D12PipelineState() const override final { return m_pd3d12PSO; }

    /// Implementation of IPipelineStateD3D12::GetD3D12RootSignature().
    virtual ID3D12RootSignature* GetD3D12RootSignature() const override final { return m_RootSig.GetD3D12RootSignature(); }

    struct CommitAndTransitionResourcesAttribs
    {
        Uint32                  CtxId                  = 0;
        IShaderResourceBinding* pShaderResourceBinding = nullptr;
        bool                    CommitResources        = false;
        bool                    TransitionResources    = false;
        bool                    ValidateStates         = false;
    };
    ShaderResourceCacheD3D12* CommitAndTransitionShaderResources(class DeviceContextD3D12Impl*        pDeviceCtx,
                                                                 class CommandContext&                CmdCtx,
                                                                 CommitAndTransitionResourcesAttribs& Attrib) const;

    const RootSignature& GetRootSignature() const { return m_RootSig; }

    const ShaderResourceLayoutD3D12& GetShaderResLayout(Uint32 ShaderInd) const
    {
        VERIFY_EXPR(ShaderInd < m_NumShaders);
        return m_pShaderResourceLayouts[ShaderInd];
    }

    const ShaderResourceLayoutD3D12& GetStaticShaderResLayout(Uint32 ShaderInd) const
    {
        VERIFY_EXPR(ShaderInd < m_NumShaders);
        return m_pShaderResourceLayouts[m_NumShaders + ShaderInd];
    }

    ShaderResourceCacheD3D12& GetStaticShaderResCache(Uint32 ShaderInd) const
    {
        VERIFY_EXPR(ShaderInd < m_NumShaders);
        return m_pStaticResourceCaches[ShaderInd];
    }

    bool ContainsShaderResources() const;

    SRBMemoryAllocator& GetSRBMemoryAllocator()
    {
        return m_SRBMemAllocator;
    }

private:
    CComPtr<ID3D12PipelineState> m_pd3d12PSO;
    RootSignature                m_RootSig;

    // Must be defined before default SRB
    SRBMemoryAllocator m_SRBMemAllocator;

    ShaderResourceLayoutD3D12*  m_pShaderResourceLayouts = nullptr;
    ShaderResourceCacheD3D12*   m_pStaticResourceCaches  = nullptr;
    ShaderVariableManagerD3D12* m_pStaticVarManagers     = nullptr;
    // Resource layout index in m_ResourceLayouts[] array for every shader stage
    Int8 m_ResourceLayoutIndex[6] = {-1, -1, -1, -1, -1, -1};
};

} // namespace Diligent

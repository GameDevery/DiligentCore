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
#include "DeviceContextD3D11Impl.hpp"
#include "BufferD3D11Impl.hpp"
#include "ShaderD3D11Impl.hpp"
#include "Texture1D_D3D11.hpp"
#include "Texture2D_D3D11.hpp"
#include "Texture3D_D3D11.hpp"
#include "SamplerD3D11Impl.hpp"
#include "D3D11TypeConversions.hpp"
#include "TextureViewD3D11Impl.hpp"
#include "PipelineStateD3D11Impl.hpp"
#include "ShaderResourceBindingD3D11Impl.hpp"
#include "CommandListD3D11Impl.hpp"
#include "RenderDeviceD3D11Impl.hpp"
#include "FenceD3D11Impl.hpp"
#include "QueryD3D11Impl.hpp"
#include "DeviceMemoryD3D11Impl.hpp"
#include "D3D11TileMappingHelper.hpp"

namespace Diligent
{

DeviceContextD3D11Impl::DeviceContextD3D11Impl(IReferenceCounters*      pRefCounters,
                                               RenderDeviceD3D11Impl*   pDevice,
                                               const DeviceContextDesc& Desc,
                                               ID3D11DeviceContext1*    pd3d11DeviceContext) :
    // clang-format off
    TDeviceContextBase
    {
        pRefCounters,
        pDevice,
        Desc
    },
    m_pd3d11DeviceContext {pd3d11DeviceContext},
#ifdef DILIGENT_DEVELOPMENT
    m_D3D11ValidationFlags{pDevice->GetProperties().D3D11ValidationFlags},
#endif
    m_CmdListAllocator    {GetRawAllocator(), sizeof(CommandListD3D11Impl), 64}
// clang-format on
{
}

IMPLEMENT_QUERY_INTERFACE(DeviceContextD3D11Impl, IID_DeviceContextD3D11, TDeviceContextBase)


void DeviceContextD3D11Impl::Begin(Uint32 ImmediateContextId)
{
    DEV_CHECK_ERR(ImmediateContextId == 0, "Direct3D11 supports only one immediate context");
    TDeviceContextBase::Begin(DeviceContextIndex{ImmediateContextId}, COMMAND_QUEUE_TYPE_GRAPHICS);
}

void DeviceContextD3D11Impl::SetPipelineState(IPipelineState* pPipelineState)
{
    if (!TDeviceContextBase::SetPipelineState(pPipelineState, PipelineStateD3D11Impl::IID_InternalImpl))
        return;

    const PipelineStateDesc& Desc = m_pPipelineState->GetDesc();
    if (Desc.PipelineType == PIPELINE_TYPE_COMPUTE)
    {
        ID3D11ComputeShader* pd3d11CS = m_pPipelineState->GetD3D11ComputeShader();
        if (pd3d11CS == nullptr)
        {
            LOG_ERROR("Compute shader is not set in the pipeline");
            return;
        }

#define COMMIT_SHADER(SN, ShaderName)                                       \
    do                                                                      \
    {                                                                       \
        auto* pd3d11Shader = m_pPipelineState->GetD3D11##ShaderName();      \
        if (m_CommittedD3DShaders[SN##Ind] != pd3d11Shader)                 \
        {                                                                   \
            m_CommittedD3DShaders[SN##Ind] = pd3d11Shader;                  \
            m_pd3d11DeviceContext->SN##SetShader(pd3d11Shader, nullptr, 0); \
        }                                                                   \
    } while (false)

        COMMIT_SHADER(CS, ComputeShader);
    }
    else if (Desc.PipelineType == PIPELINE_TYPE_GRAPHICS)
    {
        COMMIT_SHADER(VS, VertexShader);
        COMMIT_SHADER(PS, PixelShader);
        COMMIT_SHADER(GS, GeometryShader);
        COMMIT_SHADER(HS, HullShader);
        COMMIT_SHADER(DS, DomainShader);
#undef COMMIT_SHADER

        const GraphicsPipelineDesc& GraphicsPipeline = m_pPipelineState->GetGraphicsPipelineDesc();

        m_pd3d11DeviceContext->OMSetBlendState(m_pPipelineState->GetD3D11BlendState(), m_BlendFactors, GraphicsPipeline.SampleMask);
        m_pd3d11DeviceContext->RSSetState(m_pPipelineState->GetD3D11RasterizerState());
        m_pd3d11DeviceContext->OMSetDepthStencilState(m_pPipelineState->GetD3D11DepthStencilState(), m_StencilRef);

        ID3D11InputLayout* pd3d11InputLayout = m_pPipelineState->GetD3D11InputLayout();
        // It is safe to perform raw pointer comparison as the device context
        // keeps bound input layout alive
        if (m_CommittedD3D11InputLayout != pd3d11InputLayout)
        {
            m_pd3d11DeviceContext->IASetInputLayout(pd3d11InputLayout);
            m_CommittedD3D11InputLayout = pd3d11InputLayout;
        }

        PRIMITIVE_TOPOLOGY PrimTopology = GraphicsPipeline.PrimitiveTopology;
        if (m_CommittedPrimitiveTopology != PrimTopology)
        {
            m_CommittedPrimitiveTopology = PrimTopology;
            m_CommittedD3D11PrimTopology = TopologyToD3D11Topology(PrimTopology);
            m_pd3d11DeviceContext->IASetPrimitiveTopology(m_CommittedD3D11PrimTopology);
        }
    }
    else
    {
        UNEXPECTED(GetPipelineTypeString(Desc.PipelineType), " pipelines '", Desc.Name, "' are not supported in Direct3D11 backend");
    }

    Uint32 DvpCompatibleSRBCount = 0;
    PrepareCommittedResources(m_BindInfo, DvpCompatibleSRBCount);

    const SHADER_TYPE ActiveStages = m_pPipelineState->GetActiveShaderStages();
    if (m_BindInfo.ActiveStages != ActiveStages)
    {
        m_BindInfo.ActiveStages = ActiveStages;
        // Reset all SRBs if the new pipeline has different shader stages.
        m_BindInfo.MakeAllStale();
    }
}

// clang-format off

/// Helper macro used to create an array of device context methods to
/// set particular resource for every shader stage
#define DEFINE_D3D11CTX_FUNC_POINTERS(ArrayName, FuncName) \
    using T##FuncName##Type = decltype (&ID3D11DeviceContext::VS##FuncName); \
    static const T##FuncName##Type ArrayName[] = \
    {                                        \
        &ID3D11DeviceContext::VS##FuncName,  \
        &ID3D11DeviceContext::PS##FuncName,  \
        &ID3D11DeviceContext::GS##FuncName,  \
        &ID3D11DeviceContext::HS##FuncName,  \
        &ID3D11DeviceContext::DS##FuncName,  \
        &ID3D11DeviceContext::CS##FuncName   \
    };

    DEFINE_D3D11CTX_FUNC_POINTERS(SetCBMethods,      SetConstantBuffers)
    DEFINE_D3D11CTX_FUNC_POINTERS(SetSRVMethods,     SetShaderResources)
    DEFINE_D3D11CTX_FUNC_POINTERS(SetSamplerMethods, SetSamplers)

typedef decltype (&ID3D11DeviceContext::CSSetUnorderedAccessViews) TSetUnorderedAccessViewsType;
static const TSetUnorderedAccessViewsType SetUAVMethods[] =
{
    nullptr, // VS
    reinterpret_cast<TSetUnorderedAccessViewsType>(&ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews),  // Little hack for PS
    nullptr, // GS
    nullptr, // HS
    nullptr, // DS
    &ID3D11DeviceContext::CSSetUnorderedAccessViews // CS
};

static const decltype (&ID3D11DeviceContext1::VSSetConstantBuffers1) SetCB1Methods[] =
{
    &ID3D11DeviceContext1::VSSetConstantBuffers1,
    &ID3D11DeviceContext1::PSSetConstantBuffers1,
    &ID3D11DeviceContext1::GSSetConstantBuffers1,
    &ID3D11DeviceContext1::HSSetConstantBuffers1,
    &ID3D11DeviceContext1::DSSetConstantBuffers1,
    &ID3D11DeviceContext1::CSSetConstantBuffers1
};

// clang-format on

void DeviceContextD3D11Impl::TransitionShaderResources(IShaderResourceBinding* pShaderResourceBinding)
{
    DEV_CHECK_ERR(pShaderResourceBinding != nullptr, "Shader resource binding must not be null");
    if (m_pActiveRenderPass)
    {
        LOG_ERROR_MESSAGE("State transitions are not allowed inside a render pass.");
        return;
    }

    ShaderResourceBindingD3D11Impl* pShaderResBindingD3D11 = ClassPtrCast<ShaderResourceBindingD3D11Impl>(pShaderResourceBinding);
    ShaderResourceCacheD3D11&       ResourceCache          = pShaderResBindingD3D11->GetResourceCache();

    ResourceCache.TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Transition>(*this);
}

void DeviceContextD3D11Impl::CommitShaderResources(IShaderResourceBinding* pShaderResourceBinding, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    DeviceContextBase::CommitShaderResources(pShaderResourceBinding, StateTransitionMode, 0 /*Dummy*/);

    ShaderResourceBindingD3D11Impl* const pShaderResBindingD3D11 = ClassPtrCast<ShaderResourceBindingD3D11Impl>(pShaderResourceBinding);
    const Uint32                          SRBIndex               = pShaderResBindingD3D11->GetBindingIndex();
    ShaderResourceCacheD3D11&             ResourceCache          = pShaderResBindingD3D11->GetResourceCache();

    m_BindInfo.Set(SRBIndex, pShaderResBindingD3D11);

    if (StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION)
    {
        ResourceCache.TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Transition>(*this);
    }
#ifdef DILIGENT_DEVELOPMENT
    else if (StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_VERIFY)
    {
        ResourceCache.TransitionResourceStates<ShaderResourceCacheD3D11::StateTransitionMode::Verify>(*this);
    }
#endif

#ifdef DILIGENT_DEBUG
    ResourceCache.DbgVerifyDynamicBufferMasks();
#endif
}


void DeviceContextD3D11Impl::BindCacheResources(const ShaderResourceCacheD3D11&    ResourceCache,
                                                const D3D11ShaderResourceCounters& BaseBindings,
                                                PixelShaderUAVBindMode&            PsUavBindMode)
{
    for (SHADER_TYPE ActiveStages = m_BindInfo.ActiveStages; ActiveStages != SHADER_TYPE_UNKNOWN;)
    {
        const Int32       ShaderInd  = ExtractFirstShaderStageIndex(ActiveStages);
        const SHADER_TYPE ShaderType = GetShaderTypeFromIndex(ShaderInd);

        if (ResourceCache.GetCBCount(ShaderInd) > 0)
        {
            ID3D11Buffer** d3d11CBs       = m_CommittedRes.d3d11CBs[ShaderInd];
            UINT*          FirstConstants = m_CommittedRes.CBFirstConstants[ShaderInd];
            UINT*          NumConstants   = m_CommittedRes.CBNumConstants[ShaderInd];
            if (ShaderResourceCacheD3D11::MinMaxSlot Slots = ResourceCache.BindCBs(ShaderInd, d3d11CBs, FirstConstants, NumConstants, BaseBindings))
            {
                auto SetCB1Method = SetCB1Methods[ShaderInd];
                (m_pd3d11DeviceContext->*SetCB1Method)(Slots.MinSlot, Slots.MaxSlot - Slots.MinSlot + 1,
                                                       d3d11CBs + Slots.MinSlot,
                                                       FirstConstants + Slots.MinSlot,
                                                       NumConstants + Slots.MinSlot);
                m_CommittedRes.NumCBs[ShaderInd] = std::max(m_CommittedRes.NumCBs[ShaderInd], static_cast<Uint8>(Slots.MaxSlot + 1));
            }
#ifdef DILIGENT_DEVELOPMENT
            if (m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE)
            {
                DvpVerifyCommittedCBs(ShaderType);
            }
#endif
        }

        if (ResourceCache.GetSRVCount(ShaderInd) > 0)
        {
            ID3D11ShaderResourceView** d3d11SRVs   = m_CommittedRes.d3d11SRVs[ShaderInd];
            ID3D11Resource**           d3d11SRVRes = m_CommittedRes.d3d11SRVResources[ShaderInd];
            if (ShaderResourceCacheD3D11::MinMaxSlot Slots = ResourceCache.BindResourceViews<D3D11_RESOURCE_RANGE_SRV>(ShaderInd, d3d11SRVs, d3d11SRVRes, BaseBindings))
            {
                auto SetSRVMethod = SetSRVMethods[ShaderInd];
                (m_pd3d11DeviceContext->*SetSRVMethod)(Slots.MinSlot, Slots.MaxSlot - Slots.MinSlot + 1, d3d11SRVs + Slots.MinSlot);
                m_CommittedRes.NumSRVs[ShaderInd] = std::max(m_CommittedRes.NumSRVs[ShaderInd], static_cast<Uint8>(Slots.MaxSlot + 1));
            }
#ifdef DILIGENT_DEVELOPMENT
            if (m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE)
            {
                DvpVerifyCommittedSRVs(ShaderType);
            }
#endif
        }

        if (ResourceCache.GetSamplerCount(ShaderInd) > 0)
        {
            ID3D11SamplerState** d3d11Samplers = m_CommittedRes.d3d11Samplers[ShaderInd];
            if (ShaderResourceCacheD3D11::MinMaxSlot Slots = ResourceCache.BindResources<D3D11_RESOURCE_RANGE_SAMPLER>(ShaderInd, d3d11Samplers, BaseBindings))
            {
                auto SetSamplerMethod = SetSamplerMethods[ShaderInd];
                (m_pd3d11DeviceContext->*SetSamplerMethod)(Slots.MinSlot, Slots.MaxSlot - Slots.MinSlot + 1, d3d11Samplers + Slots.MinSlot);
                m_CommittedRes.NumSamplers[ShaderInd] = std::max(m_CommittedRes.NumSamplers[ShaderInd], static_cast<Uint8>(Slots.MaxSlot + 1));
            }
#ifdef DILIGENT_DEVELOPMENT
            if (m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE)
            {
                DvpVerifyCommittedSamplers(ShaderType);
            }
#endif
        }

        if (ResourceCache.GetUAVCount(ShaderInd) > 0)
        {
            if (ShaderInd == PSInd && PsUavBindMode != PixelShaderUAVBindMode::Bind)
                PsUavBindMode = PixelShaderUAVBindMode::Keep;

            ID3D11UnorderedAccessView** d3d11UAVs   = m_CommittedRes.d3d11UAVs[ShaderInd];
            ID3D11Resource**            d3d11UAVRes = m_CommittedRes.d3d11UAVResources[ShaderInd];
            if (ShaderResourceCacheD3D11::MinMaxSlot Slots = ResourceCache.BindResourceViews<D3D11_RESOURCE_RANGE_UAV>(ShaderInd, d3d11UAVs, d3d11UAVRes, BaseBindings))
            {
                if (ShaderInd == PSInd)
                {
                    PsUavBindMode = PixelShaderUAVBindMode::Bind;

                    // In Direct3D11, a resource can only be bound as UAV once.
                    // Check if any resource in the range is currently bound as compute shader UAV and unbind it.
                    if (Uint8& NumCsUavSlots = m_CommittedRes.NumUAVs[CSInd])
                    {
                        ID3D11UnorderedAccessView** d3d11CsUAVs   = m_CommittedRes.d3d11UAVs[CSInd];
                        ID3D11Resource**            d3d11CsUAVRes = m_CommittedRes.d3d11UAVResources[CSInd];
                        for (Uint32 ps_slot = Slots.MinSlot; ps_slot <= Slots.MaxSlot; ++ps_slot)
                        {
                            if (ID3D11Resource* d3d11Res = d3d11UAVRes[ps_slot])
                            {
                                for (Uint32 cs_slot = 0; cs_slot < NumCsUavSlots; ++cs_slot)
                                {
                                    if (d3d11CsUAVRes[cs_slot] == d3d11Res)
                                    {
                                        d3d11CsUAVs[cs_slot]   = nullptr;
                                        d3d11CsUAVRes[cs_slot] = nullptr;
                                        m_pd3d11DeviceContext->CSSetUnorderedAccessViews(cs_slot, 1, &d3d11CsUAVs[cs_slot], nullptr);
                                    }
                                }
                            }
                        }
                        while (NumCsUavSlots > 0 && d3d11CsUAVRes[NumCsUavSlots - 1] == nullptr)
                        {
                            VERIFY_EXPR(d3d11CsUAVs[NumCsUavSlots - 1] == nullptr);
                            --NumCsUavSlots;
                        }
                    }
                }
                else if (ShaderInd == CSInd)
                {
                    // In Direct3D11, a resource can only be bound as UAV once.
                    // Check if any resource in the range is currently bound as UAV to another slot and unbind it.
                    if (const Uint32 NumUAVSlots = m_CommittedRes.NumUAVs[CSInd])
                    {
                        for (Uint32 slot = (Slots.MinSlot > 0) ? 0 : (Slots.MaxSlot + 1); slot < NumUAVSlots;)
                        {
                            if (ID3D11Resource* pRes = d3d11UAVRes[slot])
                            {
                                for (Uint32 s = Slots.MinSlot; s <= Slots.MaxSlot; ++s)
                                {
                                    if (d3d11UAVRes[s] == pRes)
                                    {
                                        d3d11UAVRes[slot] = nullptr;
                                        d3d11UAVs[slot]   = nullptr;
                                        m_pd3d11DeviceContext->CSSetUnorderedAccessViews(slot, 1, d3d11UAVs + slot, nullptr);
                                    }
                                }
                            }

                            ++slot;
                            if (slot == Slots.MinSlot)
                                slot = Slots.MaxSlot + 1;
                        }
                    }

                    // This can only be CS
                    VERIFY_EXPR(SetUAVMethods[ShaderInd] == &ID3D11DeviceContext::CSSetUnorderedAccessViews);
                    m_pd3d11DeviceContext->CSSetUnorderedAccessViews(Slots.MinSlot, Slots.MaxSlot - Slots.MinSlot + 1, d3d11UAVs + Slots.MinSlot, nullptr);
                    m_CommittedRes.NumUAVs[ShaderInd] = std::max(m_CommittedRes.NumUAVs[ShaderInd], static_cast<Uint8>(Slots.MaxSlot + 1));
                }
                else
                {
                    UNEXPECTED("UAV is not supported in shader that is not pixel or compute");
                }
            }
#ifdef DILIGENT_DEVELOPMENT
            if ((m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE) != 0 && ShaderInd == CSInd)
            {
                DvpVerifyCommittedUAVs(ShaderType);
            }
#endif
        }
    }
}

void DeviceContextD3D11Impl::BindDynamicCBs(const ShaderResourceCacheD3D11&    ResourceCache,
                                            const D3D11ShaderResourceCounters& BaseBindings)
{
    for (SHADER_TYPE ActiveStages = m_BindInfo.ActiveStages; ActiveStages != SHADER_TYPE_UNKNOWN;)
    {
        const Int32 ShaderInd = ExtractFirstShaderStageIndex(ActiveStages);
        if (ResourceCache.GetDynamicCBOffsetsMask(ShaderInd) == 0)
        {
            // Skip stages that don't have any constant buffers with dynamic offsets
            continue;
        }

        ID3D11Buffer** d3d11CBs       = m_CommittedRes.d3d11CBs[ShaderInd];
        UINT*          FirstConstants = m_CommittedRes.CBFirstConstants[ShaderInd];
        UINT*          NumConstants   = m_CommittedRes.CBNumConstants[ShaderInd];
        auto           SetCB1Method   = SetCB1Methods[ShaderInd];

        ResourceCache.BindDynamicCBs(ShaderInd, d3d11CBs, FirstConstants, NumConstants, BaseBindings,
                                     [&](Uint32 Slot) //
                                     {
                                         (m_pd3d11DeviceContext->*SetCB1Method)(Slot, 1, d3d11CBs + Slot, FirstConstants + Slot, NumConstants + Slot);
                                     });

#ifdef DILIGENT_DEVELOPMENT
        if (m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE)
        {
            const SHADER_TYPE ShaderType = GetShaderTypeFromIndex(ShaderInd);
            DvpVerifyCommittedCBs(ShaderType);
        }
#endif
    }
}


void DeviceContextD3D11Impl::BindShaderResources(Uint32 BindSRBMask)
{
    VERIFY_EXPR(BindSRBMask != 0);

    PixelShaderUAVBindMode PsUavBindMode = m_CommittedRes.NumUAVs[PSInd] > 0 ?
        PixelShaderUAVBindMode::Clear :
        PixelShaderUAVBindMode::Keep;

    Uint32 UpToDateSRBMask = m_BindInfo.ActiveSRBMask & ~BindSRBMask;
    while (BindSRBMask != 0)
    {
        Uint32 SignBit = ExtractLSB(BindSRBMask);
        Uint32 SignIdx = PlatformMisc::GetLSB(SignBit);
        VERIFY_EXPR(SignIdx < m_pPipelineState->GetResourceSignatureCount());
        const D3D11ShaderResourceCounters& BaseBindings = m_pPipelineState->GetBaseBindings(SignIdx);

#ifdef DILIGENT_DEVELOPMENT
        m_BindInfo.BaseBindings[SignIdx] = BaseBindings;
#endif
        const ShaderResourceCacheD3D11* pResourceCache = m_BindInfo.ResourceCaches[SignIdx];
        if (pResourceCache == nullptr)
        {
            DEV_ERROR("Shader resource cache at index ", SignIdx, " is null.");
            continue;
        }

        if (m_BindInfo.StaleSRBMask & SignBit)
        {
            // Bind all cache resources
            BindCacheResources(*pResourceCache, BaseBindings, PsUavBindMode);
        }
        else
        {
            // Bind constant buffers with dynamic offsets. In Direct3D11 only those buffers are counted as dynamic.
            VERIFY((m_BindInfo.DynamicSRBMask & SignBit) != 0,
                   "When bit in StaleSRBMask is not set, the same bit in DynamicSRBMask must be set. Check GetCommitMask().");
            DEV_CHECK_ERR(pResourceCache->HasDynamicResources(),
                          "Bit in DynamicSRBMask is set, but the cache does not contain dynamic resources. This may indicate that resources "
                          "in the cache have changed, but the SRB has not been committed before the draw/dispatch command.");
            if (pResourceCache->GetUAVCount(PSInd) > 0)
            {
                if (PsUavBindMode != PixelShaderUAVBindMode::Bind)
                    PsUavBindMode = PixelShaderUAVBindMode::Keep;
            }
            BindDynamicCBs(*pResourceCache, BaseBindings);
        }
    }

    if (PsUavBindMode == PixelShaderUAVBindMode::Clear)
    {
        // Check if SRBs that are not in the BindSRBMask contain UAVs.
        // In this case we should keep UAVs, not clear them.
        while (UpToDateSRBMask != 0)
        {
            Uint32 SignBit = ExtractLSB(UpToDateSRBMask);
            Uint32 SignIdx = PlatformMisc::GetLSB(SignBit);
            VERIFY_EXPR(SignIdx < m_pPipelineState->GetResourceSignatureCount());
            const ShaderResourceCacheD3D11* pResourceCache = m_BindInfo.ResourceCaches[SignIdx];
            if (pResourceCache == nullptr)
            {
                DEV_ERROR("Shader resource cache at index ", SignIdx, " is null.");
                continue;
            }
            if (pResourceCache->GetUAVCount(PSInd) > 0)
            {
                PsUavBindMode = PixelShaderUAVBindMode::Keep;
                break;
            }
        }
    }

    m_BindInfo.StaleSRBMask &= ~m_BindInfo.ActiveSRBMask;

    if (PsUavBindMode == PixelShaderUAVBindMode::Bind)
    {
        // Pixel shader UAVs cannot be set independently; they all need to be set at the same time.
        // https://docs.microsoft.com/en-us/windows/desktop/api/d3d11/nf-d3d11-id3d11devicecontext-omsetrendertargetsandunorderedaccessviews#remarks
        const Uint32 StartUAVSlot = m_NumBoundRenderTargets;

        const Uint8 NumUAVSlots = m_pPipelineState->GetNumPixelUAVs();
        VERIFY(NumUAVSlots > StartUAVSlot, "Number of UAVs must be greater than the render target count");
        ID3D11UnorderedAccessView** d3d11UAVs   = m_CommittedRes.d3d11UAVs[PSInd];
        ID3D11Resource**            d3d11UAVRes = m_CommittedRes.d3d11UAVResources[PSInd];
        m_pd3d11DeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(
            D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr,
            StartUAVSlot, NumUAVSlots - StartUAVSlot, d3d11UAVs + StartUAVSlot, nullptr);
        // Clear previously bound UAVs, but do not clear lower slots as if
        // render target count reduces, we will bind these UAVs in CommitRenderTargets()
        for (Uint32 uav = NumUAVSlots; uav < m_CommittedRes.NumUAVs[PSInd]; ++uav)
        {
            d3d11UAVRes[uav] = nullptr;
            d3d11UAVs[uav]   = nullptr;
        }
        m_CommittedRes.NumUAVs[PSInd] = NumUAVSlots;
    }
    else if (PsUavBindMode == PixelShaderUAVBindMode::Clear)
    {
        // If pixel shader stage is inactive or does not use UAVs, unbind all committed UAVs.
        // This is important as UnbindPixelShaderUAV<> function may need to rebind
        // existing UAVs and the UAVs pointed to by CommittedD3D11UAVRes must be alive
        // (we do not keep strong references to d3d11 UAVs)
        ID3D11UnorderedAccessView** CommittedD3D11UAVs          = m_CommittedRes.d3d11UAVs[PSInd];
        ID3D11Resource**            CommittedD3D11UAVRes        = m_CommittedRes.d3d11UAVResources[PSInd];
        Uint8&                      NumCommittedPixelShaderUAVs = m_CommittedRes.NumUAVs[PSInd];
        for (Uint32 uav = 0; uav < NumCommittedPixelShaderUAVs; ++uav)
        {
            CommittedD3D11UAVRes[uav] = nullptr;
            CommittedD3D11UAVs[uav]   = nullptr;
        }
        m_pd3d11DeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(
            D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr,
            0, 0, nullptr, nullptr);
        NumCommittedPixelShaderUAVs = 0;
    }
}

#ifdef DILIGENT_DEVELOPMENT
void DeviceContextD3D11Impl::DvpValidateCommittedShaderResources()
{
    if (m_BindInfo.ResourcesValidated)
        return;

    DvpVerifySRBCompatibility(m_BindInfo);

    m_pPipelineState->DvpVerifySRBResources(m_BindInfo.ResourceCaches, m_BindInfo.BaseBindings);
    m_BindInfo.ResourcesValidated = true;
}
#endif

void DeviceContextD3D11Impl::SetStencilRef(Uint32 StencilRef)
{
    if (TDeviceContextBase::SetStencilRef(StencilRef, 0))
    {
        ID3D11DepthStencilState* pd3d11DSS =
            m_pPipelineState ? m_pPipelineState->GetD3D11DepthStencilState() : nullptr;
        m_pd3d11DeviceContext->OMSetDepthStencilState(pd3d11DSS, m_StencilRef);
    }
}


void DeviceContextD3D11Impl::SetBlendFactors(const float* pBlendFactors)
{
    if (TDeviceContextBase::SetBlendFactors(pBlendFactors, 0))
    {
        Uint32            SampleMask = 0xFFFFFFFF;
        ID3D11BlendState* pd3d11BS   = nullptr;
        if (m_pPipelineState && m_pPipelineState->GetDesc().IsAnyGraphicsPipeline())
        {
            SampleMask = m_pPipelineState->GetGraphicsPipelineDesc().SampleMask;
            pd3d11BS   = m_pPipelineState->GetD3D11BlendState();
        }
        m_pd3d11DeviceContext->OMSetBlendState(pd3d11BS, m_BlendFactors, SampleMask);
    }
}

void DeviceContextD3D11Impl::CommitD3D11IndexBuffer(VALUE_TYPE IndexType)
{
    DEV_CHECK_ERR(m_pIndexBuffer, "Index buffer is not set up for indexed draw command");

    if (m_CommittedD3D11IndexBuffer != m_pIndexBuffer->m_pd3d11Buffer ||
        m_CommittedIBFormat != IndexType ||
        m_CommittedD3D11IndexDataStartOffset != m_IndexDataStartOffset)
    {
        DXGI_FORMAT D3D11IndexFmt = DXGI_FORMAT_UNKNOWN;
        if (IndexType == VT_UINT32)
            D3D11IndexFmt = DXGI_FORMAT_R32_UINT;
        else if (IndexType == VT_UINT16)
            D3D11IndexFmt = DXGI_FORMAT_R16_UINT;
        else
        {
            UNEXPECTED("Unsupported index format. Only R16_UINT and R32_UINT are allowed.");
        }

        m_CommittedD3D11IndexBuffer          = m_pIndexBuffer->m_pd3d11Buffer;
        m_CommittedIBFormat                  = IndexType;
        m_CommittedD3D11IndexDataStartOffset = StaticCast<UINT>(m_IndexDataStartOffset);
        m_pd3d11DeviceContext->IASetIndexBuffer(m_pIndexBuffer->m_pd3d11Buffer, D3D11IndexFmt, m_CommittedD3D11IndexDataStartOffset);
    }

    m_pIndexBuffer->AddState(RESOURCE_STATE_INDEX_BUFFER);
    m_bCommittedD3D11IBUpToDate = true;
}

void DeviceContextD3D11Impl::CommitD3D11VertexBuffers(PipelineStateD3D11Impl* pPipelineStateD3D11)
{
    VERIFY(m_NumVertexStreams <= MAX_BUFFER_SLOTS, "Too many buffers are being set");
    UINT NumBuffersToSet = std::max(m_NumVertexStreams, m_NumCommittedD3D11VBs);

    bool BindVBs = m_NumVertexStreams != m_NumCommittedD3D11VBs;

    for (UINT Slot = 0; Slot < m_NumVertexStreams; ++Slot)
    {
        VertexStreamInfo<BufferD3D11Impl>& CurrStream     = m_VertexStreams[Slot];
        BufferD3D11Impl*                   pBuffD3D11Impl = CurrStream.pBuffer;
        ID3D11Buffer*                      pd3d11Buffer   = pBuffD3D11Impl ? pBuffD3D11Impl->m_pd3d11Buffer : nullptr;
        const Uint32                       Stride         = pPipelineStateD3D11->GetBufferStride(Slot);
        const UINT                         Offset         = StaticCast<UINT>(CurrStream.Offset);

        // It is safe to perform raw pointer check because device context keeps
        // all buffers alive.
        if (m_CommittedD3D11VertexBuffers[Slot] != pd3d11Buffer ||
            m_CommittedD3D11VBStrides[Slot] != Stride ||
            m_CommittedD3D11VBOffsets[Slot] != Offset)
        {
            BindVBs = true;

            m_CommittedD3D11VertexBuffers[Slot] = pd3d11Buffer;
            m_CommittedD3D11VBStrides[Slot]     = Stride;
            m_CommittedD3D11VBOffsets[Slot]     = Offset;

            if (pBuffD3D11Impl)
                pBuffD3D11Impl->AddState(RESOURCE_STATE_VERTEX_BUFFER);
        }
    }

    // Unbind all buffers at the end
    for (Uint32 Slot = m_NumVertexStreams; Slot < m_NumCommittedD3D11VBs; ++Slot)
    {
        m_CommittedD3D11VertexBuffers[Slot] = nullptr;
        m_CommittedD3D11VBStrides[Slot]     = 0;
        m_CommittedD3D11VBOffsets[Slot]     = 0;
    }

    m_NumCommittedD3D11VBs = m_NumVertexStreams;

    if (BindVBs)
    {
        m_pd3d11DeviceContext->IASetVertexBuffers(0, NumBuffersToSet, m_CommittedD3D11VertexBuffers, m_CommittedD3D11VBStrides, m_CommittedD3D11VBOffsets);
    }

    m_bCommittedD3D11VBsUpToDate = true;
}

void DeviceContextD3D11Impl::PrepareForDraw(DRAW_FLAGS Flags)
{
#ifdef DILIGENT_DEVELOPMENT
    DvpVerifyRenderTargets();
#endif

    ID3D11InputLayout* pd3d11InputLayout = m_pPipelineState->GetD3D11InputLayout();
    if (pd3d11InputLayout != nullptr && !m_bCommittedD3D11VBsUpToDate)
    {
        DEV_CHECK_ERR(m_NumVertexStreams >= m_pPipelineState->GetNumBufferSlotsUsed(), "Currently bound pipeline state '", m_pPipelineState->GetDesc().Name, "' expects ", m_pPipelineState->GetNumBufferSlotsUsed(), " input buffer slots, but only ", m_NumVertexStreams, " is bound");
        CommitD3D11VertexBuffers(m_pPipelineState);
    }

    if (Uint32 BindSRBMask = m_BindInfo.GetCommitMask(Flags & DRAW_FLAG_DYNAMIC_RESOURCE_BUFFERS_INTACT))
    {
        BindShaderResources(BindSRBMask);
    }

#ifdef DILIGENT_DEVELOPMENT
    // Must be called after BindShaderResources as it needs BaseBindings
    DvpValidateCommittedShaderResources();

    if ((Flags & DRAW_FLAG_VERIFY_STATES) != 0)
    {
        for (UINT Slot = 0; Slot < m_NumVertexStreams; ++Slot)
        {
            if (BufferD3D11Impl* pBuffD3D11Impl = m_VertexStreams[Slot].pBuffer)
            {
                if (pBuffD3D11Impl->IsInKnownState() && pBuffD3D11Impl->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
                {
                    LOG_ERROR_MESSAGE("Buffer '", pBuffD3D11Impl->GetDesc().Name,
                                      "' used as vertex buffer at slot ", Slot,
                                      " is in RESOURCE_STATE_UNORDERED_ACCESS state. Use appropriate transition mode or "
                                      "explicitly transition the buffer to RESOURCE_STATE_VERTEX_BUFFER state.");
                }
            }
        }

        if ((m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE) != 0)
        {
            // Verify bindings after all resources are set
            const SHADER_TYPE ActiveStages = m_pPipelineState->GetActiveShaderStages();
            DvpVerifyCommittedSRVs(ActiveStages);
            DvpVerifyCommittedSamplers(ActiveStages);
            DvpVerifyCommittedCBs(ActiveStages);
            DvpVerifyCommittedVertexBuffers();
            DvpVerifyCommittedIndexBuffer();
            DvpVerifyCommittedShaders();
        }
    }
#endif
}

void DeviceContextD3D11Impl::PrepareForIndexedDraw(DRAW_FLAGS Flags, VALUE_TYPE IndexType)
{
    if (m_CommittedIBFormat != IndexType)
        m_bCommittedD3D11IBUpToDate = false;
    if (!m_bCommittedD3D11IBUpToDate)
    {
        CommitD3D11IndexBuffer(IndexType);
    }
#ifdef DILIGENT_DEVELOPMENT
    if (Flags & DRAW_FLAG_VERIFY_STATES)
    {
        if (m_pIndexBuffer->IsInKnownState() && m_pIndexBuffer->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
        {
            LOG_ERROR_MESSAGE("Buffer '", m_pIndexBuffer->GetDesc().Name,
                              "' used as index buffer is in RESOURCE_STATE_UNORDERED_ACCESS state."
                              " Use appropriate state transition mode or explicitly transition the buffer to RESOURCE_STATE_INDEX_BUFFER state.");
        }
    }
#endif
    // We need to commit index buffer first because PrepareForDraw
    // may verify committed resources.
    PrepareForDraw(Flags);
}

void DeviceContextD3D11Impl::Draw(const DrawAttribs& Attribs)
{
    TDeviceContextBase::Draw(Attribs, 0);

    PrepareForDraw(Attribs.Flags);

    if (Attribs.NumVertices > 0 && Attribs.NumInstances > 0)
    {
        if (Attribs.NumInstances > 1 || Attribs.FirstInstanceLocation != 0)
            m_pd3d11DeviceContext->DrawInstanced(Attribs.NumVertices, Attribs.NumInstances, Attribs.StartVertexLocation, Attribs.FirstInstanceLocation);
        else
            m_pd3d11DeviceContext->Draw(Attribs.NumVertices, Attribs.StartVertexLocation);
    }
}

void DeviceContextD3D11Impl::MultiDraw(const MultiDrawAttribs& Attribs)
{
    TDeviceContextBase::MultiDraw(Attribs, 0);

    PrepareForDraw(Attribs.Flags);

    if (Attribs.NumInstances > 1 || Attribs.FirstInstanceLocation != 0)
    {
        for (Uint32 i = 0; i < Attribs.DrawCount; ++i)
        {
            const MultiDrawItem& Item = Attribs.pDrawItems[i];
            if (Item.NumVertices > 0)
            {
                m_pd3d11DeviceContext->DrawInstanced(Item.NumVertices, Attribs.NumInstances, Item.StartVertexLocation, Attribs.FirstInstanceLocation);
            }
        }
    }
    else if (Attribs.NumInstances > 0)
    {
        for (Uint32 i = 0; i < Attribs.DrawCount; ++i)
        {
            const MultiDrawItem& Item = Attribs.pDrawItems[i];
            if (Item.NumVertices > 0)
            {
                m_pd3d11DeviceContext->Draw(Item.NumVertices, Item.StartVertexLocation);
            }
        }
    }
}

void DeviceContextD3D11Impl::DrawIndexed(const DrawIndexedAttribs& Attribs)
{
    TDeviceContextBase::DrawIndexed(Attribs, 0);

    PrepareForIndexedDraw(Attribs.Flags, Attribs.IndexType);

    if (Attribs.NumIndices > 0 && Attribs.NumInstances > 0)
    {
        if (Attribs.NumInstances > 1 || Attribs.FirstInstanceLocation != 0)
            m_pd3d11DeviceContext->DrawIndexedInstanced(Attribs.NumIndices, Attribs.NumInstances, Attribs.FirstIndexLocation, Attribs.BaseVertex, Attribs.FirstInstanceLocation);
        else
            m_pd3d11DeviceContext->DrawIndexed(Attribs.NumIndices, Attribs.FirstIndexLocation, Attribs.BaseVertex);
    }
}

void DeviceContextD3D11Impl::MultiDrawIndexed(const MultiDrawIndexedAttribs& Attribs)
{
    TDeviceContextBase::MultiDrawIndexed(Attribs, 0);

    PrepareForIndexedDraw(Attribs.Flags, Attribs.IndexType);

    if (Attribs.NumInstances > 1 || Attribs.FirstInstanceLocation != 0)
    {
        for (Uint32 i = 0; i < Attribs.DrawCount; ++i)
        {
            const MultiDrawIndexedItem& Item = Attribs.pDrawItems[i];
            m_pd3d11DeviceContext->DrawIndexedInstanced(Item.NumIndices, Attribs.NumInstances, Item.FirstIndexLocation, Item.BaseVertex, Attribs.FirstInstanceLocation);
        }
    }
    else if (Attribs.NumInstances > 0)
    {
        for (Uint32 i = 0; i < Attribs.DrawCount; ++i)
        {
            const MultiDrawIndexedItem& Item = Attribs.pDrawItems[i];
            m_pd3d11DeviceContext->DrawIndexed(Item.NumIndices, Item.FirstIndexLocation, Item.BaseVertex);
        }
    }
}

void DeviceContextD3D11Impl::DrawIndirect(const DrawIndirectAttribs& Attribs)
{
    TDeviceContextBase::DrawIndirect(Attribs, 0);
    DEV_CHECK_ERR(Attribs.pCounterBuffer == nullptr, "Direct3D11 does not support indirect counter buffer");

    PrepareForDraw(Attribs.Flags);

    BufferD3D11Impl* pIndirectDrawAttribsD3D11 = ClassPtrCast<BufferD3D11Impl>(Attribs.pAttribsBuffer);
    ID3D11Buffer*    pd3d11ArgsBuff            = pIndirectDrawAttribsD3D11->m_pd3d11Buffer;

    bool NativeMultiDrawExecuted = false;
    if (Attribs.DrawCount > 1)
    {
#ifdef DILIGENT_ENABLE_D3D_NVAPI
        if (m_pDevice->IsNvApiEnabled())
        {
            NativeMultiDrawExecuted =
                NvAPI_D3D11_MultiDrawInstancedIndirect(m_pd3d11DeviceContext,
                                                       Attribs.DrawCount,
                                                       pd3d11ArgsBuff,
                                                       StaticCast<UINT>(Attribs.DrawArgsOffset),
                                                       Attribs.DrawArgsStride) == NVAPI_OK;
        }
#endif
    }

    if (!NativeMultiDrawExecuted)
    {
        for (Uint32 draw = 0; draw < Attribs.DrawCount; ++draw)
        {
            const Uint64 ArgsOffset = Attribs.DrawArgsOffset + Uint64{draw} * Uint64{Attribs.DrawArgsStride};
            m_pd3d11DeviceContext->DrawInstancedIndirect(pd3d11ArgsBuff, StaticCast<UINT>(ArgsOffset));
        }
    }
}


void DeviceContextD3D11Impl::DrawIndexedIndirect(const DrawIndexedIndirectAttribs& Attribs)
{
    TDeviceContextBase::DrawIndexedIndirect(Attribs, 0);
    DEV_CHECK_ERR(Attribs.pCounterBuffer == nullptr, "Direct3D11 does not support indirect counter buffer");

    PrepareForIndexedDraw(Attribs.Flags, Attribs.IndexType);

    BufferD3D11Impl* pIndirectDrawAttribsD3D11 = ClassPtrCast<BufferD3D11Impl>(Attribs.pAttribsBuffer);
    ID3D11Buffer*    pd3d11ArgsBuff            = pIndirectDrawAttribsD3D11->m_pd3d11Buffer;

    bool NativeMultiDrawExecuted = false;
    if (Attribs.DrawCount >= 1)
    {
#ifdef DILIGENT_ENABLE_D3D_NVAPI
        if (m_pDevice->IsNvApiEnabled())
        {
            NativeMultiDrawExecuted =
                NvAPI_D3D11_MultiDrawIndexedInstancedIndirect(m_pd3d11DeviceContext,
                                                              Attribs.DrawCount,
                                                              pd3d11ArgsBuff,
                                                              StaticCast<UINT>(Attribs.DrawArgsOffset),
                                                              Attribs.DrawArgsStride) == NVAPI_OK;
        }
#endif
    }

    if (!NativeMultiDrawExecuted)
    {
        for (Uint32 draw = 0; draw < Attribs.DrawCount; ++draw)
        {
            const Uint64 ArgsOffset = Attribs.DrawArgsOffset + Uint64{draw} * Uint64{Attribs.DrawArgsStride};
            m_pd3d11DeviceContext->DrawIndexedInstancedIndirect(pd3d11ArgsBuff, StaticCast<UINT>(ArgsOffset));
        }
    }
}

void DeviceContextD3D11Impl::DrawMesh(const DrawMeshAttribs& Attribs)
{
    UNSUPPORTED("DrawMesh is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::DrawMeshIndirect(const DrawMeshIndirectAttribs& Attribs)
{
    UNSUPPORTED("DrawMeshIndirect is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::DispatchCompute(const DispatchComputeAttribs& Attribs)
{
    TDeviceContextBase::DispatchCompute(Attribs, 0);

    if (Uint32 BindSRBMask = m_BindInfo.GetCommitMask())
    {
        BindShaderResources(BindSRBMask);
    }

#ifdef DILIGENT_DEVELOPMENT
    // Must be called after BindShaderResources as it needs BaseBindings
    DvpValidateCommittedShaderResources();

    if (m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE)
    {
        // Verify bindings
        DvpVerifyCommittedSRVs(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedUAVs(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedSamplers(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedCBs(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedShaders();
    }
#endif

    if (Attribs.ThreadGroupCountX > 0 && Attribs.ThreadGroupCountY > 0 && Attribs.ThreadGroupCountZ > 0)
    {
        m_pd3d11DeviceContext->Dispatch(Attribs.ThreadGroupCountX, Attribs.ThreadGroupCountY, Attribs.ThreadGroupCountZ);
    }
}

void DeviceContextD3D11Impl::DispatchComputeIndirect(const DispatchComputeIndirectAttribs& Attribs)
{
    TDeviceContextBase::DispatchComputeIndirect(Attribs, 0);

    if (Uint32 BindSRBMask = m_BindInfo.GetCommitMask())
    {
        BindShaderResources(BindSRBMask);
    }

#ifdef DILIGENT_DEVELOPMENT
    // Must be called after BindShaderResources as it needs BaseBindings
    DvpValidateCommittedShaderResources();

    if (m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE)
    {
        // Verify bindings
        DvpVerifyCommittedSRVs(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedUAVs(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedSamplers(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedCBs(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedShaders();
    }
#endif

    ID3D11Buffer* pd3d11Buff = ClassPtrCast<BufferD3D11Impl>(Attribs.pAttribsBuffer)->GetD3D11Buffer();
    m_pd3d11DeviceContext->DispatchIndirect(pd3d11Buff, StaticCast<UINT>(Attribs.DispatchArgsByteOffset));
}


void DeviceContextD3D11Impl::ClearDepthStencil(ITextureView*                  pView,
                                               CLEAR_DEPTH_STENCIL_FLAGS      ClearFlags,
                                               float                          fDepth,
                                               Uint8                          Stencil,
                                               RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    TDeviceContextBase::ClearDepthStencil(pView);

    VERIFY_EXPR(pView != nullptr);

    TextureViewD3D11Impl*   pViewD3D11 = ClassPtrCast<TextureViewD3D11Impl>(pView);
    ID3D11DepthStencilView* pd3d11DSV  = static_cast<ID3D11DepthStencilView*>(pViewD3D11->GetD3D11View());

    UINT32 d3d11ClearFlags = 0;
    if (ClearFlags & CLEAR_DEPTH_FLAG) d3d11ClearFlags |= D3D11_CLEAR_DEPTH;
    if (ClearFlags & CLEAR_STENCIL_FLAG) d3d11ClearFlags |= D3D11_CLEAR_STENCIL;
    // The full extent of the resource view is always cleared.
    // Viewport and scissor settings are not applied.
    m_pd3d11DeviceContext->ClearDepthStencilView(pd3d11DSV, d3d11ClearFlags, fDepth, Stencil);
}

void DeviceContextD3D11Impl::ClearRenderTarget(ITextureView* pView, const void* RGBA, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    TDeviceContextBase::ClearRenderTarget(pView);

    VERIFY_EXPR(pView != nullptr);

    TextureViewD3D11Impl*   pViewD3D11 = ClassPtrCast<TextureViewD3D11Impl>(pView);
    ID3D11RenderTargetView* pd3d11RTV  = static_cast<ID3D11RenderTargetView*>(pViewD3D11->GetD3D11View());

    static constexpr float Zero[4] = {0.f, 0.f, 0.f, 0.f};
    if (RGBA == nullptr)
        RGBA = Zero;

#ifdef DILIGENT_DEVELOPMENT
    {
        const TEXTURE_FORMAT        RTVFormat  = pViewD3D11->GetDesc().Format;
        const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(RTVFormat);
        if (FmtAttribs.ComponentType == COMPONENT_TYPE_SINT ||
            FmtAttribs.ComponentType == COMPONENT_TYPE_UINT)
        {
            DEV_CHECK_ERR(memcmp(RGBA, Zero, 4 * sizeof(float)) == 0, "Integer render targets can at the moment only be cleared to zero in Direct3D12");
        }
    }
#endif

    // The full extent of the resource view is always cleared.
    // Viewport and scissor settings are not applied.
    m_pd3d11DeviceContext->ClearRenderTargetView(pd3d11RTV, static_cast<const float*>(RGBA));
}

void DeviceContextD3D11Impl::Flush()
{
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "Flushing device context inside an active render pass.");
    m_pd3d11DeviceContext->Flush();
}

void DeviceContextD3D11Impl::UpdateBuffer(IBuffer*                       pBuffer,
                                          Uint64                         Offset,
                                          Uint64                         Size,
                                          const void*                    pData,
                                          RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    TDeviceContextBase::UpdateBuffer(pBuffer, Offset, Size, pData, StateTransitionMode);

    BufferD3D11Impl* pBufferD3D11Impl = ClassPtrCast<BufferD3D11Impl>(pBuffer);

    D3D11_BOX DstBox;
    DstBox.left        = StaticCast<UINT>(Offset);
    DstBox.right       = StaticCast<UINT>(Offset + Size);
    DstBox.top         = 0;
    DstBox.bottom      = 1;
    DstBox.front       = 0;
    DstBox.back        = 1;
    D3D11_BOX* pDstBox = (Offset == 0 && Size == pBufferD3D11Impl->GetDesc().Size) ? nullptr : &DstBox;
    m_pd3d11DeviceContext->UpdateSubresource(pBufferD3D11Impl->m_pd3d11Buffer, 0, pDstBox, pData, 0, 0);
}

void DeviceContextD3D11Impl::CopyBuffer(IBuffer*                       pSrcBuffer,
                                        Uint64                         SrcOffset,
                                        RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                                        IBuffer*                       pDstBuffer,
                                        Uint64                         DstOffset,
                                        Uint64                         Size,
                                        RESOURCE_STATE_TRANSITION_MODE DstBufferTransitionMode)
{
    TDeviceContextBase::CopyBuffer(pSrcBuffer, SrcOffset, SrcBufferTransitionMode, pDstBuffer, DstOffset, Size, DstBufferTransitionMode);

    BufferD3D11Impl* pSrcBufferD3D11Impl = ClassPtrCast<BufferD3D11Impl>(pSrcBuffer);
    BufferD3D11Impl* pDstBufferD3D11Impl = ClassPtrCast<BufferD3D11Impl>(pDstBuffer);

    D3D11_BOX SrcBox;
    SrcBox.left   = StaticCast<UINT>(SrcOffset);
    SrcBox.right  = StaticCast<UINT>(SrcOffset + Size);
    SrcBox.top    = 0;
    SrcBox.bottom = 1;
    SrcBox.front  = 0;
    SrcBox.back   = 1;
    m_pd3d11DeviceContext->CopySubresourceRegion(pDstBufferD3D11Impl->m_pd3d11Buffer, 0, StaticCast<UINT>(DstOffset), 0, 0, pSrcBufferD3D11Impl->m_pd3d11Buffer, 0, &SrcBox);
}


void DeviceContextD3D11Impl::MapBuffer(IBuffer* pBuffer, MAP_TYPE MapType, MAP_FLAGS MapFlags, PVoid& pMappedData)
{
    TDeviceContextBase::MapBuffer(pBuffer, MapType, MapFlags, pMappedData);

    BufferD3D11Impl* pBufferD3D11  = ClassPtrCast<BufferD3D11Impl>(pBuffer);
    D3D11_MAP        d3d11MapType  = static_cast<D3D11_MAP>(0);
    UINT             d3d11MapFlags = 0;
    MapParamsToD3D11MapParams(MapType, MapFlags, d3d11MapType, d3d11MapFlags);

    D3D11_MAPPED_SUBRESOURCE MappedBuff;

    HRESULT hr = m_pd3d11DeviceContext->Map(pBufferD3D11->m_pd3d11Buffer, 0, d3d11MapType, d3d11MapFlags, &MappedBuff);
    if ((d3d11MapFlags & D3D11_MAP_FLAG_DO_NOT_WAIT) == 0)
    {
        DEV_CHECK_ERR(SUCCEEDED(hr), "Failed to map buffer '", pBufferD3D11->GetDesc().Name, "'");
    }
    pMappedData = SUCCEEDED(hr) ? MappedBuff.pData : nullptr;
}

void DeviceContextD3D11Impl::UnmapBuffer(IBuffer* pBuffer, MAP_TYPE MapType)
{
    TDeviceContextBase::UnmapBuffer(pBuffer, MapType);
    BufferD3D11Impl* pBufferD3D11 = ClassPtrCast<BufferD3D11Impl>(pBuffer);
    m_pd3d11DeviceContext->Unmap(pBufferD3D11->m_pd3d11Buffer, 0);
}

void DeviceContextD3D11Impl::UpdateTexture(ITexture*                      pTexture,
                                           Uint32                         MipLevel,
                                           Uint32                         Slice,
                                           const Box&                     DstBox,
                                           const TextureSubResData&       SubresData,
                                           RESOURCE_STATE_TRANSITION_MODE SrcBufferTransitionMode,
                                           RESOURCE_STATE_TRANSITION_MODE DstTextureTransitionMode)
{
    TDeviceContextBase::UpdateTexture(pTexture, MipLevel, Slice, DstBox, SubresData, SrcBufferTransitionMode, DstTextureTransitionMode);

    TextureBaseD3D11*  pTexD3D11 = ClassPtrCast<TextureBaseD3D11>(pTexture);
    const TextureDesc& Desc      = pTexD3D11->GetDesc();

    // Direct3D11 backend uses UpdateData() to initialize textures, so we can't check the usage in ValidateUpdateTextureParams()
    DEV_CHECK_ERR(Desc.Usage == USAGE_DEFAULT || Desc.Usage == USAGE_SPARSE,
                  "Only USAGE_DEFAULT or USAGE_SPARSE textures should be updated with UpdateData()");

    if (SubresData.pSrcBuffer != nullptr)
    {
        LOG_ERROR("D3D11 does not support updating texture subresource from a GPU buffer");
        return;
    }

    D3D11_BOX D3D11Box;
    D3D11Box.left                          = DstBox.MinX;
    D3D11Box.right                         = DstBox.MaxX;
    D3D11Box.top                           = DstBox.MinY;
    D3D11Box.bottom                        = DstBox.MaxY;
    D3D11Box.front                         = DstBox.MinZ;
    D3D11Box.back                          = DstBox.MaxZ;
    const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(Desc.Format);
    if (FmtAttribs.ComponentType == COMPONENT_TYPE_COMPRESSED)
    {
        // Align update region by the compressed block size
        VERIFY((D3D11Box.left % FmtAttribs.BlockWidth) == 0, "Update region min X coordinate (", D3D11Box.left, ") must be multiple of a compressed block width (", Uint32{FmtAttribs.BlockWidth}, ")");
        VERIFY((FmtAttribs.BlockWidth & (FmtAttribs.BlockWidth - 1)) == 0, "Compressed block width (", Uint32{FmtAttribs.BlockWidth}, ") is expected to be power of 2");
        D3D11Box.right = (D3D11Box.right + FmtAttribs.BlockWidth - 1) & ~(FmtAttribs.BlockWidth - 1);

        VERIFY((D3D11Box.top % FmtAttribs.BlockHeight) == 0, "Update region min Y coordinate (", D3D11Box.top, ") must be multiple of a compressed block height (", Uint32{FmtAttribs.BlockHeight}, ")");
        VERIFY((FmtAttribs.BlockHeight & (FmtAttribs.BlockHeight - 1)) == 0, "Compressed block height (", Uint32{FmtAttribs.BlockHeight}, ") is expected to be power of 2");
        D3D11Box.bottom = (D3D11Box.bottom + FmtAttribs.BlockHeight - 1) & ~(FmtAttribs.BlockHeight - 1);
    }
    UINT SubresIndex = D3D11CalcSubresource(MipLevel, Slice, Desc.MipLevels);
    m_pd3d11DeviceContext->UpdateSubresource(pTexD3D11->GetD3D11Texture(), SubresIndex, &D3D11Box, SubresData.pData, StaticCast<UINT>(SubresData.Stride), StaticCast<UINT>(SubresData.DepthStride));
}

void DeviceContextD3D11Impl::CopyTexture(const CopyTextureAttribs& CopyAttribs)
{
    TDeviceContextBase::CopyTexture(CopyAttribs);

    TextureBaseD3D11* pSrcTexD3D11 = ClassPtrCast<TextureBaseD3D11>(CopyAttribs.pSrcTexture);
    TextureBaseD3D11* pDstTexD3D11 = ClassPtrCast<TextureBaseD3D11>(CopyAttribs.pDstTexture);

    D3D11_BOX D3D11SrcBox, *pD3D11SrcBox = nullptr;
    if (const Box* pSrcBox = CopyAttribs.pSrcBox)
    {
        D3D11SrcBox.left   = pSrcBox->MinX;
        D3D11SrcBox.right  = pSrcBox->MaxX;
        D3D11SrcBox.top    = pSrcBox->MinY;
        D3D11SrcBox.bottom = pSrcBox->MaxY;
        D3D11SrcBox.front  = pSrcBox->MinZ;
        D3D11SrcBox.back   = pSrcBox->MaxZ;
        pD3D11SrcBox       = &D3D11SrcBox;
    }
    UINT SrcSubRes = D3D11CalcSubresource(CopyAttribs.SrcMipLevel, CopyAttribs.SrcSlice, pSrcTexD3D11->GetDesc().MipLevels);
    UINT DstSubRes = D3D11CalcSubresource(CopyAttribs.DstMipLevel, CopyAttribs.DstSlice, pDstTexD3D11->GetDesc().MipLevels);
    m_pd3d11DeviceContext->CopySubresourceRegion(pDstTexD3D11->GetD3D11Texture(), DstSubRes, CopyAttribs.DstX, CopyAttribs.DstY, CopyAttribs.DstZ,
                                                 pSrcTexD3D11->GetD3D11Texture(), SrcSubRes, pD3D11SrcBox);
}


void DeviceContextD3D11Impl::MapTextureSubresource(ITexture*                 pTexture,
                                                   Uint32                    MipLevel,
                                                   Uint32                    ArraySlice,
                                                   MAP_TYPE                  MapType,
                                                   MAP_FLAGS                 MapFlags,
                                                   const Box*                pMapRegion,
                                                   MappedTextureSubresource& MappedData)
{
    TDeviceContextBase::MapTextureSubresource(pTexture, MipLevel, ArraySlice, MapType, MapFlags, pMapRegion, MappedData);

    TextureBaseD3D11*  pTexD3D11     = ClassPtrCast<TextureBaseD3D11>(pTexture);
    const TextureDesc& TexDesc       = pTexD3D11->GetDesc();
    D3D11_MAP          d3d11MapType  = static_cast<D3D11_MAP>(0);
    UINT               d3d11MapFlags = 0;
    MapParamsToD3D11MapParams(MapType, MapFlags, d3d11MapType, d3d11MapFlags);

    UINT                     Subresource = D3D11CalcSubresource(MipLevel, ArraySlice, TexDesc.MipLevels);
    D3D11_MAPPED_SUBRESOURCE MappedTex;
    HRESULT                  hr = m_pd3d11DeviceContext->Map(pTexD3D11->GetD3D11Texture(), Subresource, d3d11MapType, d3d11MapFlags, &MappedTex);
    if (FAILED(hr))
    {
        VERIFY_EXPR(hr == DXGI_ERROR_WAS_STILL_DRAWING);
        MappedData = MappedTextureSubresource();
    }
    else
    {
        MappedData.pData       = MappedTex.pData;
        MappedData.Stride      = MappedTex.RowPitch;
        MappedData.DepthStride = MappedTex.DepthPitch;
    }
}

void DeviceContextD3D11Impl::UnmapTextureSubresource(ITexture* pTexture, Uint32 MipLevel, Uint32 ArraySlice)
{
    TDeviceContextBase::UnmapTextureSubresource(pTexture, MipLevel, ArraySlice);

    TextureBaseD3D11*  pTexD3D11   = ClassPtrCast<TextureBaseD3D11>(pTexture);
    const TextureDesc& TexDesc     = pTexD3D11->GetDesc();
    UINT               Subresource = D3D11CalcSubresource(MipLevel, ArraySlice, TexDesc.MipLevels);
    m_pd3d11DeviceContext->Unmap(pTexD3D11->GetD3D11Texture(), Subresource);
}

void DeviceContextD3D11Impl::GenerateMips(ITextureView* pTextureView)
{
    TDeviceContextBase::GenerateMips(pTextureView);
    TextureViewD3D11Impl&     TexViewD3D11 = *ClassPtrCast<TextureViewD3D11Impl>(pTextureView);
    ID3D11ShaderResourceView* pd3d11SRV    = static_cast<ID3D11ShaderResourceView*>(TexViewD3D11.GetD3D11View());
    m_pd3d11DeviceContext->GenerateMips(pd3d11SRV);
}

void DeviceContextD3D11Impl::FinishFrame()
{
    if (m_ActiveDisjointQuery)
    {
        m_pd3d11DeviceContext->End(m_ActiveDisjointQuery->pd3d11Query);
        m_ActiveDisjointQuery->IsEnded = true;
        m_ActiveDisjointQuery.reset();
    }

    TDeviceContextBase::EndFrame();
}

void DeviceContextD3D11Impl::SetVertexBuffers(Uint32                         StartSlot,
                                              Uint32                         NumBuffersSet,
                                              IBuffer* const*                ppBuffers,
                                              const Uint64*                  pOffsets,
                                              RESOURCE_STATE_TRANSITION_MODE StateTransitionMode,
                                              SET_VERTEX_BUFFERS_FLAGS       Flags)
{
    TDeviceContextBase::SetVertexBuffers(StartSlot, NumBuffersSet, ppBuffers, pOffsets, StateTransitionMode, Flags);
    for (Uint32 Slot = 0; Slot < m_NumVertexStreams; ++Slot)
    {
        VertexStreamInfo<BufferD3D11Impl>& CurrStream = m_VertexStreams[Slot];
        if (BufferD3D11Impl* pBuffD3D11Impl = CurrStream.pBuffer)
        {
            if (StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION)
            {
                if (pBuffD3D11Impl->IsInKnownState() && pBuffD3D11Impl->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
                {
                    UnbindResourceFromUAV(pBuffD3D11Impl->m_pd3d11Buffer);
                    pBuffD3D11Impl->ClearState(RESOURCE_STATE_UNORDERED_ACCESS);
                }
            }
#ifdef DILIGENT_DEVELOPMENT
            else if (StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_VERIFY)
            {
                if (pBuffD3D11Impl->IsInKnownState() && pBuffD3D11Impl->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
                {
                    LOG_ERROR_MESSAGE("Buffer '", pBuffD3D11Impl->GetDesc().Name, "' used as vertex buffer at slot ", Slot, " is in RESOURCE_STATE_UNORDERED_ACCESS state. "
                                                                                                                            "Use RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the buffer to RESOURCE_STATE_VERTEX_BUFFER state.");
                }
            }
#endif
        }
    }

    m_bCommittedD3D11VBsUpToDate = false;
}

void DeviceContextD3D11Impl::SetIndexBuffer(IBuffer* pIndexBuffer, Uint64 ByteOffset, RESOURCE_STATE_TRANSITION_MODE StateTransitionMode)
{
    TDeviceContextBase::SetIndexBuffer(pIndexBuffer, ByteOffset, StateTransitionMode);

    if (m_pIndexBuffer)
    {
        if (StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION)
        {
            if (m_pIndexBuffer->IsInKnownState() && m_pIndexBuffer->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            {
                UnbindResourceFromUAV(m_pIndexBuffer->m_pd3d11Buffer);
                m_pIndexBuffer->ClearState(RESOURCE_STATE_UNORDERED_ACCESS);
            }
        }
#ifdef DILIGENT_DEVELOPMENT
        else if (StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_VERIFY)
        {
            if (m_pIndexBuffer->IsInKnownState() && m_pIndexBuffer->CheckState(RESOURCE_STATE_UNORDERED_ACCESS))
            {
                LOG_ERROR_MESSAGE("Buffer '", m_pIndexBuffer->GetDesc().Name, "' used as index buffer is in RESOURCE_STATE_UNORDERED_ACCESS state."
                                                                              " Use RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode or explicitly transition the buffer to RESOURCE_STATE_INDEX_BUFFER state.");
            }
        }
#endif
    }

    m_bCommittedD3D11IBUpToDate = false;
}

void DeviceContextD3D11Impl::SetViewports(Uint32 NumViewports, const Viewport* pViewports, Uint32 RTWidth, Uint32 RTHeight)
{
    static_assert(MAX_VIEWPORTS >= D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, "MaxViewports constant must be greater than D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE");
    TDeviceContextBase::SetViewports(NumViewports, pViewports, RTWidth, RTHeight);

    D3D11_VIEWPORT d3d11Viewports[MAX_VIEWPORTS];
    VERIFY(NumViewports == m_NumViewports, "Unexpected number of viewports");
    for (Uint32 vp = 0; vp < m_NumViewports; ++vp)
    {
        d3d11Viewports[vp].TopLeftX = m_Viewports[vp].TopLeftX;
        d3d11Viewports[vp].TopLeftY = m_Viewports[vp].TopLeftY;
        d3d11Viewports[vp].Width    = m_Viewports[vp].Width;
        d3d11Viewports[vp].Height   = m_Viewports[vp].Height;
        d3d11Viewports[vp].MinDepth = m_Viewports[vp].MinDepth;
        d3d11Viewports[vp].MaxDepth = m_Viewports[vp].MaxDepth;
    }
    // All viewports must be set atomically as one operation.
    // Any viewports not defined by the call are disabled.
    m_pd3d11DeviceContext->RSSetViewports(NumViewports, d3d11Viewports);
}

void DeviceContextD3D11Impl::SetScissorRects(Uint32 NumRects, const Rect* pRects, Uint32 RTWidth, Uint32 RTHeight)
{
    static_assert(MAX_VIEWPORTS >= D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, "MaxViewports constant must be greater than D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE");
    TDeviceContextBase::SetScissorRects(NumRects, pRects, RTWidth, RTHeight);

    D3D11_RECT d3d11ScissorRects[MAX_VIEWPORTS];
    VERIFY(NumRects == m_NumScissorRects, "Unexpected number of scissor rects");
    for (Uint32 sr = 0; sr < NumRects; ++sr)
    {
        d3d11ScissorRects[sr].left   = m_ScissorRects[sr].left;
        d3d11ScissorRects[sr].top    = m_ScissorRects[sr].top;
        d3d11ScissorRects[sr].right  = m_ScissorRects[sr].right;
        d3d11ScissorRects[sr].bottom = m_ScissorRects[sr].bottom;
    }

    // All scissor rects must be set atomically as one operation.
    // Any scissor rects not defined by the call are disabled.
    m_pd3d11DeviceContext->RSSetScissorRects(NumRects, d3d11ScissorRects);
}

void DeviceContextD3D11Impl::CommitRenderTargets()
{
    const Uint32 MaxD3D11RTs      = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
    Uint32       NumRenderTargets = m_NumBoundRenderTargets;
    VERIFY(NumRenderTargets <= MaxD3D11RTs, "D3D11 only allows 8 simultaneous render targets");
    NumRenderTargets = std::min(MaxD3D11RTs, NumRenderTargets);

    // Do not waste time setting RTVs to null
    ID3D11RenderTargetView* pd3d11RTs[MaxD3D11RTs];
    ID3D11DepthStencilView* pd3d11DSV = nullptr;

    for (Uint32 rt = 0; rt < NumRenderTargets; ++rt)
    {
        TextureViewD3D11Impl* pViewD3D11 = m_pBoundRenderTargets[rt];
        pd3d11RTs[rt]                    = pViewD3D11 != nullptr ? static_cast<ID3D11RenderTargetView*>(pViewD3D11->GetD3D11View()) : nullptr;
    }

    if (m_pBoundDepthStencil != nullptr)
    {
        pd3d11DSV = static_cast<ID3D11DepthStencilView*>(m_pBoundDepthStencil->GetD3D11View());
    }

    Uint8& NumCommittedPixelShaderUAVs = m_CommittedRes.NumUAVs[PSInd];
    if (NumCommittedPixelShaderUAVs > 0)
    {
        m_pd3d11DeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(NumRenderTargets, NumRenderTargets > 0 ? pd3d11RTs : nullptr, pd3d11DSV,
                                                                         0, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, nullptr, nullptr);

        ID3D11UnorderedAccessView** CommittedD3D11UAVs   = m_CommittedRes.d3d11UAVs[PSInd];
        ID3D11Resource**            CommittedD3D11UAVRes = m_CommittedRes.d3d11UAVResources[PSInd];
        for (Uint32 slot = 0; slot < NumRenderTargets; ++slot)
        {
            CommittedD3D11UAVs[slot]   = nullptr;
            CommittedD3D11UAVRes[slot] = nullptr;
        }
        if (NumRenderTargets >= NumCommittedPixelShaderUAVs)
            NumCommittedPixelShaderUAVs = 0;
    }
    else
    {
        m_pd3d11DeviceContext->OMSetRenderTargets(NumRenderTargets, NumRenderTargets > 0 ? pd3d11RTs : nullptr, pd3d11DSV);
    }
}


void UnbindView(ID3D11DeviceContext* pContext, TSetShaderResourcesType SetSRVMethod, UINT Slot)
{
    ID3D11ShaderResourceView* ppNullView[] = {nullptr};
    (pContext->*SetSRVMethod)(Slot, 1, ppNullView);
}

void UnbindView(ID3D11DeviceContext* pContext, TSetUnorderedAccessViewsType SetUAVMethod, UINT Slot)
{
    ID3D11UnorderedAccessView* ppNullView[] = {nullptr};
    (pContext->*SetUAVMethod)(Slot, 1, ppNullView, nullptr);
}

template <typename TD3D11ResourceViewType, typename TSetD3D11View>
bool UnbindPixelShaderUAV(ID3D11DeviceContext*    pDeviceCtx,
                          TD3D11ResourceViewType* CommittedD3D11Resources[],
                          Uint32                  NumCommittedSlots,
                          Uint32                  NumCommittedRenderTargets,
                          TSetD3D11View           SetD3D11ViewMethod)
{
    // For other resource view types do nothing
    return false;
}

template <>
bool UnbindPixelShaderUAV<ID3D11UnorderedAccessView, TSetUnorderedAccessViewsType>(
    ID3D11DeviceContext*         pDeviceCtx,
    ID3D11UnorderedAccessView*   CommittedD3D11UAVs[],
    Uint32                       NumCommittedUAVs,
    Uint32                       NumCommittedRenderTargets,
    TSetUnorderedAccessViewsType SetD3D11UAVMethod)
{
    if (SetD3D11UAVMethod == reinterpret_cast<TSetUnorderedAccessViewsType>(&ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews))
    {
        // Pixel shader UAVs are bound in a special way simultaneously with the render targets
        Uint32 UAVStartSlot = NumCommittedRenderTargets;
        // UAVs cannot be set independently; they all need to be set at the same time.
        // https://docs.microsoft.com/en-us/windows/desktop/api/d3d11/nf-d3d11-id3d11devicecontext-omsetrendertargetsandunorderedaccessviews#remarks

        // There is potential problem here: since device context does not keep strong references to
        // UAVs, there is no guarantee the objects are alive
        pDeviceCtx->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr,
                                                              UAVStartSlot, NumCommittedUAVs - UAVStartSlot,
                                                              CommittedD3D11UAVs + UAVStartSlot, nullptr);
        return true;
    }

    return false;
}



/// \tparam TD3D11ResourceViewType    - Type of the D3D11 resource view (ID3D11ShaderResourceView or ID3D11UnorderedAccessView)
/// \tparam TSetD3D11View             - Type of the D3D11 device context method used to set the D3D11 view
/// \param CommittedD3D11ViewsArr     - Pointer to the array of currently bound D3D11
///                                     shader resource views, for each shader stage
/// \param CommittedD3D11ResourcesArr - Pointer to the array of currently bound D3D11
///                                     shader resources, for each shader stage
/// \param CommittedResourcesArr      - Pointer to the array of strong references to currently bound
///                                     shader resources, for each shader stage
/// \param pd3d11ResToUndind          - D3D11 resource to unbind
/// \param SetD3D11ViewMethods        - Array of pointers to device context methods used to set the view,
///                                     for every shader stage
template <typename TD3D11ResourceViewType,
          typename TSetD3D11View,
          size_t NumSlots>
void DeviceContextD3D11Impl::UnbindResourceView(TD3D11ResourceViewType CommittedD3D11ViewsArr[][NumSlots],
                                                ID3D11Resource*        CommittedD3D11ResourcesArr[][NumSlots],
                                                Uint8                  NumCommittedResourcesArr[],
                                                ID3D11Resource*        pd3d11ResToUndind,
                                                TSetD3D11View          SetD3D11ViewMethods[])
{
    for (Int32 ShaderTypeInd = 0; ShaderTypeInd < NumShaderTypes; ++ShaderTypeInd)
    {
        auto*  CommittedD3D11Views     = CommittedD3D11ViewsArr[ShaderTypeInd];
        auto*  CommittedD3D11Resources = CommittedD3D11ResourcesArr[ShaderTypeInd];
        Uint8& NumCommittedSlots       = NumCommittedResourcesArr[ShaderTypeInd];

        for (Uint32 Slot = 0; Slot < NumCommittedSlots; ++Slot)
        {
            if (CommittedD3D11Resources[Slot] == pd3d11ResToUndind)
            {
                CommittedD3D11Resources[Slot] = nullptr;
                CommittedD3D11Views[Slot]     = nullptr;

                auto SetViewMethod = SetD3D11ViewMethods[ShaderTypeInd];
                VERIFY(SetViewMethod != nullptr, "No appropriate ID3D11DeviceContext method");

                // Pixel shader UAVs require special handling
                if (!UnbindPixelShaderUAV(m_pd3d11DeviceContext, CommittedD3D11Views, NumCommittedSlots, m_NumBoundRenderTargets, SetViewMethod))
                {
                    UnbindView(m_pd3d11DeviceContext, SetViewMethod, Slot);
                }
            }
        }

        // Pop null resources from the end of arrays
        while (NumCommittedSlots > 0 && CommittedD3D11Resources[NumCommittedSlots - 1] == nullptr)
        {
            VERIFY(CommittedD3D11Views[NumSlots - 1] == nullptr, "Unexpected non-null resource view");
            --NumCommittedSlots;
        }
    }
}

void DeviceContextD3D11Impl::UnbindTextureFromInput(TextureBaseD3D11& Texture, ID3D11Resource* pd3d11Resource)
{
    UnbindResourceView(m_CommittedRes.d3d11SRVs, m_CommittedRes.d3d11SRVResources, m_CommittedRes.NumSRVs, pd3d11Resource, SetSRVMethods);
    if (Texture.IsInKnownState())
        Texture.ClearState(RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INPUT_ATTACHMENT);
}

void DeviceContextD3D11Impl::UnbindBufferFromInput(BufferD3D11Impl& Buffer, RESOURCE_STATE OldState, ID3D11Resource* pd3d11Buffer)
{
    if (OldState & RESOURCE_STATE_SHADER_RESOURCE)
    {
        UnbindResourceView(m_CommittedRes.d3d11SRVs, m_CommittedRes.d3d11SRVResources, m_CommittedRes.NumSRVs, pd3d11Buffer, SetSRVMethods);
        if (Buffer.IsInKnownState())
            Buffer.ClearState(RESOURCE_STATE_SHADER_RESOURCE);
    }

    if (OldState & RESOURCE_STATE_INDEX_BUFFER)
    {
        ID3D11Buffer* pd3d11IndBuffer = Buffer.GetD3D11Buffer();
        if (pd3d11IndBuffer == m_CommittedD3D11IndexBuffer)
        {
            // Only unbind D3D11 buffer from the context!
            // m_pIndexBuffer.Release();
            m_CommittedD3D11IndexBuffer.Release();
            m_CommittedIBFormat                  = VT_UNDEFINED;
            m_CommittedD3D11IndexDataStartOffset = 0;
            m_bCommittedD3D11IBUpToDate          = false;
            m_pd3d11DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, m_CommittedD3D11IndexDataStartOffset);
        }
        if (Buffer.IsInKnownState())
            Buffer.ClearState(RESOURCE_STATE_INDEX_BUFFER);
    }

    if (OldState & RESOURCE_STATE_VERTEX_BUFFER)
    {
        ID3D11Buffer* pd3d11VB = Buffer.GetD3D11Buffer();
        for (Uint32 Slot = 0; Slot < m_NumCommittedD3D11VBs; ++Slot)
        {
            ID3D11Buffer*& CommittedD3D11VB = m_CommittedD3D11VertexBuffers[Slot];
            if (CommittedD3D11VB == pd3d11VB)
            {
                // Unbind only D3D11 buffer
                //*VertStream = VertexStreamInfo();
                ID3D11Buffer* ppNullBuffer[]        = {nullptr};
                const UINT    Zero[]                = {0};
                m_CommittedD3D11VertexBuffers[Slot] = nullptr;
                m_CommittedD3D11VBStrides[Slot]     = 0;
                m_CommittedD3D11VBOffsets[Slot]     = 0;
                m_bCommittedD3D11VBsUpToDate        = false;
                m_pd3d11DeviceContext->IASetVertexBuffers(Slot, _countof(ppNullBuffer), ppNullBuffer, Zero, Zero);
            }
        }
        if (Buffer.IsInKnownState())
            Buffer.ClearState(RESOURCE_STATE_VERTEX_BUFFER);
    }

    if (OldState & RESOURCE_STATE_CONSTANT_BUFFER)
    {
        for (Int32 ShaderTypeInd = 0; ShaderTypeInd < NumShaderTypes; ++ShaderTypeInd)
        {
            ID3D11Buffer** CommittedD3D11CBs = m_CommittedRes.d3d11CBs[ShaderTypeInd];
            const Uint8    NumSlots          = m_CommittedRes.NumCBs[ShaderTypeInd];
            for (Uint32 Slot = 0; Slot < NumSlots; ++Slot)
            {
                if (CommittedD3D11CBs[Slot] == pd3d11Buffer)
                {
                    CommittedD3D11CBs[Slot]      = nullptr;
                    auto          SetCBMethod    = SetCBMethods[ShaderTypeInd];
                    ID3D11Buffer* ppNullBuffer[] = {nullptr};
                    (m_pd3d11DeviceContext->*SetCBMethod)(Slot, 1, ppNullBuffer);
                }
            }
        }
        if (Buffer.IsInKnownState())
            Buffer.ClearState(RESOURCE_STATE_CONSTANT_BUFFER);
    }
}

void DeviceContextD3D11Impl::UnbindResourceFromUAV(ID3D11Resource* pd3d11Resource)
{
    UnbindResourceView(m_CommittedRes.d3d11UAVs, m_CommittedRes.d3d11UAVResources, m_CommittedRes.NumUAVs, pd3d11Resource, SetUAVMethods);
}

void DeviceContextD3D11Impl::UnbindTextureFromRenderTarget(TextureBaseD3D11& Texture)
{
    bool bCommitRenderTargets = false;
    for (Uint32 rt = 0; rt < m_NumBoundRenderTargets; ++rt)
    {
        if (TextureViewD3D11Impl* pTexView = m_pBoundRenderTargets[rt])
        {
            if (pTexView->GetTexture() == &Texture)
            {
                m_pBoundRenderTargets[rt].Release();
                bCommitRenderTargets = true;
            }
        }
    }

    if (bCommitRenderTargets)
    {
        while (m_NumBoundRenderTargets > 0 && !m_pBoundRenderTargets[m_NumBoundRenderTargets - 1])
            --m_NumBoundRenderTargets;

        CommitRenderTargets();
    }

    if (Texture.IsInKnownState())
        Texture.ClearState(RESOURCE_STATE_RENDER_TARGET);
}

void DeviceContextD3D11Impl::UnbindTextureFromDepthStencil(TextureBaseD3D11& TexD3D11)
{
    if (m_pBoundDepthStencil && m_pBoundDepthStencil->GetTexture() == &TexD3D11)
    {
        m_pBoundDepthStencil.Release();
        CommitRenderTargets();
    }
    if (TexD3D11.IsInKnownState())
        TexD3D11.ClearState(RESOURCE_STATE_DEPTH_WRITE);
}

void DeviceContextD3D11Impl::ResetRenderTargets()
{
    TDeviceContextBase::ResetRenderTargets();
    m_pd3d11DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void DeviceContextD3D11Impl::SetRenderTargetsExt(const SetRenderTargetsAttribs& Attribs)
{
#ifdef DILIGENT_DEVELOPMENT
    if (m_pActiveRenderPass != nullptr)
    {
        LOG_ERROR_MESSAGE("Calling SetRenderTargets inside active render pass is invalid. End the render pass first");
        return;
    }
#endif

    if (TDeviceContextBase::SetRenderTargets(Attribs))
    {
        for (Uint32 RT = 0; RT < m_NumBoundRenderTargets; ++RT)
        {
            if (m_pBoundRenderTargets[RT])
            {
                TextureBaseD3D11* pTex = m_pBoundRenderTargets[RT]->GetTexture<TextureBaseD3D11>();
                if (Attribs.StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION)
                {
                    UnbindTextureFromInput(*pTex, pTex->GetD3D11Texture());
                    if (pTex->IsInKnownState())
                        pTex->SetState(RESOURCE_STATE_RENDER_TARGET);
                }
#ifdef DILIGENT_DEVELOPMENT
                else if (Attribs.StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_VERIFY)
                {
                    DvpVerifyTextureState(*pTex, RESOURCE_STATE_RENDER_TARGET, "Setting render targets (DeviceContextD3D11Impl::SetRenderTargets)");
                }
#endif
            }
        }

        if (m_pBoundDepthStencil)
        {
            const TEXTURE_VIEW_TYPE ViewType = m_pBoundDepthStencil->GetDesc().ViewType;
            VERIFY_EXPR(ViewType == TEXTURE_VIEW_DEPTH_STENCIL || ViewType == TEXTURE_VIEW_READ_ONLY_DEPTH_STENCIL);
            const RESOURCE_STATE NewState = ViewType == TEXTURE_VIEW_DEPTH_STENCIL ?
                RESOURCE_STATE_DEPTH_WRITE :
                RESOURCE_STATE_DEPTH_READ;

            TextureBaseD3D11* pTex = m_pBoundDepthStencil->GetTexture<TextureBaseD3D11>();
            if (Attribs.StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_TRANSITION)
            {
                UnbindTextureFromInput(*pTex, pTex->GetD3D11Texture());
                if (pTex->IsInKnownState())
                    pTex->SetState(NewState);
            }
#ifdef DILIGENT_DEVELOPMENT
            else if (Attribs.StateTransitionMode == RESOURCE_STATE_TRANSITION_MODE_VERIFY)
            {
                DvpVerifyTextureState(*pTex, NewState, "Setting depth-stencil buffer (DeviceContextD3D11Impl::SetRenderTargets)");
            }
#endif
        }

        CommitRenderTargets();

        // Set the viewport to match the render target size
        SetViewports(1, nullptr, 0, 0);
    }
}

void DeviceContextD3D11Impl::BeginSubpass()
{
    VERIFY_EXPR(m_pActiveRenderPass);
    const RenderPassDesc& RPDesc = m_pActiveRenderPass->GetDesc();
    VERIFY_EXPR(m_SubpassIndex < RPDesc.SubpassCount);
    const SubpassDesc&     Subpass = RPDesc.pSubpasses[m_SubpassIndex];
    const FramebufferDesc& FBDesc  = m_pBoundFramebuffer->GetDesc();

    // Unbind these attachments that will be used for output by the subpass.
    // There is no need to unbind textures from output as the new subpass attachments
    // will be committed as render target/depth stencil anyway, so these that can be used for
    // input will be unbound.

    auto UnbindAttachmentFromInput = [&](const AttachmentReference& AttachmentRef) //
    {
        if (AttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
        {
            if (ITextureView* pTexView = FBDesc.ppAttachments[AttachmentRef.AttachmentIndex])
            {
                TextureBaseD3D11* pTexD3D11 = ClassPtrCast<TextureBaseD3D11>(pTexView->GetTexture());
                UnbindResourceView(m_CommittedRes.d3d11SRVs, m_CommittedRes.d3d11SRVResources, m_CommittedRes.NumSRVs, pTexD3D11->GetD3D11Texture(), SetSRVMethods);
            }
        }
    };

    for (Uint32 rt = 0; rt < Subpass.RenderTargetAttachmentCount; ++rt)
    {
        UnbindAttachmentFromInput(Subpass.pRenderTargetAttachments[rt]);
        if (Subpass.pResolveAttachments != nullptr)
        {
            UnbindAttachmentFromInput(Subpass.pResolveAttachments[rt]);
        }
    }

    if (Subpass.pDepthStencilAttachment != nullptr)
    {
        UnbindAttachmentFromInput(*Subpass.pDepthStencilAttachment);
    }

    CommitRenderTargets();

    for (Uint32 rt = 0; rt < Subpass.RenderTargetAttachmentCount; ++rt)
    {
        const AttachmentReference& AttachmentRef   = Subpass.pRenderTargetAttachments[rt];
        const Uint32               RTAttachmentIdx = AttachmentRef.AttachmentIndex;
        if (RTAttachmentIdx != ATTACHMENT_UNUSED)
        {
            const Uint32 AttachmentFirstUse = m_pActiveRenderPass->GetAttachmentFirstLastUse(RTAttachmentIdx).first;
            if (AttachmentFirstUse == m_SubpassIndex && RPDesc.pAttachments[RTAttachmentIdx].LoadOp == ATTACHMENT_LOAD_OP_CLEAR)
            {
                if (ITextureView* pTexView = FBDesc.ppAttachments[RTAttachmentIdx])
                {
                    TextureViewD3D11Impl* const   pViewD3D11 = ClassPtrCast<TextureViewD3D11Impl>(pTexView);
                    ID3D11RenderTargetView* const pd3d11RTV  = static_cast<ID3D11RenderTargetView*>(pViewD3D11->GetD3D11View());
                    const OptimizedClearValue&    ClearValue = m_AttachmentClearValues[RTAttachmentIdx];
                    m_pd3d11DeviceContext->ClearRenderTargetView(pd3d11RTV, ClearValue.Color);
                }
            }
        }
    }

    if (Subpass.pDepthStencilAttachment)
    {
        Uint32 DSAttachmentIdx = Subpass.pDepthStencilAttachment->AttachmentIndex;
        if (DSAttachmentIdx != ATTACHMENT_UNUSED)
        {
            const Uint32 AttachmentFirstUse = m_pActiveRenderPass->GetAttachmentFirstLastUse(DSAttachmentIdx).first;
            if (AttachmentFirstUse == m_SubpassIndex && RPDesc.pAttachments[DSAttachmentIdx].LoadOp == ATTACHMENT_LOAD_OP_CLEAR)
            {
                if (ITextureView* pTexView = FBDesc.ppAttachments[DSAttachmentIdx])
                {
                    TextureViewD3D11Impl* const   pViewD3D11 = ClassPtrCast<TextureViewD3D11Impl>(pTexView);
                    ID3D11DepthStencilView* const pd3d11DSV  = static_cast<ID3D11DepthStencilView*>(pViewD3D11->GetD3D11View());
                    const OptimizedClearValue&    ClearValue = m_AttachmentClearValues[DSAttachmentIdx];
                    m_pd3d11DeviceContext->ClearDepthStencilView(pd3d11DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, ClearValue.DepthStencil.Depth, ClearValue.DepthStencil.Stencil);
                }
            }
        }
    }
}

void DeviceContextD3D11Impl::EndSubpass()
{
    VERIFY_EXPR(m_pActiveRenderPass);
    const RenderPassDesc& RPDesc = m_pActiveRenderPass->GetDesc();
    VERIFY_EXPR(m_SubpassIndex < RPDesc.SubpassCount);
    const SubpassDesc&     Subpass = RPDesc.pSubpasses[m_SubpassIndex];
    const FramebufferDesc& FBDesc  = m_pBoundFramebuffer->GetDesc();

    if (Subpass.pResolveAttachments != nullptr)
    {
        for (Uint32 rt = 0; rt < Subpass.RenderTargetAttachmentCount; ++rt)
        {
            const AttachmentReference& RslvAttachmentRef = Subpass.pResolveAttachments[rt];
            if (RslvAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
            {
                const AttachmentReference& RTAttachmentRef = Subpass.pRenderTargetAttachments[rt];
                VERIFY_EXPR(RTAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED);
                ITextureView*     pSrcView     = FBDesc.ppAttachments[RTAttachmentRef.AttachmentIndex];
                ITextureView*     pDstView     = FBDesc.ppAttachments[RslvAttachmentRef.AttachmentIndex];
                TextureBaseD3D11* pSrcTexD3D11 = ClassPtrCast<TextureBaseD3D11>(pSrcView->GetTexture());
                TextureBaseD3D11* pDstTexD3D11 = ClassPtrCast<TextureBaseD3D11>(pDstView->GetTexture());

                const TextureViewDesc& SrcViewDesc = pSrcView->GetDesc();
                const TextureViewDesc& DstViewDesc = pDstView->GetDesc();
                const TextureDesc&     SrcTexDesc  = pSrcTexD3D11->GetDesc();
                const TextureDesc&     DstTexDesc  = pDstTexD3D11->GetDesc();

                DXGI_FORMAT DXGIFmt        = TexFormatToDXGI_Format(RPDesc.pAttachments[RTAttachmentRef.AttachmentIndex].Format);
                UINT        SrcSubresIndex = D3D11CalcSubresource(SrcViewDesc.MostDetailedMip, SrcViewDesc.FirstArraySlice, SrcTexDesc.MipLevels);
                UINT        DstSubresIndex = D3D11CalcSubresource(DstViewDesc.MostDetailedMip, DstViewDesc.FirstArraySlice, DstTexDesc.MipLevels);
                m_pd3d11DeviceContext->ResolveSubresource(pDstTexD3D11->GetD3D11Texture(), DstSubresIndex, pSrcTexD3D11->GetD3D11Texture(), SrcSubresIndex, DXGIFmt);
            }
        }
    }
}

void DeviceContextD3D11Impl::BeginRenderPass(const BeginRenderPassAttribs& Attribs)
{
    TDeviceContextBase::BeginRenderPass(Attribs);
    // BeginRenderPass() transitions resources to required states

    m_AttachmentClearValues.resize(Attribs.ClearValueCount);
    for (Uint32 i = 0; i < Attribs.ClearValueCount; ++i)
        m_AttachmentClearValues[i] = Attribs.pClearValues[i];

    BeginSubpass();

    // Set the viewport to match the framebuffer size
    SetViewports(1, nullptr, 0, 0);
}

void DeviceContextD3D11Impl::NextSubpass()
{
    EndSubpass();

    TDeviceContextBase::NextSubpass();

    BeginSubpass();
}

void DeviceContextD3D11Impl::EndRenderPass()
{
    EndSubpass();
    TDeviceContextBase::EndRenderPass();
    m_AttachmentClearValues.clear();
}


template <typename TD3D11ResourceType, typename TSetD3D11ResMethodType>
void SetD3D11ResourcesHelper(ID3D11DeviceContext*   pDeviceCtx,
                             TSetD3D11ResMethodType SetD3D11ResMethod,
                             UINT                   StartSlot,
                             UINT                   NumSlots,
                             TD3D11ResourceType**   ppResources)
{
    (pDeviceCtx->*SetD3D11ResMethod)(StartSlot, NumSlots, ppResources);
}

template <>
void SetD3D11ResourcesHelper(ID3D11DeviceContext*         pDeviceCtx,
                             TSetUnorderedAccessViewsType SetD3D11UAVMethod,
                             UINT                         StartSlot,
                             UINT                         NumSlots,
                             ID3D11UnorderedAccessView**  ppUAVs)
{
    (pDeviceCtx->*SetD3D11UAVMethod)(StartSlot, NumSlots, ppUAVs, nullptr);
}

template <typename TD3D11ResourceType, typename TSetD3D11ResMethodType>
void ReleaseCommittedShaderResourcesHelper(TD3D11ResourceType     CommittedD3D11Res[],
                                           Uint8                  NumCommittedResources,
                                           TSetD3D11ResMethodType SetD3D11ResMethod,
                                           ID3D11DeviceContext*   pDeviceCtx)
{
    if (NumCommittedResources > 0)
    {
        memset(CommittedD3D11Res, 0, NumCommittedResources * sizeof(CommittedD3D11Res[0]));
        SetD3D11ResourcesHelper(pDeviceCtx, SetD3D11ResMethod, 0, NumCommittedResources, CommittedD3D11Res);
    }
}

void ReleaseCommittedPSUAVs(ID3D11UnorderedAccessView* CommittedD3D11UAVs[],
                            Uint8                      NumCommittedResources,
                            ID3D11DeviceContext*       pDeviceCtx)
{
    if (NumCommittedResources > 0)
    {
        memset(CommittedD3D11UAVs, 0, NumCommittedResources * sizeof(CommittedD3D11UAVs[0]));
        pDeviceCtx->OMSetRenderTargetsAndUnorderedAccessViews(
            D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr,
            0, 0, nullptr, nullptr);
    }
}

void DeviceContextD3D11Impl::ReleaseCommittedShaderResources()
{
    // Make sure all resources are committed next time
    m_BindInfo.MakeAllStale();

    for (int ShaderType = 0; ShaderType < NumShaderTypes; ++ShaderType)
    {
        // clang-format off
        ReleaseCommittedShaderResourcesHelper(m_CommittedRes.d3d11CBs[ShaderType],      m_CommittedRes.NumCBs[ShaderType],      SetCBMethods[ShaderType],      m_pd3d11DeviceContext);
        ReleaseCommittedShaderResourcesHelper(m_CommittedRes.d3d11SRVs[ShaderType],     m_CommittedRes.NumSRVs[ShaderType],     SetSRVMethods[ShaderType],     m_pd3d11DeviceContext);
        ReleaseCommittedShaderResourcesHelper(m_CommittedRes.d3d11Samplers[ShaderType], m_CommittedRes.NumSamplers[ShaderType], SetSamplerMethods[ShaderType], m_pd3d11DeviceContext);
        // clang-format on

        if (ShaderType == PSInd)
            ReleaseCommittedPSUAVs(m_CommittedRes.d3d11UAVs[ShaderType], m_CommittedRes.NumUAVs[ShaderType], m_pd3d11DeviceContext);
        else
            ReleaseCommittedShaderResourcesHelper(m_CommittedRes.d3d11UAVs[ShaderType], m_CommittedRes.NumUAVs[ShaderType], SetUAVMethods[ShaderType], m_pd3d11DeviceContext);

        memset(m_CommittedRes.d3d11SRVResources[ShaderType], 0, sizeof(m_CommittedRes.d3d11SRVResources[ShaderType][0]) * m_CommittedRes.NumSRVs[ShaderType]);
        memset(m_CommittedRes.d3d11UAVResources[ShaderType], 0, sizeof(m_CommittedRes.d3d11UAVResources[ShaderType][0]) * m_CommittedRes.NumUAVs[ShaderType]);
        memset(m_CommittedRes.CBFirstConstants[ShaderType], 0, sizeof(m_CommittedRes.CBFirstConstants[ShaderType][0]) * m_CommittedRes.NumCBs[ShaderType]);
        memset(m_CommittedRes.CBNumConstants[ShaderType], 0, sizeof(m_CommittedRes.CBNumConstants[ShaderType][0]) * m_CommittedRes.NumCBs[ShaderType]);
        m_CommittedRes.NumCBs[ShaderType]      = 0;
        m_CommittedRes.NumSRVs[ShaderType]     = 0;
        m_CommittedRes.NumSamplers[ShaderType] = 0;
        m_CommittedRes.NumUAVs[ShaderType]     = 0;
    }

#ifdef DILIGENT_DEVELOPMENT
    if (m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE)
    {
        constexpr SHADER_TYPE AllStages = SHADER_TYPE_ALL_GRAPHICS | SHADER_TYPE_COMPUTE;
        DvpVerifyCommittedSRVs(AllStages);
        DvpVerifyCommittedUAVs(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedSamplers(AllStages);
        DvpVerifyCommittedCBs(AllStages);
    }
#endif
    // We do not unbind vertex buffers and index buffer as this can explicitly
    // be done by the user
}


void DeviceContextD3D11Impl::FinishCommandList(ICommandList** ppCommandList)
{
    DEV_CHECK_ERR(IsDeferred(), "Only deferred contexts can record command list");
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "Finishing command list inside an active render pass.");

    CComPtr<ID3D11CommandList> pd3d11CmdList;
    m_pd3d11DeviceContext->FinishCommandList(
        FALSE, // A Boolean flag that determines whether the runtime saves deferred context state before it
               // executes FinishCommandList and restores it afterwards.
               // * TRUE indicates that the runtime needs to save and restore the state.
               // * FALSE indicates that the runtime will not save or restore any state.
               //   In this case, the deferred context will return to its default state
               //   after the call to FinishCommandList() completes as if
               //   ID3D11DeviceContext::ClearState() was called.
        &pd3d11CmdList);

    CommandListD3D11Impl* pCmdListD3D11(NEW_RC_OBJ(m_CmdListAllocator, "CommandListD3D11Impl instance", CommandListD3D11Impl)(m_pDevice, this, pd3d11CmdList));
    pCmdListD3D11->QueryInterface(IID_CommandList, reinterpret_cast<IObject**>(ppCommandList));

    // Device context is now in default state
    InvalidateState();

#ifdef DILIGENT_DEVELOPMENT
    if (m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE)
    {
        // Verify bindings
        constexpr SHADER_TYPE AllStages = SHADER_TYPE_ALL_GRAPHICS | SHADER_TYPE_COMPUTE;
        DvpVerifyCommittedSRVs(AllStages);
        DvpVerifyCommittedUAVs(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedSamplers(AllStages);
        DvpVerifyCommittedCBs(AllStages);
        DvpVerifyCommittedVertexBuffers();
        DvpVerifyCommittedIndexBuffer();
        DvpVerifyCommittedShaders();
    }
#endif

    TDeviceContextBase::FinishCommandList();
}

void DeviceContextD3D11Impl::ExecuteCommandLists(Uint32               NumCommandLists,
                                                 ICommandList* const* ppCommandLists)
{
    DEV_CHECK_ERR(!IsDeferred(), "Only immediate context can execute command list");

    if (NumCommandLists == 0)
        return;
    DEV_CHECK_ERR(ppCommandLists != nullptr, "ppCommandLists must not be null when NumCommandLists is not zero");

    for (Uint32 i = 0; i < NumCommandLists; ++i)
    {
        CommandListD3D11Impl* pCmdListD3D11 = ClassPtrCast<CommandListD3D11Impl>(ppCommandLists[i]);
        ID3D11CommandList*    pd3d11CmdList = pCmdListD3D11->GetD3D11CommandList();
        m_pd3d11DeviceContext->ExecuteCommandList(pd3d11CmdList,
                                                  FALSE // A Boolean flag that determines whether the target context state is
                                                        // saved prior to and restored after the execution of a command list.
                                                        // * TRUE indicates that the runtime needs to save and restore the state.
                                                        // * FALSE indicate that no state shall be saved or restored, which causes the
                                                        //   target context to return to its default state after the command list executes as if
                                                        //   ID3D11DeviceContext::ClearState() was called.
        );
    }

    // Device context is now in default state
    InvalidateState();

#ifdef DILIGENT_DEVELOPMENT
    if (m_D3D11ValidationFlags & D3D11_VALIDATION_FLAG_VERIFY_COMMITTED_RESOURCE_RELEVANCE)
    {
        // Verify bindings
        constexpr SHADER_TYPE AllStages = SHADER_TYPE_ALL_GRAPHICS | SHADER_TYPE_COMPUTE;
        DvpVerifyCommittedSRVs(AllStages);
        DvpVerifyCommittedUAVs(SHADER_TYPE_COMPUTE);
        DvpVerifyCommittedSamplers(AllStages);
        DvpVerifyCommittedCBs(AllStages);
        DvpVerifyCommittedVertexBuffers();
        DvpVerifyCommittedIndexBuffer();
        DvpVerifyCommittedShaders();
    }
#endif
}


static CComPtr<ID3D11Query> CreateD3D11QueryEvent(ID3D11Device* pd3d11Device)
{
    D3D11_QUERY_DESC QueryDesc{};
    QueryDesc.Query = D3D11_QUERY_EVENT; // Determines whether or not the GPU is finished processing commands.
                                         // When the GPU is finished processing commands ID3D11DeviceContext::GetData will
                                         // return S_OK, and pData will point to a BOOL with a value of TRUE. When using this
                                         // type of query, ID3D11DeviceContext::Begin is disabled.
    QueryDesc.MiscFlags = 0;

    CComPtr<ID3D11Query> pd3d11Query;
    HRESULT              hr = pd3d11Device->CreateQuery(&QueryDesc, &pd3d11Query);
    DEV_CHECK_ERR(SUCCEEDED(hr), "Failed to create D3D11 query");
    VERIFY_EXPR(pd3d11Query);
    return pd3d11Query;
}

void DeviceContextD3D11Impl::EnqueueSignal(IFence* pFence, Uint64 Value)
{
    TDeviceContextBase::EnqueueSignal(pFence, Value, 0);

    ID3D11Device*        pd3d11Device = m_pDevice->GetD3D11Device();
    CComPtr<ID3D11Query> pd3d11Query  = CreateD3D11QueryEvent(pd3d11Device);
    m_pd3d11DeviceContext->End(pd3d11Query);
    FenceD3D11Impl* pFenceD3D11Impl = ClassPtrCast<FenceD3D11Impl>(pFence);
    pFenceD3D11Impl->AddPendingQuery(m_pd3d11DeviceContext, std::move(pd3d11Query), Value);
}

void DeviceContextD3D11Impl::DeviceWaitForFence(IFence* pFence, Uint64 Value)
{
    DEV_ERROR("DeviceWaitForFence() is not supported in Direct3D11");
}

void DeviceContextD3D11Impl::WaitForIdle()
{
    DEV_CHECK_ERR(!IsDeferred(), "Only immediate contexts can be idled");
    Flush();
    ID3D11Device*        pd3d11Device = m_pDevice->GetD3D11Device();
    CComPtr<ID3D11Query> pd3d11Query  = CreateD3D11QueryEvent(pd3d11Device);
    m_pd3d11DeviceContext->End(pd3d11Query);
    BOOL Data;
    while (m_pd3d11DeviceContext->GetData(pd3d11Query, &Data, sizeof(Data), 0) != S_OK)
        std::this_thread::sleep_for(std::chrono::microseconds{1});
}

std::shared_ptr<DisjointQueryPool::DisjointQueryWrapper> DeviceContextD3D11Impl::BeginDisjointQuery()
{
    if (!m_ActiveDisjointQuery)
    {
        m_ActiveDisjointQuery = m_DisjointQueryPool.GetDisjointQuery(m_pDevice->GetD3D11Device());
        // Disjoint timestamp queries should only be invoked once per frame or less.
        m_pd3d11DeviceContext->Begin(m_ActiveDisjointQuery->pd3d11Query);
        m_ActiveDisjointQuery->IsEnded = false;
    }
    return m_ActiveDisjointQuery;
}

void DeviceContextD3D11Impl::BeginQuery(IQuery* pQuery)
{
    TDeviceContextBase::BeginQuery(pQuery, 0);

    QueryD3D11Impl* const pQueryD3D11Impl = ClassPtrCast<QueryD3D11Impl>(pQuery);
    if (pQueryD3D11Impl->GetDesc().Type == QUERY_TYPE_DURATION)
    {
        pQueryD3D11Impl->SetDisjointQuery(BeginDisjointQuery());
        m_pd3d11DeviceContext->End(pQueryD3D11Impl->GetD3D11Query(0));
    }
    else
    {
        m_pd3d11DeviceContext->Begin(pQueryD3D11Impl->GetD3D11Query(0));
    }
}

void DeviceContextD3D11Impl::EndQuery(IQuery* pQuery)
{
    TDeviceContextBase::EndQuery(pQuery, 0);

    QueryD3D11Impl* const pQueryD3D11Impl = ClassPtrCast<QueryD3D11Impl>(pQuery);

    const QUERY_TYPE QueryType = pQueryD3D11Impl->GetDesc().Type;
    DEV_CHECK_ERR(QueryType != QUERY_TYPE_DURATION || m_ActiveDisjointQuery,
                  "There is no active disjoint query. Did you forget to call BeginQuery for the duration query?");
    if (QueryType == QUERY_TYPE_TIMESTAMP)
    {
        pQueryD3D11Impl->SetDisjointQuery(BeginDisjointQuery());
    }
    m_pd3d11DeviceContext->End(pQueryD3D11Impl->GetD3D11Query(QueryType == QUERY_TYPE_DURATION ? 1 : 0));
}

void DeviceContextD3D11Impl::ClearStateCache()
{
    TDeviceContextBase::ClearStateCache();

    m_BindInfo = {};
    m_CommittedRes.Clear();

    for (int ShaderType = 0; ShaderType < NumShaderTypes; ++ShaderType)
    {
        m_CommittedD3DShaders[ShaderType].Release();
    }

    for (Uint32 vb = 0; vb < m_NumCommittedD3D11VBs; ++vb)
    {
        m_CommittedD3D11VertexBuffers[vb] = nullptr;
        m_CommittedD3D11VBStrides[vb]     = 0;
        m_CommittedD3D11VBOffsets[vb]     = 0;
    }
    m_NumCommittedD3D11VBs       = 0;
    m_bCommittedD3D11VBsUpToDate = false;

    m_CommittedD3D11InputLayout = nullptr;

    m_CommittedD3D11IndexBuffer.Release();
    m_CommittedIBFormat                  = VT_UNDEFINED;
    m_CommittedD3D11IndexDataStartOffset = 0;
    m_bCommittedD3D11IBUpToDate          = false;

    m_CommittedD3D11PrimTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    m_CommittedPrimitiveTopology = PRIMITIVE_TOPOLOGY_UNDEFINED;
}

void DeviceContextD3D11Impl::InvalidateState()
{
    TDeviceContextBase::InvalidateState();

    ReleaseCommittedShaderResources();
    for (int ShaderType = 0; ShaderType < NumShaderTypes; ++ShaderType)
        m_CommittedD3DShaders[ShaderType].Release();
    m_pd3d11DeviceContext->VSSetShader(nullptr, nullptr, 0);
    m_pd3d11DeviceContext->GSSetShader(nullptr, nullptr, 0);
    m_pd3d11DeviceContext->PSSetShader(nullptr, nullptr, 0);
    m_pd3d11DeviceContext->HSSetShader(nullptr, nullptr, 0);
    m_pd3d11DeviceContext->DSSetShader(nullptr, nullptr, 0);
    m_pd3d11DeviceContext->CSSetShader(nullptr, nullptr, 0);
    ID3D11RenderTargetView* d3d11NullRTV[] = {nullptr};
    m_pd3d11DeviceContext->OMSetRenderTargets(1, d3d11NullRTV, nullptr);

    if (m_NumCommittedD3D11VBs > 0)
    {
        for (Uint32 vb = 0; vb < m_NumCommittedD3D11VBs; ++vb)
        {
            m_CommittedD3D11VertexBuffers[vb] = nullptr;
            m_CommittedD3D11VBStrides[vb]     = 0;
            m_CommittedD3D11VBOffsets[vb]     = 0;
        }
        m_pd3d11DeviceContext->IASetVertexBuffers(0, m_NumCommittedD3D11VBs, m_CommittedD3D11VertexBuffers, m_CommittedD3D11VBStrides, m_CommittedD3D11VBOffsets);
        m_NumCommittedD3D11VBs = 0;
    }

    m_bCommittedD3D11VBsUpToDate = false;

    if (m_CommittedD3D11InputLayout != nullptr)
    {
        m_pd3d11DeviceContext->IASetInputLayout(nullptr);
        m_CommittedD3D11InputLayout = nullptr;
    }

    if (m_CommittedD3D11IndexBuffer)
    {
        m_pd3d11DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
        m_CommittedD3D11IndexBuffer.Release();
    }

    m_CommittedIBFormat                  = VT_UNDEFINED;
    m_CommittedD3D11IndexDataStartOffset = 0;
    m_bCommittedD3D11IBUpToDate          = false;

    m_CommittedD3D11PrimTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    m_CommittedPrimitiveTopology = PRIMITIVE_TOPOLOGY_UNDEFINED;

    m_BindInfo.Invalidate();
}


static void AliasingBarrier(ID3D11DeviceContext* pd3d11Ctx, IDeviceObject* pResourceBefore, IDeviceObject* pResourceAfter)
{
    DEV_CHECK_ERR(CComQIPtr<ID3D11DeviceContext2>{pd3d11Ctx}, "Failed to query ID3D11DeviceContext2");
    ID3D11DeviceContext2* pd3d11DeviceContext2 = static_cast<ID3D11DeviceContext2*>(pd3d11Ctx);
    bool                  UseNVApi             = false;

    auto GetD3D11Resource = [&UseNVApi](IDeviceObject* pResource) -> ID3D11Resource* //
    {
        if (RefCntAutoPtr<ITextureD3D11> pTexture{pResource, IID_TextureD3D11})
        {
            const TextureBaseD3D11* pTexD3D11 = pTexture.ConstPtr<TextureBaseD3D11>();
            if (pTexD3D11->IsUsingNVApi())
                UseNVApi = true;
            return pTexture->GetD3D11Texture();
        }
        else if (RefCntAutoPtr<IBufferD3D11> pBuffer{pResource, IID_BufferD3D11})
        {
            return pBuffer.RawPtr<BufferD3D11Impl>()->GetD3D11Buffer();
        }
        else
        {
            return nullptr;
        }
    };

    ID3D11Resource* pd3d11ResourceBefore = GetD3D11Resource(pResourceBefore);
    ID3D11Resource* pd3d11ResourceAfter  = GetD3D11Resource(pResourceAfter);

#ifdef DILIGENT_ENABLE_D3D_NVAPI
    if (UseNVApi)
    {
        NvAPI_D3D11_TiledResourceBarrier(pd3d11DeviceContext2, pd3d11ResourceBefore, pd3d11ResourceAfter);
    }
    else
#endif
    {
        VERIFY_EXPR(!UseNVApi);
        pd3d11DeviceContext2->TiledResourceBarrier(pd3d11ResourceBefore, pd3d11ResourceAfter);
    }
}

void DeviceContextD3D11Impl::TransitionResourceStates(Uint32 BarrierCount, const StateTransitionDesc* pResourceBarriers)
{
    DEV_CHECK_ERR(m_pActiveRenderPass == nullptr, "State transitions are not allowed inside a render pass");

    for (Uint32 i = 0; i < BarrierCount; ++i)
    {
        const StateTransitionDesc& Barrier = pResourceBarriers[i];
#ifdef DILIGENT_DEVELOPMENT
        DvpVerifyStateTransitionDesc(Barrier);
#endif

        if (Barrier.TransitionType == STATE_TRANSITION_TYPE_BEGIN)
        {
            // Skip begin-split barriers
            VERIFY((Barrier.Flags & STATE_TRANSITION_FLAG_UPDATE_STATE) == 0, "Resource state can't be updated in begin-split barrier");
            continue;
        }
        VERIFY(Barrier.TransitionType == STATE_TRANSITION_TYPE_IMMEDIATE || Barrier.TransitionType == STATE_TRANSITION_TYPE_END, "Unexpected barrier type");

        if (Barrier.Flags & STATE_TRANSITION_FLAG_ALIASING)
        {
            AliasingBarrier(m_pd3d11DeviceContext, Barrier.pResourceBefore, Barrier.pResource);
        }
        else
        {
            DEV_CHECK_ERR(Barrier.NewState != RESOURCE_STATE_UNKNOWN, "New resource state can't be unknown");
            if (RefCntAutoPtr<TextureBaseD3D11> pTexture{Barrier.pResource, IID_TextureD3D11})
            {
                TransitionResource(*pTexture, Barrier.NewState, Barrier.OldState);
            }
            else if (RefCntAutoPtr<BufferD3D11Impl> pBuffer{Barrier.pResource, IID_BufferD3D11})
            {
                TransitionResource(*pBuffer, Barrier.NewState, Barrier.OldState);
            }
            else
            {
                UNEXPECTED("The type of resource '", Barrier.pResource->GetDesc().Name, "' is not support in D3D11");
            }
        }
    }
}

void DeviceContextD3D11Impl::TransitionResource(TextureBaseD3D11& Texture, RESOURCE_STATE NewState, RESOURCE_STATE OldState, bool UpdateResourceState)
{
    if (OldState == RESOURCE_STATE_UNKNOWN)
    {
        if (Texture.IsInKnownState())
        {
            OldState = Texture.GetState();
        }
        else
        {
            LOG_ERROR_MESSAGE("Failed to transition the state of texture '", Texture.GetDesc().Name, "' because its state is unknown and is not explicitly specified");
            return;
        }
    }
    else
    {
        if (Texture.IsInKnownState() && Texture.GetState() != OldState)
        {
            LOG_ERROR_MESSAGE("The state ", GetResourceStateString(Texture.GetState()), " of texture '",
                              Texture.GetDesc().Name, "' does not match the old state ", GetResourceStateString(OldState),
                              " specified by the barrier");
        }
    }

    if ((NewState & RESOURCE_STATE_UNORDERED_ACCESS) != 0)
    {
        DEV_CHECK_ERR((NewState & (RESOURCE_STATE_GENERIC_READ | RESOURCE_STATE_INPUT_ATTACHMENT)) == 0, "Unordered access state is not compatible with any input state");
        UnbindTextureFromInput(Texture, Texture.GetD3D11Texture());
    }

    if ((NewState & (RESOURCE_STATE_GENERIC_READ | RESOURCE_STATE_INPUT_ATTACHMENT)) != 0)
    {
        if ((OldState & RESOURCE_STATE_RENDER_TARGET) != 0)
            UnbindTextureFromRenderTarget(Texture);

        if ((OldState & RESOURCE_STATE_DEPTH_WRITE) != 0)
            UnbindTextureFromDepthStencil(Texture);

        if ((OldState & RESOURCE_STATE_UNORDERED_ACCESS) != 0)
        {
            UnbindResourceFromUAV(Texture.GetD3D11Texture());
            if (Texture.IsInKnownState())
                Texture.ClearState(RESOURCE_STATE_UNORDERED_ACCESS);
        }
    }

    if (UpdateResourceState)
    {
        Texture.SetState(NewState);
    }
}

void DeviceContextD3D11Impl::TransitionResource(BufferD3D11Impl& Buffer, RESOURCE_STATE NewState, RESOURCE_STATE OldState, bool UpdateResourceState)
{
    if (OldState == RESOURCE_STATE_UNKNOWN)
    {
        if (Buffer.IsInKnownState())
        {
            OldState = Buffer.GetState();
        }
        else
        {
            LOG_ERROR_MESSAGE("Failed to transition the state of buffer '", Buffer.GetDesc().Name, "' because the buffer state is unknown and is not explicitly specified");
            return;
        }
    }
    else
    {
        if (Buffer.IsInKnownState() && Buffer.GetState() != OldState)
        {
            LOG_ERROR_MESSAGE("The state ", GetResourceStateString(Buffer.GetState()), " of buffer '",
                              Buffer.GetDesc().Name, "' does not match the old state ", GetResourceStateString(OldState),
                              " specified by the barrier");
        }
    }

    if ((NewState & RESOURCE_STATE_UNORDERED_ACCESS) != 0)
    {
        DEV_CHECK_ERR((NewState & RESOURCE_STATE_GENERIC_READ) == 0, "Unordered access state is not compatible with any input state");
        UnbindBufferFromInput(Buffer, OldState, Buffer.m_pd3d11Buffer);
    }

    if ((NewState & RESOURCE_STATE_GENERIC_READ) != 0)
    {
        UnbindResourceFromUAV(Buffer.m_pd3d11Buffer);
        if (Buffer.IsInKnownState())
            Buffer.ClearState(RESOURCE_STATE_UNORDERED_ACCESS);
    }

    if (UpdateResourceState)
    {
        Buffer.SetState(NewState);
    }
}

void DeviceContextD3D11Impl::ResolveTextureSubresource(ITexture*                               pSrcTexture,
                                                       ITexture*                               pDstTexture,
                                                       const ResolveTextureSubresourceAttribs& ResolveAttribs)
{
    TDeviceContextBase::ResolveTextureSubresource(pSrcTexture, pDstTexture, ResolveAttribs);

    TextureBaseD3D11* const pSrcTexD3D11 = ClassPtrCast<TextureBaseD3D11>(pSrcTexture);
    TextureBaseD3D11* const pDstTexD3D11 = ClassPtrCast<TextureBaseD3D11>(pDstTexture);
    const TextureDesc&      SrcTexDesc   = pSrcTexD3D11->GetDesc();
    const TextureDesc&      DstTexDesc   = pDstTexD3D11->GetDesc();

    TEXTURE_FORMAT Format = ResolveAttribs.Format;
    if (Format == TEX_FORMAT_UNKNOWN)
    {
        const TextureFormatAttribs& SrcFmtAttribs = GetTextureFormatAttribs(SrcTexDesc.Format);
        if (!SrcFmtAttribs.IsTypeless)
        {
            Format = SrcTexDesc.Format;
        }
        else
        {
            const TextureFormatAttribs& DstFmtAttribs = GetTextureFormatAttribs(DstTexDesc.Format);
            DEV_CHECK_ERR(!DstFmtAttribs.IsTypeless, "Resolve operation format can't be typeless when both source and destination textures are typeless");
            Format = DstFmtAttribs.Format;
        }
    }

    DXGI_FORMAT DXGIFmt        = TexFormatToDXGI_Format(Format);
    UINT        SrcSubresIndex = D3D11CalcSubresource(ResolveAttribs.SrcMipLevel, ResolveAttribs.SrcSlice, SrcTexDesc.MipLevels);
    UINT        DstSubresIndex = D3D11CalcSubresource(ResolveAttribs.DstMipLevel, ResolveAttribs.DstSlice, DstTexDesc.MipLevels);
    m_pd3d11DeviceContext->ResolveSubresource(pDstTexD3D11->GetD3D11Texture(), DstSubresIndex, pSrcTexD3D11->GetD3D11Texture(), SrcSubresIndex, DXGIFmt);
}

void DeviceContextD3D11Impl::BuildBLAS(const BuildBLASAttribs& Attribs)
{
    UNSUPPORTED("BuildBLAS is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::BuildTLAS(const BuildTLASAttribs& Attribs)
{
    UNSUPPORTED("BuildTLAS is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::CopyBLAS(const CopyBLASAttribs& Attribs)
{
    UNSUPPORTED("CopyBLAS is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::CopyTLAS(const CopyTLASAttribs& Attribs)
{
    UNSUPPORTED("CopyTLAS is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::WriteBLASCompactedSize(const WriteBLASCompactedSizeAttribs& Attribs)
{
    UNSUPPORTED("CopyTLAS is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::WriteTLASCompactedSize(const WriteTLASCompactedSizeAttribs& Attribs)
{
    UNSUPPORTED("CopyTLAS is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::TraceRays(const TraceRaysAttribs& Attribs)
{
    UNSUPPORTED("TraceRays is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::TraceRaysIndirect(const TraceRaysIndirectAttribs& Attribs)
{
    UNSUPPORTED("TraceRaysIndirect is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::UpdateSBT(IShaderBindingTable* pSBT, const UpdateIndirectRTBufferAttribs* pUpdateIndirectBufferAttribs)
{
    UNSUPPORTED("UpdateSBT is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::SetShadingRate(SHADING_RATE BaseRate, SHADING_RATE_COMBINER PrimitiveCombiner, SHADING_RATE_COMBINER TextureCombiner)
{
    UNSUPPORTED("SetShadingRate is not supported in DirectX 11");
}

void DeviceContextD3D11Impl::BindSparseResourceMemory(const BindSparseResourceMemoryAttribs& Attribs)
{
    TDeviceContextBase::BindSparseResourceMemory(Attribs, 0);

    VERIFY_EXPR(Attribs.NumBufferBinds != 0 || Attribs.NumTextureBinds != 0);

    DEV_CHECK_ERR(CComQIPtr<ID3D11DeviceContext2>{m_pd3d11DeviceContext}, "Failed to query ID3D11DeviceContext2");
    ID3D11DeviceContext2* pd3d11DeviceContext2 = static_cast<ID3D11DeviceContext2*>(m_pd3d11DeviceContext.p);

    D3D11TileMappingHelper TileMapping;
    for (Uint32 i = 0; i < Attribs.NumBufferBinds; ++i)
    {
        const SparseBufferMemoryBindInfo& BuffBind   = Attribs.pBufferBinds[i];
        BufferD3D11Impl*                  pBuffD3D11 = ClassPtrCast<BufferD3D11Impl>(BuffBind.pBuffer);

        for (Uint32 r = 0; r < BuffBind.NumRanges; ++r)
        {
            const SparseBufferMemoryBindRange& BindRange = BuffBind.pRanges[r];
            DEV_CHECK_ERR((BindRange.MemoryOffset % D3D11_2_TILED_RESOURCE_TILE_SIZE_IN_BYTES) == 0,
                          "MemoryOffset must be a multiple of sparse block size");

            TileMapping.AddBufferBindRange(BindRange);
        }

        TileMapping.Commit(pd3d11DeviceContext2, pBuffD3D11);
    }

    for (Uint32 i = 0; i < Attribs.NumTextureBinds; ++i)
    {
        const SparseTextureMemoryBindInfo& TexBind        = Attribs.pTextureBinds[i];
        TextureBaseD3D11*                  pTexD3D11      = ClassPtrCast<TextureBaseD3D11>(TexBind.pTexture);
        const SparseTextureProperties&     TexSparseProps = pTexD3D11->GetSparseProperties();
        const TextureDesc&                 TexDesc        = pTexD3D11->GetDesc();
        const bool                         UseNVApi       = pTexD3D11->IsUsingNVApi();

        for (Uint32 r = 0; r < TexBind.NumRanges; ++r)
        {
            const SparseTextureMemoryBindRange& BindRange = TexBind.pRanges[r];
            TileMapping.AddTextureBindRange(BindRange, TexSparseProps, TexDesc, UseNVApi);
        }

        TileMapping.Commit(pd3d11DeviceContext2, pTexD3D11);
    }
}

bool DeviceContextD3D11Impl::ResizeTilePool(ID3D11Buffer* pBuffer, UINT NewSize)
{
    DEV_CHECK_ERR(CComQIPtr<ID3D11DeviceContext2>{m_pd3d11DeviceContext}, "Failed to query ID3D11DeviceContext2");
    ID3D11DeviceContext2* pd3d11DeviceContext2 = static_cast<ID3D11DeviceContext2*>(m_pd3d11DeviceContext.p);

    return SUCCEEDED(pd3d11DeviceContext2->ResizeTilePool(pBuffer, NewSize));
}

void DeviceContextD3D11Impl::BeginDebugGroup(const Char* Name, const float* pColor)
{
    TDeviceContextBase::BeginDebugGroup(Name, pColor, 0);

    if (CComQIPtr<ID3DUserDefinedAnnotation> pAnnotation{m_pd3d11DeviceContext})
    {
        pAnnotation->BeginEvent(WidenString(Name).c_str());
    }
}

void DeviceContextD3D11Impl::EndDebugGroup()
{
    TDeviceContextBase::EndDebugGroup(0);

    if (CComQIPtr<ID3DUserDefinedAnnotation> pAnnotation{m_pd3d11DeviceContext})
    {
        pAnnotation->EndEvent();
    }
}

void DeviceContextD3D11Impl::InsertDebugLabel(const Char* Label, const float* pColor)
{
    TDeviceContextBase::InsertDebugLabel(Label, pColor, 0);

    if (CComQIPtr<ID3DUserDefinedAnnotation> pAnnotation{m_pd3d11DeviceContext})
    {
        pAnnotation->SetMarker(WidenString(Label).c_str());
    }
}

// clang-format off
#ifdef DILIGENT_DEVELOPMENT
    DEFINE_D3D11CTX_FUNC_POINTERS(GetCBMethods,      GetConstantBuffers)
    DEFINE_D3D11CTX_FUNC_POINTERS(GetSRVMethods,     GetShaderResources)
    DEFINE_D3D11CTX_FUNC_POINTERS(GetSamplerMethods, GetSamplers)

    typedef decltype (&ID3D11DeviceContext::CSGetUnorderedAccessViews) TGetUnorderedAccessViewsType;
    static const TGetUnorderedAccessViewsType GetUAVMethods[] =
    {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &ID3D11DeviceContext::CSGetUnorderedAccessViews
    };
// clang-format on

/// \tparam MaxResources              - Maximum number of resources that can be bound to D3D11 context
/// \tparam TD3D11ResourceType        - Type of D3D11 resource being checked (ID3D11ShaderResourceView,
///                                     ID3D11UnorderedAccessView, ID3D11Buffer or ID3D11SamplerState).
/// \tparam TGetD3D11ResourcesType    - Type of the device context method used to get the bound
///                                     resources
/// \param CommittedD3D11ResourcesArr - Pointer to the array of currently bound D3D11
///                                     resources, for each shader stage
/// \param GetD3D11ResMethods         - Pointer to the array of device context methods to get the bound
///                                     resources, for each shader stage
/// \param ResourceName               - Resource name
/// \param ShaderStages               - Shader stages for which to check the resources.
template <UINT MaxResources,
          typename TD3D11ResourceType,
          typename TGetD3D11ResourcesType>
void DeviceContextD3D11Impl::DvpVerifyCommittedResources(TD3D11ResourceType     CommittedD3D11ResourcesArr[][MaxResources],
                                                         Uint8                  NumCommittedResourcesArr[],
                                                         TGetD3D11ResourcesType GetD3D11ResMethods[],
                                                         const Char*            ResourceName,
                                                         SHADER_TYPE            ShaderStages)
{
    while (ShaderStages != SHADER_TYPE_UNKNOWN)
    {
        const SHADER_TYPE Stage        = ExtractLSB(ShaderStages);
        const Int32       ShaderInd    = GetShaderTypeIndex(Stage);
        const char*       ShaderName   = GetShaderTypeLiteralName(Stage);
        const auto        GetResMethod = GetD3D11ResMethods[ShaderInd];

        TD3D11ResourceType pctxResources[MaxResources] = {};
        if (GetResMethod)
        {
            (m_pd3d11DeviceContext->*GetResMethod)(0, _countof(pctxResources), pctxResources);
        }
        const auto* CommittedResources    = CommittedD3D11ResourcesArr[ShaderInd];
        const Uint8 NumCommittedResources = NumCommittedResourcesArr[ShaderInd];
        for (Uint32 Slot = 0; Slot < _countof(pctxResources); ++Slot)
        {
            if (Slot < NumCommittedResources)
            {
                DEV_CHECK_ERR(CommittedResources[Slot] == pctxResources[Slot], ResourceName, " binding mismatch found for ", ShaderName, " shader type at slot ", Slot);
            }
            else
            {
                DEV_CHECK_ERR(pctxResources[Slot] == nullptr, ResourceName, " binding mismatch found for ", ShaderName, " shader type at slot ", Slot);
                DEV_CHECK_ERR(CommittedResources[Slot] == nullptr, ResourceName, " unexpected non-null resource found for ", ShaderName, " shader type at slot ", Slot);
            }

            if (pctxResources[Slot])
                pctxResources[Slot]->Release();
        }
    }
}

template <UINT MaxResources, typename TD3D11ViewType>
void DeviceContextD3D11Impl::DvpVerifyViewConsistency(TD3D11ViewType  CommittedD3D11ViewArr[][MaxResources],
                                                      ID3D11Resource* CommittedD3D11ResourcesArr[][MaxResources],
                                                      Uint8           NumCommittedResourcesArr[],
                                                      const Char*     ResourceName,
                                                      SHADER_TYPE     ShaderStages)
{
    while (ShaderStages != SHADER_TYPE_UNKNOWN)
    {
        const SHADER_TYPE Stage                 = ExtractLSB(ShaderStages);
        const Int32       ShaderInd             = GetShaderTypeIndex(Stage);
        const char*       ShaderName            = GetShaderTypeLiteralName(Stage);
        auto*             Views                 = CommittedD3D11ViewArr[ShaderInd];
        auto*             Resources             = CommittedD3D11ResourcesArr[ShaderInd];
        const Uint8       NumCommittedResources = NumCommittedResourcesArr[ShaderInd];
        for (Uint32 Slot = 0; Slot < NumCommittedResources; ++Slot)
        {
            if (Views[Slot] != nullptr)
            {
                CComPtr<ID3D11Resource> pRefRes;
                Views[Slot]->GetResource(&pRefRes);
                DEV_CHECK_ERR(pRefRes == Resources[Slot], "Inconsistent ", ResourceName, " detected at slot ", Slot, " in shader ", ShaderName, ". The resource in the view does not match cached D3D11 resource");
            }
        }
    }
}

void DeviceContextD3D11Impl::DvpVerifyCommittedSRVs(SHADER_TYPE ShaderStages)
{
    DvpVerifyCommittedResources<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>(m_CommittedRes.d3d11SRVs, m_CommittedRes.NumSRVs, GetSRVMethods, "SRV", ShaderStages);
    DvpVerifyViewConsistency<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT>(m_CommittedRes.d3d11SRVs, m_CommittedRes.d3d11SRVResources, m_CommittedRes.NumSRVs, "SRV", ShaderStages);
}

void DeviceContextD3D11Impl::DvpVerifyCommittedUAVs(SHADER_TYPE ShaderStages)
{
    DvpVerifyCommittedResources<D3D11_PS_CS_UAV_REGISTER_COUNT>(m_CommittedRes.d3d11UAVs, m_CommittedRes.NumUAVs, GetUAVMethods, "UAV", ShaderStages);
    DvpVerifyViewConsistency<D3D11_PS_CS_UAV_REGISTER_COUNT>(m_CommittedRes.d3d11UAVs, m_CommittedRes.d3d11UAVResources, m_CommittedRes.NumUAVs, "UAV", ShaderStages);
}

void DeviceContextD3D11Impl::DvpVerifyCommittedSamplers(SHADER_TYPE ShaderStages)
{
    DvpVerifyCommittedResources<D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT>(m_CommittedRes.d3d11Samplers, m_CommittedRes.NumSamplers, GetSamplerMethods, "Sampler", ShaderStages);
}

void DeviceContextD3D11Impl::DvpVerifyCommittedCBs(SHADER_TYPE ShaderStages)
{
    DvpVerifyCommittedResources<D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT>(m_CommittedRes.d3d11CBs, m_CommittedRes.NumCBs, GetCBMethods, "Constant buffer", ShaderStages);
}

void DeviceContextD3D11Impl::DvpVerifyCommittedIndexBuffer()
{
    RefCntAutoPtr<ID3D11Buffer> pctxIndexBuffer;
    DXGI_FORMAT                 Fmt    = DXGI_FORMAT_UNKNOWN;
    UINT                        Offset = 0;
    m_pd3d11DeviceContext->IAGetIndexBuffer(&pctxIndexBuffer, &Fmt, &Offset);
    if (m_CommittedD3D11IndexBuffer && !pctxIndexBuffer)
        UNEXPECTED("D3D11 index buffer is not bound to the context");
    if (!m_CommittedD3D11IndexBuffer && pctxIndexBuffer)
        UNEXPECTED("Unexpected D3D11 index buffer is bound to the context");

    if (m_CommittedD3D11IndexBuffer && pctxIndexBuffer)
    {
        DEV_CHECK_ERR(m_CommittedD3D11IndexBuffer == pctxIndexBuffer, "Index buffer binding mismatch detected");
        if (Fmt == DXGI_FORMAT_R32_UINT)
        {
            DEV_CHECK_ERR(m_CommittedIBFormat == VT_UINT32, "Index buffer format mismatch detected");
        }
        else if (Fmt == DXGI_FORMAT_R16_UINT)
        {
            DEV_CHECK_ERR(m_CommittedIBFormat == VT_UINT16, "Index buffer format mismatch detected");
        }
        DEV_CHECK_ERR(m_CommittedD3D11IndexDataStartOffset == Offset, "Index buffer offset mismatch detected");
    }
}

void DeviceContextD3D11Impl::DvpVerifyCommittedVertexBuffers()
{
    CComPtr<ID3D11InputLayout> pInputLayout;
    m_pd3d11DeviceContext->IAGetInputLayout(&pInputLayout);
    DEV_CHECK_ERR(pInputLayout == m_CommittedD3D11InputLayout, "Inconsistent input layout");

    const Uint32  MaxVBs = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
    ID3D11Buffer* pVBs[MaxVBs];
    UINT          Strides[MaxVBs];
    UINT          Offsets[MaxVBs];
    m_pd3d11DeviceContext->IAGetVertexBuffers(0, MaxVBs, pVBs, Strides, Offsets);
    UINT NumBoundVBs = m_NumCommittedD3D11VBs;
    for (Uint32 Slot = 0; Slot < MaxVBs; ++Slot)
    {
        if (Slot < NumBoundVBs)
        {
            const auto& BoundD3D11VB  = m_CommittedD3D11VertexBuffers[Slot];
            UINT        BoundVBStride = m_CommittedD3D11VBStrides[Slot];
            UINT        BoundVBOffset = m_CommittedD3D11VBOffsets[Slot];
            if (BoundD3D11VB && !pVBs[Slot])
                DEV_CHECK_ERR(pVBs[Slot] == nullptr, "Missing D3D11 buffer detected at slot ", Slot);
            if (!BoundD3D11VB && pVBs[Slot])
                DEV_CHECK_ERR(pVBs[Slot] == nullptr, "Unexpected D3D11 buffer detected at slot ", Slot);
            if (BoundD3D11VB && pVBs[Slot])
            {
                DEV_CHECK_ERR(BoundD3D11VB == pVBs[Slot], "Vertex buffer mismatch detected at slot ", Slot);
                DEV_CHECK_ERR(BoundVBOffset == Offsets[Slot], "Offset mismatch detected at slot ", Slot);
                DEV_CHECK_ERR(BoundVBStride == Strides[Slot], "Stride mismatch detected at slot ", Slot);
            }
        }
        else
        {
            DEV_CHECK_ERR(pVBs[Slot] == nullptr, "Unexpected D3D11 buffer detected at slot ", Slot);
        }

        if (pVBs[Slot])
            pVBs[Slot]->Release();
    }
}

template <typename TD3D11ShaderType, typename TGetShaderMethodType>
void DvpVerifyCommittedShadersHelper(SHADER_TYPE                      ShaderType,
                                     const CComPtr<ID3D11DeviceChild> BoundD3DShaders[],
                                     ID3D11DeviceContext*             pCtx,
                                     TGetShaderMethodType             GetShaderMethod)
{
    RefCntAutoPtr<TD3D11ShaderType> pctxShader;
    (pCtx->*GetShaderMethod)(&pctxShader, nullptr, nullptr);
    const auto& BoundShader = BoundD3DShaders[GetShaderTypeIndex(ShaderType)];
    DEV_CHECK_ERR(BoundShader == pctxShader, GetShaderTypeLiteralName(ShaderType), " binding mismatch detected");
}
void DeviceContextD3D11Impl::DvpVerifyCommittedShaders()
{
#    define VERIFY_SHADER(NAME, Name, N) DvpVerifyCommittedShadersHelper<ID3D11##Name##Shader>(SHADER_TYPE_##NAME, m_CommittedD3DShaders, m_pd3d11DeviceContext, &ID3D11DeviceContext::N##SGetShader)
    // These shaders which are not set will be unbound from the D3D11 device context
    VERIFY_SHADER(VERTEX, Vertex, V);
    VERIFY_SHADER(PIXEL, Pixel, P);
    VERIFY_SHADER(GEOMETRY, Geometry, G);
    VERIFY_SHADER(DOMAIN, Domain, D);
    VERIFY_SHADER(HULL, Hull, H);
    VERIFY_SHADER(COMPUTE, Compute, C);
}

#endif // DILIGENT_DEVELOPMENT

} // namespace Diligent

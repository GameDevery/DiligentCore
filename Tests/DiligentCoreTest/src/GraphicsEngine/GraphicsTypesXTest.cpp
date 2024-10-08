/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

#include "GraphicsTypesX.hpp"
#include "CommonlyUsedStates.h"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

template <typename TypeX, typename Type>
void TestCtorsAndAssignments(const Type& Ref)
{
    TypeX DescX{Ref};
    EXPECT_TRUE(DescX == Ref);

    TypeX DescX2{DescX};
    EXPECT_TRUE(DescX2 == Ref);
    EXPECT_TRUE(DescX2 == DescX);

    TypeX DescX3;
    EXPECT_TRUE(DescX3 != Ref);
    EXPECT_TRUE(DescX3 != DescX);

    DescX3 = DescX;
    EXPECT_TRUE(DescX3 == Ref);
    EXPECT_TRUE(DescX3 == DescX);

    TypeX DescX4{std::move(DescX3)};
    EXPECT_TRUE(DescX4 == Ref);
    EXPECT_TRUE(DescX4 == DescX);

    DescX3 = std::move(DescX4);
    EXPECT_TRUE(DescX3 == Ref);
    EXPECT_TRUE(DescX3 == DescX);

    TypeX DescX5;
    DescX5 = DescX;
    EXPECT_TRUE(DescX5 == Ref);
    EXPECT_TRUE(DescX5 == DescX);

    DescX5.Clear();
    EXPECT_TRUE(DescX5 == Type{});
}

struct StringPool
{
    const char* operator()(const char* str)
    {
        return Strings.emplace(str).first->c_str();
    }

    void Clear()
    {
        std::unordered_set<std::string> Empty;
        std::swap(Strings, Empty);
    }

private:
    std::unordered_set<std::string> Strings;
};

constexpr const char* RawStr(const char* str) { return str; }

TEST(GraphicsTypesXTest, SubpassDescX)
{
    constexpr AttachmentReference   Inputs[]        = {{2, RESOURCE_STATE_SHADER_RESOURCE}, {4, RESOURCE_STATE_SHADER_RESOURCE}};
    constexpr AttachmentReference   RenderTargets[] = {{1, RESOURCE_STATE_RENDER_TARGET}, {2, RESOURCE_STATE_RENDER_TARGET}};
    constexpr AttachmentReference   Resolves[]      = {{3, RESOURCE_STATE_RESOLVE_DEST}, {4, RESOURCE_STATE_RESOLVE_DEST}};
    constexpr AttachmentReference   Resolves2[]     = {{ATTACHMENT_UNUSED, RESOURCE_STATE_UNKNOWN}, {4, RESOURCE_STATE_RESOLVE_DEST}};
    constexpr AttachmentReference   DepthStencil    = {5, RESOURCE_STATE_DEPTH_WRITE};
    constexpr Uint32                Preserves[]     = {1, 3, 5};
    constexpr ShadingRateAttachment ShadingRate     = {{6, RESOURCE_STATE_SHADING_RATE}, 128, 256};

    SubpassDesc Ref;
    Ref.InputAttachmentCount = _countof(Inputs);
    Ref.pInputAttachments    = Inputs;
    TestCtorsAndAssignments<SubpassDescX>(Ref);

    Ref.RenderTargetAttachmentCount = _countof(RenderTargets);
    Ref.pRenderTargetAttachments    = RenderTargets;
    TestCtorsAndAssignments<SubpassDescX>(Ref);

    Ref.pResolveAttachments = Resolves;
    TestCtorsAndAssignments<SubpassDescX>(Ref);

    Ref.PreserveAttachmentCount = _countof(Preserves);
    Ref.pPreserveAttachments    = Preserves;
    TestCtorsAndAssignments<SubpassDescX>(Ref);

    Ref.pDepthStencilAttachment = &DepthStencil;
    Ref.pShadingRateAttachment  = &ShadingRate;
    TestCtorsAndAssignments<SubpassDescX>(Ref);

    SubpassDescX DescCopy;
    SubpassDescX DescMove;
    SubpassDesc  Ref2;
    {
        SubpassDescX DescX;
        DescX
            .AddInput(Inputs[0])
            .AddInput(Inputs[1])
            .AddRenderTarget(RenderTargets[0], &Resolves[0])
            .AddRenderTarget(RenderTargets[1], &Resolves[1])
            .SetDepthStencil(&DepthStencil)
            .SetShadingRate(&ShadingRate)
            .AddPreserve(Preserves[0])
            .AddPreserve(Preserves[1])
            .AddPreserve(Preserves[2]);
        EXPECT_EQ(DescX, Ref);

        DescX.ClearRenderTargets();
        Ref.RenderTargetAttachmentCount = 0;
        Ref.pRenderTargetAttachments    = nullptr;
        Ref.pResolveAttachments         = nullptr;
        EXPECT_EQ(DescX, Ref);

        Ref.RenderTargetAttachmentCount = _countof(RenderTargets);
        Ref.pRenderTargetAttachments    = RenderTargets;
        DescX
            .AddRenderTarget(RenderTargets[0])
            .AddRenderTarget(RenderTargets[1]);
        EXPECT_EQ(DescX, Ref);

        Ref.pResolveAttachments = Resolves2;
        DescX.ClearRenderTargets();
        DescX
            .AddRenderTarget(RenderTargets[0])
            .AddRenderTarget(RenderTargets[1], &Resolves2[1]);
        EXPECT_EQ(DescX, Ref);

        {
            DescCopy       = DescX;
            auto DescCopy2 = DescCopy;
            DescMove       = std::move(DescCopy2);
            Ref2           = Ref;
        }

        DescX.ClearInputs();
        Ref.InputAttachmentCount = 0;
        Ref.pInputAttachments    = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearPreserves();
        Ref.PreserveAttachmentCount = 0;
        Ref.pPreserveAttachments    = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.SetDepthStencil(nullptr);
        Ref.pDepthStencilAttachment = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.SetShadingRate(nullptr);
        Ref.pShadingRateAttachment = nullptr;
        EXPECT_EQ(DescX, Ref);
    }

    EXPECT_EQ(DescCopy, Ref2);
    EXPECT_EQ(DescMove, Ref2);

    SubpassDescX DescCopy2{DescCopy};
    SubpassDescX DescMove2{std::move(DescMove)};
    EXPECT_EQ(DescCopy2, Ref2);
    EXPECT_EQ(DescMove2, Ref2);
    EXPECT_EQ(DescCopy2, DescMove2);
}

TEST(GraphicsTypesXTest, RenderPassDescX)
{
    const RenderPassAttachmentDesc Attachments[] =
        {
            {TEX_FORMAT_RGBA8_UNORM_SRGB, 2},
            {TEX_FORMAT_RGBA32_FLOAT},
            {TEX_FORMAT_R16_UINT},
            {TEX_FORMAT_D32_FLOAT},
        };

    RenderPassDesc Ref;
    Ref.AttachmentCount = _countof(Attachments);
    Ref.pAttachments    = Attachments;
    TestCtorsAndAssignments<RenderPassDescX>(Ref);

    SubpassDescX Subpass0, Subpass1;
    Subpass0
        .AddInput({1, RESOURCE_STATE_SHADER_RESOURCE})
        .AddRenderTarget({2, RESOURCE_STATE_RENDER_TARGET})
        .AddRenderTarget({3, RESOURCE_STATE_RENDER_TARGET})
        .SetDepthStencil({4, RESOURCE_STATE_DEPTH_WRITE});
    Subpass1
        .AddPreserve(5)
        .AddPreserve(6)
        .AddRenderTarget({7, RESOURCE_STATE_RENDER_TARGET})
        .SetShadingRate({{6, RESOURCE_STATE_SHADING_RATE}, 128, 256});

    SubpassDesc Subpasses[] = {Subpass0, Subpass1};
    Ref.SubpassCount        = _countof(Subpasses);
    Ref.pSubpasses          = Subpasses;
    TestCtorsAndAssignments<RenderPassDescX>(Ref);

    constexpr SubpassDependencyDesc Dependencies[] =
        {
            {0, 1, PIPELINE_STAGE_FLAG_DRAW_INDIRECT, PIPELINE_STAGE_FLAG_VERTEX_INPUT, ACCESS_FLAG_INDIRECT_COMMAND_READ, ACCESS_FLAG_INDEX_READ},
            {2, 3, PIPELINE_STAGE_FLAG_VERTEX_SHADER, PIPELINE_STAGE_FLAG_HULL_SHADER, ACCESS_FLAG_VERTEX_READ, ACCESS_FLAG_UNIFORM_READ},
            {4, 5, PIPELINE_STAGE_FLAG_DOMAIN_SHADER, PIPELINE_STAGE_FLAG_GEOMETRY_SHADER, ACCESS_FLAG_SHADER_READ, ACCESS_FLAG_SHADER_WRITE},
        };
    Ref.DependencyCount = _countof(Dependencies);
    Ref.pDependencies   = Dependencies;
    TestCtorsAndAssignments<RenderPassDescX>(Ref);

    RenderPassDescX DescCopy;
    RenderPassDescX DescMove;
    RenderPassDesc  Ref2;
    {
        RenderPassDescX DescX;
        DescX
            .AddAttachment(Attachments[0])
            .AddAttachment(Attachments[1])
            .AddAttachment(Attachments[2])
            .AddAttachment(Attachments[3])
            .AddSubpass(Subpass0)
            .AddSubpass(Subpass1)
            .AddDependency(Dependencies[0])
            .AddDependency(Dependencies[1])
            .AddDependency(Dependencies[2]);
        EXPECT_EQ(DescX, Ref);

        {
            DescCopy       = DescX;
            auto DescCopy2 = DescCopy;
            DescMove       = std::move(DescCopy2);
            Ref2           = Ref;
        }

        DescX.ClearAttachments();
        Ref.AttachmentCount = 0;
        Ref.pAttachments    = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearSubpasses();
        Ref.SubpassCount = 0;
        Ref.pSubpasses   = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearDependencies();
        Ref.DependencyCount = 0;
        Ref.pDependencies   = nullptr;
        EXPECT_EQ(DescX, Ref);
    }

    EXPECT_EQ(DescCopy, Ref2);
    EXPECT_EQ(DescMove, Ref2);

    RenderPassDescX DescCopy2{DescCopy};
    RenderPassDescX DescMove2{std::move(DescMove)};
    EXPECT_EQ(DescCopy2, Ref2);
    EXPECT_EQ(DescMove2, Ref2);
    EXPECT_EQ(DescCopy2, DescMove2);
}


TEST(GraphicsTypesXTest, InputLayoutDescX)
{
    // clang-format off

#define ATTRIB1(POOL) POOL("ATTRIB1"), 0u, 0u, 2u, VT_FLOAT32
#define ATTRIB2(POOL) POOL("ATTRIB2"), 1u, 0u, 2u, VT_FLOAT32
#define ATTRIB3(POOL) POOL("ATTRIB3"), 2u, 0u, 4u, VT_UINT8, True
#define ATTRIB4(POOL) POOL("ATTRIB4"), 3u, 0u, 3u, VT_INT32

    // clang-format on

    constexpr LayoutElement Elements[] =
        {
            {ATTRIB1(RawStr)},
            {ATTRIB2(RawStr)},
            {ATTRIB3(RawStr)},
        };

    InputLayoutDesc Ref;
    Ref.NumElements    = _countof(Elements);
    Ref.LayoutElements = Elements;
    TestCtorsAndAssignments<InputLayoutDescX>(Ref);

    {
        StringPool       Pool;
        InputLayoutDescX DescX;
        DescX
            .Add({ATTRIB1(Pool)})
            .Add(ATTRIB2(Pool))
            .Add({ATTRIB3(Pool)});
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);

        DescX.Clear();
        EXPECT_EQ(DescX, InputLayoutDesc{});
    }

    InputLayoutDescX DescCopy;
    InputLayoutDescX DescMove;
    {
        StringPool       Pool;
        InputLayoutDescX DescX{
            {
                {ATTRIB1(Pool)},
                {ATTRIB2(Pool)},
                {ATTRIB3(Pool)},
            } //
        };
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);

        DescCopy = DescX;
        DescMove = std::move(DescX);
    }

    EXPECT_EQ(DescCopy, Ref);
    EXPECT_EQ(DescMove, Ref);

    InputLayoutDescX DescCopy2{DescCopy};
    InputLayoutDescX DescMove2{std::move(DescMove)};
    EXPECT_EQ(DescCopy2, Ref);
    EXPECT_EQ(DescMove2, Ref);
    EXPECT_EQ(DescCopy2, DescMove2);

    {
        StringPool       Pool;
        InputLayoutDescX DescX;
        DescX
            .Add({ATTRIB1(Pool)})
            .Add({ATTRIB4(Pool)})
            .Add(ATTRIB2(Pool))
            .Add(ATTRIB4(Pool))
            .Add({ATTRIB3(Pool)})
            .Add({ATTRIB4(Pool)});
        Pool.Clear();

        DescX
            .Remove(5)
            .Remove(1)
            .Remove(2);
        EXPECT_EQ(DescX, Ref);

        EXPECT_EQ(DescX.GetNumElements(), 3u);
        for (Uint32 i = 0; i < std::min(Ref.NumElements, DescX.GetNumElements()); ++i)
        {
            EXPECT_EQ(Ref.LayoutElements[i], DescX[i]);
        }
    }

#undef ATTRIB1
#undef ATTRIB2
#undef ATTRIB3
#undef ATTRIB4
}


TEST(GraphicsTypesXTest, FramebufferDescX)
{
    ITextureView* ppAttachments[] = {
        reinterpret_cast<ITextureView*>(uintptr_t{0x1}),
        reinterpret_cast<ITextureView*>(uintptr_t{0x2}),
        reinterpret_cast<ITextureView*>(uintptr_t{0x3}),
    };
    FramebufferDesc Ref;
    Ref.Name            = "Test";
    Ref.pRenderPass     = reinterpret_cast<IRenderPass*>(uintptr_t{0xA});
    Ref.AttachmentCount = _countof(ppAttachments);
    Ref.ppAttachments   = ppAttachments;
    Ref.Width           = 256;
    Ref.Height          = 128;
    Ref.NumArraySlices  = 6;
    TestCtorsAndAssignments<FramebufferDescX>(Ref);

    FramebufferDescX DescCopy;
    FramebufferDescX DescMove;
    FramebufferDesc  Ref2;
    {
        FramebufferDescX DescX;

        StringPool Pool;
        DescX.SetName(Pool("Test"));
        Pool.Clear();

        DescX
            .SetRenderPass(reinterpret_cast<IRenderPass*>(uintptr_t{0xA}))
            .SetWidth(256)
            .SetHeight(128)
            .SetNumArraySlices(6)
            .AddAttachment(ppAttachments[0])
            .AddAttachment(ppAttachments[1])
            .AddAttachment(ppAttachments[2]);
        EXPECT_EQ(DescX, Ref);

        {
            DescCopy       = DescX;
            auto DescCopy2 = DescCopy;
            DescMove       = std::move(DescCopy2);
            Ref2           = Ref;
        }

        DescX.ClearAttachments();
        Ref.AttachmentCount = 0;
        Ref.ppAttachments   = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.Clear();
        EXPECT_EQ(DescX, FramebufferDesc{});
    }

    EXPECT_EQ(DescCopy, Ref2);
    EXPECT_EQ(DescMove, Ref2);

    FramebufferDescX DescCopy2{DescCopy};
    FramebufferDescX DescMove2{std::move(DescMove)};
    EXPECT_EQ(DescCopy2, Ref2);
    EXPECT_EQ(DescMove2, Ref2);
    EXPECT_EQ(DescCopy2, DescMove2);
}

TEST(GraphicsTypesXTest, PipelineResourceSignatureDescX)
{
    // clang-format off

#define RES1(POOL) SHADER_TYPE_VERTEX,  POOL("g_Tex2D_1"),   1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV,     SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC
#define RES2(POOL) SHADER_TYPE_PIXEL,   POOL("g_Tex2D_2"),   1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV,     SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE
#define RES3(POOL) SHADER_TYPE_COMPUTE, POOL("ConstBuff_1"), 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_STATIC

#define SAM1(POOL) SHADER_TYPE_ALL_GRAPHICS, POOL("g_Sampler"),  SamplerDesc{FILTER_TYPE_POINT,  FILTER_TYPE_POINT,  FILTER_TYPE_POINT}
#define SAM2(POOL) SHADER_TYPE_ALL_GRAPHICS, POOL("g_Sampler2"), SamplerDesc{FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR}

    // clang-format on
    constexpr PipelineResourceDesc Resources[] =
        {
            {RES1(RawStr)},
            {RES2(RawStr)},
            {RES3(RawStr)},
        };

    PipelineResourceSignatureDesc Ref;
    Ref.Name                       = "Test";
    Ref.BindingIndex               = 4;
    Ref.CombinedSamplerSuffix      = "Suffix";
    Ref.UseCombinedTextureSamplers = true;
    Ref.NumResources               = _countof(Resources);
    Ref.Resources                  = Resources;
    TestCtorsAndAssignments<PipelineResourceSignatureDescX>(Ref);

    constexpr ImmutableSamplerDesc ImtblSamplers[] =
        {
            {SAM1(RawStr)},
            {SAM2(RawStr)},
        };
    Ref.NumImmutableSamplers = _countof(ImtblSamplers);
    Ref.ImmutableSamplers    = ImtblSamplers;
    TestCtorsAndAssignments<PipelineResourceSignatureDescX>(Ref);

    {
        StringPool                     Pool;
        PipelineResourceSignatureDescX DescX{
            {
                {RES1(Pool)},
                {RES2(Pool)},
                {RES3(Pool)},
            },
            {
                {SAM1(Pool)},
                {SAM2(Pool)},
            } //
        };
        Pool.Clear();
        DescX
            .SetName(Pool("Test"))
            .SetCombinedSamplerSuffix(Pool("Suffix"))
            .SetBindingIndex(4)
            .SetUseCombinedTextureSamplers(true);
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);
    }


    PipelineResourceSignatureDescX DescCopy;
    PipelineResourceSignatureDescX DescMove;
    PipelineResourceSignatureDesc  Ref2;
    {
        Ref.NumImmutableSamplers = 0;
        Ref.ImmutableSamplers    = nullptr;

        StringPool Pool;

        PipelineResourceSignatureDescX DescX;
        DescX.SetName(Pool("Test"));
        DescX.SetCombinedSamplerSuffix(Pool("Suffix"));
        Pool.Clear();
        DescX.BindingIndex               = 4;
        DescX.UseCombinedTextureSamplers = true;
        DescX
            .AddResource({RES1(Pool)})
            .AddResource(RES2(Pool))
            .AddResource({RES3(Pool)});
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);

        Ref.NumImmutableSamplers = _countof(ImtblSamplers);
        Ref.ImmutableSamplers    = ImtblSamplers;
        DescX
            .AddImmutableSampler({SAM1(Pool)})
            .AddImmutableSampler(SAM2(Pool));
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);

        {
            DescCopy       = DescX;
            auto DescCopy2 = DescCopy;
            DescMove       = std::move(DescCopy2);
            Ref2           = Ref;
        }

        DescX.RemoveImmutableSampler("g_Sampler2");
        --Ref.NumImmutableSamplers;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearImmutableSamplers();
        Ref.NumImmutableSamplers = 0;
        Ref.ImmutableSamplers    = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.RemoveResource("ConstBuff_1");
        --Ref.NumResources;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearResources();
        Ref.NumResources = 0;
        Ref.Resources    = nullptr;
        EXPECT_EQ(DescX, Ref);
    }

    EXPECT_EQ(DescCopy, Ref2);
    EXPECT_EQ(DescMove, Ref2);

    PipelineResourceSignatureDescX DescCopy2{DescCopy};
    PipelineResourceSignatureDescX DescMove2{std::move(DescMove)};
    EXPECT_EQ(DescCopy2, Ref2);
    EXPECT_EQ(DescMove2, Ref2);
    EXPECT_EQ(DescCopy2, DescMove2);

#undef RES1
#undef RES2
#undef RES3

#undef SAM1
#undef SAM2
}

TEST(GraphicsTypesXTest, PipelineResourceLayoutDescX)
{
    // clang-format off

#define VAR1(POOL) SHADER_TYPE_VERTEX,  POOL("g_Tex2D_1"),   SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC
#define VAR2(POOL) SHADER_TYPE_PIXEL,   POOL("g_Tex2D_2"),   SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE
#define VAR3(POOL) SHADER_TYPE_COMPUTE, POOL("ConstBuff_1"), SHADER_RESOURCE_VARIABLE_TYPE_STATIC

#define SAM1(POOL) SHADER_TYPE_ALL_GRAPHICS, POOL("g_Sampler"),  SamplerDesc{FILTER_TYPE_POINT,  FILTER_TYPE_POINT,  FILTER_TYPE_POINT}
#define SAM2(POOL) SHADER_TYPE_ALL_GRAPHICS, POOL("g_Sampler2"), SamplerDesc{FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR}

    // clang-format on

    constexpr ShaderResourceVariableDesc Variables[] =
        {
            {VAR1(RawStr)},
            {VAR2(RawStr)},
            {VAR3(RawStr)},
        };

    PipelineResourceLayoutDesc Ref;
    Ref.NumVariables = _countof(Variables);
    Ref.Variables    = Variables;
    TestCtorsAndAssignments<PipelineResourceLayoutDescX>(Ref);

    constexpr ImmutableSamplerDesc ImtblSamplers[] =
        {
            {SAM1(RawStr)},
            {SAM2(RawStr)},
        };
    Ref.NumImmutableSamplers = _countof(ImtblSamplers);
    Ref.ImmutableSamplers    = ImtblSamplers;
    TestCtorsAndAssignments<PipelineResourceLayoutDescX>(Ref);

    {
        StringPool                  Pool;
        PipelineResourceLayoutDescX DescX{
            {
                {VAR1(Pool)},
                {VAR2(Pool)},
                {VAR3(Pool)},
            },
            {
                {SAM1(Pool)},
                {SAM2(Pool)},
            } //
        };
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);
    }


    PipelineResourceLayoutDescX DescCopy;
    PipelineResourceLayoutDescX DescMove;
    PipelineResourceLayoutDesc  Ref2;
    {
        Ref.NumImmutableSamplers = 0;
        Ref.ImmutableSamplers    = nullptr;

        Ref.DefaultVariableType        = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
        Ref.DefaultVariableMergeStages = SHADER_TYPE_ALL_GRAPHICS;

        StringPool                  Pool;
        PipelineResourceLayoutDescX DescX;
        DescX
            .SetDefaultVariableType(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .SetDefaultVariableMergeStages(SHADER_TYPE_ALL_GRAPHICS)
            .AddVariable({VAR1(Pool)})
            .AddVariable(VAR2(Pool))
            .AddVariable({VAR3(Pool)});
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);

        Ref.NumImmutableSamplers = _countof(ImtblSamplers);
        Ref.ImmutableSamplers    = ImtblSamplers;
        DescX
            .AddImmutableSampler({SAM1(Pool)})
            .AddImmutableSampler(SAM2(Pool));
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);

        {
            DescCopy       = DescX;
            auto DescCopy2 = DescCopy;
            DescMove       = std::move(DescCopy2);
            Ref2           = Ref;
        }

        DescX.RemoveImmutableSampler("g_Sampler2");
        --Ref.NumImmutableSamplers;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearImmutableSamplers();
        Ref.NumImmutableSamplers = 0;
        Ref.ImmutableSamplers    = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.RemoveVariable("ConstBuff_1");
        --Ref.NumVariables;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearVariables();
        Ref.NumVariables = 0;
        Ref.Variables    = nullptr;
        EXPECT_EQ(DescX, Ref);
    }

    EXPECT_EQ(DescCopy, Ref2);
    EXPECT_EQ(DescMove, Ref2);

    PipelineResourceLayoutDescX DescCopy2{DescCopy};
    PipelineResourceLayoutDescX DescMove2{std::move(DescMove)};
    EXPECT_EQ(DescCopy2, Ref2);
    EXPECT_EQ(DescMove2, Ref2);
    EXPECT_EQ(DescCopy2, DescMove2);

#undef VAR1
#undef VAR2
#undef VAR3

#undef SAM1
#undef SAM2
}

TEST(GraphicsTypesXTest, BottomLevelASDescX)
{
    // clang-format off

#define TRI1(POOL) POOL("Tri1"), 10u, VT_FLOAT32, Uint8{3}, 100u, VT_UINT16
#define TRI2(POOL) POOL("Tri2"), 20u, VT_FLOAT16, Uint8{2}, 200u, VT_UINT32
#define TRI3(POOL) POOL("Tri3"), 30u, VT_INT16,   Uint8{4}, 300u, VT_UINT32

#define BOX1(POOL) POOL("Box1"), 16u
#define BOX2(POOL) POOL("Box2"), 32u

    // clang-format on

    const BLASTriangleDesc Triangles[] = {
        {TRI1(RawStr)},
        {TRI2(RawStr)},
        {TRI3(RawStr)},
    };

    BottomLevelASDesc Ref;
    Ref.Name          = "BLAS test";
    Ref.TriangleCount = _countof(Triangles);
    Ref.pTriangles    = Triangles;
    TestCtorsAndAssignments<BottomLevelASDescX>(Ref);

    const BLASBoundingBoxDesc Boxes[] = {
        {BOX1(RawStr)},
        {BOX2(RawStr)},
    };
    Ref.BoxCount = _countof(Boxes);
    Ref.pBoxes   = Boxes;
    TestCtorsAndAssignments<BottomLevelASDescX>(Ref);

    {
        StringPool         Pool;
        BottomLevelASDescX DescX{
            {
                {TRI1(Pool)},
                {TRI2(Pool)},
                {TRI3(Pool)},
            },
            {
                {BOX1(Pool)},
                {BOX2(Pool)},
            } //
        };
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);
    }

    BottomLevelASDescX DescCopy;
    BottomLevelASDescX DescMove;
    BottomLevelASDesc  Ref2;
    {
        Ref.Flags                = RAYTRACING_BUILD_AS_ALLOW_UPDATE;
        Ref.CompactedSize        = 1024;
        Ref.ImmediateContextMask = 0x0F;

        StringPool         Pool;
        BottomLevelASDescX DescX;
        DescX
            .SetName("BLAS test")
            .SetFlags(RAYTRACING_BUILD_AS_ALLOW_UPDATE)
            .SetCompactedSize(1024)
            .SetImmediateContextMask(0x0F)
            .AddTriangleGeomerty({TRI1(Pool)})
            .AddTriangleGeomerty(TRI2(Pool))
            .AddTriangleGeomerty({TRI3(Pool)})
            .AddBoxGeomerty({BOX1(Pool)})
            .AddBoxGeomerty(BOX2(Pool));
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);

        {
            DescCopy       = DescX;
            auto DescCopy2 = DescCopy;
            DescMove       = std::move(DescCopy2);
            Ref2           = Ref;
        }

        DescX.RemoveTriangleGeomerty("Tri3");
        --Ref.TriangleCount;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearTriangles();
        Ref.TriangleCount = 0;
        Ref.pTriangles    = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.RemoveBoxGeomerty("Box2");
        --Ref.BoxCount;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearBoxes();
        Ref.BoxCount = 0;
        Ref.pBoxes   = nullptr;
        EXPECT_EQ(DescX, Ref);
    }

    EXPECT_EQ(DescCopy, Ref2);
    EXPECT_EQ(DescMove, Ref2);

    BottomLevelASDescX DescCopy2{DescCopy};
    BottomLevelASDescX DescMove2{std::move(DescMove)};
    EXPECT_EQ(DescCopy2, Ref2);
    EXPECT_EQ(DescMove2, Ref2);
    EXPECT_EQ(DescCopy2, DescMove2);

#undef TRI1
#undef TRI2
#undef TRI3

#undef BOX1
#undef BOX2
}

struct DumyShader final : IShader
{
    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final {}

    virtual ReferenceCounterValueType DILIGENT_CALL_TYPE AddRef() override final { return 0; }
    virtual ReferenceCounterValueType DILIGENT_CALL_TYPE Release() override final { return 0; }
    virtual IReferenceCounters* DILIGENT_CALL_TYPE       GetReferenceCounters() const override final { return nullptr; }

    virtual const ShaderDesc& DILIGENT_CALL_TYPE GetDesc() const override final
    {
        static const ShaderDesc DummyDesc;
        return DummyDesc;
    }

    virtual Int32 DILIGENT_CALL_TYPE GetUniqueID() const override final { return 0; }

    virtual void DILIGENT_CALL_TYPE SetUserData(IObject* pUserData) override final {}

    virtual IObject* DILIGENT_CALL_TYPE GetUserData() const override final { return nullptr; }

    virtual Uint32 DILIGENT_CALL_TYPE GetResourceCount() const override final { return 0; }

    virtual void DILIGENT_CALL_TYPE GetResourceDesc(Uint32 Index, ShaderResourceDesc& ResourceDesc) const override final {}

    virtual const ShaderCodeBufferDesc* DILIGENT_CALL_TYPE GetConstantBufferDesc(Uint32 Index) const override final { return nullptr; }

    virtual void DILIGENT_CALL_TYPE GetBytecode(const void** ppBytecode, Uint64& Size) const override final {}

    virtual SHADER_STATUS DILIGENT_CALL_TYPE GetStatus(bool WaitForCompletion) override final { return SHADER_STATUS_UNINITIALIZED; }
};

TEST(GraphicsTypesXTest, RayTracingPipelineStateCreateInfoX)
{
    {
        RayTracingPipelineStateCreateInfoX DescX{"Test Name"};
        EXPECT_STREQ(DescX.PSODesc.Name, "Test Name");
    }
    {
        RayTracingPipelineStateCreateInfoX DescX{std::string{"Test "} + std::string{"Name"}};
        EXPECT_STREQ(DescX.PSODesc.Name, "Test Name");
    }

    // clang-format off

    DumyShader Shaders[17];

#define GENERAL_SHADER_1(POOL) POOL("General Shader 1"), &Shaders[0]
#define GENERAL_SHADER_2(POOL) POOL("General Shader 2"), &Shaders[1]

#define TRI_HIT_SHADER_1(POOL) POOL("Tri Hit Shader 1"), &Shaders[2], &Shaders[3]
#define TRI_HIT_SHADER_2(POOL) POOL("Tri Hit Shader 2"), &Shaders[4], &Shaders[5]
#define TRI_HIT_SHADER_3(POOL) POOL("Tri Hit Shader 3"), &Shaders[6], &Shaders[7]

#define PROC_HIT_SHADER_1(POOL) POOL("Proc Hit Shader 1"), &Shaders[8], &Shaders[9], &Shaders[10]
#define PROC_HIT_SHADER_2(POOL) POOL("Proc Hit Shader 2"), &Shaders[11], &Shaders[12], &Shaders[13]
#define PROC_HIT_SHADER_3(POOL) POOL("Proc Hit Shader 3"), &Shaders[14], &Shaders[15], &Shaders[16]

    // clang-format on

    const RayTracingGeneralShaderGroup GeneralShaders[] = {
        {GENERAL_SHADER_1(RawStr)},
        {GENERAL_SHADER_2(RawStr)},
    };

    const RayTracingTriangleHitShaderGroup TriHitShaders[] = {
        {TRI_HIT_SHADER_1(RawStr)},
        {TRI_HIT_SHADER_2(RawStr)},
        {TRI_HIT_SHADER_3(RawStr)},
    };

    const RayTracingProceduralHitShaderGroup ProcHitShaders[] = {
        {PROC_HIT_SHADER_1(RawStr)},
        {PROC_HIT_SHADER_2(RawStr)},
        {PROC_HIT_SHADER_3(RawStr)},
    };

    RayTracingPipelineStateCreateInfo Ref;
    Ref.PSODesc.Name = "RayTracingPipelineStateCreateInfoX test";

    Ref.GeneralShaderCount = _countof(GeneralShaders);
    Ref.pGeneralShaders    = GeneralShaders;
    TestCtorsAndAssignments<RayTracingPipelineStateCreateInfoX>(Ref);

    Ref.TriangleHitShaderCount = _countof(TriHitShaders);
    Ref.pTriangleHitShaders    = TriHitShaders;
    TestCtorsAndAssignments<RayTracingPipelineStateCreateInfoX>(Ref);

    Ref.ProceduralHitShaderCount = _countof(ProcHitShaders);
    Ref.pProceduralHitShaders    = ProcHitShaders;
    TestCtorsAndAssignments<RayTracingPipelineStateCreateInfoX>(Ref);

    {
        StringPool                         Pool;
        RayTracingPipelineStateCreateInfoX DescX{
            {
                {GENERAL_SHADER_1(Pool)},
                {GENERAL_SHADER_2(Pool)},
            },
            {
                {TRI_HIT_SHADER_1(Pool)},
                {TRI_HIT_SHADER_2(Pool)},
                {TRI_HIT_SHADER_3(Pool)},
            },
            {
                {PROC_HIT_SHADER_1(Pool)},
                {PROC_HIT_SHADER_2(Pool)},
                {PROC_HIT_SHADER_3(Pool)},
            } //
        };
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);
    }

    RayTracingPipelineStateCreateInfoX DescCopy;
    RayTracingPipelineStateCreateInfoX DescMove;
    RayTracingPipelineStateCreateInfo  Ref2;
    {
        StringPool                         Pool;
        RayTracingPipelineStateCreateInfoX DescX;
        DescX
            .SetName(std::string{"RayTracingPipelineState"} + std::string{"CreateInfoX test"})
            .AddGeneralShader({GENERAL_SHADER_1(Pool)})
            .AddGeneralShader(GENERAL_SHADER_2(Pool))
            .AddTriangleHitShader({TRI_HIT_SHADER_1(Pool)})
            .AddTriangleHitShader(TRI_HIT_SHADER_2(Pool))
            .AddTriangleHitShader({TRI_HIT_SHADER_3(Pool)})
            .AddProceduralHitShader({PROC_HIT_SHADER_1(Pool)})
            .AddProceduralHitShader(PROC_HIT_SHADER_2(Pool))
            .AddProceduralHitShader({PROC_HIT_SHADER_3(Pool)});
        Pool.Clear();
        EXPECT_EQ(DescX, Ref);

        DescX.RemoveGeneralShader("General Shader 2");
        --Ref.GeneralShaderCount;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearGeneralShaders();
        Ref.GeneralShaderCount = 0;
        Ref.pGeneralShaders    = nullptr;
        EXPECT_EQ(DescX, Ref);

        {
            DescCopy       = DescX;
            auto DescCopy2 = DescCopy;
            DescMove       = std::move(DescCopy2);
            Ref2           = Ref;
        }

        DescX.RemoveTriangleHitShader("Tri Hit Shader 3");
        --Ref.TriangleHitShaderCount;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearTriangleHitShaders();
        Ref.TriangleHitShaderCount = 0;
        Ref.pTriangleHitShaders    = nullptr;
        EXPECT_EQ(DescX, Ref);

        DescX.RemoveProceduralHitShader("Proc Hit Shader 3");
        --Ref.ProceduralHitShaderCount;
        EXPECT_EQ(DescX, Ref);

        DescX.ClearProceduralHitShaders();
        Ref.ProceduralHitShaderCount = 0;
        Ref.pProceduralHitShaders    = nullptr;
        EXPECT_EQ(DescX, Ref);
    }

    EXPECT_EQ(DescCopy, Ref2);
    EXPECT_EQ(DescMove, Ref2);

    RayTracingPipelineStateCreateInfoX DescCopy2{DescCopy};
    RayTracingPipelineStateCreateInfoX DescMove2{std::move(DescMove)};
    EXPECT_EQ(DescCopy2, Ref2);
    EXPECT_EQ(DescMove2, Ref2);
    EXPECT_EQ(DescCopy2, DescMove2);

#undef GENERAL_SHADER_1
#undef GENERAL_SHADER_2

#undef TRI_HIT_SHADER_1
#undef TRI_HIT_SHADER_2
#undef TRI_HIT_SHADER_3

#undef PROC_HIT_SHADER_1
#undef PROC_HIT_SHADER_2
#undef PROC_HIT_SHADER_3
}

TEST(GraphicsTypesXTest, GraphicsPipelineStateCreateInfoX)
{
    {
        GraphicsPipelineStateCreateInfoX DescX{"Test Name"};
        EXPECT_STREQ(DescX.PSODesc.Name, "Test Name");
    }
    {
        GraphicsPipelineStateCreateInfoX DescX{std::string{"Test "} + std::string{"Name"}};
        EXPECT_STREQ(DescX.PSODesc.Name, "Test Name");

        DescX.SetResourceLayout(SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_TYPE_MESH);
        DescX.SetBlendDesc(true, true);
        DescX.SetRasterizerDesc(FILL_MODE_WIREFRAME, CULL_MODE_NONE);
        DescX.SetDepthStencilDesc(true, false, COMPARISON_FUNC_GREATER_EQUAL);

        GraphicsPipelineStateCreateInfo Ref;
        Ref.PSODesc.ResourceLayout            = {SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_TYPE_MESH};
        Ref.GraphicsPipeline.BlendDesc        = {true, true};
        Ref.GraphicsPipeline.RasterizerDesc   = {FILL_MODE_WIREFRAME, CULL_MODE_NONE};
        Ref.GraphicsPipeline.DepthStencilDesc = {true, false, COMPARISON_FUNC_GREATER_EQUAL};
        EXPECT_EQ(DescX, Ref);
    }

    GraphicsPipelineStateCreateInfoX DescX{std::string{"Test "} + std::string{"Name"}};
    GraphicsPipelineStateCreateInfo  Ref;
    Ref.PSODesc.Name = "Test Name";
    EXPECT_EQ(DescX, Ref);

    PipelineResourceLayoutDescX ResLayoutDescX;
    ResLayoutDescX.AddVariable(SHADER_TYPE_VERTEX, "Test Var", SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    ResLayoutDescX.AddImmutableSampler(SHADER_TYPE_VERTEX, "Test Sampler", SamplerDesc{});

    InputLayoutDescX InputLayoutX;
    InputLayoutX
        .Add(0u, 0u, 3u, VT_FLOAT32)
        .Add(1u, 1u, 4u, VT_FLOAT32);

    constexpr Uint64 ImmediateCtxMask         = 0x1234;
    constexpr Uint32 SRBAllocationGranularity = 100;

    DescX
        .SetName(std::string{"Test "} + std::string{"Name2"})
        .SetFlags(PSO_CREATE_FLAG_DONT_REMAP_SHADER_RESOURCES)
        .SetResourceLayout(ResLayoutDescX)
        .SetImmediateContextMask(ImmediateCtxMask)
        .SetSRBAllocationGranularity(SRBAllocationGranularity)
        .SetBlendDesc(BS_AlphaBlend)
        .SetSampleMask(0x12345678)
        .SetRasterizerDesc(RS_WireFillNoCull)
        .SetDepthStencilDesc(DSS_DisableDepth)
        .SetInputLayout(InputLayoutX)
        .SetNumViewports(7)
        .SetSubpassIndex(6)
        .SetShadingRateFlags(PIPELINE_SHADING_RATE_FLAG_PER_PRIMITIVE)
        .AddRenderTarget(TEX_FORMAT_RGBA8_UNORM_SRGB)
        .AddRenderTarget(TEX_FORMAT_RGBA32_FLOAT)
        .SetDepthFormat(TEX_FORMAT_D32_FLOAT)
        .SetSampleDesc({1, 5})
        .SetNodeMask(0x7531);


    if (Ref.Flags != PSO_CREATE_FLAG_NONE)
    {
        UNEXPECTED("This code should never run - it is there to check that it compiles properly");
        DescX.AddSignature(nullptr);
        DescX.RemoveSignature(nullptr);
        DescX.ClearSignatures();
        DescX.SetPipelineStateCache(nullptr);
        DescX.AddShader(nullptr);
        DescX.RemoveShader(nullptr);
        DescX.SetRenderPass(nullptr);
    }

    Ref.PSODesc.Name                      = "Test Name2";
    Ref.Flags                             = PSO_CREATE_FLAG_DONT_REMAP_SHADER_RESOURCES;
    Ref.PSODesc.ResourceLayout            = ResLayoutDescX;
    Ref.PSODesc.ImmediateContextMask      = ImmediateCtxMask;
    Ref.PSODesc.SRBAllocationGranularity  = SRBAllocationGranularity;
    Ref.GraphicsPipeline.BlendDesc        = BS_AlphaBlend;
    Ref.GraphicsPipeline.SampleMask       = 0x12345678;
    Ref.GraphicsPipeline.RasterizerDesc   = RS_WireFillNoCull;
    Ref.GraphicsPipeline.DepthStencilDesc = DSS_DisableDepth;
    Ref.GraphicsPipeline.InputLayout      = InputLayoutX;
    Ref.GraphicsPipeline.NumViewports     = 7;
    Ref.GraphicsPipeline.NumRenderTargets = 2;
    Ref.GraphicsPipeline.SubpassIndex     = 6;
    Ref.GraphicsPipeline.ShadingRateFlags = PIPELINE_SHADING_RATE_FLAG_PER_PRIMITIVE;
    Ref.GraphicsPipeline.RTVFormats[0]    = TEX_FORMAT_RGBA8_UNORM_SRGB;
    Ref.GraphicsPipeline.RTVFormats[1]    = TEX_FORMAT_RGBA32_FLOAT;
    Ref.GraphicsPipeline.DSVFormat        = TEX_FORMAT_D32_FLOAT;
    Ref.GraphicsPipeline.SmplDesc         = {1, 5};
    Ref.GraphicsPipeline.NodeMask         = 0x7531;
    EXPECT_STREQ(DescX.PSODesc.Name, Ref.PSODesc.Name);
    EXPECT_EQ(DescX, Ref);

    {
        GraphicsPipelineStateCreateInfoX DescX2{DescX};
        EXPECT_EQ(DescX2, Ref);
        EXPECT_EQ(DescX2, DescX);
    }

    {
        GraphicsPipelineStateCreateInfoX DescX2;
        DescX2 = DescX;
        EXPECT_EQ(DescX2, Ref);
        EXPECT_EQ(DescX2, DescX);
    }

    {
        GraphicsPipelineStateCreateInfoX DescX2{std::move(DescX)};
        EXPECT_EQ(DescX2, Ref);

        GraphicsPipelineStateCreateInfoX DescX3;
        DescX3 = std::move(DescX2);
    }
}

TEST(GraphicsTypesXTest, ComputePipelineStateCreateInfoX)
{
    {
        ComputePipelineStateCreateInfoX DescX{"Test Name"};
        EXPECT_STREQ(DescX.PSODesc.Name, "Test Name");
    }
    {
        ComputePipelineStateCreateInfoX DescX{std::string{"Test "} + std::string{"Name"}};
        EXPECT_STREQ(DescX.PSODesc.Name, "Test Name");
    }
}


TEST(GraphicsTypesXTest, TilePipelineStateCreateInfoX)
{
    {
        TilePipelineStateCreateInfoX DescX{"Test Name"};
        EXPECT_STREQ(DescX.PSODesc.Name, "Test Name");
    }
    {
        TilePipelineStateCreateInfoX DescX{std::string{"Test "} + std::string{"Name"}};
        EXPECT_STREQ(DescX.PSODesc.Name, "Test Name");
    }

    TilePipelineStateCreateInfoX DescX;
    DescX
        .AddRenderTarget(TEX_FORMAT_RGBA8_UNORM_SRGB)
        .AddRenderTarget(TEX_FORMAT_RGBA32_FLOAT)
        .SetSampleCount(5);

    TilePipelineStateCreateInfo Ref;
    Ref.TilePipeline.NumRenderTargets = 2;
    Ref.TilePipeline.RTVFormats[0]    = TEX_FORMAT_RGBA8_UNORM_SRGB;
    Ref.TilePipeline.RTVFormats[1]    = TEX_FORMAT_RGBA32_FLOAT;
    Ref.TilePipeline.SampleCount      = 5;
    EXPECT_EQ(DescX, Ref);
}

TEST(GraphicsTypesXTest, RenderDeviceX)
{
    constexpr bool Execute = false;
    if (Execute)
    {
        RenderDeviceX<> Device{nullptr};
        {
            auto pBuffer = Device.CreateBuffer(BufferDesc{});
            pBuffer.Release();
        }
        {
            auto pBuffer = Device.CreateBuffer("Name", 1024);
            pBuffer.Release();
        }
        {
            auto pTex = Device.CreateTexture(TextureDesc{});
            pTex.Release();
        }
        {
            auto pShader = Device.CreateShader(ShaderCreateInfo{});
            pShader.Release();
            pShader = Device.CreateShader("FilePath", nullptr, SHADER_SOURCE_LANGUAGE_HLSL, ShaderDesc{"Shader Name", SHADER_TYPE_VERTEX});
            pShader.Release();
        }
        {
            auto pSampler = Device.CreateSampler(SamplerDesc{});
            pSampler.Release();
        }
        {
            auto pResMapping = Device.CreateResourceMapping(ResourceMappingCreateInfo{});
            pResMapping.Release();
        }
        {
            auto pPSO = Device.CreateGraphicsPipelineState(GraphicsPipelineStateCreateInfo{});
            pPSO.Release();
            pPSO = Device.CreatePipelineState(GraphicsPipelineStateCreateInfo{});
            pPSO.Release();
        }
        {
            auto pPSO = Device.CreateComputePipelineState(ComputePipelineStateCreateInfo{});
            pPSO.Release();
            pPSO = Device.CreatePipelineState(ComputePipelineStateCreateInfo{});
            pPSO.Release();
        }
        {
            auto pPSO = Device.CreateRayTracingPipelineState(RayTracingPipelineStateCreateInfo{});
            pPSO.Release();
            pPSO = Device.CreatePipelineState(RayTracingPipelineStateCreateInfo{});
            pPSO.Release();
        }
        {
            auto pPSO = Device.CreateTilePipelineState(TilePipelineStateCreateInfo{});
            pPSO.Release();
            pPSO = Device.CreatePipelineState(TilePipelineStateCreateInfo{});
            pPSO.Release();
        }
        {
            auto pFence = Device.CreateFence(FenceDesc{});
            pFence.Release();
        }
        {
            auto pQuery = Device.CreateQuery(QueryDesc{});
            pQuery.Release();
        }
        {
            auto pRenderPass = Device.CreateRenderPass(RenderPassDesc{});
            pRenderPass.Release();
        }
        {
            auto pFramebuffer = Device.CreateFramebuffer(FramebufferDesc{});
            pFramebuffer.Release();
        }
        {
            auto pBLAS = Device.CreateBLAS(BottomLevelASDesc{});
            pBLAS.Release();
        }
        {
            auto pTLAS = Device.CreateTLAS(TopLevelASDesc{});
            pTLAS.Release();
        }
        {
            auto pSBT = Device.CreateSBT(ShaderBindingTableDesc{});
            pSBT.Release();
        }
        {
            auto pPRS = Device.CreatePipelineResourceSignature(PipelineResourceSignatureDesc{});
            pPRS.Release();
        }
        {
            auto pMem = Device.CreateDeviceMemory(DeviceMemoryCreateInfo{});
            pMem.Release();
        }
        {
            auto pPsoCache = Device.CreatePipelineStateCache(PipelineStateCacheCreateInfo{});
            pPsoCache.Release();
        }
        {
            const auto& DeviceInfo = Device.GetDeviceInfo();
            (void)DeviceInfo;
        }
        {
            const auto& AdapterInfo = Device.GetAdapterInfo();
            (void)AdapterInfo;
        }
        {
            const auto& TexFmtInfo = Device.GetTextureFormatInfo(TEX_FORMAT_UNKNOWN);
            (void)TexFmtInfo;
        }
        {
            const auto& TexFmtInfoEx = Device.GetTextureFormatInfoExt(TEX_FORMAT_UNKNOWN);
            (void)TexFmtInfoEx;
        }
        {
            auto SparseInfo = Device.GetSparseTextureFormatInfo(TEX_FORMAT_UNKNOWN, RESOURCE_DIM_BUFFER, 0);
            (void)SparseInfo;
        }
        Device.ReleaseStaleResources();
        Device.IdleGPU();
        {
            auto* pFactory = Device.GetEngineFactory();
            (void)pFactory;
        }
    }
}

} // namespace

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

#include <limits>
#include "VulkanErrors.hpp"
#include "VulkanUtilities/LogicalDevice.hpp"
#include "VulkanUtilities/Debug.hpp"
#include "VulkanUtilities/ObjectWrappers.hpp"

namespace VulkanUtilities
{

std::shared_ptr<LogicalDevice> LogicalDevice::Create(const CreateInfo& CI)
{
    return std::shared_ptr<LogicalDevice>{new LogicalDevice{CI}};
}

LogicalDevice::~LogicalDevice()
{
    vkDestroyDevice(m_VkDevice, m_VkAllocator);
}

LogicalDevice::LogicalDevice(const CreateInfo& CI) :
    m_VkDevice{CI.vkDevice},
    m_VkAllocator{CI.vkAllocator},
    m_EnabledFeatures{CI.EnabledFeatures},
    m_EnabledExtFeatures{CI.EnabledExtFeatures}
{
#if DILIGENT_USE_VOLK
    // Since we only use one device at this time, load device function entries
    // https://github.com/zeux/volk#optimizing-device-calls
    volkLoadDevice(m_VkDevice);
#endif

    VkPipelineStageFlags GraphicsStages =
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    VkPipelineStageFlags ComputeStages =
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    VkAccessFlags GraphicsAccessMask =
        VK_ACCESS_INDEX_READ_BIT |
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    VkAccessFlags ComputeAccessMask =
        VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
        VK_ACCESS_UNIFORM_READ_BIT |
        VK_ACCESS_SHADER_READ_BIT |
        VK_ACCESS_SHADER_WRITE_BIT;
    VkAccessFlags TransferAccessMask =
        VK_ACCESS_TRANSFER_READ_BIT |
        VK_ACCESS_TRANSFER_WRITE_BIT |
        VK_ACCESS_HOST_READ_BIT |
        VK_ACCESS_HOST_WRITE_BIT;

    if (m_EnabledFeatures.geometryShader)
        GraphicsStages |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    if (m_EnabledFeatures.tessellationShader)
        GraphicsStages |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    if (m_EnabledExtFeatures.MeshShader.meshShader != VK_FALSE && m_EnabledExtFeatures.MeshShader.taskShader != VK_FALSE)
        GraphicsStages |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
    if (m_EnabledExtFeatures.RayTracingPipeline.rayTracingPipeline != VK_FALSE)
    {
        ComputeStages |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        ComputeAccessMask |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    }
    if (m_EnabledExtFeatures.ShadingRate.attachmentFragmentShadingRate != VK_FALSE)
    {
        GraphicsStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
        GraphicsAccessMask |= VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
    }
    if (m_EnabledExtFeatures.FragmentDensityMap.fragmentDensityMap != VK_FALSE)
    {
        GraphicsStages |= VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
        GraphicsAccessMask |= VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
    }

    const size_t QueueCount = CI.PhysDevice.GetQueueProperties().size();
    m_SupportedStagesMask.resize(QueueCount, 0);
    m_SupportedAccessMask.resize(QueueCount, 0);
    for (size_t q = 0; q < QueueCount; ++q)
    {
        const VkQueueFamilyProperties& Queue      = CI.PhysDevice.GetQueueProperties()[q];
        VkPipelineStageFlags&          StageMask  = m_SupportedStagesMask[q];
        VkAccessFlags&                 AccessMask = m_SupportedAccessMask[q];

        if (Queue.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            StageMask |= GraphicsStages | ComputeStages | VK_PIPELINE_STAGE_ALL_TRANSFER;
            AccessMask |= GraphicsAccessMask | ComputeAccessMask | TransferAccessMask;
        }
        else if (Queue.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            StageMask |= ComputeStages | VK_PIPELINE_STAGE_ALL_TRANSFER;
            AccessMask |= ComputeAccessMask | TransferAccessMask;
        }
        else if (Queue.queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            StageMask |= VK_PIPELINE_STAGE_ALL_TRANSFER;
            AccessMask |= TransferAccessMask;
        }
    }
}

VkQueue LogicalDevice::GetQueue(HardwareQueueIndex queueFamilyIndex, uint32_t queueIndex)
{
    VkQueue vkQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(m_VkDevice,
                     queueFamilyIndex, // Index of the queue family to which the queue belongs
                     0,                // Index within this queue family of the queue to retrieve
                     &vkQueue);
    VERIFY_EXPR(vkQueue != VK_NULL_HANDLE);
    return vkQueue;
}

void LogicalDevice::WaitIdle() const
{
    VkResult err = vkDeviceWaitIdle(m_VkDevice);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to idle device");
    (void)err;
}

template <typename VkObjectType,
          VulkanHandleTypeId VkTypeId,
          typename VkCreateObjectFuncType,
          typename VkObjectCreateInfoType>
ObjectWrapper<VkObjectType, VkTypeId> LogicalDevice::CreateVulkanObject(VkCreateObjectFuncType        VkCreateObject,
                                                                        const VkObjectCreateInfoType& CreateInfo,
                                                                        const char*                   DebugName,
                                                                        const char*                   ObjectType) const
{
    if (DebugName == nullptr)
        DebugName = "";

    VkObjectType VkObject = VK_NULL_HANDLE;

    VkResult err = VkCreateObject(m_VkDevice, &CreateInfo, m_VkAllocator, &VkObject);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to create Vulkan ", ObjectType, " '", DebugName, '\'');

    if (*DebugName != 0)
        SetVulkanObjectName<VkObjectType, VkTypeId>(m_VkDevice, VkObject, DebugName);

    return ObjectWrapper<VkObjectType, VkTypeId>{GetSharedPtr(), std::move(VkObject)};
}

CommandPoolWrapper LogicalDevice::CreateCommandPool(const VkCommandPoolCreateInfo& CmdPoolCI,
                                                    const char*                    DebugName) const
{
    VERIFY_EXPR(CmdPoolCI.sType == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    return CreateVulkanObject<VkCommandPool, VulkanHandleTypeId::CommandPool>(vkCreateCommandPool, CmdPoolCI, DebugName, "command pool");
}

BufferWrapper LogicalDevice::CreateBuffer(const VkBufferCreateInfo& BufferCI,
                                          const char*               DebugName) const
{
    VERIFY_EXPR(BufferCI.sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    return CreateVulkanObject<VkBuffer, VulkanHandleTypeId::Buffer>(vkCreateBuffer, BufferCI, DebugName, "buffer");
}

BufferViewWrapper LogicalDevice::CreateBufferView(const VkBufferViewCreateInfo& BuffViewCI,
                                                  const char*                   DebugName) const
{
    VERIFY_EXPR(BuffViewCI.sType == VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
    return CreateVulkanObject<VkBufferView, VulkanHandleTypeId::BufferView>(vkCreateBufferView, BuffViewCI, DebugName, "buffer view");
}

ImageWrapper LogicalDevice::CreateImage(const VkImageCreateInfo& ImageCI,
                                        const char*              DebugName) const
{
    VERIFY_EXPR(ImageCI.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    return CreateVulkanObject<VkImage, VulkanHandleTypeId::Image>(vkCreateImage, ImageCI, DebugName, "image");
}

ImageViewWrapper LogicalDevice::CreateImageView(const VkImageViewCreateInfo& ImageViewCI,
                                                const char*                  DebugName) const
{
    VERIFY_EXPR(ImageViewCI.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    return CreateVulkanObject<VkImageView, VulkanHandleTypeId::ImageView>(vkCreateImageView, ImageViewCI, DebugName, "image view");
}

SamplerWrapper LogicalDevice::CreateSampler(const VkSamplerCreateInfo& SamplerCI, const char* DebugName) const
{
    VERIFY_EXPR(SamplerCI.sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
    return CreateVulkanObject<VkSampler, VulkanHandleTypeId::Sampler>(vkCreateSampler, SamplerCI, DebugName, "sampler");
}

FenceWrapper LogicalDevice::CreateFence(const VkFenceCreateInfo& FenceCI, const char* DebugName) const
{
    VERIFY_EXPR(FenceCI.sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    return CreateVulkanObject<VkFence, VulkanHandleTypeId::Fence>(vkCreateFence, FenceCI, DebugName, "fence");
}

RenderPassWrapper LogicalDevice::CreateRenderPass(const VkRenderPassCreateInfo& RenderPassCI, const char* DebugName) const
{
    VERIFY_EXPR(RenderPassCI.sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    return CreateVulkanObject<VkRenderPass, VulkanHandleTypeId::RenderPass>(vkCreateRenderPass, RenderPassCI, DebugName, "render pass");
}

RenderPassWrapper LogicalDevice::CreateRenderPass(const VkRenderPassCreateInfo2& RenderPassCI, const char* DebugName) const
{
#if DILIGENT_USE_VOLK
    VERIFY_EXPR(RenderPassCI.sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2);
    VERIFY_EXPR(GetEnabledExtFeatures().RenderPass2 != VK_FALSE);
    return CreateVulkanObject<VkRenderPass, VulkanHandleTypeId::RenderPass>(vkCreateRenderPass2KHR, RenderPassCI, DebugName, "render pass 2");
#else
    UNSUPPORTED("vkCreateRenderPass2KHR is only available through Volk");
    return RenderPassWrapper{};
#endif
}

DeviceMemoryWrapper LogicalDevice::AllocateDeviceMemory(const VkMemoryAllocateInfo& AllocInfo,
                                                        const char*                 DebugName) const
{
    VERIFY_EXPR(AllocInfo.sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);

    if (DebugName == nullptr)
        DebugName = "";

    VkDeviceMemory vkDeviceMem = VK_NULL_HANDLE;

    VkResult err = vkAllocateMemory(m_VkDevice, &AllocInfo, m_VkAllocator, &vkDeviceMem);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to allocate device memory '", DebugName, '\'');

    if (*DebugName != 0)
        SetDeviceMemoryName(m_VkDevice, vkDeviceMem, DebugName);

    return DeviceMemoryWrapper{GetSharedPtr(), std::move(vkDeviceMem)};
}

PipelineWrapper LogicalDevice::CreateComputePipeline(const VkComputePipelineCreateInfo& PipelineCI,
                                                     VkPipelineCache                    cache,
                                                     const char*                        DebugName) const
{
    VERIFY_EXPR(PipelineCI.sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);

    if (DebugName == nullptr)
        DebugName = "";

    VkPipeline vkPipeline = VK_NULL_HANDLE;

    VkResult err = vkCreateComputePipelines(m_VkDevice, cache, 1, &PipelineCI, m_VkAllocator, &vkPipeline);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to create compute pipeline '", DebugName, '\'');

    if (*DebugName != 0)
        SetPipelineName(m_VkDevice, vkPipeline, DebugName);

    return PipelineWrapper{GetSharedPtr(), std::move(vkPipeline)};
}

PipelineWrapper LogicalDevice::CreateGraphicsPipeline(const VkGraphicsPipelineCreateInfo& PipelineCI,
                                                      VkPipelineCache                     cache,
                                                      const char*                         DebugName) const
{
    VERIFY_EXPR(PipelineCI.sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);

    if (DebugName == nullptr)
        DebugName = "";

    VkPipeline vkPipeline = VK_NULL_HANDLE;

    VkResult err = vkCreateGraphicsPipelines(m_VkDevice, cache, 1, &PipelineCI, m_VkAllocator, &vkPipeline);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to create graphics pipeline '", DebugName, '\'');

    if (*DebugName != 0)
        SetPipelineName(m_VkDevice, vkPipeline, DebugName);

    return PipelineWrapper{GetSharedPtr(), std::move(vkPipeline)};
}

PipelineWrapper LogicalDevice::CreateRayTracingPipeline(const VkRayTracingPipelineCreateInfoKHR& PipelineCI, VkPipelineCache cache, const char* DebugName) const
{
#if DILIGENT_USE_VOLK
    VERIFY_EXPR(PipelineCI.sType == VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR);

    if (DebugName == nullptr)
        DebugName = "";

    VkPipeline vkPipeline = VK_NULL_HANDLE;

    VkResult err = vkCreateRayTracingPipelinesKHR(m_VkDevice, VK_NULL_HANDLE, cache, 1, &PipelineCI, m_VkAllocator, &vkPipeline);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to create ray tracing pipeline '", DebugName, '\'');

    if (*DebugName != 0)
        SetPipelineName(m_VkDevice, vkPipeline, DebugName);

    return PipelineWrapper{GetSharedPtr(), std::move(vkPipeline)};
#else
    UNSUPPORTED("vkCreateRayTracingPipelinesKHR is only available through Volk");
    return PipelineWrapper{};
#endif
}

ShaderModuleWrapper LogicalDevice::CreateShaderModule(const VkShaderModuleCreateInfo& ShaderModuleCI, const char* DebugName) const
{
    VERIFY_EXPR(ShaderModuleCI.sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    return CreateVulkanObject<VkShaderModule, VulkanHandleTypeId::ShaderModule>(vkCreateShaderModule, ShaderModuleCI, DebugName, "shader module");
}

PipelineLayoutWrapper LogicalDevice::CreatePipelineLayout(const VkPipelineLayoutCreateInfo& PipelineLayoutCI, const char* DebugName) const
{
    VERIFY_EXPR(PipelineLayoutCI.sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    return CreateVulkanObject<VkPipelineLayout, VulkanHandleTypeId::PipelineLayout>(vkCreatePipelineLayout, PipelineLayoutCI, DebugName, "pipeline layout");
}

FramebufferWrapper LogicalDevice::CreateFramebuffer(const VkFramebufferCreateInfo& FramebufferCI, const char* DebugName) const
{
    VERIFY_EXPR(FramebufferCI.sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    return CreateVulkanObject<VkFramebuffer, VulkanHandleTypeId::Framebuffer>(vkCreateFramebuffer, FramebufferCI, DebugName, "framebuffer");
}

DescriptorPoolWrapper LogicalDevice::CreateDescriptorPool(const VkDescriptorPoolCreateInfo& DescrPoolCI, const char* DebugName) const
{
    VERIFY_EXPR(DescrPoolCI.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
    return CreateVulkanObject<VkDescriptorPool, VulkanHandleTypeId::DescriptorPool>(vkCreateDescriptorPool, DescrPoolCI, DebugName, "descriptor pool");
}

DescriptorSetLayoutWrapper LogicalDevice::CreateDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo& LayoutCI, const char* DebugName) const
{
    VERIFY_EXPR(LayoutCI.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
    return CreateVulkanObject<VkDescriptorSetLayout, VulkanHandleTypeId::DescriptorSetLayout>(vkCreateDescriptorSetLayout, LayoutCI, DebugName, "descriptor set layout");
}

SemaphoreWrapper LogicalDevice::CreateSemaphore(const VkSemaphoreCreateInfo& SemaphoreCI, const char* DebugName) const
{
    VERIFY_EXPR(SemaphoreCI.sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    return CreateVulkanObject<VkSemaphore, VulkanHandleTypeId::Semaphore>(vkCreateSemaphore, SemaphoreCI, DebugName, "semaphore");
}

SemaphoreWrapper LogicalDevice::CreateTimelineSemaphore(uint64_t InitialValue, const char* DebugName) const
{
    VERIFY_EXPR(m_EnabledExtFeatures.TimelineSemaphore.timelineSemaphore == VK_TRUE);

    VkSemaphoreTypeCreateInfo TimelineCI{};
    TimelineCI.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    TimelineCI.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    TimelineCI.initialValue  = InitialValue;

    VkSemaphoreCreateInfo SemaphoreCI{};
    SemaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    SemaphoreCI.pNext = &TimelineCI;

    return CreateVulkanObject<VkSemaphore, VulkanHandleTypeId::Semaphore>(vkCreateSemaphore, SemaphoreCI, DebugName, "timeline semaphore");
}

QueryPoolWrapper LogicalDevice::CreateQueryPool(const VkQueryPoolCreateInfo& QueryPoolCI, const char* DebugName) const
{
    VERIFY_EXPR(QueryPoolCI.sType == VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
    return CreateVulkanObject<VkQueryPool, VulkanHandleTypeId::QueryPool>(vkCreateQueryPool, QueryPoolCI, DebugName, "query pool");
}

AccelStructWrapper LogicalDevice::CreateAccelStruct(const VkAccelerationStructureCreateInfoKHR& CI, const char* DebugName) const
{
#if DILIGENT_USE_VOLK
    VERIFY_EXPR(CI.sType == VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR);
    return CreateVulkanObject<VkAccelerationStructureKHR, VulkanHandleTypeId::AccelerationStructureKHR>(vkCreateAccelerationStructureKHR, CI, DebugName, "acceleration structure");
#else
    UNSUPPORTED("vkCreateAccelerationStructureKHR is only available through Volk");
    return AccelStructWrapper{};
#endif
}

VkCommandBuffer LogicalDevice::AllocateVkCommandBuffer(const VkCommandBufferAllocateInfo& AllocInfo, const char* DebugName) const
{
    VERIFY_EXPR(AllocInfo.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);

    if (DebugName == nullptr)
        DebugName = "";

    VkCommandBuffer CmdBuff = VK_NULL_HANDLE;

    VkResult err = vkAllocateCommandBuffers(m_VkDevice, &AllocInfo, &CmdBuff);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to allocate command buffer '", DebugName, '\'');
    (void)err;

    if (*DebugName != 0)
        SetCommandBufferName(m_VkDevice, CmdBuff, DebugName);

    return CmdBuff;
}

VkDescriptorSet LogicalDevice::AllocateVkDescriptorSet(const VkDescriptorSetAllocateInfo& AllocInfo, const char* DebugName) const
{
    VERIFY_EXPR(AllocInfo.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    VERIFY_EXPR(AllocInfo.descriptorSetCount == 1);

    if (DebugName == nullptr)
        DebugName = "";

    VkDescriptorSet DescrSet = VK_NULL_HANDLE;

    VkResult err = vkAllocateDescriptorSets(m_VkDevice, &AllocInfo, &DescrSet);
    if (err != VK_SUCCESS)
        return VK_NULL_HANDLE;

    if (*DebugName != 0)
        SetDescriptorSetName(m_VkDevice, DescrSet, DebugName);

    return DescrSet;
}

PipelineCacheWrapper LogicalDevice::CreatePipelineCache(const VkPipelineCacheCreateInfo& CI, const char* DebugName) const
{
    VERIFY_EXPR(CI.sType == VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
    return CreateVulkanObject<VkPipelineCache, VulkanHandleTypeId::PipelineCache>(vkCreatePipelineCache, CI, DebugName, "pipeline cache");
}

void LogicalDevice::ReleaseVulkanObject(CommandPoolWrapper&& CmdPool) const
{
    vkDestroyCommandPool(m_VkDevice, CmdPool.m_VkObject, m_VkAllocator);
    CmdPool.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(BufferWrapper&& Buffer) const
{
    vkDestroyBuffer(m_VkDevice, Buffer.m_VkObject, m_VkAllocator);
    Buffer.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(BufferViewWrapper&& BufferView) const
{
    vkDestroyBufferView(m_VkDevice, BufferView.m_VkObject, m_VkAllocator);
    BufferView.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(ImageWrapper&& Image) const
{
    vkDestroyImage(m_VkDevice, Image.m_VkObject, m_VkAllocator);
    Image.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(ImageViewWrapper&& ImageView) const
{
    vkDestroyImageView(m_VkDevice, ImageView.m_VkObject, m_VkAllocator);
    ImageView.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(SamplerWrapper&& Sampler) const
{
    vkDestroySampler(m_VkDevice, Sampler.m_VkObject, m_VkAllocator);
    Sampler.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(FenceWrapper&& Fence) const
{
    vkDestroyFence(m_VkDevice, Fence.m_VkObject, m_VkAllocator);
    Fence.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(RenderPassWrapper&& RenderPass) const
{
    vkDestroyRenderPass(m_VkDevice, RenderPass.m_VkObject, m_VkAllocator);
    RenderPass.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(DeviceMemoryWrapper&& Memory) const
{
    vkFreeMemory(m_VkDevice, Memory.m_VkObject, m_VkAllocator);
    Memory.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(PipelineWrapper&& Pipeline) const
{
    vkDestroyPipeline(m_VkDevice, Pipeline.m_VkObject, m_VkAllocator);
    Pipeline.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(ShaderModuleWrapper&& ShaderModule) const
{
    vkDestroyShaderModule(m_VkDevice, ShaderModule.m_VkObject, m_VkAllocator);
    ShaderModule.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(PipelineLayoutWrapper&& PipelineLayout) const
{
    vkDestroyPipelineLayout(m_VkDevice, PipelineLayout.m_VkObject, m_VkAllocator);
    PipelineLayout.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(FramebufferWrapper&& Framebuffer) const
{
    vkDestroyFramebuffer(m_VkDevice, Framebuffer.m_VkObject, m_VkAllocator);
    Framebuffer.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(DescriptorPoolWrapper&& DescriptorPool) const
{
    vkDestroyDescriptorPool(m_VkDevice, DescriptorPool.m_VkObject, m_VkAllocator);
    DescriptorPool.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(DescriptorSetLayoutWrapper&& DescriptorSetLayout) const
{
    vkDestroyDescriptorSetLayout(m_VkDevice, DescriptorSetLayout.m_VkObject, m_VkAllocator);
    DescriptorSetLayout.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(SemaphoreWrapper&& Semaphore) const
{
    vkDestroySemaphore(m_VkDevice, Semaphore.m_VkObject, m_VkAllocator);
    Semaphore.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(QueryPoolWrapper&& QueryPool) const
{
    vkDestroyQueryPool(m_VkDevice, QueryPool.m_VkObject, m_VkAllocator);
    QueryPool.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::ReleaseVulkanObject(AccelStructWrapper&& AccelStruct) const
{
#if DILIGENT_USE_VOLK
    vkDestroyAccelerationStructureKHR(m_VkDevice, AccelStruct.m_VkObject, m_VkAllocator);
    AccelStruct.m_VkObject = VK_NULL_HANDLE;
#else
    UNSUPPORTED("vkDestroyAccelerationStructureKHR is only available through Volk");
#endif
}

void LogicalDevice::ReleaseVulkanObject(PipelineCacheWrapper&& PipeCache) const
{
    vkDestroyPipelineCache(m_VkDevice, PipeCache.m_VkObject, m_VkAllocator);
    PipeCache.m_VkObject = VK_NULL_HANDLE;
}

void LogicalDevice::FreeDescriptorSet(VkDescriptorPool Pool, VkDescriptorSet Set) const
{
    VERIFY_EXPR(Pool != VK_NULL_HANDLE && Set != VK_NULL_HANDLE);
    vkFreeDescriptorSets(m_VkDevice, Pool, 1, &Set);
}


void LogicalDevice::FreeCommandBuffer(VkCommandPool Pool, VkCommandBuffer CmdBuffer) const
{
    VERIFY_EXPR(Pool != VK_NULL_HANDLE && CmdBuffer != VK_NULL_HANDLE);
    vkFreeCommandBuffers(m_VkDevice, Pool, 1, &CmdBuffer);
}


VkMemoryRequirements LogicalDevice::GetBufferMemoryRequirements(VkBuffer vkBuffer) const
{
    VkMemoryRequirements MemReqs = {};
    vkGetBufferMemoryRequirements(m_VkDevice, vkBuffer, &MemReqs);
    return MemReqs;
}

VkMemoryRequirements LogicalDevice::GetImageMemoryRequirements(VkImage vkImage) const
{
    VkMemoryRequirements MemReqs = {};
    vkGetImageMemoryRequirements(m_VkDevice, vkImage, &MemReqs);
    return MemReqs;
}

VkResult LogicalDevice::BindBufferMemory(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) const
{
    return vkBindBufferMemory(m_VkDevice, buffer, memory, memoryOffset);
}

VkResult LogicalDevice::BindImageMemory(VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) const
{
    return vkBindImageMemory(m_VkDevice, image, memory, memoryOffset);
}

VkDeviceAddress LogicalDevice::GetAccelerationStructureDeviceAddress(VkAccelerationStructureKHR AS) const
{
#if DILIGENT_USE_VOLK
    VkAccelerationStructureDeviceAddressInfoKHR Info = {};

    Info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    Info.accelerationStructure = AS;

    return vkGetAccelerationStructureDeviceAddressKHR(m_VkDevice, &Info);
#else
    UNSUPPORTED("vkGetAccelerationStructureDeviceAddressKHR is only available through Volk");
    return VK_ERROR_FEATURE_NOT_PRESENT;
#endif
}

void LogicalDevice::GetAccelerationStructureBuildSizes(const VkAccelerationStructureBuildGeometryInfoKHR& BuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR& SizeInfo) const
{
#if DILIGENT_USE_VOLK
    vkGetAccelerationStructureBuildSizesKHR(m_VkDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &BuildInfo, pMaxPrimitiveCounts, &SizeInfo);
#else
    UNSUPPORTED("vkGetAccelerationStructureBuildSizesKHR is only available through Volk");
#endif
}

VkResult LogicalDevice::MapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) const
{
    return vkMapMemory(m_VkDevice, memory, offset, size, flags, ppData);
}

void LogicalDevice::UnmapMemory(VkDeviceMemory memory) const
{
    vkUnmapMemory(m_VkDevice, memory);
}

VkResult LogicalDevice::InvalidateMappedMemoryRanges(uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) const
{
    return vkInvalidateMappedMemoryRanges(m_VkDevice, memoryRangeCount, pMemoryRanges);
}

VkResult LogicalDevice::FlushMappedMemoryRanges(uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) const
{
    return vkFlushMappedMemoryRanges(m_VkDevice, memoryRangeCount, pMemoryRanges);
}

VkResult LogicalDevice::GetFenceStatus(VkFence fence) const
{
    return vkGetFenceStatus(m_VkDevice, fence);
}

VkResult LogicalDevice::ResetFence(VkFence fence) const
{
    VkResult err = vkResetFences(m_VkDevice, 1, &fence);
    DEV_CHECK_ERR(err == VK_SUCCESS, "vkResetFences() failed");
    return err;
}

VkResult LogicalDevice::WaitForFences(uint32_t       fenceCount,
                                      const VkFence* pFences,
                                      VkBool32       waitAll,
                                      uint64_t       timeout) const
{
    return vkWaitForFences(m_VkDevice, fenceCount, pFences, waitAll, timeout);
}

VkResult LogicalDevice::GetSemaphoreCounter(VkSemaphore TimelineSemaphore, uint64_t* pSemaphoreValue) const
{
#if DILIGENT_USE_VOLK
    return vkGetSemaphoreCounterValueKHR(m_VkDevice, TimelineSemaphore, pSemaphoreValue);
#else
    UNSUPPORTED("vkGetSemaphoreCounterValueKHR is only available through Volk");
    return VK_ERROR_FEATURE_NOT_PRESENT;
#endif
}

VkResult LogicalDevice::SignalSemaphore(const VkSemaphoreSignalInfo& SignalInfo) const
{
#if DILIGENT_USE_VOLK
    VERIFY_EXPR(SignalInfo.sType == VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO);
    return vkSignalSemaphoreKHR(m_VkDevice, &SignalInfo);
#else
    UNSUPPORTED("vkSignalSemaphoreKHR is only available through Volk");
    return VK_ERROR_FEATURE_NOT_PRESENT;
#endif
}

VkResult LogicalDevice::WaitSemaphores(const VkSemaphoreWaitInfo& WaitInfo, uint64_t Timeout) const
{
#if DILIGENT_USE_VOLK
    VERIFY_EXPR(WaitInfo.sType == VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO);
    return vkWaitSemaphoresKHR(m_VkDevice, &WaitInfo, Timeout);
#else
    UNSUPPORTED("vkWaitSemaphoresKHR is only available through Volk");
    return VK_ERROR_FEATURE_NOT_PRESENT;
#endif
}

void LogicalDevice::UpdateDescriptorSets(uint32_t                    descriptorWriteCount,
                                         const VkWriteDescriptorSet* pDescriptorWrites,
                                         uint32_t                    descriptorCopyCount,
                                         const VkCopyDescriptorSet*  pDescriptorCopies) const
{
    vkUpdateDescriptorSets(m_VkDevice, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

VkResult LogicalDevice::ResetCommandPool(VkCommandPool           vkCmdPool,
                                         VkCommandPoolResetFlags flags) const
{
    VkResult err = vkResetCommandPool(m_VkDevice, vkCmdPool, flags);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to reset command pool");
    return err;
}

VkResult LogicalDevice::ResetDescriptorPool(VkDescriptorPool           vkDescriptorPool,
                                            VkDescriptorPoolResetFlags flags) const
{
    VkResult err = vkResetDescriptorPool(m_VkDevice, vkDescriptorPool, flags);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to reset descriptor pool");
    return err;
}

void LogicalDevice::ResetQueryPool(VkQueryPool queryPool,
                                   uint32_t    firstQuery,
                                   uint32_t    queryCount) const
{
#if DILIGENT_USE_VOLK
    vkResetQueryPoolEXT(m_VkDevice, queryPool, firstQuery, queryCount);
#else
    UNSUPPORTED("Host query reset is not supported when vulkan library is linked statically");
#endif
}

VkResult LogicalDevice::CopyMemoryToImage(const VkCopyMemoryToImageInfoEXT& CopyInfo) const
{
#if DILIGENT_USE_VOLK
    VkResult err = vkCopyMemoryToImageEXT(m_VkDevice, &CopyInfo);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to copy memory to image");
    return err;
#else
    UNSUPPORTED("Host image copy is not supported when vulkan library is linked statically");
    return VK_ERROR_FEATURE_NOT_PRESENT;
#endif
}

VkResult LogicalDevice::HostTransitionImageLayout(const VkHostImageLayoutTransitionInfoEXT& TransitionInfo) const
{
#if DILIGENT_USE_VOLK
    VkResult err = vkTransitionImageLayoutEXT(m_VkDevice, 1, &TransitionInfo);
    DEV_CHECK_ERR(err == VK_SUCCESS, "Failed to transition image layout");
    return err;
#else
    UNSUPPORTED("Host image layout transition is not supported when vulkan library is linked statically");
    return VK_ERROR_FEATURE_NOT_PRESENT;
#endif
}

VkResult LogicalDevice::GetRayTracingShaderGroupHandles(VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) const
{
#if DILIGENT_USE_VOLK
    return vkGetRayTracingShaderGroupHandlesKHR(m_VkDevice, pipeline, firstGroup, groupCount, dataSize, pData);
#else
    UNSUPPORTED("vkGetRayTracingShaderGroupHandlesKHR is only available through Volk");
    return VK_ERROR_FEATURE_NOT_PRESENT;
#endif
}

} // namespace VulkanUtilities

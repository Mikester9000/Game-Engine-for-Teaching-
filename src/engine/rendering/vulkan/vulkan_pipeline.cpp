/**
 * @file vulkan_pipeline.cpp
 * @brief VulkanPipeline — graphics PSO creation from SPIR-V shaders.
 *
 * Read this file in order:
 *   LoadSpvFile()        — §1 load binary SPIR-V
 *   CreateShaderModule() — §2 wrap in VkShaderModule
 *   Create()             — §3 build the full PSO (calls §1, §2, then all state)
 *   Bind()               — §4 per-frame bind command
 *   Destroy()            — §5 cleanup
 *
 * @author  Educational Game Engine Project
 * @version 1.0
 * @date    2024
 */

#include "engine/rendering/vulkan/vulkan_pipeline.hpp"
#include "engine/rendering/vulkan/vulkan_mesh.hpp"   // Vertex binding/attribute descs

#include <iostream>
#include <fstream>
#include <stdexcept>

namespace engine {
namespace rendering {

// ===========================================================================
// §1 — LoadSpvFile
// ===========================================================================
// TEACHING NOTE — Reading Binary Files in C++
// std::ios::ate positions the stream at the END so tellg() returns the file
// size immediately.  We allocate a vector of that size, seek back to the
// beginning, and read the whole file in one call.
// ===========================================================================
std::vector<char> VulkanPipeline::LoadSpvFile(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "[VulkanPipeline] Cannot open shader: " << path << "\n";
        return {};
    }

    std::streamsize fileSize = file.tellg();
    std::vector<char> buffer(static_cast<std::size_t>(fileSize));

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

// ===========================================================================
// §2 — CreateShaderModule
// ===========================================================================
// TEACHING NOTE — VkShaderModuleCreateInfo
// pCode is a pointer to uint32_t (4-byte words), but we loaded bytes.
// The reinterpret_cast is safe because std::vector guarantees contiguous
// storage and SPIR-V modules are always 4-byte aligned.
// codeSize is in BYTES (not words) — a common gotcha.
// ===========================================================================
VkShaderModule VulkanPipeline::CreateShaderModule(VkDevice                 device,
                                                  const std::vector<char>& code)
{
    if (code.empty()) return VK_NULL_HANDLE;

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        std::cerr << "[VulkanPipeline] vkCreateShaderModule failed.\n";
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

// ===========================================================================
// §3 — Create
// ===========================================================================
// TEACHING NOTE — Pipeline Creation Order
// Vulkan's graphics pipeline is built from a large struct,
// VkGraphicsPipelineCreateInfo, that references many smaller sub-structs
// describing every pipeline stage.  We fill them from top to bottom,
// following the logical order of the GPU pipeline:
//
//   Vertex input  →  Input assembly  →  Vertex shader
//   →  Rasterisation  →  Fragment shader  →  Colour blend  →  Output
//
// The pipeline layout must be created first because the pipeline references it.
// The shader modules can be destroyed immediately after the pipeline is created.
// ===========================================================================
bool VulkanPipeline::Create(VkDevice           device,
                            VkRenderPass       renderPass,
                            const std::string& vertSpvPath,
                            const std::string& fragSpvPath)
{
    m_device = device;

    // -----------------------------------------------------------------------
    // Load and compile shader modules.
    // -----------------------------------------------------------------------
    auto vertCode = LoadSpvFile(vertSpvPath);
    auto fragCode = LoadSpvFile(fragSpvPath);

    if (vertCode.empty() || fragCode.empty())
    {
        std::cerr << "[VulkanPipeline] Failed to load shader SPIR-V files.\n";
        return false;
    }

    VkShaderModule vertModule = CreateShaderModule(device, vertCode);
    VkShaderModule fragModule = CreateShaderModule(device, fragCode);

    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE)
    {
        if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, vertModule, nullptr);
        if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    // -----------------------------------------------------------------------
    // TEACHING NOTE — VkPipelineShaderStageCreateInfo
    // -----------------------------------------------------------------------
    // One entry per programmable stage.  Key fields:
    //   stage  — which stage (VERTEX, FRAGMENT, GEOMETRY, …).
    //   module — the VkShaderModule containing the SPIR-V.
    //   pName  — entry-point function name in the GLSL source ("main").
    // -----------------------------------------------------------------------
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};

    shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertModule;
    shaderStages[0].pName  = "main";

    shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragModule;
    shaderStages[1].pName  = "main";

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Vertex Input State
    // -----------------------------------------------------------------------
    // Describes the memory layout of the vertex buffer.  We get the binding
    // and attribute descriptions from VulkanMesh which owns the Vertex struct.
    // This keeps the vertex layout defined in one place (VulkanMesh) and
    // referenced here without duplication.
    // -----------------------------------------------------------------------
    auto bindingDesc   = VulkanMesh::GetBindingDescription();
    auto attributeDesc = VulkanMesh::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDesc.size());
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDesc.data();

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Input Assembly State
    // -----------------------------------------------------------------------
    // Describes how vertex indices form primitives.
    // TRIANGLE_LIST = every 3 consecutive vertices form one triangle.
    // primitiveRestartEnable = VK_FALSE (only relevant for indexed strip draws).
    // -----------------------------------------------------------------------
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Dynamic Viewport and Scissor
    // -----------------------------------------------------------------------
    // By listing VIEWPORT and SCISSOR as dynamic states, we tell Vulkan that
    // these values will be set by vkCmdSetViewport / vkCmdSetScissor in the
    // command buffer rather than being baked into the pipeline.
    //
    // Benefit: when the window is resized, we only need to update the
    // viewport/scissor commands — we do NOT need to recreate the pipeline.
    // This is the modern (Vulkan 1.3) recommended approach.
    // -----------------------------------------------------------------------
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    // The viewport and scissor counts must still be declared even though
    // the values are dynamic.
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Rasterisation State
    // -----------------------------------------------------------------------
    // polygonMode  FILL  = solid triangles (wireframe = FILL → LINE, requires a GPU feature).
    // cullMode     NONE  = draw both front and back faces (no backface culling for our demo).
    // frontFace    CCW   = counter-clockwise winding = front face (standard convention).
    // depthClamp / depthBias — disabled for this demo.
    // lineWidth    1.0f  = standard; >1.0 requires the wideLines GPU feature.
    // -----------------------------------------------------------------------
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode                = VK_CULL_MODE_NONE;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;
    rasterizer.lineWidth               = 1.0f;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Multisampling (MSAA)
    // -----------------------------------------------------------------------
    // rasterizationSamples = 1 bit = no anti-aliasing.  MSAA is introduced
    // in a later milestone when render quality becomes a concern.
    // -----------------------------------------------------------------------
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable  = VK_FALSE;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Colour Blend Attachment
    // -----------------------------------------------------------------------
    // blendEnable = VK_FALSE means no blending: the fragment shader output
    // completely replaces the existing pixel.  srcColorBlendFactor and
    // dstColorBlendFactor are ignored when blendEnable is FALSE.
    //
    // colorWriteMask: which channels to write.  RGBA = write all four.
    // -----------------------------------------------------------------------
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable    = VK_FALSE;
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorBlendAttachment;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Pipeline Layout
    // -----------------------------------------------------------------------
    // The layout declares the descriptor sets (textures, UBOs) and push
    // constants the shaders use.  For a vertex-only-input triangle, no
    // resources are needed, so we create an empty layout.
    // -----------------------------------------------------------------------
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 0;
    layoutInfo.pSetLayouts            = nullptr;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges    = nullptr;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        std::cerr << "[VulkanPipeline] vkCreatePipelineLayout failed.\n";
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        return false;
    }

    // -----------------------------------------------------------------------
    // Assemble the final pipeline creation struct.
    // -----------------------------------------------------------------------
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = nullptr;   // No depth buffer for M1
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;   // subpass index within the render pass
    // TEACHING NOTE — basePipelineHandle
    // Vulkan allows you to derive a new pipeline from an existing one for
    // faster creation.  We don't use this feature yet.
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex   = -1;

    // -----------------------------------------------------------------------
    // TEACHING NOTE — Pipeline Cache
    // -----------------------------------------------------------------------
    // The second parameter is an optional VkPipelineCache.  A cache stores
    // the compiled pipeline data so it can be reused across application runs.
    // Huge win for startup time on real games; we pass VK_NULL_HANDLE for
    // simplicity (the cache is treated as empty and nothing is saved).
    // -----------------------------------------------------------------------
    VkResult result = vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);

    // Shader modules are no longer needed after pipeline creation.
    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanPipeline] vkCreateGraphicsPipelines failed: " << result << "\n";
        return false;
    }

    m_initialised = true;
    std::cout << "[VulkanPipeline] Graphics pipeline created.\n";
    return true;
}

// ===========================================================================
// §4 — Bind
// ===========================================================================
void VulkanPipeline::Bind(VkCommandBuffer commandBuffer) const
{
    if (!m_initialised) return;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
}

// ===========================================================================
// §5 — Destroy
// ===========================================================================
// TEACHING NOTE — Layout vs Pipeline Destruction Order
// The pipeline MUST be destroyed before the pipeline layout that it was
// created with.  The layout itself can be destroyed any time after the
// pipeline that references it is destroyed.
// ===========================================================================
void VulkanPipeline::Destroy()
{
    if (!m_initialised) return;
    m_initialised = false;

    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
}

} // namespace rendering
} // namespace engine

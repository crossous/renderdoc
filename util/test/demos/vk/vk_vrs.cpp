/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "vk_test.h"

RD_TEST(VK_VRS, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Checks that VRS is correctly replayed and that state is inspectable";

  std::string pixel = R"EOSHADER(

#extension GL_EXT_fragment_shading_rate : require

uint wang_hash(uint seed)
{
  seed = (seed ^ 61) ^ (seed >> 16);
  seed *= 9;
  seed = seed ^ (seed >> 4);
  seed *= 0x27d4eb2d;
  seed = seed ^ (seed >> 15);
  return seed;
}

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  uint col = wang_hash(uint(gl_FragCoord.x * 10000.0f + gl_FragCoord.y));

  Color.x = float((col & 0xff000000u) >> 24u) / 255.0f;
  Color.y = float((col & 0x00ff0000u) >> 16u) / 255.0f;
  Color.z = float((col & 0x0000ff00u) >>  8u) / 255.0f;
  Color.w = gl_ShadingRateEXT * 100.0f + dFdxFine(Color.x);
}

)EOSHADER";

  std::string vertex = R"EOSHADER(

#extension GL_EXT_fragment_shading_rate : require

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;

void main()
{
	gl_Position = vec4(Position, 1.0);

#ifdef VERT_VRS
  gl_PrimitiveShadingRateEXT = int(Color.x) << 2 | int(Color.y);
#endif
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    instExts.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    devExts.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    devExts.push_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceFragmentShadingRateFeaturesKHR rate2Features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,
    };

    getPhysFeatures2(&rate2Features);

    if(!rate2Features.attachmentFragmentShadingRate)
      Avail = "'attachmentFragmentShadingRate' not available";
    else if(!rate2Features.pipelineFragmentShadingRate)
      Avail = "'pipelineFragmentShadingRate' not available";
    else if(!rate2Features.primitiveFragmentShadingRate)
      Avail = "'primitiveFragmentShadingRate' not available";

    devInfoNext = &rate2Features;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPhysicalDeviceFragmentShadingRatePropertiesKHR props = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR,
    };

    getPhysProperties2(&props);

    VkRenderPass renderPass = VK_NULL_HANDLE, imgRenderPass = VK_NULL_HANDLE;
    {
      vkh::RenderPassCreator2 renderPassCreateInfo;

      vkh::AttachmentDescription2KHR colorAtt(VK_FORMAT_R32G32B32A32_SFLOAT,
                                              VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
      colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      renderPassCreateInfo.attachments.push_back(colorAtt);

      vkh::AttachmentDescription2KHR rateAtt(VK_FORMAT_R8_UINT, VK_IMAGE_LAYOUT_GENERAL,
                                             VK_IMAGE_LAYOUT_GENERAL);
      rateAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      renderPassCreateInfo.attachments.push_back(rateAtt);

      VkAttachmentReference2 rateAttRef =
          vkh::AttachmentReference2KHR(1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

      VkFragmentShadingRateAttachmentInfoKHR rateAttInfo = {
          VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,
      };
      rateAttInfo.pFragmentShadingRateAttachment = &rateAttRef;
      rateAttInfo.shadingRateAttachmentTexelSize = props.maxFragmentShadingRateAttachmentTexelSize;

      renderPassCreateInfo.addSubpass(
          {vkh::AttachmentReference2KHR(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_IMAGE_ASPECT_COLOR_BIT)},
          {vkh::AttachmentReference2KHR(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED)});

      // add deps to allow clear/copy on the main target before or after
      renderPassCreateInfo.dependencies.push_back(vkh::SubpassDependency2KHR(
          ~0U, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
      renderPassCreateInfo.dependencies.push_back(vkh::SubpassDependency2KHR(
          0, ~0U, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT));

      CHECK_VKR(vkCreateRenderPass2KHR(device, renderPassCreateInfo, NULL, &renderPass));

      renderPassCreateInfo.subpasses.back().pNext = &rateAttInfo;

      CHECK_VKR(vkCreateRenderPass2KHR(device, renderPassCreateInfo, NULL, &imgRenderPass));
    }

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
    };

    std::string header = "#version 460 core\n";

    pipeCreateInfo.stages = {
        CompileShaderModule(header + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(header + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.dynamicState.dynamicStates.push_back(VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR);

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.renderPass = imgRenderPass;

    VkPipeline imgpipe = createGraphicsPipeline(pipeCreateInfo);

    header += "#define VERT_VRS 1\n";

    pipeCreateInfo.stages = {
        CompileShaderModule(header + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(header + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.renderPass = renderPass;

    VkPipeline vertpipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.renderPass = imgRenderPass;

    VkPipeline imgvertpipe = createGraphicsPipeline(pipeCreateInfo);

    const DefaultA2V tris[6] = {
        {Vec3f(-1.0f, 0.6f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, -0.4f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, 0.6f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(0.0f, 0.4f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.5f, -0.6f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(1.0f, 0.4f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(tris), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(tris);

    uint32_t offWidth =
        AlignUp((uint32_t)screenWidth, props.maxFragmentShadingRateAttachmentTexelSize.width);
    uint32_t offHeight =
        AlignUp((uint32_t)screenHeight, props.maxFragmentShadingRateAttachmentTexelSize.height);

    AllocatedImage img(
        this,
        vkh::ImageCreateInfo(offWidth, offHeight, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView imgview = createImageView(
        vkh::ImageViewCreateInfo(img.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT));

    AllocatedImage rateimg(
        this,
        vkh::ImageCreateInfo(
            offWidth / props.maxFragmentShadingRateAttachmentTexelSize.width,
            offHeight / props.maxFragmentShadingRateAttachmentTexelSize.height, 0, VK_FORMAT_R8_UINT,
            VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));
    VkImageView rate_view = createImageView(
        vkh::ImageViewCreateInfo(rateimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8_UINT));

    VkFramebuffer framebuffer = createFramebuffer(
        vkh::FramebufferCreateInfo(renderPass, {imgview, rate_view}, mainWindow->scissor.extent));

    VkFramebuffer imgframebuffer = createFramebuffer(vkh::FramebufferCreateInfo(
        imgRenderPass, {imgview, rate_view}, mainWindow->scissor.extent));

    VkRenderPass clearRP;
    {
      vkh::RenderPassCreator renderPassCreateInfo;

      renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
          VK_FORMAT_R8_UINT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL));

      renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})});

      // add deps to allow clear/copy on the main target before or after
      renderPassCreateInfo.dependencies.push_back(vkh::SubpassDependency(
          ~0U, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT));
      renderPassCreateInfo.dependencies.push_back(vkh::SubpassDependency(
          0, ~0U, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT));

      clearRP = createRenderPass(renderPassCreateInfo);
    }

    VkExtent2D clearRect = {offWidth / props.maxFragmentShadingRateAttachmentTexelSize.width,
                            offHeight / props.maxFragmentShadingRateAttachmentTexelSize.height};

    VkFramebuffer clearFB =
        createFramebuffer(vkh::FramebufferCreateInfo(clearRP, {rate_view}, clearRect));

    const VkFragmentShadingRateCombinerOpKHR combiners[2] = {
        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR,
        VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR,
    };

    const VkExtent2D frag2x2 = {2, 2};
    const VkExtent2D frag1x1 = {1, 1};

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg = StartUsingBackbuffer(cmd);

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL, rateimg.image),
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL, img.image),
          });

      pushMarker(cmd, "Clear");
      {
        VkClearRect rect = {vkh::Rect2D({0, 0}, clearRect), 0, 1};

        vkCmdBeginRenderPass(cmd, vkh::RenderPassBeginInfo(clearRP, clearFB, rect.rect),
                             VK_SUBPASS_CONTENTS_INLINE);

        VkClearAttachment att = {VK_IMAGE_ASPECT_COLOR_BIT, 0, vkh::ClearValue(5U, 0U, 0U, 0U)};
        vkCmdClearAttachments(cmd, 1, &att, 1, &rect);

        rect.rect.extent.width /= 8;
        rect.rect.extent.height = (clearRect.height * 3) / 4;
        rect.rect.offset.x = clearRect.width - rect.rect.extent.width;
        att.clearValue.color.uint32[0] = 0;
        vkCmdClearAttachments(cmd, 1, &att, 1, &rect);

        rect.rect.extent.width--;
        rect.rect.offset.x = clearRect.width - rect.rect.extent.width;
        rect.rect.offset.y += rect.rect.extent.height;
        rect.rect.extent.height = clearRect.height / 4;
        att.clearValue.color.uint32[0] = 0;
        vkCmdClearAttachments(cmd, 1, &att, 1, &rect);

        vkCmdEndRenderPass(cmd);

        vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                             vkh::ImageSubresourceRange());
      }
      popMarker(cmd);

      vkh::cmdBindVertexBuffers(cmd, {vb.buffer});

      mainWindow->setViewScissor(cmd);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

      const float x = (float)offWidth / 4.0f;
      const float y = (float)offHeight / 4.0f;

      pushMarker(cmd, "First");
      {
        vkCmdBeginRenderPass(cmd,
                             vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor),
                             VK_SUBPASS_CONTENTS_INLINE);

        {
          setMarker(cmd, "Default");

          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
          vkCmdSetFragmentShadingRateKHR(cmd, &frag1x1, combiners);

          vkh::cmdSetViewport(cmd, vkh::Viewport(x * 0.0f, 0.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmd, 6, 1, 0, 0);
        }

        {
          setMarker(cmd, "Base");

          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
          vkCmdSetFragmentShadingRateKHR(cmd, &frag2x2, combiners);

          vkh::cmdSetViewport(cmd, vkh::Viewport(x * 1.0f, 0.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmd, 6, 1, 0, 0);
        }

        {
          setMarker(cmd, "Vertex");

          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vertpipe);
          vkCmdSetFragmentShadingRateKHR(cmd, &frag1x1, combiners);

          vkh::cmdSetViewport(cmd, vkh::Viewport(x * 2.0f, 0.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmd, 6, 1, 0, 0);
        }

        {
          setMarker(cmd, "Base + Vertex");

          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vertpipe);
          vkCmdSetFragmentShadingRateKHR(cmd, &frag2x2, combiners);

          vkh::cmdSetViewport(cmd, vkh::Viewport(x * 0.0f, y, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmd, 6, 1, 0, 0);
        }

        vkCmdEndRenderPass(cmd);

        vkCmdBeginRenderPass(
            cmd, vkh::RenderPassBeginInfo(imgRenderPass, imgframebuffer, mainWindow->scissor),
            VK_SUBPASS_CONTENTS_INLINE);

        {
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imgpipe);
          vkCmdSetFragmentShadingRateKHR(cmd, &frag1x1, combiners);
          setMarker(cmd, "Image");

          vkh::cmdSetViewport(cmd, vkh::Viewport(x * 3.0f, 0.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmd, 6, 1, 0, 0);

          setMarker(cmd, "Base + Image");

          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imgpipe);
          vkCmdSetFragmentShadingRateKHR(cmd, &frag2x2, combiners);

          vkh::cmdSetViewport(cmd, vkh::Viewport(x * 3.0f, y, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmd, 6, 1, 0, 0);

          setMarker(cmd, "Vertex + Image");

          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imgvertpipe);
          vkCmdSetFragmentShadingRateKHR(cmd, &frag1x1, combiners);

          vkh::cmdSetViewport(cmd, vkh::Viewport(x * 3.0f, y * 2.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmd, 6, 1, 0, 0);
        }

        {
          setMarker(cmd, "Image (partial)");

          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imgpipe);
          vkCmdSetFragmentShadingRateKHR(cmd, &frag1x1, combiners);

          vkh::cmdSetViewport(cmd, vkh::Viewport(x * 3.0f, y * 3.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmd, 6, 1, 0, 0);
        }

        vkCmdEndRenderPass(cmd);
      }
      popMarker(cmd);

      vkEndCommandBuffer(cmd);

      VkCommandBuffer cmdB = GetCommandBuffer();

      vkBeginCommandBuffer(cmdB, vkh::CommandBufferBeginInfo());

      vkCmdBindPipeline(cmdB, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetFragmentShadingRateKHR(cmdB, &frag1x1, combiners);

      vkh::cmdBindVertexBuffers(cmdB, {vb.buffer});

      mainWindow->setViewScissor(cmdB);
      vkh::cmdSetViewport(cmdB, vkh::Viewport(0.0f, 0.0f, x, y, 0.0f, 1.0f));

      vkCmdBeginRenderPass(cmdB,
                           vkh::RenderPassBeginInfo(renderPass, framebuffer, mainWindow->scissor),
                           VK_SUBPASS_CONTENTS_INLINE);

      pushMarker(cmdB, "Second");
      {
        {
          setMarker(cmdB, "Default");

          vkh::cmdSetViewport(cmdB, vkh::Viewport(x * 0.0f, 0.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmdB, 6, 1, 0, 0);
        }

        {
          setMarker(cmdB, "Base");

          vkh::cmdSetViewport(cmdB, vkh::Viewport(x * 1.0f, 0.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmdB, 0, 0, 0, 0);
        }

        {
          setMarker(cmdB, "Vertex");

          vkh::cmdSetViewport(cmdB, vkh::Viewport(x * 2.0f, 0.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmdB, 0, 0, 0, 0);

          setMarker(cmdB, "Image");

          vkh::cmdSetViewport(cmdB, vkh::Viewport(x * 3.0f, 0.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmdB, 0, 0, 0, 0);

          setMarker(cmdB, "Base + Vertex");

          vkh::cmdSetViewport(cmdB, vkh::Viewport(x * 0.0f, y, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmdB, 0, 0, 0, 0);

          setMarker(cmdB, "Base + Image");

          vkh::cmdSetViewport(cmdB, vkh::Viewport(x * 3.0f, y, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmdB, 0, 0, 0, 0);

          setMarker(cmdB, "Vertex + Image");

          vkh::cmdSetViewport(cmdB, vkh::Viewport(x * 3.0f, y * 2.0f, x, y, 0.0f, 1.0f));
          vkCmdDraw(cmdB, 0, 0, 0, 0);
        }
      }
      popMarker(cmdB);

      vkCmdEndRenderPass(cmdB);

      blitToSwap(cmdB, img.image, VK_IMAGE_LAYOUT_GENERAL, swapimg, VK_IMAGE_LAYOUT_GENERAL);

      FinishUsingBackbuffer(cmdB);

      vkEndCommandBuffer(cmdB);

      SubmitAndPresent({cmd, cmdB});
    }

    vkDestroyRenderPass(device, renderPass, NULL);
    vkDestroyRenderPass(device, imgRenderPass, NULL);

    return 0;
  }
};

REGISTER_TEST();

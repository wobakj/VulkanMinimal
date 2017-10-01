#include "debug_reporter.hpp"
#include "init_utils.hpp"

#include <lodepng.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>
#include <cstdint>
#include <cmath>

template<typename T>
void encodeOneStep(const char* filename, T image, unsigned width, unsigned height) {
  //Encode the image
  // unsigned error = lodepng::encode(filename, image, width, height);
  std::vector<unsigned char> png;

  unsigned error = lodepng::encode(png, image, width, height);
  if(!error) error = lodepng::save_file(png, filename);

  //if there's an error, display it
  if(error) std::cout << "encoder error " << error << ": "<< lodepng_error_text(error) << std::endl;
}

int main(int argc, char* argv[]) {
// create Instance
  vk::ApplicationInfo appInfo{};
  appInfo.apiVersion = VK_API_VERSION_1_0;

  vk::InstanceCreateInfo createInfo{};
  createInfo.pApplicationInfo = &appInfo;
  // fill layer info
  std::vector<char const*> layers{"VK_LAYER_LUNARG_standard_validation"};
  createInfo.ppEnabledLayerNames = layers.data();
  createInfo.enabledLayerCount = uint32_t(layers.size());
  // fill extension info 
  std::vector<char const*> extensions = get_required_extensions(false, true);
  createInfo.enabledExtensionCount = uint32_t(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  vk::Instance instance = vk::createInstance(createInfo);
  // create DebugCallback objetc and attach to instance
  DebugReporter debug_reporter{instance};

///////////////////////////////////////////////////////////////////////////////

// choose physical device
  std::vector<vk::PhysicalDevice> phys_devices = instance.enumeratePhysicalDevices();
  // fallback in case optimal device is found
  vk::PhysicalDevice chosen_device{phys_devices.front()};
  // search for discrete GPU
  for (const auto& phys_device : phys_devices) {
    vk::PhysicalDeviceProperties properties = phys_device.getProperties();
    // check if device is discrete GPU
    if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
      chosen_device = phys_device;
      break;
    }
  }
// choose queue family
  std::vector<vk::QueueFamilyProperties> queue_families =chosen_device.getQueueFamilyProperties();
  vk::DeviceQueueCreateInfo info_queue{};
  info_queue.queueCount = 1;
  float priority = 1.0f;
  info_queue.pQueuePriorities = &priority;
  // search for usable queue
  for (uint32_t i = 0; i < queue_families.size(); ++i) {
    if (queue_families[i].queueFlags & vk::QueueFlagBits::eCompute 
     && queue_families[i].queueFlags & vk::QueueFlagBits::eTransfer) {
      info_queue.queueFamilyIndex = i;
      break;
    }
  }
  // fill info struct
  vk::DeviceCreateInfo info_device{};
  info_device.queueCreateInfoCount = 1;
  info_device.pQueueCreateInfos = &info_queue;

  vk::Device device = chosen_device.createDevice(info_device);

///////////////////////////////////////////////////////////////////////////////

// get queue
  vk::Queue queue = device.getQueue(info_queue.queueFamilyIndex, 0);
  queue.waitIdle();

///////////////////////////////////////////////////////////////////////////////

// create command pool
  vk::CommandPoolCreateInfo info_command_pool{};
  info_command_pool.queueFamilyIndex = info_queue.queueFamilyIndex;
  info_command_pool.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

  vk::CommandPool command_pool = device.createCommandPool(info_command_pool);
// allocate command buffer
  vk::CommandBufferAllocateInfo info_command_buffer{};
  info_command_buffer.commandPool =  command_pool;
  info_command_buffer.level =  vk::CommandBufferLevel::ePrimary;
  info_command_buffer.commandBufferCount =  1;
  
  vk::CommandBuffer command_buffer = device.allocateCommandBuffers(info_command_buffer).front();

  vk::CommandBufferBeginInfo info_cb_begin{};
// record empty buffer
  info_cb_begin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
  command_buffer.begin(info_cb_begin);
  command_buffer.end();

  vk::SubmitInfo info_submit{};
  info_submit.pCommandBuffers = &command_buffer;
  info_submit.commandBufferCount = 1;

// logic
  queue.submit(info_submit, {});
  queue.waitIdle();

///////////////////////////////////////////////////////////////////////////////

// create host memory
  vk::MemoryAllocateInfo info_memory_host{};
  info_memory_host.allocationSize = 512 * 512 * 4 * 2;
  vk::PhysicalDeviceMemoryProperties mem_properties_buffer = chosen_device.getMemoryProperties();
  // search for coherent, mapable type
  for (uint32_t i = 0; i < mem_properties_buffer.memoryTypeCount; ++i) {
    if (mem_properties_buffer.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible
     && mem_properties_buffer.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) {
      info_memory_host.memoryTypeIndex = i;
      break;
    }
  }

  vk::DeviceMemory memory_host = device.allocateMemory(info_memory_host);
  uint8_t* ptr_mem = static_cast<uint8_t*>(device.mapMemory(memory_host, 0, 512 * 512 * 4 * 2));
  std::memset(ptr_mem, 255, 512 * 512 * 4);

///////////////////////////////////////////////////////////////////////////////

// create buffer
  vk::BufferCreateInfo info_buffer{};
  info_buffer.size = 512 * 512 * 4;
  info_buffer.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst; 

  vk::Buffer buffer = device.createBuffer(info_buffer);
// bind buffer to memory
  vk::MemoryRequirements requirements_buffer = device.getBufferMemoryRequirements(buffer);
  // check that memory type is supported
  if(!(requirements_buffer.memoryTypeBits & (1 << info_memory_host.memoryTypeIndex))) throw std::exception{};
  if(requirements_buffer.size > info_memory_host.allocationSize) throw std::exception{};
  
  device.bindBufferMemory(buffer, memory_host, 0);

  // rerecord buffer
  info_cb_begin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
  command_buffer.begin(info_cb_begin);
  // fill half of image with white
  command_buffer.fillBuffer(buffer, 0, 512 * 256 * 4, 0);
  command_buffer.end();

  queue.submit(info_submit, {});
  queue.waitIdle();

///////////////////////////////////////////////////////////////////////////////

// create image
  vk::ImageCreateInfo info_image_linear{};
  info_image_linear.imageType = vk::ImageType::e2D;
  info_image_linear.extent = vk::Extent3D{512, 512, 1};
  info_image_linear.format = vk::Format::eR8G8B8A8Unorm;
  info_image_linear.tiling = vk::ImageTiling::eLinear;
  info_image_linear.mipLevels = 1;
  info_image_linear.arrayLayers = 1;
  info_image_linear.initialLayout = vk::ImageLayout::eUndefined;
  info_image_linear.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;

  vk::Image image_linear = device.createImage(info_image_linear);

// bind image
  vk::MemoryRequirements requirements_image_linear = device.getImageMemoryRequirements(image_linear);
  // check that memory type is supported
  if(!(requirements_image_linear.memoryTypeBits & (1 << info_memory_host.memoryTypeIndex))) throw std::exception{};
  // calculate offset
  vk::DeviceSize offset = vk::DeviceSize(std::ceil(float(requirements_buffer.size / requirements_image_linear.alignment))) * requirements_image_linear.alignment;
  if(offset + requirements_image_linear.size > info_memory_host.allocationSize) throw std::exception{};

  device.bindImageMemory(image_linear, memory_host, offset);

  // /create full range
  vk::ImageSubresourceRange image_range_full{};
  image_range_full.baseMipLevel = 0;
  image_range_full.levelCount = 1;
  image_range_full.baseArrayLayer = 0;
  image_range_full.layerCount = 1;
  image_range_full.aspectMask = vk::ImageAspectFlagBits::eColor;

  // rerecord buffer
  info_cb_begin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
  command_buffer.begin(info_cb_begin);
  // transform to general layout
  vk::ImageMemoryBarrier barrier_img{};
  barrier_img.image = image_linear;
  barrier_img.subresourceRange = image_range_full;
  barrier_img.oldLayout = vk::ImageLayout::eUndefined;
  barrier_img.newLayout = vk::ImageLayout::eGeneral;
  barrier_img.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
  command_buffer.pipelineBarrier(
    vk::PipelineStageFlagBits::eTopOfPipe,
    vk::PipelineStageFlagBits::eTransfer,
    vk::DependencyFlags{},
    {},
    {},
    barrier_img
  );
  
  // fill half of image with white
  vk::ClearColorValue clear_colors{};
  clear_colors.setFloat32({1.0f, 0.0f, 1.0f, 1.0f});
  command_buffer.clearColorImage(image_linear, vk::ImageLayout::eGeneral, clear_colors, image_range_full);
  command_buffer.end();

  queue.submit(info_submit, {});
  queue.waitIdle();
///////////////////////////////////////////////////////////////////////////////

// // create device memory
//   vk::MemoryAllocateInfo info_memory_device{};
//   info_memory_device.allocationSize = 512 * 512 * 4 * 2;
//   vk::PhysicalDeviceMemoryProperties mem_properties_image = chosen_device.getMemoryProperties();
//   // search for coherent, mapable type
//   for (uint32_t i = 0; i < mem_properties_image.memoryTypeCount; ++i) {
//     if (mem_properties_image.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) {
//       info_memory_device.memoryTypeIndex = i;
//       break;
//     }
//   }

//   vk::DeviceMemory memory_device = device.allocateMemory(info_memory_device);
// synchronisation
// // create image
//   vk::ImageCreateInfo info_image{};
//   info_image.imageType = vk::ImageType::e2D;
//   info_image.extent = vk::Extent3D{512, 512, 1};
//   info_image.format = vk::Format::eR8G8B8A8Unorm;
//   info_image.tiling = vk::ImageTiling::eOptimal;
//   info_image.mipLevels = 1;
//   info_image.arrayLayers = 1;
//   info_image.initialLayout = vk::ImageLayout::eUndefined;
//   info_image.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
//   // for view
//   info_image.usage |= vk::ImageUsageFlagBits::eStorage;

//   vk::Image image = device.createImage(info_image);

// // bind image
//   vk::MemoryRequirements requirements_image = device.getImageMemoryRequirements(image);
//   // check that memory type is supported
//   if(!(requirements_image.memoryTypeBits & (1 << info_memory_device.memoryTypeIndex))) throw std::exception{};
//   if(requirements_image.size > info_memory_device.allocationSize) throw std::exception{};

//   device.bindImageMemory(image, memory_device, 0);

//   // rerecord buffer
//   info_cb_begin.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
//   command_buffer.begin(info_cb_begin);
//   // transform to general layout
//   vk::ImageMemoryBarrier barrier_img{};
//   barrier_img.image = image;
//   barrier_img.subresourceRange = image_range_full;
//   barrier_img.oldLayout = vk::ImageLayout::eUndefined;
//   barrier_img.newLayout = vk::ImageLayout::eGeneral;
//   barrier_img.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
//   command_buffer.pipelineBarrier(
//     vk::PipelineStageFlagBits::eTopOfPipe,
//     vk::PipelineStageFlagBits::eTransfer,
//     vk::DependencyFlags{},
//     {},
//     {},
//     barrier_img
//   );
  
//   // fill half of image with white
//   vk::ClearColorValue clear_colors{};
//   clear_colors.setFloat32({1.0f, 0.0f, 1.0f, 1.0f});
//   command_buffer.clearColorImage(image, vk::ImageLayout::eGeneral, clear_colors, image_range_full);
//   command_buffer.end();

//   queue.submit(info_submit, {});
//   queue.waitIdle();

//   // copy image to buffer
//   // /create full layer range
//   vk::ImageSubresourceLayers image_layers_full{};
//   image_layers_full.mipLevel = 0;
//   image_layers_full.baseArrayLayer = 0;
//   image_layers_full.layerCount = 1;
//   image_layers_full.aspectMask = vk::ImageAspectFlagBits::eColor;

//   command_buffer.begin(info_cb_begin);

//   vk::BufferImageCopy copy_region{};
//   copy_region.imageSubresource = image_layers_full;
//   copy_region.imageExtent = info_image.extent;
//   command_buffer.copyImageToBuffer(image, vk::ImageLayout::eGeneral, buffer, copy_region);
//   command_buffer.end();

//   queue.submit(info_submit, {});
//   queue.waitIdle();

///////////////////////////////////////////////////////////////////////////////

// create view

  // vk::ComponentMapping mapping{};
  // mapping.r = vk::ComponentSwizzle::eR;
  // mapping.g = vk::ComponentSwizzle::eG;
  // mapping.b = vk::ComponentSwizzle::eB;
  // mapping.a = vk::ComponentSwizzle::eA;

  // vk::ImageViewCreateInfo info_image_view{};
  // info_image_view.image = image;
  // info_image_view.subresourceRange = image_range_full;
  // info_image_view.format = info_image.format;
  // info_image_view.viewType = vk::ImageViewType::e2D;
  // info_image_view.components = mapping;

  // vk::ImageView view_image = device.createImageView(info_image_view);

  // device.waitIdle();

///////////////////////////////////////////////////////////////////////////////

// end
  std::vector<uint8_t> pixels(512 * 512 * 4, 255);
  auto filename = resource_path(argv[0]) + "out.png";
  std::cout << filename << std::endl;
  // encodeOneStep(filename.c_str(), pixels, 512, 512);
  encodeOneStep(filename.c_str(), ptr_mem + offset, 512, 512);
  
  // device.destroyImageView(view_image);
  device.destroyImage(image_linear);
  device.destroyBuffer(buffer);
  device.freeMemory(memory_host);
  // device.freeMemory(memory_device);
  device.destroyCommandPool(command_pool);
  device.destroy();
  debug_reporter = {};
  instance.destroy();

  return 0;
}

#include "debug_reporter.hpp"

#include <lodepng.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <vector>
#include <cstdint>

static std::string resource_path(std::string const& path_exe) {
  std::string resource_path{};
  resource_path = path_exe.substr(0, path_exe.find_last_of("/\\"));
  resource_path += "/resources/";
  return resource_path;
}

std::vector<const char*> getRequiredExtensions(bool display, bool debug) {
  std::vector<const char*> extensions;

  if (display) {
    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (unsigned int i = 0; i < glfwExtensionCount; i++) {
      extensions.push_back(glfwExtensions[i]);
    }
  }

  if (debug) {
    extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }

  return extensions;
}

void encodeOneStep(const char* filename, std::vector<unsigned char> const& image, unsigned width, unsigned height) {
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
  std::vector<char const*> extensions = getRequiredExtensions(false, true);
  createInfo.enabledExtensionCount = uint32_t(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  vk::Instance instance = vk::createInstance(createInfo);
  // create DebugCallback objetc and attach to instance
  DebugReporter debug_reporter{instance};

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

// logic
  device.waitIdle();
// end
  device.destroy();
  debug_reporter = {};
  instance.destroy();

  std::vector<uint8_t> pixels(512 * 512 * 4, 255);
  auto filename = resource_path(argv[0]) + "out.png";
  std::cout << filename << std::endl;
  encodeOneStep(filename.c_str(), pixels, 512, 512);
  return 0;
}

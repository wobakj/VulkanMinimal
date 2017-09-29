#include "debug_reporter.hpp"

#include <iostream>

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugReportFlagsEXT flags,
  VkDebugReportObjectTypeEXT objType,
  uint64_t obj,
  size_t location,
  int32_t code,
  const char* layerPrefix,
  const char* msg,
  void* userData) 
{
  std::string msg_type;
  bool throwing = false; 
  if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    msg_type = "Error ";
    throwing = true;
  }
  else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
    msg_type = "Warning ";
  }
  else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    msg_type = "Performance ";
  }
  else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
    msg_type = "Info ";
  }
  else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
    msg_type = "Debug ";
  }
  std::cerr << msg_type << std::to_string(code) << " - " << msg << std::endl;
  if (throwing) {
    throw std::runtime_error{"Vulkan Validation " + msg_type + std::to_string(code)};
  }
  return VK_FALSE;
}

DebugReporter::DebugReporter()
 :m_instance{}
 ,m_callback{}
{}

DebugReporter::DebugReporter(vk::Instance const& inst, vk::DebugReportFlagsEXT const& flags)
 :m_instance{inst}
 ,m_callback{}
{
  vk::DebugReportCallbackCreateInfoEXT info_callback{};
  info_callback.flags = flags;
  info_callback.pfnCallback = debug_callback;
  // get construction function address
  auto fptr_create = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");
  if (fptr_create != nullptr) {
    auto result = fptr_create(m_instance, reinterpret_cast<const VkDebugReportCallbackCreateInfoEXT*>(&info_callback), nullptr, reinterpret_cast<VkDebugReportCallbackEXT*>(&m_callback));
    if (result != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create debug callback"};
    }
  } 
  else {
    throw std::runtime_error{"Failed to get function address"};
  }
}

DebugReporter::DebugReporter(DebugReporter&& rhs)
 :DebugReporter{}
{
  std::swap(m_instance, rhs.m_instance);
  std::swap(m_callback, rhs.m_callback);
}

DebugReporter const& DebugReporter::operator=(DebugReporter&& rhs)  {
  cleanup();
  std::swap(m_instance, rhs.m_instance);
  std::swap(m_callback, rhs.m_callback);
  return *this;
}

DebugReporter::~DebugReporter() {
  cleanup();
}

void DebugReporter::cleanup() { 
  if (m_callback) {
    // get destruction function address
    auto fptr_destroy = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
    if (fptr_destroy != nullptr) {
      fptr_destroy(m_instance, m_callback, nullptr);
    }
    else {
      throw std::runtime_error{"Failed to get function address"};
    }
    m_callback = vk::DebugReportCallbackEXT{};
  }
}

#ifndef DEBUG_REPORTER_HPP
#define DEBUG_REPORTER_HPP

#include <vulkan/vulkan.hpp>

// simple class to wrap DebugCallback
// queries extension function addresses
class DebugReporter {
 public:
  DebugReporter();
  DebugReporter(
    vk::Instance const& inst, 
    vk::DebugReportFlagsEXT const& flags = 
          vk::DebugReportFlagBitsEXT::eError
        | vk::DebugReportFlagBitsEXT::eWarning 
        | vk::DebugReportFlagBitsEXT::ePerformanceWarning 
       // | vk::DebugReportFlagBitsEXT::eDebug 
       // | vk::DebugReportFlagBitsEXT::eInformation
  );
  DebugReporter(DebugReporter&& rhs);
  DebugReporter(DebugReporter const&) = delete;

  ~DebugReporter();

  DebugReporter const& operator=(DebugReporter&& rhs);
  DebugReporter const& operator=(DebugReporter const&) = delete;

 private:
  void cleanup();
  
  vk::Instance m_instance;
  vk::DebugReportCallbackEXT m_callback; 
};

#endif
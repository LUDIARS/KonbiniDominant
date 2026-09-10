// Pictor remains the renderer. Adapt its creation calls to MoltenVK's
// advertised portability requirements without changing the pinned checkout.
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstring>
#include <new>
#include <vector>
extern "C" VKAPI_ATTR VkResult VKAPI_CALL konbiniCreateInstance(
    const VkInstanceCreateInfo* input,const VkAllocationCallbacks* allocator,VkInstance* instance) {
    try {
        uint32_t count=0;
        auto result=vkEnumerateInstanceExtensionProperties(nullptr,&count,nullptr);
        if(result!=VK_SUCCESS) return result;
        std::vector<VkExtensionProperties> available(count);
        result=vkEnumerateInstanceExtensionProperties(nullptr,&count,available.data());
        if(result!=VK_SUCCESS) return result;
        auto config=*input;
        std::vector<const char*> names;
        for(uint32_t i=0;i<input->enabledExtensionCount;++i) names.push_back(input->ppEnabledExtensionNames[i]);
        const char* portability="VK_KHR_portability_enumeration";
        const auto matches=[&](const auto& e){return std::strcmp(e.extensionName,portability)==0;};
        if(std::any_of(available.begin(),available.end(),matches)) {
            if(std::none_of(names.begin(),names.end(),[&](const char* e){return std::strcmp(e,portability)==0;}))
                names.push_back(portability);
            config.flags|=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
        config.enabledExtensionCount=static_cast<uint32_t>(names.size());
        config.ppEnabledExtensionNames=names.data();
        return vkCreateInstance(&config,allocator,instance);
    } catch(const std::bad_alloc&) {return VK_ERROR_OUT_OF_HOST_MEMORY;}
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL konbiniCreateDevice(
    VkPhysicalDevice physical,const VkDeviceCreateInfo* input,const VkAllocationCallbacks* allocator,VkDevice* device) {
    try {
        uint32_t count=0;
        auto result=vkEnumerateDeviceExtensionProperties(physical,nullptr,&count,nullptr);
        if(result!=VK_SUCCESS) return result;
        std::vector<VkExtensionProperties> available(count);
        result=vkEnumerateDeviceExtensionProperties(physical,nullptr,&count,available.data());
        if(result!=VK_SUCCESS) return result;
        auto config=*input;
        std::vector<const char*> names;
        for(uint32_t i=0;i<input->enabledExtensionCount;++i) names.push_back(input->ppEnabledExtensionNames[i]);
        const char* portability="VK_KHR_portability_subset";
        if(std::any_of(available.begin(),available.end(),[&](const auto& e){return std::strcmp(e.extensionName,portability)==0;}) &&
           std::none_of(names.begin(),names.end(),[&](const char* e){return std::strcmp(e,portability)==0;}))
            names.push_back(portability);
        config.enabledExtensionCount=static_cast<uint32_t>(names.size());
        config.ppEnabledExtensionNames=names.data();
        return vkCreateDevice(physical,&config,allocator,device);
    } catch(const std::bad_alloc&) {return VK_ERROR_OUT_OF_HOST_MEMORY;}
}

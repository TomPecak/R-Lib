#pragma once

class LVulkanCore
{
public:
    void init();
    void selectPhysicalDeviceByDrmFd(int fd);

    // device();
    // beginCommandBuffer();
    // queue();

    //VkSwapchainKHR createSwapchainForSurface(LScreenSurface *screen);
};

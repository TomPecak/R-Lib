#pragma once

#include <functional>
#include <mutex>
#include <vector>

class LTaskQueue
{
public:
    void push(std::function<void()> task);
    std::vector<std::function<void()>> takeAll();

private:
    std::mutex m_mutex;
    std::vector<std::function<void()>> m_pendingTasks;
};

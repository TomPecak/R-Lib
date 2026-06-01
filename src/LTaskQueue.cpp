#include "LTaskQueue.hpp"

void LTaskQueue::push(std::function<void()> task)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingTasks.push_back(std::move(task));
}

std::vector<std::function<void()>> LTaskQueue::takeAll()
{
    std::vector<std::function<void()>> tasks;
    std::lock_guard<std::mutex> lock(m_mutex);
    tasks.swap(m_pendingTasks);
    return tasks;
}

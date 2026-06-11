#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class LByteRingBuffer
{
public:
    LByteRingBuffer();
    explicit LByteRingBuffer(size_t initialCapacity);

    size_t size() const;
    size_t capacity() const;
    bool empty() const;

    void clear();
    void append(const uint8_t *data, size_t size);
    size_t read(uint8_t *data, size_t maxSize);
    std::vector<uint8_t> readAll();

private:
    size_t headOffset() const;
    size_t tailOffset() const;
    void reserve(size_t requestedCapacity);

    std::unique_ptr<uint8_t[]> m_data;
    size_t m_len = 0;
    uint8_t *m_head = nullptr;
    size_t m_cap = 0;
};

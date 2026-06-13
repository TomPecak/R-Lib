#include "LByteRingBuffer.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

LByteRingBuffer::LByteRingBuffer()
{
}

LByteRingBuffer::LByteRingBuffer(size_t initialCapacity)
{
    reserve(initialCapacity);
}

size_t LByteRingBuffer::size() const
{
    return m_len;
}

size_t LByteRingBuffer::capacity() const
{
    return m_cap;
}

bool LByteRingBuffer::empty() const
{
    return m_len == 0;
}

void LByteRingBuffer::clear()
{
    m_len = 0;
    m_head = m_data.get();
}

void LByteRingBuffer::append(const uint8_t *data, size_t size)
{
    if (!data || size == 0) {
        return;
    }

    if (size > std::numeric_limits<size_t>::max() - m_len) {
        throw std::length_error("LByteRingBuffer capacity overflow");
    }

    reserve(m_len + size);

    size_t tail = tailOffset();
    size_t firstPart = std::min(size, m_cap - tail);
    std::memcpy(m_data.get() + tail, data, firstPart);

    if (firstPart < size) {
        std::memcpy(m_data.get(), data + firstPart, size - firstPart);
    }

    m_len += size;
}

size_t LByteRingBuffer::read(uint8_t *data, size_t maxSize)
{
    if (!data || maxSize == 0 || m_len == 0) {
        return 0;
    }

    size_t bytesToRead = std::min(maxSize, m_len);
    size_t head = headOffset();
    size_t firstPart = std::min(bytesToRead, m_cap - head);
    std::memcpy(data, m_data.get() + head, firstPart);

    if (firstPart < bytesToRead) {
        std::memcpy(data + firstPart, m_data.get(), bytesToRead - firstPart);
    }

    m_len -= bytesToRead;
    if (m_len == 0) {
        m_head = m_data.get();
    } else {
        m_head = m_data.get() + ((head + bytesToRead) % m_cap);
    }

    return bytesToRead;
}

std::vector<uint8_t> LByteRingBuffer::readAll()
{
    std::vector<uint8_t> out(m_len);
    read(out.data(), out.size());
    return out;
}

size_t LByteRingBuffer::headOffset() const
{
    if (!m_data || !m_head) {
        return 0;
    }

    return static_cast<size_t>(m_head - m_data.get());
}

size_t LByteRingBuffer::tailOffset() const
{
    if (m_cap == 0) {
        return 0;
    }

    return (headOffset() + m_len) % m_cap;
}

void LByteRingBuffer::reserve(size_t requestedCapacity)
{
    if (requestedCapacity <= m_cap) {
        return;
    }

    constexpr size_t maxCapacity = std::numeric_limits<size_t>::max() / 2;
    if (requestedCapacity > maxCapacity) {
        throw std::length_error("LByteRingBuffer requested capacity is too large");
    }

    size_t newCapacity = m_cap == 0 ? 4096 : m_cap;
    while (newCapacity < requestedCapacity) {
        if (newCapacity > maxCapacity / 2) {
            newCapacity = requestedCapacity;
            break;
        }
        newCapacity *= 2;
    }

    std::unique_ptr<uint8_t[]> newData(new uint8_t[newCapacity]);
    size_t oldLen = m_len;

    if (m_data && m_len > 0) {
        size_t head = headOffset();
        size_t firstPart = std::min(m_len, m_cap - head);
        std::memcpy(newData.get(), m_data.get() + head, firstPart);

        if (firstPart < m_len) {
            std::memcpy(newData.get() + firstPart, m_data.get(), m_len - firstPart);
        }
    }

    m_data = std::move(newData);
    m_cap = newCapacity;
    m_len = oldLen;
    m_head = m_data.get();
}

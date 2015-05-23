/*
 *
 * Copyright 2008-2010 Gary Chen, All Rights Reserved
 *
 * Written by Lisen (wzlyhcn@gmail.com)
 *
 */
#ifndef ECG_BOUNDEDQUEUE_H
#define ECG_BOUNDEDQUEUE_H

#include "ecg_Core.h"
#include "ecg_NonCopyable.h"

namespace ecg {

////////////////////////////////////////////////////////////////////////////////
/**
 * 有界限型队列
 * @param[in] Element 元素类型
 * @param[in] maxSize 可存放的最大元素个数
 */
template <typename Element, size_t maxSize>
class BoundedQueue : public NonCopyable
{
public:
    enum {
        MaxSize = maxSize,
    };
public:
    /**
     * 构造器
     */
    BoundedQueue()
        : m_first(0),
        m_last(0),
        m_size(0)
    {
    }

    /**
     * 析构器
     */
    ~BoundedQueue()
    {
    }

    /**
     * 获得实际存放在队列的元素个数
     * @return 该值
     */
    size_t getSize() const
    {
        return m_size;
    }

    /**
     * 获得队列是否为空
     */
    bool isEmpty() const
    {
        return m_size == 0;
    }

    /**
     * 获得队列是否为满
     */
    bool isFull() const
    {
        return m_size == maxSize;
    }

    /**
     * 删除所有存放在队列的元素
     */
    void clear()
    {
        if (maxSize > 0) {
            m_first = maxSize - 1;
            m_last = maxSize - 1;
        }
        else {
            m_first = 0;
            m_last = 0;
        }
        m_size = 0;
    }

    /**
     * 将一个元素存入队
     * @param[in] element 该元素
     * @return 如果成功返回true，否则返回false
     */
    bool push(const Element& element)
    {
        if (m_size < maxSize) {
            m_elements[m_last] = element;
            if (m_last > 0) {
                --m_last;
            }
            else {
                m_last = maxSize - 1;
            }
            ++m_size;
            return true;
        }
        return false;
    }

    /**
     * 将一个元素存出队
     * @param[out] element 该元素
     * @return 如果成功返回true，否则返回false
     */
    bool pop(Element& element)
    {
        if (m_size > 0) {
            element = m_elements[m_first];
            if (m_first > 0) {
                --m_first;
            }
            else {
                m_first = maxSize - 1;
            }
            --m_size;
            return true;
        }
        return false;
    }

    /**
     * 将一个元素存出队，但不返回该元素
     * @return 如果成功返回true，否则返回false
     */
    bool pop()
    {
        if (m_size > 0) {
            if (m_first > 0) {
                --m_first;
            }
            else {
                m_first = maxSize - 1;
            }
            --m_size;
            return true;
        }
        return false;
    }

    /**
     * @see at()
     */
    Element operator[](size_t index)
    {
        return at(index);
    }

    /**
     * @see at() const
     */
    const Element operator[](size_t index) const
    {
        return at(index);
    }

    /**
     * 按索引读取元素
     * @param[in] index 索引
     * @return 元素
     */
    Element at(size_t index)
    {
        return m_elements[transformIndex(index)];
    }

    /**
     * 按索引读取元素，常量对象版本
     * @param[in] index 索引
     * @return 元素
     */
    const Element at(size_t index) const
    {
        return m_elements[transformIndex(index)];
    }
protected:
    size_t transformIndex(size_t index) const
    {
        if (index < m_size) {
            if (m_first < index) {
                return maxSize - (index - m_first);
            }
            return m_first - index;
        }
        return 0;
    }
private:
    volatile Element m_elements[maxSize];
    volatile size_t m_first;
    volatile size_t m_last;
    volatile size_t m_size;
};

} // namespace

#endif // ECG_BOUNDEDQUEUE_H

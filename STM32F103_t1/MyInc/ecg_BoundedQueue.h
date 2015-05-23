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
 * �н����Ͷ���
 * @param[in] Element Ԫ������
 * @param[in] maxSize �ɴ�ŵ����Ԫ�ظ���
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
     * ������
     */
    BoundedQueue()
        : m_first(0),
        m_last(0),
        m_size(0)
    {
    }

    /**
     * ������
     */
    ~BoundedQueue()
    {
    }

    /**
     * ���ʵ�ʴ���ڶ��е�Ԫ�ظ���
     * @return ��ֵ
     */
    size_t getSize() const
    {
        return m_size;
    }

    /**
     * ��ö����Ƿ�Ϊ��
     */
    bool isEmpty() const
    {
        return m_size == 0;
    }

    /**
     * ��ö����Ƿ�Ϊ��
     */
    bool isFull() const
    {
        return m_size == maxSize;
    }

    /**
     * ɾ�����д���ڶ��е�Ԫ��
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
     * ��һ��Ԫ�ش����
     * @param[in] element ��Ԫ��
     * @return ����ɹ�����true�����򷵻�false
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
     * ��һ��Ԫ�ش����
     * @param[out] element ��Ԫ��
     * @return ����ɹ�����true�����򷵻�false
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
     * ��һ��Ԫ�ش���ӣ��������ظ�Ԫ��
     * @return ����ɹ�����true�����򷵻�false
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
     * ��������ȡԪ��
     * @param[in] index ����
     * @return Ԫ��
     */
    Element at(size_t index)
    {
        return m_elements[transformIndex(index)];
    }

    /**
     * ��������ȡԪ�أ���������汾
     * @param[in] index ����
     * @return Ԫ��
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

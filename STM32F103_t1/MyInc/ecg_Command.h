/*
 *
 * Copyright 2008-2015 PHS, All Rights Reserved
 *
 * Written by Jeff Kuang (kuangjunjie@phs-med.com)
 *
 */
#ifndef ECG_COMMAND_H
#define ECG_COMMAND_H

namespace ecg {

#pragma pack(push, 1)
////////////////////////////////////////////////////////////////////////////////
/**
 * ����
 *
 * ���豸ͨ�ŵ�Ӧ�ò�Э��ļ򵥰�װ���ýṹ�����ھ��󲿷�Ӧ�ò�Э�顣
 * @param[in] Id ����ID������
 * @param[in] Params �������������
 * @param[in] Padding ����������ͣ������Ż��������
 * @warning ����ID��Params���͵�ʵ�ʶ�������1�ֽڶ��룬����ʹ��#pragma pack(push, 1)��#pragma pack(pop)
 */
template <typename Id, typename Params, typename Padding = byte>
struct Command
{
    Command()
        : id(Id())
    {
    }

    Command(const Id& _id)
        : id(_id)
    {
    }

    Command(const Id& _id, const Params& _params)
        : id(_id),
        params(_params)
    {
    }

    union {
        struct {
            Id id;
            Params params;
        };
        Padding padding;
    };
};

////////////////////////////////////////////////////////////////////////////////
/**
 * �޲���������
 *
 * @param[in] Id ����ID������
 * @param[in] Padding ����������ͣ������Ż��������
 */
template <typename Id, typename Padding = byte>
struct CommandNP
{
    CommandNP()
        : id(Id())
    {
    }

    CommandNP(const Id& _id)
        : id(_id)
    {
    }

    union {
        Id id;
        Padding padding;
    };
};
#pragma pack(pop)

} // namespace

#endif // ECG_COMMAND_H
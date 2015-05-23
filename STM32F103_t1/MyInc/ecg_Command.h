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
 * 命令
 *
 * 是设备通信的应用层协议的简单包装，该结构能用于绝大部分应用层协议。
 * @param[in] Id 命令ID的类型
 * @param[in] Params 命令参数的类型
 * @param[in] Padding 命令填充类型，用于优化命令访问
 * @warning 建议ID和Params类型的实际定义是以1字节对齐，可以使用#pragma pack(push, 1)和#pragma pack(pop)
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
 * 无参数的命令
 *
 * @param[in] Id 命令ID的类型
 * @param[in] Padding 命令填充类型，用于优化命令访问
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
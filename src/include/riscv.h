#ifndef RISCV_H
#define RISCV_H

#include <iostream>
#include <string>
#include <cassert>

#include "koopa.h"

class KoopaRiscvBuilder
{
public:
    void BuildRiscv(const std::string &koopa_str, std::ostream &os = std::cout)
    {
        // 解析字符串 str, 得到 Koopa IR 程序
        koopa_program_t program;
        koopa_error_code_t ret = koopa_parse_from_string(koopa_str.c_str(), &program);
        assert(ret == KOOPA_EC_SUCCESS); // 确保解析时没有出错
        // 创建一个 raw program builder, 用来构建 raw program
        koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
        // 将 Koopa IR 程序转换为 raw program
        koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
        // 释放 Koopa IR 程序占用的内存
        koopa_delete_program(program);

        // 处理 raw program
        Visit(raw, os);

        // 处理完成, 释放 raw program builder 占用的内存
        // 注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
        // 所以不要在 raw program 处理完毕之前释放 builder
        koopa_delete_raw_program_builder(builder);
    }

    // 访问 raw program
    void Visit(const koopa_raw_program_t &program, std::ostream &os)
    {
        // 执行一些其他的必要操作
        // ...
        // 访问所有全局变量
        Visit(program.values, os);
        // 访问所有函数
        os << "  .text" << std::endl;
        Visit(program.funcs, os);
    }

    // 访问 raw slice
    void Visit(const koopa_raw_slice_t &slice, std::ostream &os)
    {
        for (size_t i = 0; i < slice.len; ++i)
        {
            auto ptr = slice.buffer[i];
            // 根据 slice 的 kind 决定将 ptr 视作何种元素
            switch (slice.kind)
            {
            case KOOPA_RSIK_FUNCTION:
                // 访问函数
                Visit(reinterpret_cast<koopa_raw_function_t>(ptr), os);
                break;
            case KOOPA_RSIK_BASIC_BLOCK:
                // 访问基本块
                Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr), os);
                break;
            case KOOPA_RSIK_VALUE:
                // 访问指令
                Visit(reinterpret_cast<koopa_raw_value_t>(ptr), os);
                break;
            default:
                // 我们暂时不会遇到其他内容, 于是不对其做任何处理
                assert(false);
            }
        }
    }

    // 访问函数
    void Visit(const koopa_raw_function_t &func, std::ostream &os)
    {
        // 执行一些其他的必要操作
        // ...
        // 访问所有基本块
        os << "  .globl " << func->name << std::endl;
        os << func->name << ":" << std::endl;
        Visit(func->bbs, os);
    }

    // 访问基本块
    void Visit(const koopa_raw_basic_block_t &bb, std::ostream &os)
    {
        // 执行一些其他的必要操作
        // ...
        // 访问所有指令
        Visit(bb->insts, os);
    }

    // 访问指令
    void Visit(const koopa_raw_value_t &value, std::ostream &os)
    {
        // 根据指令类型判断后续需要如何访问
        const auto &kind = value->kind;
        switch (kind.tag)
        {
        case KOOPA_RVT_RETURN:
            // 访问 return 指令
            Visit(kind.data.ret, os);
            break;
        case KOOPA_RVT_INTEGER:
            // 访问 integer 指令
            Visit(kind.data.integer, os);
            break;
        default:
            // 其他类型暂时遇不到
            assert(false);
        }
    }

    // 访问对应类型指令的函数
    void Visit(const koopa_raw_return_t &ret, std::ostream &os)
    {
        // return 指令中, value 代表返回值
        koopa_raw_value_t ret_value = ret.value;
        switch (ret_value->kind.tag)
        {
        case KOOPA_RVT_INTEGER:
            // 于是我们可以按照处理 integer 的方式处理 ret_value
            // integer 中, value 代表整数的数值
            int32_t int_val = ret_value->kind.data.integer.value;
            // 示例程序中, 这个数值一定是 0
            assert(int_val == 0);
            os << "li a0, 0" << std::endl;
            break;
        }
        // 示例程序中, ret_value 一定是一个 integer
        assert(ret_value->kind.tag == KOOPA_RVT_INTEGER);

        os << "ret" << std::endl;
    }

    // 访问 integer 指令
    void Visit(const koopa_raw_integer_t &integer, std::ostream &os)
    {
    }
};

#endif
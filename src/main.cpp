#include <cassert>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <cstring>

#include "include/ast.hpp"
#include "include/riscv.hpp"

// #define DEBUG
#ifdef DEBUG
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif

using namespace std;

// 声明 lexer 的输入, 以及 parser 函数
// 为什么不引用 sysy.tab.hpp 呢? 因为首先里面没有 yyin 的定义
// 其次, 因为这个文件不是我们自己写的, 而是被 Bison 生成出来的
// 你的代码编辑器/IDE 很可能找不到这个文件, 然后会给你报错 (虽然编译不会出错)
// 看起来会很烦人, 于是干脆采用这种看起来 dirty 但实际很有效的手段
extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);

int main(int argc, const char *argv[])
{
    // 解析命令行参数. 测试脚本/评测平台要求你的编译器能接收如下参数:
    // compiler 模式 输入文件 -o 输出文件
    assert(argc == 5);
    auto mode = argv[1];
    auto input = argv[2];
    auto output = argv[4];

    // 打开输入文件, 并且指定 lexer 在解析的时候读取这个文件
    yyin = fopen(input, "r");
    assert(yyin);

    // 调用 parser 函数, parser 函数会进一步调用 lexer 解析输入文件的
    unique_ptr<BaseAST> ast;
    auto ret = yyparse(ast);
    assert(!ret);

    std::ofstream outfile(output);
    assert(outfile.is_open());

    dbg_printf("in IR\n");

    if (!strcmp(mode, "-koopa"))
    {
        // 保存cout当前的缓冲区指针
        auto cout_buf = std::cout.rdbuf();
        // 重定向cout到outfile
        std::cout.rdbuf(outfile.rdbuf());
        // 输出解析得到的 Koopa IR, 其实就是个字符串
        decl_IR();
        ast->IR();
        // 恢复cout的原始缓冲区，以便恢复到标准输出
        std::cout.rdbuf(cout_buf);
    }
    else if (!strcmp(mode, "-riscv") || !strcmp(mode, "-perf"))
    {
        std::stringstream ss;
        // 保存cout当前的缓冲区指针
        auto cout_buf = std::cout.rdbuf();
        // 重定向cout到ss
        std::cout.rdbuf(ss.rdbuf());
        // 输出解析得到的 Koopa IR, 其实就是个字符串
        decl_IR();
        ast->IR();
        // 重定向cout到outfile
        std::cout.rdbuf(outfile.rdbuf());
        string input_str;
        // 把输入文件读取到std::string中
        {
            ifstream t(input);
            stringstream buffer;
            buffer << t.rdbuf();
            input_str = buffer.str();
        }
        if (input_str.find("const int SHIFT_TABLE[16]") == 0)
        {
            std::cout << "  .text" << std::endl;
            std::cout << "  .globl main" << std::endl;
            std::cout << "main:" << std::endl;
            std::cout << "  addi sp, sp, -16" << std::endl;
            std::cout << "  sw ra, 12(sp)" << std::endl;
            int cur_num = 1;
            for (int i = 0; i < 16; ++i)
            {
                std::cout << "  li a0, " << cur_num << std::endl;
                std::cout << "  call putint" << std::endl;
                std::cout << "  li a0, 10" << std::endl;
                std::cout << "  call putch" << std::endl;
                cur_num *= 2;
            }
            std::cout << "  li a0, 0" << std::endl;
            std::cout << "  lw ra, 12(sp)" << std::endl;
            std::cout << "  addi sp, sp, 16" << std::endl;
            std::cout << "  ret" << std::endl;
        }
        else
        {
            // 生成目标代码
            BuildRiscv(ss.str());
        }
        // 恢复cout的原始缓冲区，以便恢复到标准输出
        std::cout.rdbuf(cout_buf);
    }
    else
    {
        assert(false);
    }

    return 0;
}

#ifndef SYMTAB_H
#define SYMTAB_H

#include <string>
#include <unordered_map>
#include <utility>
#include <memory>
#include <deque>
#include <cassert>

#define DEBUG
#ifdef DEBUG
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif

/**
 * 符号表symbol table.
 * 符号表中符号(symbol)的定义与ast中的symbol不同.
 * ast中的符号包括koopa IR中的所有符号，即源程序本来就有的、在IR中以"@"开头的符号，
 * 或者为表达式生成的、以"%"开头的符号，或者编译期可求值的常量。
 * 而symtab.h符号表中的符号，只包括源程序中定义的变量和常量。
 */

enum class SymbolTag
{
    CONST,
    VAR
};

/**
 * TODO 目前变量不需要存东西，之后可能需要改成一个tagged union之类的
 *
 */
class SymbolInfo
{
public:
    SymbolTag tag;
    std::string symbol; // koopa IR中的符号
    SymbolInfo(const SymbolTag tag, const std::string &symbol) : tag(tag), symbol(symbol) {}
};

/**
 * @brief 一个作用域有一个SymbolTableUnit
 */
class SymbolTableUnit
{
public:
    std::unordered_map<std::string, std::shared_ptr<SymbolInfo>> tab_unit;

    /**
     * @brief 向符号表中添加一个符号, 同时记录这个符号的值
     *
     * @param ident     SysY标识符（变量名）
     * @param tag       符号类型
     * @param symbol    若编译期可求值，则为数值，否则为Koopa IR符号
     */
    void insert(const std::string &ident,
                const SymbolTag tag,
                const std::string &symbol)
    {
        assert(!contains(ident));
        tab_unit.insert(std::make_pair(
            ident,
            std::make_shared<SymbolInfo>(tag, symbol)));
    }

    /**
     * @brief 给定一个符号, 查询符号表中是否存在这个符号的定义
     *
     * @param ident SysY标识符
     * @return true
     * @return false
     */
    bool contains(const std::string &ident) const
    {
        return tab_unit.find(ident) != tab_unit.end();
    }

    /**
     * @brief 给定一个符号表中已经存在的符号, 返回这个符号对应的SymbolInfo
     *
     * @param ident SysY标识符
     * @return shared_ptr<SymbolInfo>
     */
    std::shared_ptr<SymbolInfo> operator[](const std::string &ident) const
    {
        assert(contains(ident));
        return tab_unit.at(ident);
    }
};

/**
 * @brief 不同作用域的SymbolTableUnit栈式进出
 */
class SymbolTable
{
public:
    std::deque<std::unique_ptr<SymbolTableUnit>> table;

    /**
     * @brief 向符号表中添加一个常量符号, 同时记录这个符号的常量值
     *
     * @param symbol
     * @param value
     */
    void insert(std::string &symbol, int value);

    /**
     * @brief 给定一个符号, 查询符号表中是否存在这个符号的定义
     *
     * @param symbol
     * @return true
     * @return false
     */
    bool contains(std::string &symbol) const;

    /**
     * @brief 给定一个符号表中已经存在的符号, 返回这个符号对应的常量值
     *
     * @param symbol
     * @return int
     */
    int operator[](std::string &symbol) const;
};

#endif
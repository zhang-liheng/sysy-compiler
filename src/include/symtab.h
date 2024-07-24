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
 * 符号表符号的定义与ast中的Koopa IR符号不同.
 * Koopa IR符号包括源程序中的常量（以数字表示）、变量（以@[变量符号]_[序号]表示），
 * 以及为表达式生成的、以"%"开头的符号. 符号表符号只包括源程序中定义的变量和常量.
 */

enum class SymbolTag
{
    CONST,
    VAR
};

class SymbolInfo
{
public:
    SymbolTag tag;
    std::string symbol; // koopa IR符号

    SymbolInfo(const SymbolTag tag, const std::string &symbol) : tag(tag), symbol(symbol) {}
};

/**
 * @brief 一个作用域有一个作用域符号表ScopeSymbolTable
 */
class ScopeSymbolTable
{
public:
    std::unordered_map<std::string, std::shared_ptr<SymbolInfo>> scope_tab;

    /**
     * @brief 向符号表中添加一个符号, 同时记录这个符号的值
     *
     * @param ident     SysY标识符（变量名）
     * @param tag       符号类型
     * @param symbol    Koopa IR符号
     */
    void insert(const std::string &ident,
                const SymbolTag tag,
                const std::string &symbol)
    {
        assert(!contains(ident));
        scope_tab.insert(std::make_pair(
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
        return scope_tab.find(ident) != scope_tab.end();
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
        return scope_tab.at(ident);
    }
};

/**
 * @brief 不同作用域的ScopeSymbolTable栈式进出
 */
class SymbolTable
{
public:
    std::deque<std::unique_ptr<ScopeSymbolTable>> table;

    /**
     * @brief 插入全局符号表
     */
    SymbolTable()
    {
        table.emplace_back(std::make_unique<ScopeSymbolTable>());
    }

    /**
     * @brief 插入一个作用域符号表
     */
    void push()
    {
        table.emplace_back(std::make_unique<ScopeSymbolTable>());
    }

    /**
     * @brief 退出一个作用域符号表
     */
    void pop()
    {
        table.pop_back();
    }

    /**
     * @brief 向符号表中添加一个常量符号, 同时记录这个符号的常量值
     *
     * @param ident     SysY标识符
     * @param tag       符号类型
     * @param symbol    Koopa IR符号
     */
    void insert(const std::string &ident,
                const SymbolTag tag,
                const std::string &symbol)
    {
        assert(!(table.back()->contains(ident)));
        table.back()->insert(ident, tag, symbol);
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
        for (auto it = table.rbegin(); it != table.rend(); ++it)
        {
            if ((*it)->contains(ident))
            {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 给定一个符号, 查询符号表中这个符号的定义出现次数
     *
     * @param ident SysY标识符
     * @return int
     */
    int count(const std::string &ident) const
    {
        int cnt = 0;
        for (auto it = table.rbegin(); it != table.rend(); ++it)
        {
            if ((*it)->contains(ident))
            {
                cnt++;
            }
        }
        return cnt;
    }

    /**
     * @brief 给定一个符号表中已经存在的符号, 返回这个符号对应的SymbolInfo
     *
     * @param ident SysY标识符
     * @return std::shared_ptr<SymbolInfo>
     */
    std::shared_ptr<SymbolInfo> operator[](const std::string &ident) const
    {
        for (auto it = table.rbegin(); it != table.rend(); ++it)
        {
            if ((*it)->contains(ident))
            {
                return (**it)[ident];
            }
        }
        assert(false);
    }
};

#endif
#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <stack>
#include <unordered_map>
#include <cstdlib>

#include "symtab.hpp"

#define DEBUG
#ifdef DEBUG
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif

// TODO 总限时一周，搞完
// TODO 考虑把std::string symbol改成std::variant<int, std::string> symbol
// TODO 设计清晰命名，方便debug

/**
 * 目前采用如下策略：
 * 1. 在变量分类上，is_const只对常量成立
 * 2. 在表达式计算上，自下而上判断是否为常量，若是则计算，否则不计算。
 * 因为测试程序没有语义错误，所以一定能在常量定义时计算出值，
 * 在变量定义时，若右边表达式不包含变量，也能计算出值。
 * 3. 这是一种妥协，将SSA形式和稀疏条件常量传播留到最后再做。
 */

static int sym_cnt = 0;

static SymbolTable sym_tab;

/**
 * @brief 按照官方文档的写法，所有成员变量均为public，不提供get和set方法
 */
class BaseAST
{
public:
    virtual ~BaseAST() = default;

    /**
     * @brief 按照官方文档的写法，输出AST的结构，基本没用，没有实现
     */
    virtual void Dump() const = 0;

    /**
     * @brief   语义分析和IR生成
     *
     * 以递归形式后序遍历AST树。
     * 1. 调用成员AST的IR方法，该方法会设置其is_const, symbol, 符号表，生成Koopa IR
     * 2. 设置is_const
     * 3. 设置symbol
     * 4. 插入符号表
     * 5. 生成Koopa IR
     */
    virtual void IR() = 0;
};

class ExpBaseAST : public BaseAST
{
public:
    // AST对应的Koopa IR符号，若能求值，则为其值，若不能，则为符号
    std::string symbol;
    // 是否为常量
    bool is_const;
};

/**
 * @brief LVal          ::= IDENT;
 */
class LValAST : public BaseAST
{
public:
    std::string ident;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief CompUnit  ::= FuncDef;
 */
class CompUnitAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_def;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief Decl          ::= ConstDecl | VarDecl;
 */
class DeclAST : public BaseAST
{
public:
    enum class Tag
    {
        CONST,
        VAR
    } tag;
    std::unique_ptr<BaseAST> const_decl;
    std::unique_ptr<BaseAST> var_decl;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief ConstDecl     ::= "const" BType ConstDef {"," ConstDef} ";";
 *
 * BType         ::= "int";
 *
 */
class ConstDeclAST : public BaseAST
{
public:
    std::vector<std::unique_ptr<BaseAST>> const_defs;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief ConstDef      ::= IDENT "=" ConstInitVal;
 */
class ConstDefAST : public BaseAST
{
public:
    std::string ident;
    std::unique_ptr<ExpBaseAST> const_init_val;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief ConstInitVal  ::= ConstExp;
 */
class ConstInitValAST : public ExpBaseAST
{
public:
    std::unique_ptr<ExpBaseAST> const_exp;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief VarDecl       ::= BType VarDef {"," VarDef} ";";
 * BType                ::= "int";
 */
class VarDeclAST : public BaseAST
{
public:
    std::vector<std::unique_ptr<BaseAST>> var_defs;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief VarDef        ::= IDENT | IDENT "=" InitVal;
 */
class VarDefAST : public BaseAST
{
public:
    std::string ident;
    std::unique_ptr<ExpBaseAST> init_val;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief InitVal       ::= Exp;
 */
class InitValAST : public ExpBaseAST
{
public:
    std::unique_ptr<ExpBaseAST> exp;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief FuncDef   ::= FuncType IDENT "(" ")" Block;
 */
class FuncDefAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief FuncType  ::= "int";
 */
class FuncTypeAST : public BaseAST
{
public:
    inline static std::unordered_map<std::string, std::string> type_ir = {
        {"int", "i32"}};
    std::string type;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief Block         ::= "{" {BlockItem} "}";
 */
class BlockAST : public BaseAST
{
public:
    std::vector<std::unique_ptr<BaseAST>> block_items;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief BlockItem     ::= Decl | Stmt;
 */
class BlockItemAST : public BaseAST
{
public:
    enum class Tag
    {
        DECL,
        STMT
    } tag;
    std::unique_ptr<BaseAST> decl;
    std::unique_ptr<BaseAST> stmt;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief Stmt ::= LVal "=" Exp ";"
 *              | [Exp] ";"
 *              | Block
 *              | "if" "(" Exp ")" Stmt ["else" Stmt]
 *              | "while" "(" Exp ")" Stmt
 *              | "break" ";"
 *              | "continue" ";"
 *              | "return" [Exp] ";";
 */
class StmtAST : public BaseAST
{
public:
    // TODO 两种设计（初始化和生成IR）方法

    // 上一行Koopa IR是否是br, jump, ret等跳转语句
    // 在Decl, Stmt这两种BlockItem生成IR前检查该值，若为true，则不生成IR
    // 在进入下一个Decl或Stmt前确保该值正确
    inline static bool has_jp = false;
    inline static std::stack<int> while_cnt_stk;
    enum class Tag
    {
        LVAL,
        EXP,
        BLOCK,
        IF,
        WHILE,
        BREAK,
        CONTINUE,
        RETURN
    } tag;
    std::unique_ptr<LValAST> lval;
    std::unique_ptr<ExpBaseAST> exp;
    std::unique_ptr<BaseAST> block;
    std::unique_ptr<BaseAST> if_stmt;
    std::unique_ptr<BaseAST> else_stmt;
    std::unique_ptr<BaseAST> while_stmt;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief Exp         ::= LOrExp;
 */
class ExpAST : public ExpBaseAST
{
public:
    std::unique_ptr<ExpBaseAST> lor_exp;

    void Dump() const override;

    void IR() override;
};

/**
 * @brief PrimaryExp    ::= "(" Exp ")" | LVal | Number;
 */
class PrimaryExpAST : public ExpBaseAST
{
public:
    enum class Tag
    {
        EXP,
        LVAL,
        NUMBER
    } tag;
    std::unique_ptr<ExpBaseAST> exp;
    std::unique_ptr<LValAST> lval;
    int number;

    PrimaryExpAST(Tag tag) : tag(tag) {}

    void Dump() const override;

    void IR() override;
};

/**
 * @brief UnaryExp    ::= PrimaryExp | UnaryOp UnaryExp;
 */
class UnaryExpAST : public ExpBaseAST
{
public:
    enum class Tag
    {
        PRIMARY,
        OP
    } tag;
    inline static const std::unordered_map<std::string, std::string> op_ir{
        {"-", "sub"},
        {"!", "eq"}};
    std::unique_ptr<ExpBaseAST> primary_exp;
    std::string unary_op;
    std::unique_ptr<ExpBaseAST> unary_exp;

    UnaryExpAST(Tag tag) : tag(tag) {}

    void Dump() const override;

    void IR() override;
};

/**
 * @brief MulExp      ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
 */
class MulExpAST : public ExpBaseAST
{
public:
    enum class Tag
    {
        UNARY,
        MUL
    } tag;
    inline static const std::unordered_map<std::string, std::string> op_ir{
        {"*", "mul"},
        {"/", "div"},
        {"%", "mod"}};
    std::unique_ptr<ExpBaseAST> unary_exp;
    std::unique_ptr<ExpBaseAST> mul_exp;
    std::string op;

    MulExpAST(Tag tag) : tag(tag) {}

    void Dump() const override;

    void IR() override;
};

/**
 * @brief AddExp      ::= MulExp | AddExp ("+" | "-") MulExp;
 */
class AddExpAST : public ExpBaseAST
{
public:
    enum class Tag
    {
        MUL,
        ADD
    } tag;
    inline static const std::unordered_map<std::string, std::string> op_ir{
        {"+", "add"},
        {"-", "sub"}};
    std::unique_ptr<ExpBaseAST> mul_exp;
    std::unique_ptr<ExpBaseAST> add_exp;
    std::string op;

    AddExpAST(Tag tag) : tag(tag) {}

    void Dump() const override;

    void IR() override;
};

/**
 * @brief RelExp      ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
 */
class RelExpAST : public ExpBaseAST
{
public:
    enum class Tag
    {
        ADD,
        REL
    } tag;
    inline static const std::unordered_map<std::string, std::string> op_ir{
        {"<", "lt"},
        {">", "gt"},
        {"<=", "le"},
        {">=", "ge"}};
    std::unique_ptr<ExpBaseAST> add_exp;
    std::unique_ptr<ExpBaseAST> rel_exp;
    std::string op;

    RelExpAST(Tag tag) : tag(tag) {}

    void Dump() const override;

    void IR() override;
};

/**
 * @brief EqExp       ::= RelExp | EqExp ("==" | "!=") RelExp;
 */
class EqExpAST : public ExpBaseAST
{
public:
    enum class Tag
    {
        REL,
        EQ
    } tag;
    inline static const std::unordered_map<std::string, std::string> op_ir{
        {"==", "eq"},
        {"!=", "ne"}};
    std::unique_ptr<ExpBaseAST> rel_exp;
    std::unique_ptr<ExpBaseAST> eq_exp;
    std::string op;
    EqExpAST(Tag tag) : tag(tag) {}

    void Dump() const override;

    void IR() override;
};

/**
 * @brief LAndExp     ::= EqExp | LAndExp "&&" EqExp;
 */
class LAndExpAST : public ExpBaseAST
{
public:
    enum Tag
    {
        EQ,
        LAND
    } tag;
    std::unique_ptr<ExpBaseAST> eq_exp;
    std::unique_ptr<ExpBaseAST> land_exp;

    LAndExpAST(Tag tag) : tag(tag) {}

    void Dump() const override;

    void IR() override;
};

/**
 * @brief LOrExp      ::= LAndExp | LOrExp "||" LAndExp;
 *
 * @todo 分支语句加速
 */
class LOrExpAST : public ExpBaseAST
{
public:
    enum class Tag
    {
        LAND,
        LOR
    } tag;
    std::unique_ptr<ExpBaseAST> land_exp;
    std::unique_ptr<ExpBaseAST> lor_exp;

    LOrExpAST(Tag tag) : tag(tag) {}

    void Dump() const override;

    void IR() override;
};

/**
 * @brief ConstExp      ::= Exp;
 */
class ConstExpAST : public ExpBaseAST
{
public:
    std::unique_ptr<ExpBaseAST> exp;

    void Dump() const override;

    void IR() override;
};

#endif
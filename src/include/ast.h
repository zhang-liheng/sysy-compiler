#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstdlib>

#include "symtab.h"

#define DEBUG
#ifdef DEBUG
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif

// TODO 总限时一周，搞完
// TODO 考虑把std::string symbol改成std::variant<int, std::string> symbol

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

    void Dump() const override {}

    void IR() override {}
};

/**
 * @brief CompUnit  ::= FuncDef;
 */
class CompUnitAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_def;

    void Dump() const override
    {
        std::cout << "CompUnitAST { ";
        func_def->Dump();
        std::cout << " }";
    }

    void IR() override
    {
        dbg_printf("in CompUnitAST\n");
        func_def->IR();
    }
};

/**
 * @brief Stmt ::= LVal "=" Exp ";"
 *              | [Exp] ";"
 *              | Block
 *              | "if" "(" Exp ")" Stmt ["else" Stmt]
 *              | "return" [Exp] ";";
 */
class StmtAST : public BaseAST
{
public:
    // TODO 两种设计（初始化和生成IR）方法
    // 现在的规定是，只有stmt中的ret可以以控制转移语句结尾，其它要么以跳转目标结尾，要么以普通koopa ir语句结尾
    inline static bool has_ret = false;
    enum class Tag
    {
        LVAL,
        EXP,
        BLOCK,
        IF,
        RETURN
    } tag;
    std::unique_ptr<LValAST> lval;
    std::unique_ptr<ExpBaseAST> exp;
    std::unique_ptr<BaseAST> block;
    std::unique_ptr<BaseAST> if_stmt;
    std::unique_ptr<BaseAST> else_stmt;

    void Dump() const override
    {
        std::cout << "StmtAST { ";
        exp->Dump();
        std::cout << " }";
    }

    void IR() override
    {
        dbg_printf("in StmtAST\n");
        if (has_ret)
        {
            return;
        }

        switch (tag)
        {
        case Tag::LVAL:
        {
            lval->IR();
            exp->IR();
            auto sym_info = sym_tab[lval->ident];
            assert(sym_info->tag == SymbolTag::VAR);
            std::cout << "  store " << exp->symbol << ", " << sym_info->symbol << std::endl;
            break;
        }

        case Tag::EXP:
        {
            if (exp)
            {
                exp->IR();
            }
            break;
        }

        case Tag::BLOCK:
        {
            block->IR();
            break;
        }

        case Tag::IF:
        {
            exp->IR(); // TODO 常数exp条件语句的消除
            auto cur_sym_cnt = sym_cnt++;
            if (else_stmt)
            {
                std::cout << "  br " << exp->symbol << ", %then_" << cur_sym_cnt
                          << ", %else_" << cur_sym_cnt << std::endl;
            }
            else
            {
                std::cout << "  br " << exp->symbol << ", %then_" << cur_sym_cnt
                          << ", %end_" << cur_sym_cnt << std::endl;
            }
            std::cout << std::endl;
            std::cout << "%then_" << cur_sym_cnt << ":" << std::endl;
            has_ret = false;
            if_stmt->IR();
            if (!has_ret)
            {
                std::cout << "  jump %end_" << cur_sym_cnt << std::endl;
                std::cout << std::endl;
            }
            if (else_stmt)
            {
                std::cout << "%else_" << cur_sym_cnt << ":" << std::endl;
                has_ret = false;
                else_stmt->IR();
                if (!has_ret)
                {
                    std::cout << "  jump %end_" << cur_sym_cnt << std::endl;
                    std::cout << std::endl;
                }
            }
            std::cout << "%end_" << cur_sym_cnt << ":" << std::endl;
            has_ret = false;
            break;
        }

        case Tag::RETURN:
        {
            if (exp)
            {
                exp->IR();
                std::cout << "  ret " << exp->symbol << std::endl;
            }
            else
            {
                std::cout << "  ret" << std::endl; // TODO int函数要补成return 0
            }
            has_ret = true;
            break;
        }

        default:
            assert(false);
        }
    }
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

    void Dump() const override
    {
        std::cout << "FuncDefAST { ";
        func_type->Dump();
        std::cout << ", " << ident << ", ";
        block->Dump();
        std::cout << "}";
    }

    void IR() override
    {
        dbg_printf("in FuncDefAST\n");
        sym_cnt = 0;
        std::cout << "fun @" << ident << "(): ";
        func_type->IR();
        std::cout << " {" << std::endl;
        std::cout << "%entry:" << std::endl; // TODO 权宜之计
        StmtAST::has_ret = false;
        block->IR();
        std::cout << "}" << std::endl;
    }
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

    void Dump() const override
    {
        std::cout << "FuncTypeAST { ";
        std::cout << type;
        std::cout << " }";
    }

    void IR() override
    {
        dbg_printf("in FuncTypeAST\n");
        std::cout << type_ir[type];
    }
};

/**
 * @brief Block         ::= "{" {BlockItem} "}";
 */
class BlockAST : public BaseAST
{
public:
    std::vector<std::unique_ptr<BaseAST>> block_items;
    void Dump() const override
    {
        std::cout << "BlockAST { ";
        for (auto &item : block_items)
        {
            item->Dump();
        }
        std::cout << " }";
    }

    void IR() override
    {
        dbg_printf("in BlockAST\n");
        sym_tab.push();
        for (auto &item : block_items)
        {
            item->IR();
        }
        sym_tab.pop();
    }
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

    void Dump() const override {}

    void IR() override
    {
        switch (tag)
        {
        case Tag::DECL:
            decl->IR();
            break;
        case Tag::STMT:
            stmt->IR();
            break;
        default:
            assert(false);
        }
    }
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

    void Dump() const override {}

    void IR() override
    {
        if (StmtAST::has_ret)
        {
            return;
        }
        if (tag == Tag::CONST)
        {
            const_decl->IR();
        }
        else
        {
            var_decl->IR();
        }
    }
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

    void Dump() const override {}

    void IR() override
    {
        for (auto &def : const_defs)
        {
            def->IR();
        }
    }
};

/**
 * @brief VarDecl       ::= BType VarDef {"," VarDef} ";";
 * BType                ::= "int";
 */
class VarDeclAST : public BaseAST
{
public:
    std::vector<std::unique_ptr<BaseAST>> var_defs;

    void Dump() const override {}

    void IR() override
    {
        for (auto &def : var_defs)
        {
            def->IR();
        }
    }
};

/**
 * @brief VarDef        ::= IDENT | IDENT "=" InitVal;
 */
class VarDefAST : public BaseAST
{
public:
    std::string ident;
    std::unique_ptr<ExpBaseAST> init_val;

    void Dump() const override {}

    void IR() override
    {
        // TODO 不能用count
        auto symbol = "@" + ident + "_" + std::to_string(sym_cnt++);
        sym_tab.insert(ident, SymbolTag::VAR, symbol);
        std::cout << "  " << symbol << " = alloc i32" << std::endl;
        if (init_val)
        {
            init_val->IR();
            std::cout << "  store " << init_val->symbol << ", " << symbol << std::endl;
        }
    }
};

/**
 * @brief InitVal       ::= Exp;
 */
class InitValAST : public ExpBaseAST
{
public:
    std::unique_ptr<ExpBaseAST> exp;

    void Dump() const override {}

    void IR() override
    {
        exp->IR();
        is_const = exp->is_const;
        symbol = exp->symbol;
    }
};

/**
 * @brief ConstDef      ::= IDENT "=" ConstInitVal;
 */
class ConstDefAST : public BaseAST
{
public:
    std::string ident;
    std::unique_ptr<ExpBaseAST> const_init_val;

    void Dump() const override {}

    void IR() override
    {
        const_init_val->IR();
        sym_tab.insert(ident, SymbolTag::CONST, const_init_val->symbol);
    }
};

/**
 * @brief ConstInitVal  ::= ConstExp;
 */
class ConstInitValAST : public ExpBaseAST
{
public:
    std::unique_ptr<ExpBaseAST> const_exp;

    void Dump() const override {}

    void IR() override
    {
        const_exp->IR();
        is_const = const_exp->is_const;
        symbol = const_exp->symbol;
    }
};

/**
 * @brief ConstExp      ::= Exp;
 */
class ConstExpAST : public ExpBaseAST
{
public:
    std::unique_ptr<ExpBaseAST> exp;

    void Dump() const override {}

    void IR() override
    {
        exp->IR();
        is_const = exp->is_const;
        symbol = exp->symbol;
    }
};

/**
 * @brief Exp         ::= LOrExp;
 */
class ExpAST : public ExpBaseAST
{
public:
    std::unique_ptr<ExpBaseAST> lor_exp;

    void Dump() const override
    {
        lor_exp->Dump();
    }

    void IR() override
    {
        dbg_printf("in ExpAST\n");
        lor_exp->IR();
        is_const = lor_exp->is_const;
        symbol = lor_exp->symbol;
        dbg_printf("not in exp\n");
    }
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

    void Dump() const override {}

    void IR() override
    {
        dbg_printf("in LOrExpAST\n");
        if (tag == Tag::LAND)
        {
            land_exp->IR();
            is_const = land_exp->is_const;
            symbol = land_exp->symbol;
        }
        else
        {
            /**
             * 本质上做了这个操作:
             * int result = 1;
             * if (lhs == 0)
             * {
             *      result = rhs != 0;
             * }
             * 表达式的结果即是 result
             */
            lor_exp->IR();
            if (lor_exp->is_const)
            {
                bool lor_true = atoi(lor_exp->symbol.c_str());
                if (lor_true)
                {
                    is_const = true;
                    symbol = "1";
                }
                else
                {
                    land_exp->IR();
                    is_const = land_exp->is_const;
                    if (is_const)
                    {
                        bool land_true = atoi(land_exp->symbol.c_str());
                        symbol = std::to_string(land_true);
                    }
                    else
                    {
                        symbol = "%" + std::to_string(sym_cnt++);
                        std::cout << "  " << symbol << " = ne "
                                  << land_exp->symbol << ", 0" << std::endl;
                    }
                }
            }
            else
            {
                // TODO 考虑变量koopa ir是否可能重名，还有各种ir
                is_const = false;
                auto cur_sym_cnt = sym_cnt++;
                auto res_sym = "%lor_res_" + std::to_string(sym_cnt++);
                sym_tab.insert(res_sym, SymbolTag::VAR, res_sym);
                std::cout << "  " << res_sym << " = alloc i32" << std::endl;
                std::cout << "  store 1, " << res_sym << std::endl;
                std::cout << "  br " << lor_exp->symbol
                          << ", %lor_end_" << cur_sym_cnt
                          << ", %left_false_" << cur_sym_cnt << std::endl;
                std::cout << std::endl;
                std::cout << "%left_false_" << cur_sym_cnt << ":" << std::endl;
                land_exp->IR();
                std::cout << "  store " << land_exp->symbol
                          << ", " << res_sym << std::endl;
                std::cout << "  jump %lor_end_" << cur_sym_cnt << std::endl;
                std::cout << std::endl;
                std::cout << "%lor_end_" << cur_sym_cnt << ":" << std::endl;
                symbol = "%" + std::to_string(sym_cnt++);
                std::cout << "  " << symbol << " = load "
                          << res_sym << std::endl;
            }
        }
    }
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

    void Dump() const override {}

    void IR() override
    {
        dbg_printf("in LAndExpAST\n");
        if (tag == Tag::EQ)
        {
            eq_exp->IR();
            is_const = eq_exp->is_const;
            symbol = eq_exp->symbol;
        }
        else
        {
            /**
             * 本质上做了这个操作:
             * int result = 0;
             * if (lhs == 1)
             * {
             *      result = rhs != 0;
             * }
             * 表达式的结果即是 result
             */
            land_exp->IR();
            if (land_exp->is_const)
            {
                bool land_true = atoi(land_exp->symbol.c_str());
                if (!land_true)
                {
                    is_const = true;
                    symbol = "0";
                }
                else
                {
                    eq_exp->IR();
                    is_const = eq_exp->is_const;
                    if (is_const)
                    {
                        bool eq_true = atoi(eq_exp->symbol.c_str());
                        symbol = std::to_string(eq_true);
                    }
                    else
                    {
                        symbol = "%" + std::to_string(sym_cnt++);
                        std::cout << "  " << symbol << " = ne "
                                  << eq_exp->symbol << ", 0" << std::endl;
                    }
                }
            }
            else
            {
                // TODO 考虑变量koopa ir是否可能重名，还有各种ir
                is_const = false;
                auto cur_sym_cnt = sym_cnt++;
                auto res_sym = "%land_res_" + std::to_string(sym_cnt++);
                sym_tab.insert(res_sym, SymbolTag::VAR, res_sym);
                std::cout << "  " << res_sym << " = alloc i32" << std::endl;
                std::cout << "  store 0, " << res_sym << std::endl;
                std::cout << "  br " << land_exp->symbol
                          << ", %left_true_" << cur_sym_cnt
                          << ", %land_end_" << cur_sym_cnt << std::endl;
                std::cout << std::endl;
                std::cout << "%left_true_" << cur_sym_cnt << ":" << std::endl;
                eq_exp->IR();
                std::cout << "  store " << eq_exp->symbol
                          << ", " << res_sym << std::endl;
                std::cout << "  jump %land_end_" << cur_sym_cnt << std::endl;
                std::cout << std::endl;
                std::cout << "%land_end_" << cur_sym_cnt << ":" << std::endl;
                symbol = "%" + std::to_string(sym_cnt++);
                std::cout << "  " << symbol << " = load "
                          << res_sym << std::endl;
            }
        }
    }
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

    void Dump() const override {}

    void IR() override
    {
        dbg_printf("in EqExpAST\n");
        if (tag == Tag::REL)
        {
            rel_exp->IR();
            is_const = rel_exp->is_const;
            symbol = rel_exp->symbol;
        }
        else
        {
            rel_exp->IR();
            eq_exp->IR();
            is_const = eq_exp->is_const && rel_exp->is_const;
            if (is_const)
            {
                int eq_val = atoi(eq_exp->symbol.c_str());
                int rel_val = atoi(rel_exp->symbol.c_str());
                if (op == "==")
                {
                    symbol = std::to_string(eq_val == rel_val);
                }
                else
                {
                    symbol = std::to_string(eq_val != rel_val);
                }
            }
            else
            {
                symbol = "%" + std::to_string(sym_cnt++);
                std::cout << "  " << symbol << " = " << op_ir.at(op) << " "
                          << eq_exp->symbol << ", " << rel_exp->symbol << std::endl;
            }
        }
    }
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

    void Dump() const override {}

    void IR() override
    {
        dbg_printf("in RelExpAST\n");
        if (tag == Tag::ADD)
        {
            add_exp->IR();
            is_const = add_exp->is_const;
            symbol = add_exp->symbol;
        }
        else
        {
            add_exp->IR();
            rel_exp->IR();
            is_const = rel_exp->is_const && add_exp->is_const;
            if (is_const)
            {
                int rel_val = atoi(rel_exp->symbol.c_str());
                int add_val = atoi(add_exp->symbol.c_str());
                if (op == "<")
                {
                    symbol = std::to_string(rel_val < add_val);
                }
                else if (op == ">")
                {
                    symbol = std::to_string(rel_val > add_val);
                }
                else if (op == "<=")
                {
                    symbol = std::to_string(rel_val <= add_val);
                }
                else
                {
                    symbol = std::to_string(rel_val >= add_val);
                }
            }
            else
            {
                symbol = "%" + std::to_string(sym_cnt++);
                std::cout << "  " << symbol << " = " << op_ir.at(op) << " "
                          << rel_exp->symbol << ", " << add_exp->symbol << std::endl;
            }
        }
    }
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

    void Dump() const override {}

    void IR() override
    {
        dbg_printf("in AddExpAST\n");
        if (tag == Tag::MUL)
        {
            mul_exp->IR();
            is_const = mul_exp->is_const;
            symbol = mul_exp->symbol;
        }
        else
        {
            mul_exp->IR();
            add_exp->IR();
            is_const = add_exp->is_const && mul_exp->is_const;
            if (is_const)
            {
                int add_val = atoi(add_exp->symbol.c_str());
                int mul_val = atoi(mul_exp->symbol.c_str());
                if (op == "+")
                {
                    symbol = std::to_string(add_val + mul_val);
                }
                else
                {
                    symbol = std::to_string(add_val - mul_val);
                }
            }
            else
            {
                symbol = "%" + std::to_string(sym_cnt++);
                std::cout << "  " << symbol << " = " << op_ir.at(op) << " "
                          << add_exp->symbol << ", " << mul_exp->symbol << std::endl;
            }
        }
        dbg_printf("not in add\n");
    }
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

    void Dump() const override {}

    void IR() override
    {
        dbg_printf("in MulExpAST\n");

        if (tag == Tag::UNARY)
        {
            unary_exp->IR();
            is_const = unary_exp->is_const;
            symbol = unary_exp->symbol;
        }
        else
        {
            unary_exp->IR();
            mul_exp->IR();
            is_const = mul_exp->is_const && unary_exp->is_const;
            if (is_const)
            {
                int mul_val = atoi(mul_exp->symbol.c_str());
                int unary_val = atoi(unary_exp->symbol.c_str());
                if (op == "*")
                {
                    symbol = std::to_string(mul_val * unary_val);
                }
                else if (op == "/")
                {
                    symbol = std::to_string(mul_val / unary_val);
                }
                else
                {
                    symbol = std::to_string(mul_val % unary_val);
                }
            }
            else
            {
                symbol = "%" + std::to_string(sym_cnt++);
                std::cout << "  " << symbol << " = " << op_ir.at(op) << " "
                          << mul_exp->symbol << ", " << unary_exp->symbol << std::endl;
            }
        }
        dbg_printf("not in mul\n");
    }
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

    void Dump() const override
    {
        if (tag == Tag::PRIMARY)
        {
            primary_exp->Dump();
        }
        else
        {
            std::cout << unary_op << " ";
            unary_exp->Dump();
        }
    }

    void IR() override
    {
        dbg_printf("in UnaryExpAST\n");
        if (tag == Tag::PRIMARY)
        {
            primary_exp->IR();
            is_const = primary_exp->is_const;
            symbol = primary_exp->symbol;
        }
        else
        {
            unary_exp->IR();
            is_const = unary_exp->is_const;
            if (is_const)
            {
                int unary_val = atoi(unary_exp->symbol.c_str());
                if (unary_op == "-")
                {
                    symbol = std::to_string(-unary_val);
                }
                else if (unary_op == "!")
                {
                    symbol = std::to_string(!unary_val);
                }
                else
                {
                    symbol = std::to_string(unary_val);
                }
            }
            else
            {
                if (unary_op == "+")
                {
                    symbol = unary_exp->symbol;
                }
                else
                {
                    symbol = "%" + std::to_string(sym_cnt++);
                    std::cout << "  " << symbol << " = " << op_ir.at(unary_op)
                              << " 0, " << unary_exp->symbol << std::endl;
                }
            }
        }
        dbg_printf("not in unary\n");
    }
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

    void Dump() const override
    {
        if (tag == Tag::EXP)
        {
            std::cout << "(";
            exp->Dump();
            std::cout << ")";
        }
        else if (tag == Tag::NUMBER)
        {
            std::cout << number;
        }
        // TODO
    }

    void IR() override
    {
        dbg_printf("in PrimaryExpAST\n");

        if (tag == Tag::EXP)
        {
            dbg_printf("is exp\n");

            exp->IR();
            is_const = exp->is_const;
            symbol = exp->symbol;
        }
        else if (tag == Tag::LVAL)
        {
            // 程序没有语义错误，LVal一定在符号表中
            // 看左值能不能编译期求值，如果能则symbol为其值，否则为其koopa ir符号
            // 要求符号表存左值与其值、符号
            lval->IR();
            auto sym_info = sym_tab[lval->ident];
            is_const = sym_info->tag == SymbolTag::CONST;
            if (is_const)
            {
                symbol = sym_info->symbol;
            }
            else
            {
                symbol = "%" + std::to_string(sym_cnt++);
                std::cout << "  " << symbol << " = load "
                          << sym_info->symbol << std::endl;
            }
        }
        else
        {
            dbg_printf("is number\n");
            is_const = true;
            symbol = std::to_string(number);
        }
        dbg_printf("not here\n");
    }
};

#endif
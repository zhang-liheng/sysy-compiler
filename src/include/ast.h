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

/**
 * 目前采用如下策略：
 * 1. 在变量分类上，is_const只对常量成立
 * 2. 在表达式计算上，自下而上判断是否为常量，若是则计算，否则不计算。
 * 因为测试程序没有语义错误，所以一定能在常量定义时计算出值，
 * 在变量定义时，若右边表达式不包含变量，也能计算出值。
 * 3. 这是一种妥协，将SSA形式和稀疏条件常量传播留到最后再做。
 */

/**
 * TODO 带变量的表达式求值   在掌管声明的AST插入符号表
 *                      求值时从符号表中拿到值，还没做
 * 目前无需判断是否在符号表中，因为程序一定符合语义规范
 * 等到作用域时才需判断
 *
 * 求值后将新的变量插入符号表中
 *
 */

static int sym_cnt = 0;

static SymbolTableUnit sym_tab;

/**
 * @brief 按照官方文档的写法，所有成员变量均为public，不提供get方法
 *
 * TODO 可能可以为表达式设计一个单独的基类
 */
class BaseAST
{
public:
    /**
     * AST对应的Koopa IR符号，若是表达式，会尽量在编译期求值，
     * 若能求值，则为其值，若不能，则为符号
     */
    std::string symbol;
    // 是否为常量
    bool is_const;

    virtual ~BaseAST() = default;

    /**
     * @brief 输出AST的结构
     */
    virtual void Dump() const = 0;

    /**
     * @brief   生成Koopa IR：
     *
     * 以递归形式后序遍历AST树。
     * 首先调用成员AST的IR方法，该方法会设置其is_const和symbol,
     * 生成Koopa IR，插入符号表；
     * 然后设置is_const, symbol, 生成Koopa IR，插入符号表。
     *
     * @param   os  KoopaIR输出到os
     */
    virtual void IR(std::ostream &os) = 0;
};

/**
 * @brief LVal          ::= IDENT;
 */
class LValAST : public BaseAST
{
public:
    std::string ident;

    void Dump() const override {}

    void IR(std::ostream &os) override {}
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

    void IR(std::ostream &os = std::cout) override
    {
        dbg_printf("in CompUnitAST\n");
        func_def->IR(os);
        // TODO 无需symbol
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

    void IR(std::ostream &os) override
    {
        dbg_printf("in FuncDefAST\n");
        sym_cnt = 0;
        os << "fun @" << ident << "(): ";
        func_type->IR(os);
        os << " {" << std::endl;
        block->IR(os);
        os << "}" << std::endl;
        // TODO 无需symbol
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

    void IR(std::ostream &os) override
    {
        dbg_printf("in FuncTypeAST\n");
        os << type_ir[type];
        // TODO 无需symbol
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

    void IR(std::ostream &os) override
    {
        dbg_printf("in BlockAST\n");
        os << "%entry:" << std::endl; // TODO 权宜之计
        for (auto &item : block_items)
        {
            item->IR(os);
        }
        // TODO 无需symbol
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

    void IR(std::ostream &os) override
    {
        switch (tag)
        {
        case Tag::DECL:
            decl->IR(os);
            break;
        case Tag::STMT:
            stmt->IR(os);
            break;
        default:
            assert(false);
        }
        // TODO 无需symbol
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

    void IR(std::ostream &os) override
    {
        if (tag == Tag::CONST)
        {
            const_decl->IR(os);
        }
        else
        {
            var_decl->IR(os);
        }
        // TODO 无需symbol
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

    void IR(std::ostream &os) override
    {
        for (auto &def : const_defs)
        {
            def->IR(os);
        }
        // TODO 无需symbol
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

    void IR(std::ostream &os) override
    {
        for (auto &def : var_defs)
        {
            def->IR(os);
        }
        // TODO 无需symbol
    }
};

// TODO 以下的AST节点的IR方法需要实现
/**
 * @brief VarDef        ::= IDENT | IDENT "=" InitVal;
 */
class VarDefAST : public BaseAST
{
public:
    std::string ident;
    std::unique_ptr<BaseAST> init_val;

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        // TODO 不需要is_const和symbol
        is_const = false;
        sym_tab.insert(ident, SymbolTag::VAR, symbol);
        os << "  @" << ident << " = alloc i32" << std::endl;
        if (init_val)
        {
            init_val->IR(os);
            os << "  store " << init_val->symbol << ", @" << ident << std::endl;
        }
    }
};

/**
 * @brief InitVal       ::= Exp;
 */
class InitValAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> exp;

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        exp->IR(os);
        is_const = exp->is_const; // TODO
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
    std::unique_ptr<BaseAST> const_init_val;

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        const_init_val->IR(os);
        is_const = const_init_val->is_const; // TODO 不需要
        symbol = const_init_val->symbol;
        sym_tab.insert(ident, SymbolTag::CONST, symbol);
    }
};

/**
 * @brief ConstInitVal  ::= ConstExp;
 */
class ConstInitValAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> const_exp;

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        const_exp->IR(os);
        is_const = const_exp->is_const;
        symbol = const_exp->symbol;
    }
};

/**
 * @brief ConstExp      ::= Exp;
 */
class ConstExpAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> exp;

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        exp->IR(os);
        is_const = exp->is_const;
        symbol = exp->symbol;
    }
};

/**
 * @brief Stmt          ::= LVal "=" Exp ";"
 *                      | "return" Exp ";";
 */
class StmtAST : public BaseAST
{
public:
    enum class Tag
    {
        LVAL,
        RETURN
    } tag;
    std::unique_ptr<LValAST> lval;
    std::unique_ptr<BaseAST> exp;

    void Dump() const override
    {
        std::cout << "StmtAST { ";
        exp->Dump();
        std::cout << " }";
    }

    void IR(std::ostream &os) override
    {
        dbg_printf("in StmtAST\n");
        exp->IR(os);
        if (tag == Tag::LVAL)
        {
            lval->IR(os);
            auto sym_info = sym_tab[lval->ident];
            assert(sym_info->tag == SymbolTag::VAR);
            // TODO 不需要is_const和symbol
            os << "  store " << exp->symbol << ", @" << lval->ident << std::endl;
        }
        else
        {
            os << "  ret " << exp->symbol << std::endl;
        }
        // TODO 无需is_const, symbol
    }
};

/**
 * @brief Exp         ::= LOrExp;
 */
class ExpAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> lor_exp;

    void Dump() const override
    {
        lor_exp->Dump();
    }

    void IR(std::ostream &os) override
    {
        dbg_printf("in ExpAST\n");
        lor_exp->IR(os);
        is_const = lor_exp->is_const;
        symbol = lor_exp->symbol;
        dbg_printf("not in exp\n");
    }
};

/**
 * @brief LOrExp      ::= LAndExp | LOrExp "||" LAndExp;
 */
class LOrExpAST : public BaseAST
{
public:
    enum class Tag
    {
        LAND,
        LOR
    } tag;
    std::unique_ptr<BaseAST> land_exp;
    std::unique_ptr<BaseAST> lor_exp;

    LOrExpAST(Tag tag) : tag(tag) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in LOrExpAST\n");
        if (tag == Tag::LAND)
        {
            land_exp->IR(os);
            is_const = land_exp->is_const;
            symbol = land_exp->symbol;
        }
        else
        {
            land_exp->IR(os);
            lor_exp->IR(os);
            is_const = land_exp->is_const && lor_exp->is_const;
            if (is_const)
            {
                bool lor_true = atoi(lor_exp->symbol.c_str());
                bool land_true = atoi(land_exp->symbol.c_str());
                symbol = std::to_string(lor_true || land_true);
            }
            else
            {
                // TODO 注意这里虽然可能有一方的symbol是数字，但是输出了Koopa IR符号
                std::string lor_sym = "%" + std::to_string(sym_cnt++);
                std::string land_sym = "%" + std::to_string(sym_cnt++);
                symbol = "%" + std::to_string(sym_cnt++);
                os << "  " << lor_sym << " = ne " << lor_exp->symbol
                   << ", 0" << std::endl;
                os << "  " << land_sym << " = ne " << land_exp->symbol
                   << ", 0" << std::endl;
                os << "  " << symbol << " = or " << lor_sym
                   << ", " << land_sym << std::endl;
            }
        }
    }
};

/**
 * @brief LAndExp     ::= EqExp | LAndExp "&&" EqExp;
 */
class LAndExpAST : public BaseAST
{
public:
    enum Tag
    {
        EQ,
        LAND
    } tag;
    std::unique_ptr<BaseAST> eq_exp;
    std::unique_ptr<BaseAST> land_exp;

    LAndExpAST(Tag tag) : tag(tag) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in LAndExpAST\n");
        if (tag == Tag::EQ)
        {
            eq_exp->IR(os);
            is_const = eq_exp->is_const;
            symbol = eq_exp->symbol;
        }
        else
        {
            eq_exp->IR(os);
            land_exp->IR(os);
            is_const = eq_exp->is_const && land_exp->is_const;
            if (is_const)
            {
                bool land_true = atoi(land_exp->symbol.c_str());
                bool eq_true = atoi(eq_exp->symbol.c_str());
                symbol = std::to_string(land_true && eq_true);
            }
            else
            {
                // TODO 注意这里
                std::string land_sym = "%" + std::to_string(sym_cnt++);
                std::string eq_sym = "%" + std::to_string(sym_cnt++);
                symbol = "%" + std::to_string(sym_cnt++);
                os << "  " << land_sym << " = ne " << land_exp->symbol
                   << ", 0" << std::endl;
                os << "  " << eq_sym << " = ne " << eq_exp->symbol
                   << ", 0" << std::endl;
                os << "  " << symbol << " = and " << land_sym
                   << ", " << eq_sym << std::endl;
            }
        }
    }
};

/**
 * @brief EqExp       ::= RelExp | EqExp ("==" | "!=") RelExp;
 */
class EqExpAST : public BaseAST
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
    std::unique_ptr<BaseAST> rel_exp;
    std::unique_ptr<BaseAST> eq_exp;
    std::string op;
    EqExpAST(Tag tag) : tag(tag) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in EqExpAST\n");
        if (tag == Tag::REL)
        {
            rel_exp->IR(os);
            is_const = rel_exp->is_const;
            symbol = rel_exp->symbol;
        }
        else
        {
            rel_exp->IR(os);
            eq_exp->IR(os);
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
                os << "  " << symbol << " = " << op_ir.at(op) << " "
                   << eq_exp->symbol << ", " << rel_exp->symbol << std::endl;
            }
        }
    }
};

/**
 * @brief RelExp      ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
 */
class RelExpAST : public BaseAST
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
    std::unique_ptr<BaseAST> add_exp;
    std::unique_ptr<BaseAST> rel_exp;
    std::string op;

    RelExpAST(Tag tag) : tag(tag) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in RelExpAST\n");
        if (tag == Tag::ADD)
        {
            add_exp->IR(os);
            is_const = add_exp->is_const;
            symbol = add_exp->symbol;
        }
        else
        {
            add_exp->IR(os);
            rel_exp->IR(os);
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
                os << "  " << symbol << " = " << op_ir.at(op) << " "
                   << rel_exp->symbol << ", " << add_exp->symbol << std::endl;
            }
        }
    }
};

/**
 * @brief AddExp      ::= MulExp | AddExp ("+" | "-") MulExp;
 */
class AddExpAST : public BaseAST
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
    std::unique_ptr<BaseAST> mul_exp;
    std::unique_ptr<BaseAST> add_exp;
    std::string op;

    AddExpAST(Tag tag) : tag(tag) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in AddExpAST\n");
        if (tag == Tag::MUL)
        {
            mul_exp->IR(os);
            is_const = mul_exp->is_const;
            symbol = mul_exp->symbol;
        }
        else
        {
            mul_exp->IR(os);
            add_exp->IR(os);
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
                os << "  " << symbol << " = " << op_ir.at(op) << " "
                   << add_exp->symbol << ", " << mul_exp->symbol << std::endl;
            }
        }
        dbg_printf("not in add\n");
    }
};

/**
 * @brief MulExp      ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
 */
class MulExpAST : public BaseAST
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
    std::unique_ptr<BaseAST> unary_exp;
    std::unique_ptr<BaseAST> mul_exp;
    std::string op;

    MulExpAST(Tag tag) : tag(tag) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in MulExpAST\n");

        if (tag == Tag::UNARY)
        {
            unary_exp->IR(os);
            is_const = unary_exp->is_const;
            symbol = unary_exp->symbol;
        }
        else
        {
            unary_exp->IR(os);
            mul_exp->IR(os);
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
                os << "  " << symbol << " = " << op_ir.at(op) << " "
                   << mul_exp->symbol << ", " << unary_exp->symbol << std::endl;
            }
        }
        dbg_printf("not in mul\n");
    }
};

/**
 * @brief UnaryExp    ::= PrimaryExp | UnaryOp UnaryExp;
 */
class UnaryExpAST : public BaseAST
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
    std::unique_ptr<BaseAST> primary_exp;
    std::string unary_op;
    std::unique_ptr<BaseAST> unary_exp;

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

    void IR(std::ostream &os) override
    {
        dbg_printf("in UnaryExpAST\n");
        if (tag == Tag::PRIMARY)
        {
            primary_exp->IR(os);
            is_const = primary_exp->is_const;
            symbol = primary_exp->symbol;
        }
        else
        {
            unary_exp->IR(os);
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
                    os << "  " << symbol << " = " << op_ir.at(unary_op)
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
class PrimaryExpAST : public BaseAST
{
public:
    enum class Tag
    {
        EXP,
        LVAL,
        NUMBER
    } tag;
    std::unique_ptr<BaseAST> exp;
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

    void IR(std::ostream &os) override
    {
        dbg_printf("in PrimaryExpAST\n");

        if (tag == Tag::EXP)
        {
            dbg_printf("is exp\n");

            exp->IR(os);
            is_const = exp->is_const;
            symbol = exp->symbol;
        }
        else if (tag == Tag::LVAL)
        {
            // 程序没有语义错误，LVal一定在符号表中
            // 看左值能不能编译期求值，如果能则symbol为其值，否则为其koopa ir符号
            // 要求符号表存左值与其值、符号
            lval->IR(os);
            auto sym_info = sym_tab[lval->ident];
            is_const = sym_info->tag == SymbolTag::CONST;
            if (is_const)
            {
                symbol = sym_info->symbol;
            }
            else
            {
                symbol = "%" + std::to_string(sym_cnt++);
                os << "  " << symbol << " = load @" << lval->ident << std::endl;
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
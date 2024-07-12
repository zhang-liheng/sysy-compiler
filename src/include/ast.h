#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>

#define DEBUG
#ifdef DEBUG
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif

static int symbol_cnt = 0;

class BaseAST
{
public:
    std::string symbol;
    virtual ~BaseAST() = default;

    virtual void Dump() const = 0;
    /*
     * @brief   生成Koopa IR：
     *
     * 后序遍历AST树，
     * 首先调用成员AST的IR方法，生成其symbol和Koopa IR，
     * 然后生成symbol、生成Koopa IR
     *
     * @param   os  KoopaIR输出到os
     * @return None
     */
    virtual void IR(std::ostream &os) = 0;
};

// CompUnit  ::= FuncDef;
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
        os << std::endl;
    }
};

// FuncDef   ::= FuncType IDENT "(" ")" Block;
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
        symbol_cnt = 0;
        os << "fun @" << ident << "(): ";
        func_type->IR(os);
        os << " { " << std::endl;
        block->IR(os);
        os << " }" << std::endl;
    }
};

// FuncType  ::= "int";
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
    }
};

// Block     ::= "{" Stmt "}";
class BlockAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> stmt;

    void Dump() const override
    {
        std::cout << "BlockAST { ";
        stmt->Dump();
        std::cout << " }";
    }

    void IR(std::ostream &os) override
    {
        dbg_printf("in BlockAST\n");
        os << "%entry: " << std::endl;
        stmt->IR(os);
    }
};

// Stmt      ::= "return" Exp ";";
class StmtAST : public BaseAST
{
public:
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
        symbol = exp->symbol;
        os << "  ret " << symbol << std::endl;
    }
};

// Exp         ::= LOrExp;
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
        symbol = lor_exp->symbol;
        dbg_printf("not in exp\n");
    }
};

// LOrExp      ::= LAndExp | LOrExp "||" LAndExp;
enum class LOrExpType
{
    LAnd,
    LOr
};
class LOrExpAST : public BaseAST
{
public:
    LOrExpType type;
    std::unique_ptr<BaseAST> land_exp;
    std::unique_ptr<BaseAST> lor_exp;

    LOrExpAST(LOrExpType type) : type(type) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in LOrExpAST\n");
        if (type == LOrExpType::LAnd)
        {
            land_exp->IR(os);
            symbol = land_exp->symbol;
        }
        else
        {
            land_exp->IR(os);
            lor_exp->IR(os);
            std::string l_true = "%" + std::to_string(symbol_cnt++);
            std::string r_true = "%" + std::to_string(symbol_cnt++);
            symbol = "%" + std::to_string(symbol_cnt++);
            os << "  " << l_true << " = ne " << lor_exp->symbol
               << ", 0" << std::endl;
            os << "  " << r_true << " = ne " << land_exp->symbol
               << ", 0" << std::endl;
            os << "  " << symbol << " = or " << l_true
               << ", " << r_true << std::endl;
        }
    }
};

// LAndExp     ::= EqExp | LAndExp "&&" EqExp;
enum class LAndExpType
{
    Eq,
    LAnd
};
class LAndExpAST : public BaseAST
{
public:
    LAndExpType type;
    std::unique_ptr<BaseAST> eq_exp;
    std::unique_ptr<BaseAST> land_exp;

    LAndExpAST(LAndExpType type) : type(type) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in LAndExpAST\n");
        if (type == LAndExpType::Eq)
        {
            eq_exp->IR(os);
            symbol = eq_exp->symbol;
        }
        else
        {
            eq_exp->IR(os);
            land_exp->IR(os);
            // symbol = "%" + std::to_string(symbol_cnt++);
            // os << "  " << symbol << " = and " << land_exp->symbol
            //    << ", " << eq_exp->symbol << std::endl;
            std::string l_true = "%" + std::to_string(symbol_cnt++);
            std::string r_true = "%" + std::to_string(symbol_cnt++);
            symbol = "%" + std::to_string(symbol_cnt++);
            os << "  " << l_true << " = ne " << land_exp->symbol
               << ", 0" << std::endl;
            os << "  " << r_true << " = ne " << eq_exp->symbol
               << ", 0" << std::endl;
            os << "  " << symbol << " = and " << l_true
               << ", " << r_true << std::endl;
        }
    }
};

// EqExp       ::= RelExp | EqExp ("==" | "!=") RelExp;
enum class EqExpType
{
    Rel,
    Eq
};
class EqExpAST : public BaseAST
{
public:
    EqExpType type;
    std::unique_ptr<BaseAST> rel_exp;
    std::unique_ptr<BaseAST> eq_exp;
    std::string op;
    EqExpAST(EqExpType type) : type(type) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in EqExpAST\n");
        if (type == EqExpType::Rel)
        {
            rel_exp->IR(os);
            symbol = rel_exp->symbol;
        }
        else
        {
            rel_exp->IR(os);
            eq_exp->IR(os);
            symbol = "%" + std::to_string(symbol_cnt++);
            if (op == "==")
            {
                os << "  " << symbol << " = eq " << eq_exp->symbol
                   << ", " << rel_exp->symbol << std::endl;
            }
            else
            {
                os << "  " << symbol << " = ne " << eq_exp->symbol
                   << ", " << rel_exp->symbol << std::endl;
            }
        }
    }
};

// RelExp      ::= AddExp | RelExp ("<" | ">" | "<=" | ">=") AddExp;
enum class RelExpType
{
    Add,
    Rel
};
class RelExpAST : public BaseAST
{
public:
    RelExpType type;
    std::unique_ptr<BaseAST> add_exp;
    std::unique_ptr<BaseAST> rel_exp;
    std::string op;

    RelExpAST(RelExpType type) : type(type) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in RelExpAST\n");
        if (type == RelExpType::Add)
        {
            add_exp->IR(os);
            symbol = add_exp->symbol;
        }
        else
        {
            add_exp->IR(os);
            rel_exp->IR(os);
            symbol = "%" + std::to_string(symbol_cnt++);
            if (op == "<")
            {
                os << "  " << symbol << " = lt " << rel_exp->symbol
                   << ", " << add_exp->symbol << std::endl;
            }
            else if (op == ">")
            {
                os << "  " << symbol << " = gt " << rel_exp->symbol
                   << ", " << add_exp->symbol << std::endl;
            }
            else if (op == "<=")
            {
                os << "  " << symbol << " = le " << rel_exp->symbol
                   << ", " << add_exp->symbol << std::endl;
            }
            else
            {
                os << "  " << symbol << " = ge " << rel_exp->symbol
                   << ", " << add_exp->symbol << std::endl;
            }
        }
    }
};

// AddExp      ::= MulExp | AddExp ("+" | "-") MulExp;
enum class AddExpType
{
    Mul,
    Add
};

class AddExpAST : public BaseAST
{
public:
    AddExpType type;
    std::unique_ptr<BaseAST> mul_exp;
    std::unique_ptr<BaseAST> add_exp;
    std::string op;

    AddExpAST(AddExpType type) : type(type) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in AddExpAST\n");
        if (type == AddExpType::Mul)
        {
            mul_exp->IR(os);
            symbol = mul_exp->symbol;
        }
        else
        {
            mul_exp->IR(os);
            add_exp->IR(os);
            symbol = "%" + std::to_string(symbol_cnt++);
            if (op == "+")
            {
                os << "  " << symbol << " = add " << add_exp->symbol
                   << ", " << mul_exp->symbol << std::endl;
            }
            else
            {
                os << "  " << symbol << " = sub " << add_exp->symbol
                   << ", " << mul_exp->symbol << std::endl;
            }
        }
        dbg_printf("not in add\n");
    }
};

// MulExp      ::= UnaryExp | MulExp ("*" | "/" | "%") UnaryExp;
enum class MulExpType
{
    Unary,
    Mul
};
class MulExpAST : public BaseAST
{
public:
    MulExpType type;
    std::unique_ptr<BaseAST> unary_exp;
    std::unique_ptr<BaseAST> mul_exp;
    std::string op;

    MulExpAST(MulExpType type) : type(type) {}

    void Dump() const override {}

    void IR(std::ostream &os) override
    {
        dbg_printf("in MulExpAST\n");

        if (type == MulExpType::Unary)
        {
            unary_exp->IR(os);
            symbol = unary_exp->symbol;
        }
        else
        {
            unary_exp->IR(os);
            mul_exp->IR(os);
            symbol = "%" + std::to_string(symbol_cnt++);
            if (op == "*")
            {
                os << "  " << symbol << " = mul " << mul_exp->symbol
                   << ", " << unary_exp->symbol << std::endl;
            }
            else if (op == "/")
            {
                os << "  " << symbol << " = div " << mul_exp->symbol
                   << ", " << unary_exp->symbol << std::endl;
            }
            else
            {
                os << "  " << symbol << " = mod " << mul_exp->symbol
                   << ", " << unary_exp->symbol << std::endl;
            }
        }
        dbg_printf("not in mul\n");
    }
};

// UnaryExp    ::= PrimaryExp | UnaryOp UnaryExp;
enum class UnaryExpType
{
    Primary,
    Op
};

class UnaryExpAST : public BaseAST
{
public:
    UnaryExpType type;
    std::unique_ptr<BaseAST> primary_exp;
    std::string unary_op;
    std::unique_ptr<BaseAST> unary_exp;

    UnaryExpAST(UnaryExpType type) : type(type) {}

    void Dump() const override
    {
        if (type == UnaryExpType::Primary)
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
        if (type == UnaryExpType::Primary)
        {
            primary_exp->IR(os);
            symbol = primary_exp->symbol;
        }
        else
        {
            unary_exp->IR(os);
            if (unary_op == "-")
            {
                symbol = "%" + std::to_string(symbol_cnt++);
                os << "  " << symbol << " = sub 0, "
                   << unary_exp->symbol << std::endl;
            }
            else if (unary_op == "!")
            {
                symbol = "%" + std::to_string(symbol_cnt++);
                os << "  " << symbol << " = eq 0, "
                   << unary_exp->symbol << std::endl;
            }
            else
            {
                symbol = unary_exp->symbol;
            }
        }
        dbg_printf("not in unary\n");
    }
};

// PrimaryExp  ::= "(" Exp ")" | Number;
enum class PrimaryExpType
{
    Exp,
    Number
};

class PrimaryExpAST : public BaseAST
{
public:
    PrimaryExpType type;
    std::unique_ptr<BaseAST> exp;
    int number;

    PrimaryExpAST(PrimaryExpType type) : type(type) {}

    void Dump() const override
    {
        if (type == PrimaryExpType::Exp)
        {
            std::cout << "(";
            exp->Dump();
            std::cout << ")";
        }
        else
        {
            std::cout << number;
        }
    }

    void IR(std::ostream &os) override
    {
        dbg_printf("in PrimaryExpAST\n");

        if (type == PrimaryExpType::Exp)
        {
            dbg_printf("is exp\n");

            exp->IR(os);
            symbol = exp->symbol;
        }
        else
        {
            dbg_printf("is number\n");

            symbol = std::to_string(number);
        }
        dbg_printf("not here\n");
    }
};
#endif
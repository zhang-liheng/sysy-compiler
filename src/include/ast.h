#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <iostream>
#include <unordered_map>

class BaseAST
{
public:
    virtual ~BaseAST() = default;

    virtual void Dump() const = 0;
    virtual void IR(std::ostream &os) const = 0;
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

    void IR(std::ostream &os = std::cout) const override
    {
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
        std::cout << " }";
    }

    void IR(std::ostream &os = std::cout) const override
    {
        os << "fun @" << ident << "(): ";
        func_type->IR(os);
        os << " { ";
        block->IR(os);
        os << " }";
    }
};

// FuncType  ::= "int";
class FuncTypeAST : public BaseAST
{
public:
    inline static std::unordered_map<std::string, std::string> ir_type = {
        {"int", "i32"}};
    std::string type;

    void Dump() const override
    {
        std::cout << "FuncTypeAST { ";
        std::cout << type;
        std::cout << " }";
    }

    void IR(std::ostream &os = std::cout) const override
    {
        os << ir_type[type];
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

    void IR(std::ostream &os = std::cout) const override
    {
        os << "%entry: ";
        stmt->IR(os);
    }
};

// Stmt      ::= "return" Number ";";
class StmtAST : public BaseAST
{
public:
    int number;

    void Dump() const override
    {
        std::cout << "StmtAST { ";
        std::cout << number;
        std::cout << " }";
    }

    void IR(std::ostream &os = std::cout) const override
    {
        os << "ret ";
        os << number;
    }
};

#endif
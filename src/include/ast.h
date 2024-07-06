#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <iostream>

class BaseAST
{
public:
    virtual ~BaseAST() = default;

    virtual void Dump() const = 0;
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
};

// FuncDef   ::= FuncType IDENT "(" ")" Block;
class FuncDefAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;

    FuncDefAST() : ident("int") {}
    void Dump() const override
    {
        std::cout << "FuncDefAST { ";
        func_type->Dump();
        std::cout << ", " << ident << ", ";
        block->Dump();
        std::cout << " }";
    }
};

// FuncType  ::= "int";
class FuncTypeAST : public BaseAST
{
public:
    std::string type;

    void Dump() const override
    {
        std::cout << "FuncTypeAST { ";
        std::cout << type;
        std::cout << " }";
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
};

// // Number    ::= INT_CONST;
// class NumberAST : public BaseAST
// {
// public:
//     std::string int_const;

//     void Dump() const override
//     {
//         std::cout << "NumberAST { ";
//         std::cout << int_const;
//         std::cout << " }";
//     }
// };

// ...

#endif
#include "include/ast.hpp"
#include "include/symtab.hpp"

void CompUnitAST::Dump() const
{
    std::cout << "CompUnitAST { ";
    func_def->Dump();
    std::cout << " }";
}

void CompUnitAST::IR()
{
    dbg_printf("in CompUnitAST\n");
    func_def->IR();
}

void DeclAST::Dump() const {}

void DeclAST::IR()
{
    if (StmtAST::has_jp)
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

void ConstDeclAST::Dump() const {}

void ConstDeclAST::IR()
{
    for (auto &def : const_defs)
    {
        def->IR();
    }
}

void ConstDefAST::Dump() const {}

void ConstDefAST::IR()
{
    const_init_val->IR();
    sym_tab.insert(ident, SymbolTag::CONST, const_init_val->symbol);
}

void ConstInitValAST::Dump() const {}

void ConstInitValAST::IR()
{
    const_exp->IR();
    is_const = const_exp->is_const;
    symbol = const_exp->symbol;
}

void VarDeclAST::Dump() const {}

void VarDeclAST::IR()
{
    for (auto &def : var_defs)
    {
        def->IR();
    }
}

void VarDefAST::Dump() const {}

void VarDefAST::IR()
{
    // TODO 不能用count, 证明命名不会冲突
    auto symbol = "@" + ident + "_" + std::to_string(sym_cnt++);
    sym_tab.insert(ident, SymbolTag::VAR, symbol);
    std::cout << "  " << symbol << " = alloc i32" << std::endl;
    if (init_val)
    {
        init_val->IR();
        std::cout << "  store " << init_val->symbol << ", " << symbol << std::endl;
    }
}

void InitValAST::Dump() const {}

void InitValAST::IR()
{
    exp->IR();
    is_const = exp->is_const;
    symbol = exp->symbol;
}

void FuncDefAST::Dump() const
{
    std::cout << "FuncDefAST { ";
    func_type->Dump();
    std::cout << ", " << ident << ", ";
    block->Dump();
    std::cout << "}";
}

void FuncDefAST::IR()
{
    dbg_printf("in FuncDefAST\n");
    sym_cnt = 0;
    std::cout << "fun @" << ident << "(): ";
    func_type->IR();
    std::cout << " {" << std::endl;
    std::cout << "%entry:" << std::endl; // TODO 权宜之计
    StmtAST::has_jp = false;
    block->IR();
    std::cout << "}" << std::endl;
}

void FuncTypeAST::Dump() const
{
    std::cout << "FuncTypeAST { ";
    std::cout << type;
    std::cout << " }";
}

void FuncTypeAST::IR()
{
    dbg_printf("in FuncTypeAST\n");
    std::cout << type_ir[type];
}

void BlockAST::Dump() const
{
    std::cout << "BlockAST { ";
    for (auto &item : block_items)
    {
        item->Dump();
    }
    std::cout << " }";
}

void BlockAST::IR()
{
    dbg_printf("in BlockAST\n");
    sym_tab.push();
    for (auto &item : block_items)
    {
        item->IR();
    }
    sym_tab.pop();
}

void BlockItemAST::Dump() const {}

void BlockItemAST::IR()
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

void StmtAST::Dump() const
{
    std::cout << "StmtAST { ";
    exp->Dump();
    std::cout << " }";
}

void StmtAST::IR()
{
    dbg_printf("in StmtAST\n");
    if (has_jp)
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
                      << ", %if_end_" << cur_sym_cnt << std::endl;
        }
        std::cout << std::endl;
        std::cout << "%then_" << cur_sym_cnt << ":" << std::endl;
        has_jp = false;
        if_stmt->IR();
        if (!has_jp)
        {
            std::cout << "  jump %if_end_" << cur_sym_cnt << std::endl;
            std::cout << std::endl;
        }
        if (else_stmt)
        {
            std::cout << "%else_" << cur_sym_cnt << ":" << std::endl;
            has_jp = false;
            else_stmt->IR();
            if (!has_jp)
            {
                std::cout << "  jump %if_end_" << cur_sym_cnt << std::endl;
                std::cout << std::endl;
            }
        }
        std::cout << "%if_end_" << cur_sym_cnt << ":" << std::endl;
        has_jp = false;
        break;
    }

    case Tag::WHILE:
    {
        auto cur_sym_cnt = sym_cnt++;
        std::cout << "  jump %while_entry_" << cur_sym_cnt << std::endl;
        std::cout << std::endl;
        std::cout << "%while_entry_" << cur_sym_cnt << ":" << std::endl;
        while_cnt_stk.push(cur_sym_cnt);
        exp->IR();
        std::cout << "  br " << exp->symbol
                  << ", %while_body_" << cur_sym_cnt
                  << ", %while_end_" << cur_sym_cnt << std::endl;
        std::cout << std::endl;
        std::cout << "%while_body_" << cur_sym_cnt << ":" << std::endl;
        has_jp = false;
        while_stmt->IR();
        if (!has_jp)
        {
            std::cout << "  jump %while_entry_" << cur_sym_cnt << std::endl;
            std::cout << std::endl;
        }
        std::cout << "%while_end_" << cur_sym_cnt << ":" << std::endl;
        while_cnt_stk.pop();
        has_jp = false;
        break;
    }

    case Tag::BREAK:
    {
        auto cur_sym_cnt = while_cnt_stk.top();
        std::cout << "  jump %while_end_" << cur_sym_cnt << std::endl;
        std::cout << std::endl;
        has_jp = true;
        break;
    }

    case Tag::CONTINUE:
    {
        auto cur_sym_cnt = while_cnt_stk.top();
        std::cout << "  jump %while_entry_" << cur_sym_cnt << std::endl;
        std::cout << std::endl;
        has_jp = true;
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
        std::cout << std::endl;
        has_jp = true;
        break;
    }

    default:
        assert(false);
    }
}

void ExpAST::Dump() const
{
    lor_exp->Dump();
}

void ExpAST::IR()
{
    dbg_printf("in ExpAST\n");
    lor_exp->IR();
    is_const = lor_exp->is_const;
    symbol = lor_exp->symbol;
    dbg_printf("not in exp\n");
}

void LValAST::Dump() const {}

void LValAST::IR() {}

void PrimaryExpAST::Dump() const
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

void PrimaryExpAST::IR()
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

void UnaryExpAST::Dump() const
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

void UnaryExpAST::IR()
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

void MulExpAST::Dump() const {}

void MulExpAST::IR()
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

void AddExpAST::Dump() const {}

void AddExpAST::IR()
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

void RelExpAST::Dump() const {}

void RelExpAST::IR()
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

void EqExpAST::Dump() const {}

void EqExpAST::IR()
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

void LAndExpAST::Dump() const {}

void LAndExpAST::IR()
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

void LOrExpAST::Dump() const {}

void LOrExpAST::IR()
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

void ConstExpAST::Dump() const {}

void ConstExpAST::IR()
{
    exp->IR();
    is_const = exp->is_const;
    symbol = exp->symbol;
}
%code requires {
  #include <memory>
  #include <string>

  #include "include/ast.h"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "include/ast.h"

// #define DEBUG
#ifdef DEBUG
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif

// 声明 lexer 函数和错误处理函数
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

// 定义 parser 函数和错误处理函数的附加参数
// 我们需要返回一个字符串作为 AST, 所以我们把附加参数定义成字符串的智能指针
// 解析完成后, 我们要手动修改这个参数, 把它设置成解析得到的字符串
%parse-param { std::unique_ptr<BaseAST> &ast }

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// 请自行 STFW 在 union 里写一个带析构函数的类会出现什么情况
%union {
    int int_val;
    std::string *str_val;
    BaseAST *ast_val;
    ExpBaseAST *exp_val;
}

// lexer 返回的所有 token 种类的声明
// 注意 IDENT 和 INT_CONST 会返回 token 的值, 分别对应 str_val 和 int_val
%token INT RETURN LT GT LE GE EQ NE LAND LOR CONST IF ELSE
%token <str_val> IDENT
%token <int_val> INT_CONST

// 非终结符的类型定义
%type <ast_val> CompUnit FuncDef FuncType Block Stmt BlockItem BlockItemList
Decl ConstDecl ConstDef ConstDefList LVal VarDecl VarDef VarDefList
%type <exp_val> Exp UnaryExp PrimaryExp AddExp MulExp LOrExp LAndExp EqExp
RelExp ConstInitVal ConstExp InitVal
%type <int_val> Number
%type <str_val> UnaryOp

//规定if-else匹配优先级，以解决空悬else问题
%precedence IFX
%precedence ELSE

%%

// 开始符, CompUnit ::= FuncDef, 大括号后声明了解析完成后 parser 要做的事情
// 之前我们定义了 FuncDef 会返回一个 str_val, 也就是字符串指针
// 而 parser 一旦解析完 CompUnit, 就说明所有的 token 都被解析了, 即解析结束了
// 此时我们应该把 FuncDef 返回的结果收集起来, 作为 AST 传给调用 parser 的函数
// $1 指代规则里第一个符号的返回值, 也就是 FuncDef 的返回值
CompUnit
  : FuncDef {
    dbg_printf("in CompUnit\n");
    auto comp_unit = make_unique<CompUnitAST>();
    comp_unit->func_def = unique_ptr<BaseAST>($1);
    ast = move(comp_unit);
  }
  ;

// FuncDef ::= FuncType IDENT '(' ')' Block;
// 我们这里可以直接写 '(' 和 ')', 因为之前在 lexer 里已经处理了单个字符的情况
// 解析完成后, 把这些符号的结果收集起来, 然后拼成一个新的字符串, 作为结果返回
// $$ 表示非终结符的返回值, 我们可以通过给这个符号赋值的方法来返回结果
// 你可能会问, FuncType, IDENT 之类的结果已经是字符串指针了
// 为什么还要用 unique_ptr 接住它们, 然后再解引用, 把它们拼成另一个字符串指针呢
// 因为所有的字符串指针都是我们 new 出来的, new 出来的内存一定要 delete
// 否则会发生内存泄漏, 而 unique_ptr 这种智能指针可以自动帮我们 delete
// 虽然此处你看不出用 unique_ptr 和手动 delete 的区别, 但当我们定义了 AST 之后
// 这种写法会省下很多内存管理的负担
FuncDef
  : FuncType IDENT '(' ')' Block {
    dbg_printf("in FuncDef\n");

    auto ast = new FuncDefAST();
    ast->func_type = unique_ptr<BaseAST>($1);
    ast->ident = *unique_ptr<string>($2);
    ast->block = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  ;

// 同上, 不再解释
FuncType
  : INT {
    dbg_printf("in FuncType\n");

    auto ast = new FuncTypeAST();
    ast->type = "int";
    $$ = ast;
  }
  ;

Block
  : '{' BlockItemList '}' {
    dbg_printf("in Block\n");
    $$ = $2;
  }
  ;

BlockItemList
  : { 
    dbg_printf("in BlockItemList 1\n");
    $$ = new BlockAST(); 
  }
  | BlockItem BlockItemList {
    dbg_printf("in BlockItemList 2\n");
    auto ast = new BlockAST();
    auto block_item = unique_ptr<BaseAST>($1);
    auto block_item_list = unique_ptr<BlockAST>((BlockAST*)($2));
    ast->block_items.emplace_back(std::move(block_item));
    for(auto &item : block_item_list->block_items)
    {
      ast->block_items.emplace_back(std::move(item));
    }
    $$ = ast;
  }
  ;

BlockItem
  : Stmt {
    dbg_printf("in BlockItem\n");
    auto ast = new BlockItemAST();
    ast->tag = BlockItemAST::Tag::STMT;
    ast->stmt = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | Decl {
    dbg_printf("in BlockItem\n");
    auto ast = new BlockItemAST();
    ast->tag = BlockItemAST::Tag::DECL;
    ast->decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

Decl
  : ConstDecl {
    dbg_printf("in Decl\n");
    auto ast = new DeclAST();
    ast->tag = DeclAST::Tag::CONST;
    ast->const_decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | VarDecl {
    dbg_printf("in Decl\n");
    auto ast = new DeclAST();
    ast->tag = DeclAST::Tag::VAR;
    ast->var_decl = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  ;

ConstDecl
  : CONST INT ConstDef ConstDefList ';' {
    dbg_printf("in ConstDecl\n");
    auto ast = new ConstDeclAST();
    ast->const_defs.emplace_back(std::move(unique_ptr<BaseAST>($3)));
    auto const_def_list = unique_ptr<ConstDeclAST>((ConstDeclAST*)($4));
    for(auto &item : const_def_list->const_defs)
    {
      ast->const_defs.emplace_back(std::move(item));
    }
    $$ = ast;
  }
  ;

ConstDefList
  : { 
    dbg_printf("in ConstDefList\n");
    $$ = new ConstDeclAST(); 
  }
  | ',' ConstDef ConstDefList {
    dbg_printf("in ConstDefList\n");
    auto ast = new ConstDeclAST();
    auto const_def = unique_ptr<BaseAST>($2);
    auto const_def_list = unique_ptr<ConstDeclAST>((ConstDeclAST*)($3));
    ast->const_defs.emplace_back(std::move(const_def));
    for(auto &item : const_def_list->const_defs)
    {
      ast->const_defs.emplace_back(std::move(item));
    }
    $$ = ast;
  }
  ;

ConstDef
  : IDENT '=' ConstInitVal {
    dbg_printf("in ConstDef\n");
    auto ast = new ConstDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->const_init_val = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  ;

ConstInitVal
  : ConstExp {
    dbg_printf("in ConstInitVal\n");
    auto ast = new ConstInitValAST();
    ast->const_exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  ;

VarDecl
  : INT VarDef VarDefList ';' {
    dbg_printf("in VarDecl\n");
    auto ast = new VarDeclAST();
    ast->var_defs.emplace_back(unique_ptr<BaseAST>($2));
    auto var_def_list = unique_ptr<VarDeclAST>((VarDeclAST*)($3));
    for(auto &item : var_def_list->var_defs)
    {
      ast->var_defs.emplace_back(std::move(item));
    }
    $$ = ast;
  }
  ;

VarDefList
  : { 
    dbg_printf("in VarDefList\n");
    $$ = new VarDeclAST(); 
  }
  | ',' VarDef VarDefList {
    dbg_printf("in VarDefList\n");
    auto ast = new VarDeclAST();
    ast->var_defs.emplace_back(std::move(unique_ptr<BaseAST>($2)));
    auto var_def_list = unique_ptr<VarDeclAST>((VarDeclAST*)($3));
    for(auto &item : var_def_list->var_defs)
    {
      ast->var_defs.emplace_back(std::move(item));
    }
    $$ = ast;
  }
  ;

VarDef
  : IDENT {
    dbg_printf("in VarDef\n");
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  | IDENT '=' InitVal {
    dbg_printf("in VarDef\n");
    auto ast = new VarDefAST();
    ast->ident = *unique_ptr<string>($1);
    ast->init_val = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  ;

InitVal
  : Exp {
    dbg_printf("in InitVal\n");
    auto ast = new InitValAST();
    ast->exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  ;

ConstExp
  : Exp {
    dbg_printf("in ConstExp\n");
    auto ast = new ConstExpAST();
    ast->exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  ;

Stmt
  : LVal '=' Exp ';' {
    dbg_printf("in Stmt\n");
    auto ast = new StmtAST();
    ast->tag = StmtAST::Tag::LVAL;
    ast->lval = unique_ptr<LValAST>((LValAST*)($1));
    ast->exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  | Exp ';' {
    dbg_printf("in Stmt\n");
    auto ast = new StmtAST();
    ast->tag = StmtAST::Tag::EXP;
    ast->exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  | ';' {
    dbg_printf("in Stmt\n");
    auto ast = new StmtAST();
    ast->tag = StmtAST::Tag::EXP;
    $$ = ast;
  }
  | Block {
    dbg_printf("in Stmt\n");
    auto ast = new StmtAST();
    ast->tag = StmtAST::Tag::BLOCK;
    ast->block = unique_ptr<BaseAST>($1);
    $$ = ast;
  }
  | IF '(' Exp ')' Stmt %prec IFX {
    dbg_printf("in Stmt\n");
    auto ast = new StmtAST();
    ast->tag = StmtAST::Tag::IF;
    ast->exp = unique_ptr<ExpBaseAST>($3);
    ast->if_stmt = unique_ptr<BaseAST>($5);
    $$ = ast;
  }
  | IF '(' Exp ')' Stmt ELSE Stmt {
    dbg_printf("in Stmt\n");
    auto ast = new StmtAST();
    ast->tag = StmtAST::Tag::IF;
    ast->exp = unique_ptr<ExpBaseAST>($3);
    ast->if_stmt = unique_ptr<BaseAST>($5);
    ast->else_stmt = unique_ptr<BaseAST>($7);
    $$ = ast;
  }
  | RETURN Exp ';' {
    dbg_printf("in Stmt\n");
    auto ast = new StmtAST();
    ast->tag = StmtAST::Tag::RETURN;
    ast->exp = unique_ptr<ExpBaseAST>($2);
    $$ = ast;
  }
  | RETURN ';' {
    dbg_printf("in Stmt\n");
    auto ast = new StmtAST();
    ast->tag = StmtAST::Tag::RETURN;
    $$ = ast;
  }
  ;

Exp
  : LOrExp {
    dbg_printf("in Exp\n");

    auto ast = new ExpAST();
    ast->lor_exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  ;

LOrExp
  : LAndExp {
    dbg_printf("in LOrExp\n");
    auto ast = new LOrExpAST(LOrExpAST::Tag::LAND);
    ast->land_exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  | LOrExp LOR LAndExp {
    dbg_printf("in LOrExp\n");
    auto ast = new LOrExpAST(LOrExpAST::Tag::LOR);
    ast->lor_exp = unique_ptr<ExpBaseAST>($1);
    ast->land_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  ;

LAndExp
  : EqExp {
    dbg_printf("in LAndExp\n");
    auto ast = new LAndExpAST(LAndExpAST::Tag::EQ);
    ast->eq_exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  | LAndExp LAND EqExp {
    dbg_printf("in LAndExp\n");
    auto ast = new LAndExpAST(LAndExpAST::Tag::LAND);
    ast->land_exp = unique_ptr<ExpBaseAST>($1);
    ast->eq_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  ;

EqExp
  : RelExp {
    dbg_printf("in EqExp\n");
    auto ast = new EqExpAST(EqExpAST::Tag::REL);
    ast->rel_exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  | EqExp EQ RelExp {
    dbg_printf("in EqExp\n");
    auto ast = new EqExpAST(EqExpAST::Tag::EQ);
    ast->eq_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = "==";
    ast->rel_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  | EqExp NE RelExp {
    dbg_printf("in EqExp\n");
    auto ast = new EqExpAST(EqExpAST::Tag::EQ);
    ast->eq_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = "!=";
    ast->rel_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  ;

RelExp
  : AddExp {
    dbg_printf("in RelExp\n");
    auto ast = new RelExpAST(RelExpAST::Tag::ADD);
    ast->add_exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  | RelExp LT AddExp {
    dbg_printf("in RelExp\n");
    auto ast = new RelExpAST(RelExpAST::Tag::REL);
    ast->rel_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = "<";
    ast->add_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  | RelExp GT AddExp {
    dbg_printf("in RelExp\n");
    auto ast = new RelExpAST(RelExpAST::Tag::REL);
    ast->rel_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = ">";
    ast->add_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  | RelExp LE AddExp {
    dbg_printf("in RelExp\n");
    auto ast = new RelExpAST(RelExpAST::Tag::REL);
    ast->rel_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = "<=";
    ast->add_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  | RelExp GE AddExp {
    dbg_printf("in RelExp\n");
    auto ast = new RelExpAST(RelExpAST::Tag::REL);
    ast->rel_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = ">=";
    ast->add_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  ;

AddExp
  : MulExp {
    dbg_printf("in AddExp\n");

    auto ast = new AddExpAST(AddExpAST::Tag::MUL);
    ast->mul_exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  | AddExp '+' MulExp {
    dbg_printf("in AddExp\n");
    auto ast = new AddExpAST(AddExpAST::Tag::ADD);
    ast->add_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = "+";
    ast->mul_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  | AddExp '-' MulExp {
    dbg_printf("in AddExp\n");
    auto ast = new AddExpAST(AddExpAST::Tag::ADD);
    ast->add_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = "-";
    ast->mul_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  ;

MulExp
  : UnaryExp {
    dbg_printf("in MulExp\n");
    auto ast = new MulExpAST(MulExpAST::Tag::UNARY);
    ast->unary_exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  | MulExp '*' UnaryExp {
    dbg_printf("in MulExp\n");
    auto ast = new MulExpAST(MulExpAST::Tag::MUL);
    ast->mul_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = "*";
    ast->unary_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  | MulExp '/' UnaryExp {
    dbg_printf("in MulExp\n");
    auto ast = new MulExpAST(MulExpAST::Tag::MUL);
    ast->mul_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = "/";
    ast->unary_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  | MulExp '%' UnaryExp {
    dbg_printf("in MulExp\n");
    auto ast = new MulExpAST(MulExpAST::Tag::MUL);
    ast->mul_exp = unique_ptr<ExpBaseAST>($1);
    ast->op = "%";
    ast->unary_exp = unique_ptr<ExpBaseAST>($3);
    $$ = ast;
  }
  ;

UnaryExp
  : PrimaryExp {
    dbg_printf("in UnaryExp\n");

    auto ast = new UnaryExpAST(UnaryExpAST::Tag::PRIMARY);
    ast->primary_exp = unique_ptr<ExpBaseAST>($1);
    $$ = ast;
  }
  | UnaryOp UnaryExp {
    dbg_printf("in UnaryExp\n");

    auto ast = new UnaryExpAST(UnaryExpAST::Tag::OP);
    ast->unary_op = *unique_ptr<string>($1);
    ast->unary_exp = unique_ptr<ExpBaseAST>($2);
    $$ = ast;
  }
  ;

PrimaryExp
  : '(' Exp ')' {
    dbg_printf("in PrimaryExp\n");

    auto ast = new PrimaryExpAST(PrimaryExpAST::Tag::EXP);
    ast->exp = unique_ptr<ExpBaseAST>($2);
    $$ = ast;
  }
  | LVal {
    dbg_printf("in PrimaryExp\n");

    auto ast = new PrimaryExpAST(PrimaryExpAST::Tag::LVAL);
    ast->lval = unique_ptr<LValAST>((LValAST*)($1));
    $$ = ast;
  }
  | Number {
    dbg_printf("in PrimaryExp\n");

    auto ast = new PrimaryExpAST(PrimaryExpAST::Tag::NUMBER);
    ast->number = $1;
    $$ = ast;
  }
  ;

LVal
  : IDENT {
    dbg_printf("in LVal\n");

    auto ast = new LValAST();
    ast->ident = *unique_ptr<string>($1);
    $$ = ast;
  }
  ;

UnaryOp
  : '+' { 
    auto op = new string("+");
    $$ = op;
  }
  | '-' {
    auto op = new string("-");
    $$ = op;
  }
  | '!' { 
    auto op = new string("!");
    $$ = op;
  }
  ;


Number
  : INT_CONST {
    $$ = $1;
  }
  ;

%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(unique_ptr<BaseAST> &ast, const char *s) {
  cerr << "error: " << s << endl;
}

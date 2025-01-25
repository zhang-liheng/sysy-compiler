# 编译原理课程实践报告：ZLex

信息科学技术学院 2200013214 张立恒

## 一、编译器概述

### 1.1 基本功能

本编译器基本具备如下功能：

1. 前端：通过词法分析、语法分析和中间代码生成等技术，生成文本形式的Koopa IR中间代码。
2. 后端：通过DFS遍历内存形式的Koopa IR，生成RISC-V目标代码。

### 1.2 主要特点

本编译器的主要特点是实现正确、代码风格良好、前后端解耦。

- 实现正确：实现了所有目标功能，通过了离线和在线测试中的所有Koopa, Risc-V和性能测试用例。
- 代码风格良好：使用C++17标准，除少量Yacc代码外，均使用智能指针、C++风格类型转换等特性，以保证内存安全。各文件、类、函数的功能划分合理，注释详细，可读性好。
- 前后端解耦：编译器前端和后端代码完全分离，后端仅使用前端输出的Koopa IR。

## 二、编译器设计

### 2.1 主要模块组成

编译器由5个主要模块组成：

- 词法分析部分 `sysy.l`负责将SysY源程序转换为token流
- 语法分析部分 `sysy.y`负责用token流建立语法分析树
- 符号表部分 `symtab.hpp`负责记录源程序符号信息，如与Koopa IR符号的对应关系
- 语义分析和中间代码生成部分 `ast.hpp, ast.cpp`负责遍历语法分析树，维护符号表，生成Koopa IR
- 目标代码生成部分 `riscv.hpp, riscv.cpp`负责用libkoopa将文本形式的Koopa IR转换为内存形式，通过DFS遍历内存形式的Koopa IR，生成RISC-V目标代码。

### 2.2 主要数据结构

#### 2.2.1 AST节点类

本编译器最核心的数据结构是 `ast.hpp`中定义的各AST节点类。在实现过程中，设计了 `BaseAST`作为基类：

```cpp
class BaseAST
{
public:
    virtual ~BaseAST() = default;

    /**
     * @brief   语义分析，生成Koopa IR，维护符号表
     *
     * 1. 生成Koopa IR，即先以递归形式后序遍历AST树，调用成员AST的IR方法，
     * 然后生成本节点的Koopa IR
     * 2. 把对应的Koopa IR符号插入符号表
     */
    virtual void IR() = 0;
};
```

为所有Exp相关的AST设计了基类 `ExpBaseAST`，为了方便其它AST访问本AST的Koopa IR符号以进行运算，设计成员变量symbol和is_const成员变量：

```cpp
class ExpBaseAST : public BaseAST
{
public:
    // AST对应的Koopa IR符号，若能求值，则为其值，若不能，则为符号
    std::string symbol;
    // 是否为常量
    bool is_const;
};
```

这些AST的 `IR`方法会设置其 `symbol`和 `is_const`成员变量。

#### 2.2.2 成员变量symbol

**symbol的内容**

| AST类型 | symbol的类型                      | e.g. SysY        | Koopa IR             | symbol |
| ------- | --------------------------------- | ---------------- | -------------------- | ------ |
| 常量    | 常量的值                          | const int a = 0; |                      | 0      |
| 变量    | 指向变量的指针                    | int a;           | @a = alloc i32       | @a     |
| LVal    | 值，另用loc_sym存放指向变量的指针 | a = 10;          | store 10, @a         | 10     |
| 数组    | 数组指针                          | int a[2];        | @a = alloc [i32, 2]  | @a     |
| 指针    | 指向数组指针的指针                | int a[][2];      | @a = alloc *[i32, 2] | @a     |

AST类型的概念是为了描述方便，编译器中并不存在这样的数据结构，一个相近的数据结构是下文的 `SymbolInfo::tag`。一个实例是，当PrimaryExpAST对应SysY中的常量时，其成员变量 `symbol`为常量的值，而当其对应SysY中的变量时，`symbol`为 `load`语句的结果。这样，上级AST就可以用该symbol直接输出Koopa IR，如 `std::cout << "  " << symbol << " = sub 0, " << unary_exp->symbol << std::endl;`。

**symbol的命名**

- 为避免符号重名，维护全局递增计数器sym_cnt，每个symbol符号都命名为 `(@|%)[SysY变量符号_]<序号>`的形式，如 `@a_0`, `%1`等。
- `main`函数和库函数除外，因为Koopa规范规定 `main`函数的Koopa IR符号必须是main。
- 容易证明所有符号都不会重名。

#### 2.2.3 其它

**语法分析部分**

`if...else...`语句涉及到二义性问题，因此，本编译器使用Yacc的 `%prec`标记，以使 `else`和最近未匹配的 `if`匹配：

```cpp
//规定if-else匹配优先级，以解决空悬else问题
%precedence IFX
%precedence ELSE
```

**语义分析和中间代码生成部分**

前端部分的全局信息包括符号计数器 `sym_cnt`（上面已经说明）、符号表 `sym_tab`（下面将要说明）和当前基本块跳转标识 `has_jp.`

这是由于，SysY中有 `return`, `break`, `continue`等多种语句对应Koopa IR的跳转语句，而Koopa IR中一个基本块有且只能有一条跳转语句作为结尾。为符合规则，设计`has_jp`表明当前基本块有无跳转指令，若已有跳转指令，则后续指令均不可达，不再为其生成Koopa IR，这也是一个优化；若基本块结尾无跳转指令，则补上跳转指令。

**目标代码生成部分**

后端部分的全局信息为 `StackInfo stk`对象，用于维护当前处理的SysY函数的栈上内存分配，以便处理各种指令的 `Visit`函数查询使用。

```cpp
class StackInfo
{
public:
    void alloc(const koopa_raw_function_t &func);

    void free(const koopa_raw_function_t &func);

    bool has_val(const koopa_raw_value_t &value);

    int offset(const koopa_raw_value_t &value);

    int size();

    int size_of_R();

private:
    std::unordered_map<koopa_raw_value_t, int> val_offset;
    int stk_sz;
    int R;

};
```

### 2.3 主要设计考虑及算法选择

#### 2.3.1 符号表的设计考虑

以symtab.hpp中的代码来说明符号表的设计：

```cpp
/**
 * 符号表symbol table.
 * 符号表符号的定义与ast中的Koopa IR符号symbol不同.
 * Koopa IR符号包括源程序中的常量对应的符号（以数字表示）、
 * 变量对应的符号（以@<变量符号>_<序号>表示），
 * 以及为表达式生成的、以"%"开头的符号. 
 * 符号表符号只包括源程序中的变量和常量对应的符号.
 */

enum class SymbolTag
{
    CONST,
    VAR,
    ARRAY,
    PTR,
    VOID, // 无返回值的函数
    INT   // 返回类型为int的函数
};

class SymbolInfo
{
public:
    SymbolTag tag;
    std::string symbol;    // koopa IR符号
    std::vector<int> dims; // 数组或数组指针的维数
    // 成员函数...
};

// 一个作用域有一个作用域符号表ScopeSymbolTable
class ScopeSymbolTable
{
public:
    std::unordered_map<std::string, std::shared_ptr<SymbolInfo>> scope_tab;
    // 成员函数...
}

// 不同作用域的ScopeSymbolTable栈式进出
class SymbolTable
{
public:
    std::deque<std::unique_ptr<ScopeSymbolTable>> table;
    // 成员函数...
}
```

**符号的管理**

用下表来分析符号表tag与符号表symbol内容的对应关系：

| SymbolInfo::tag | SymbolInfo::symbol的类型 | e.g. SysY        | Koopa IR             | symbol |
| --------------- | ------------------------ | ---------------- | -------------------- | ------ |
| CONST           | 常量值                   | const int a = 0; |                      | 0      |
| VAR             | 指针                     | int a;           | @a = alloc i32       | @a     |
| ARRAY           | 数组指针                 | int a[2];        | @a = alloc [i32, 2]  | @a     |
| PTR             | 指向数组指针的指针       | int a[][2]       | @a = alloc *[i32, 2] | @a     |
| VOID            | 函数名                   | void foo()       | fun @foo()           | @foo   |
| INT             | 函数名                   | int main()       | fun main()           | main   |

**变量作用域的管理**

实现变量作用域管理的关键在于以下4点：

1. 在SymbolTable的构造函数插入全局符号表。
2. 进入/退出一个作用域时，调用push/pop函数。
3. 插入符号时，向最上层作用域符号表插入符号：

```cpp
/**
 * @brief 向符号表中添加一个符号, 同时记录这个符号的值
 *
 * @param ident     SysY标识符
 * @param tag       符号类型
 * @param symbol    Koopa IR符号
 */
void insert(const std::string &ident,
            const SymbolTag tag,
            const std::string &symbol,
            const std::vector<int> &dims = {})
{
    assert(!(table.back()->contains(ident)));
    table.back()->insert(ident, tag, symbol, dims);
}
```

4. 查找符号时，从最上层作用域符号表开始查找。

这样查找到的变量总是所有同名变量中位于相对最上层的一个，满足了变量作用域的定义。

#### 2.3.2 寄存器分配策略

所有变量都保存在栈上，在开始处理一个函数时调用 `StackInfo::alloc`函数为所有带返回值的Koopa IR指令分配栈上内存，栈帧结构与实验文档相同。

## 三、编译器实现

### 3.1 各阶段编码细节

#### Lv1. main函数和Lv2. 初试目标代码生成

注意Flex和Bison的语法规则，如 `%union`中的 `str_val`, `int_val`, `ast_val`等将出现在 `%type`标识后，声明返回值的类型。

本编译器在 `str_val`, `int_val`和 `ast_val`类型外，增加了 `ExpBaseAST *exp_val`以兼顾代码的简单与灵活性。

#### Lv3. 表达式

注意语法规范中已经蕴含了优先级，高优先级运算结果在语法树中更靠近叶子，ast递归调用 `IR`时自然会先处理高优先级运算的Koopa IR，无需额外处理。

#### Lv4. 常量和变量

本编译器实现了符号表，上文分析了它的结构。

#### Lv5. 语句块和作用域

本编译器实现了作用域符号表，使其在符号表中栈式进出。

#### Lv6. if语句

**if/else二义性**

`if...else...`语句涉及到二义性问题，因此，本编译器使用Yacc的 `%prec`标记，以使 `else`和最近未匹配的 `if`匹配：

```cpp
//规定if-else匹配优先级，以解决空悬else问题
%precedence IFX
%precedence ELSE
```

**短路求值**

以LOrExpAST为例，本质上要将

```cpp
int result = 1;
if (lhs == 0)
{
    result = rhs != 0;
}
```

转换成Koopa IR输出。LAndExpAST完全类似。

#### Lv7. while语句

本编译器在StmtAST中维护 `whlie_cnt_stk`，保存当前 `while`语句的序号，以使 `break`和 `continue`语句能够跳转到栈顶序号对应的 `label`.

#### Lv8. 函数和全局变量

本编译器实现了`StackInfo`类用于函数栈帧上内存分配。

#### Lv9. 数组

**多维数组初始化**

本编译器在 `ConstDefAST`中定义了一系列成员函数以实现数组初始化：

```cpp
/**
 * @brief 进到待编译程序给出的初始化列表的一个大括号里，填充初始化列表
 *
 * @param init_vals         待编译程序给出的初始化列表的一个大括号对应的AST数组
 * @param full_init_vals    待填充的初始化列表
 * @param is_first          是否是第一个大括号
 * @return int              该大括号负责初始化的长度
 */
int fill_init_vals(const std::vector<std::unique_ptr<ConstInitValAST>>
                        &init_vals,
                    std::vector<std::string> &full_init_vals, bool is_first = false);

/**
 * @brief 当前已填充长度的对齐值，即当前大括号负责初始化的长度
 *
 * @param len       当前已填充长度
 * @return int      对齐值
 */
int aligned_len(int len);

/**
 * @brief 生成取数组指针和存初始值的Koopa IR
 *
 * @param full_init_vals    初始化列表
 * @param symbol            指针的Koopa IR符号
 * @param dim               当前维度
 */
void get_ptr_store_val(const std::vector<std::string> &full_init_vals,
                        const std::string &symbol, int dim);

/**
 * @brief 输出当前大括号的初始化列表
 *
 * @param full_init_vals    初始化列表
 * @param dim               当前维度
 */
void print_aggr(const std::vector<std::string> &full_init_vals, int dim);
```

生成中间代码时，`IR`函数会调用 `fill_init_vals`得到完整的初始化列表，接着调用 `print_aggr`实现 `alloc`语句，最后调用 `get_ptr_store_val`实现 `getelemptr`和 `store`等语句，完成数组初始化的Koopa IR代码。

**数组参数**

作者认为，数组参数部分的困难很大程度上是由于SysY和Koopa IR对应符号的语义不对齐，由此产生一些细微之处需要特别处理，而文档中的示例并未完全展示它们。通过分析，总结以下两条规则：

1. 与数组不同，SysY中的指针第一次解引用，对应的Koopa IR需要使用 `getptr`，而不是 `getelemptr`。例如，`a[][2]`用 `a[1]`解引用时，对应的Koopa IR应该使用 `<next_symbol> = getptr <current_symbol>, 1`而不是 `<next_symbol> = getelemptr <current_symbol>, 1`。
2. 显然，遇到解引用时，需要按照下标顺序，逐步使用 `<next_symbol> = getelemptr <current_symbol>, <index>`解引用，直到解引用完毕，得到一个值或指针。类似：

```cpp
auto cur_sym = sym_info->symbol;
for (auto &exp : exps)
{
    auto next_sym = "%ptr_" + std::to_string(sym_cnt++);
    std::cout << "  " << next_sym << " = getelemptr " << cur_sym << ", " << exp->symbol << std::endl;
    cur_sym = next_sym;
}
```

**但是**，接下来还需要进一步处理，增加相应的Koopa IR语句。例如，`int a[][2]`用a[1]解引用时（会作为 `b[]`形参的实参），对应的Koopa IR得到一个 `*[i32, 2]`类型的符号，此时还需要加一个 `<next_symbol> = getelemptr <current_symbol>, 0`语句，得到 `*i32`类型的符号（以与 `b[]`的Koopa IR符号类型相匹配）。

分析数组和指针被解引用时的用途和需要添加的语句，如下表所示：

|            | 数组                   | 指针                   |
| ---------- | ---------------------- | ---------------------- |
| 不解引用   | 用作参数，加getelemptr | 用作参数               |
| 部分解引用 | 用作参数，加getelemptr | 用作参数，加getelemptr |
| 完全解引用 | 加load                 | 加load                 |

完成这样的分类讨论后，代码就只是按照表格填空了。

### 3.2 工具软件介绍（若未使用特殊软件或库，则本部分可略过）

1. `Flex/Bison`：词法分析和语法分析；
2. `LibKoopa`：将文本形式的Koopa IR转换为内存形式的Koopa IR。

### 3.3 测试情况说明（如果进行过额外的测试，可增加此部分内容）

虽然是实践开始部分的内容，也是作业题，但是据我所知，段注释`BlockComment`的处理是许多同学都犯过的错。以下测试用例可以测试出一些BlockComment的定义问题：

```cpp
/**
 * "*"
 * “/”
 * “/*”
 * "/**"
 * /*
 */

int main()
{
    return 0;
}
```

## 四、实习总结

### 4.1 收获和体会

**体会**：感谢完善的实践文档，让开学前没有接触过编译原理的我也能轻松入门，这与课程的学习也相互促进：上课讲到语法分析和语义分析时，我可以联想到编译器对应部分的代码，更清楚这些抽象概念具体在完成什么任务；课程的内容，也加深了我对代码背后原理的理解。

**收获**：项目加深了我对编译的理解，提高了我的项目设计、组织和编码能力。

### 4.2 学习过程中的难点，以及对实习过程和内容的建议

我眼中的难点在于koopa.h部分的理解和数组解引用部分的分析。

koopa.h部分的困难之处在于，仅从该文件的注释，我不能完全理解各个结构体和成员变量的意义，有时需要理解rust源代码或打印输出才能理解各个变量的用法，进而利用内存形式生成目标代码。建议考虑提示注释的详细程度，如明确说明`koopa_raw_value_t`的`ty`成员变量是“类型”，侧重“返回值”，而`kind`成员变量是“类别”，侧重“内容”等。

数组解引用部分的困难之处上面已经详细说明，建议考虑增加更多示例或提示，以方便同学们分析。

### 4.3 对老师讲解内容与方式的建议

对上课理论部分的建议是，在内容上，可以增加一些实例，展示思考和分析的过程，例如如何从一个文法看出它表示的语言，这会对同学们的理解和考试有所帮助。

在方式上，因为老师经常有较长的、连续的讲述，建议明确当前正在讨论的问题，以让同学们更容易跟上思路。

总体上，老师讲解清晰，助教认真负责。感谢老师和助教一学期的付出！
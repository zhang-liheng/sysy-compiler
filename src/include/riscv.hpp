#pragma once

#include <iostream>
#include <string>
#include <unordered_map>

#include "koopa.h"

// #define DEBUG
#ifdef DEBUG
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif

void BuildRiscv(const std::string &koopa_str);
void Visit(const koopa_raw_program_t &program);
void Visit(const koopa_raw_slice_t &slice);
void Visit(const koopa_raw_function_t &func);
void Visit(const koopa_raw_basic_block_t &bb);
void Visit(const koopa_raw_value_t &value);
void Visit(const koopa_raw_global_alloc_t &alloc);
void Visit(const koopa_raw_load_t &load);
void Visit(const koopa_raw_store_t &store);
void Visit(const koopa_raw_binary_t &binary);
void Visit(const koopa_raw_branch_t &branch);
void Visit(const koopa_raw_jump_t &jump);
void Visit(const koopa_raw_call_t &call);
void Visit(const koopa_raw_return_t &ret);
void Visit(const koopa_raw_get_ptr_t &get_ptr);
void Visit(const koopa_raw_get_elem_ptr_t &get_elem_ptr);
void Prologue();
void Epilogue();
int SizeOfType(koopa_raw_type_t ty);
void Load(const std::string &dest, const koopa_raw_value_t &src);
void Store(const std::string &src, const koopa_raw_value_t &dest);
void VisitGlobalAlloc(const koopa_raw_value_t value);
void GetInitVals(const koopa_raw_value_t &init, std::vector<int> &vals);

class StackInfo
{
private:
    std::unordered_map<koopa_raw_value_t, int> val_offset;
    int stk_sz;
    int R;

public:
    /**
     * @brief 分配栈空间
     *
     * @param func
     */
    void alloc(const koopa_raw_function_t &func);

    void free(const koopa_raw_function_t &func);

    bool has_val(const koopa_raw_value_t &value);

    int offset(const koopa_raw_value_t &value);

    int size();

    int size_of_R();
};

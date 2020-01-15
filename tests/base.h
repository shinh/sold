#pragma once

int base_42();
int base_add_42(int x);
void base_use_stderr();
int base_init_value();

extern int base_global;

class Base {
public:
    virtual ~Base() {}
    virtual int vf() = 0;
};

Base* MakeBase();

int base_thread_var();
int base_thread_var_reloc();

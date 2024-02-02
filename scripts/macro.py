from itertools import islice
def batched(iterable, n):
    # batched('ABCDEFG', 3) --> ABC DEF G
    if n < 1:
        raise ValueError('n must be at least one')
    it = iter(iterable)
    while batch := tuple(islice(it, n)):
        if len(batch) == n:
            yield batch

def gen_batch_macro(macro_name, every, max_args, callback, separator=", "):
    print(f"#define {macro_name}(...) CONCAT({macro_name}, MacroArgCount(__VA_ARGS__))(__VA_ARGS__)")
    for j in range(0, max_args*3+1): # 每一个参数长度创建一个宏
        print(gen_one_macro(f"{macro_name}{j}", every, j, callback, separator))
def gen_one_macro(macro_name, every, arg_cnt, callback, separator=", "):
    """
    arg_cnt个参数中 每 every个参数取第 pos 个参数. pos 从 1 开始
    如 pos= 2, every=3, arg_cnt=9, 则返回
    #define _MACRO_GET1_EVERY3_9(_1, _2, _3, _4, _5, _6, _7, _8, _9) _2, _5, _8
    """
    args = [f"_{i+1}" for i in range(arg_cnt)]
    ress = [callback(*args) for args in batched(args, every)]
    return f"#define {macro_name}({', '.join(args)}) {separator.join(ress)}"

def gen_macro_arg_cnt(max_args):
    """
    max_args=10 返回:
    #define MacroArgCount(...) _MacroArgCount(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
    #define _MacroArgCount(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, COUNT, ...) COUNT
    """
    args1 = [f"{i}" for i in range(max_args, 0, -1)]
    args2 = [f"_{i}" for i in range(max_args)]
    print(f"#define MacroArgCount(...) _MacroArgCount(__VA_ARGS__, {', '.join(args1)})") 
    print(f"#define _MacroArgCount({', '.join(args2)}, COUNT, ...) COUNT")

def gen_macro(max_args):

    for i in range(1, 3+1): # 对每一个获取的位置
        pos = i
        every = 3
        gen_batch_macro(f"_MACRO_GET{pos}_EVERY{every}_", 3, max_args, lambda *t: t[pos-1])

def gen_GEN_PARAMS(max_args):
    gen_batch_macro(f"_MACRO_GEN_PARAMS_", 3, max_args, (lambda name, type, init: f"{type} {name} = {init};"), "")

def gen_cancat():
    """
    #define CONCAT(A, B) _CONCAT(A, B)
    #define _CONCAT(A, B) A##B
    """
    print("#define CONCAT(A, B) _CONCAT(A, B)")
    print("#define _CONCAT(A, B) A##B")

if __name__ == "__main__":
    max_args = 10
    gen_cancat()
    gen_macro_arg_cnt(max_args)
    gen_macro(max_args)
    gen_GEN_PARAMS(max_args)
#include "mirage.h"

internal constexpr i64 reg_params_max = 4;

internal constexpr i64 outbuf_size = MB(1);
// internal char outbuf[outbuf_size];
internal char outbuf[MB(1)];
internal i64 outpos;

internal FILE *output_file;

internal int depth;

internal char *argreg8[] = { "cl", "dl", "r8b", "r9b" };
internal char *argreg16[] = { "cx", "dx", "r8w", "r9w" };
internal char *argreg32[] = { "ecx", "edx", "r8d", "r9d" };
internal char *argreg64[] = { "rcx", "rdx", "r8", "r9" };

internal Obj *current_fn;

internal void gen_expr(Node *node);
internal void gen_stmt(Node *node);

internal void flush_output()
{
    if (outpos == 0) {
        return;
    }

    fwrite(outbuf, 1, outpos, output_file);
    outpos = 0;
}

internal void println(char *fmt, ...)
{
#if 0
    va_list p;
    va_start(p, fmt);
    vprintf(fmt, p);
    printf("\n");
#else
    va_list ap;
    va_start(ap, fmt);

    i64 left = outbuf_size - outpos;
    i64 n = vsnprintf(outbuf + outpos, left, fmt, ap);
    va_end(ap);

    if (n >= left) {
        flush_output();

        va_start(ap, fmt);
        n = vsnprintf(outbuf, outbuf_size, fmt, ap);
        va_end(ap);
    }

    outpos += n;
    outbuf[outpos++] = '\n';
#endif
}

internal int count(void)
{
    local_persist int i = 0;
    return ++i;
}

internal void push(void)
{
    println("  push rax");
    ++depth;
}

internal void pop(char *arg)
{
    println("  pop %s", arg);
    --depth;
}

internal void pushf(void)
{
    println("  sub rsp, 8");
    println("  movsd [rsp], xmm0");
    ++depth;
}

internal void popf(int reg)
{
    println("  movsd xmm%d, [rsp]", reg);
    println("  add rsp, 8");
    --depth;
}

internal void copy_struct(Type *ty, int offset)
{
    for (int i = 0; i < ty->size; i++) {
        println("  mov r10b, byte ptr [rax + %d]", i);
        println("  mov byte ptr [rsp + %d], r10b", offset + i);
    }
    println("  lea rax, [rsp + %d]", offset);
}

internal void push_struct(Type *ty)
{
    if (ty->size == 1) {
        println("  movzx eax, byte ptr [rax]");
    } else if (ty->size == 2) {
        println("  movzx eax, word ptr [rax]");
    } else if (ty->size == 4) {
        println("  mov eax, dword ptr [rax]");
    } else {
        println("  mov rax, [rax]");
    }

    push();
}

internal bool is_reg_aggregate(Type *ty)
{
    return ty->size == 1 || ty->size == 2 || ty->size == 4 || ty->size == 8;
}

internal void get_args_stack(Node *args, int *stack_struct, int *stack_arg)
{
    int nregs = 0;
    for (Node *arg = args; arg; arg = arg->next) {
        Type *ty = arg->ty;

        if ((ty->kind == TY_STRUCT || ty->kind == TY_UNION) && !is_reg_aggregate(ty)) {
            *stack_struct += (align_to(ty->size, 8) / 8);
        }

        if (nregs++ >= reg_params_max) {
            ++*stack_arg;
        }
    }
}

internal void push_args(Node *args, int *struct_offset)
{
    if (!args) {
        return;
    }

    push_args(args->next, struct_offset);

    Type *ty = args->ty;
    gen_expr(args);

    if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
        if (is_reg_aggregate(ty)) {
            push_struct(ty);
        } else {
            copy_struct(ty, *struct_offset);
            *struct_offset += align_to(ty->size, 8);
            push();
        }
    } else if (is_flonum(ty)) {
        pushf();
    } else {
        push();
    }

    *struct_offset += 8;
}

// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
int align_to(int n, int align)
{
    int result = (n + align - 1) / align * align;
    return result;
}

// Compute the absolute address of a given node.
// It's an error if a given node does not reside in memory.
internal void gen_addr(Node *node)
{
    switch (node->kind) {
    case ND_VAR:
        if (node->var->is_local) {
            // Local variable
            println("  lea rax, [rbp + %d]", node->var->offset);
        } else {
            // Global variable and function
            println("  lea rax, [rip + %s]", node->var->name);
        }
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_addr(node->rhs);
        return;
    case ND_MEMBER:
        gen_addr(node->lhs);
        println("  add rax, %d", node->member->offset);
        return;
    default:
        error_tok(node->tok, "not an lvalue");
    }
}

// Load a value from where %rax is pointing to.
internal void load(Type *ty)
{
    switch (ty->kind) {
    case TY_FUNC:
    case TY_ARRAY:
    case TY_STRUCT:
    case TY_UNION:
        // If it is an array, do not attempt to load a value to the
        // register because in general we can't load an entire array to a
        // register. As a result, the result of an evaluation of an array
        // becomes not the array itself but the address of the array.
        // This is where "array is automatically converted to a pointer to
        // the first element of the array in C" occurs.
        return;
    case TY_FLOAT:
        println("  movss xmm0, [rax]");
        return;
    case TY_DOUBLE:
        println("  movsd xmm0, [rax]");
        return;
    default:
        break;
    }

    char *insn = ty->is_unsigned ? "movzx" : "movsx";

    // When we load a char or a short value to a register, we always
    // extend them to the size of int, so we can assume the lower half of
    // a register always contains a valid value. The upper half of a
    // register for char, short and int may contain garbage. When we load
    // a long value to a register, it simply occupies the entire register.
    if (ty->size == 1) {
        println("  %s eax, byte ptr [rax]", insn);
    } else if (ty->size == 2) {
        println("  %s eax, word ptr [rax]", insn);
    } else if (ty->size == 4) {
        println("  mov eax, dword ptr [rax]");
    } else {
        println("  mov rax, [rax]");
    }
}

// Store %rax to an address that the stack top is pointing to.
internal void store(Type *ty)
{
    pop("r10");

    switch (ty->kind) {
    case TY_STRUCT:
    case TY_UNION:
        for (int i = 0; i < ty->size; ++i) {
            println("  mov r8b, byte ptr [rax + %d]", i);
            println("  mov byte ptr [r10 + %d], r8b", i);
        }
        return;
    case TY_FLOAT:
        println("  movss [r10], xmm0");
        return;
    case TY_DOUBLE:
        println("  movsd [r10], xmm0");
        return;
    default:
        break;
    }

    if (ty->size == 1) {
        println("  mov [r10], al");
    } else if (ty->size == 2) {
        println("  mov [r10], ax");
    } else if (ty->size == 4) {
        println("  mov [r10], eax");
    } else {
        println("  mov [r10], rax");
    }
}

internal void cmp_zero(Type *ty)
{
    switch (ty->kind) {
    case TY_FLOAT:
        println("  xorps xmm1, xmm1");
        println("  ucomiss xmm0, xmm1");
        return;
    case TY_DOUBLE:
        println("  xorpd xmm1, xmm1");
        println("  ucomisd xmm0, xmm1");
        return;
    default:
        break;
    }

    if (is_integer(ty) && ty->size <= 4) {
        println("  test eax, eax");
    } else {
        println("  test rax, rax");
    }
}

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64 };

internal int getTypeId(Type *ty)
{
    switch (ty->kind) {
    case TY_CHAR:
        return ty->is_unsigned ? U8 : I8;
    case TY_SHORT:
        return ty->is_unsigned ? U16 : I16;
    case TY_INT:
        return ty->is_unsigned ? U32 : I32;
    case TY_LONG:
        return ty->is_unsigned ? U64 : I64;
    case TY_FLOAT:
        return F32;
    case TY_DOUBLE:
        return F64;
    default:
        return U64;
    }
}

// The table for type casts
internal char i32i8[]  = "movsx eax, al";
internal char i32u8[]  = "movzx eax, al";
internal char i32i16[] = "movsx eax, ax";
internal char i32u16[] = "movzx eax, ax";
internal char i32f32[] = "cvtsi2ss xmm0, eax";
internal char i32i64[] = "movsxd rax, eax";
internal char i32f64[] = "cvtsi2sd xmm0, eax";

internal char u32f32[] = "mov eax, eax; cvtsi2ss xmm0, rax";
internal char u32i64[] = "mov eax, eax";
internal char u32f64[] = "mov eax, eax; cvtsi2sd xmm0, rax";

internal char i64f32[] = "cvtsi2ss xmm0, rax";
internal char i64f64[] = "cvtsi2sd xmm0, rax";

internal char u64f64[] =
  "test rax,rax; js 1f; pxor xmm0,xmm0; cvtsi2sd xmm0,rax; jmp 2f; "
  "1: mov r10,rax; and eax,1; pxor xmm0,xmm0; shr r10; "
  "or r10,rax; cvtsi2sd xmm0,r10; addsd xmm0,xmm0; 2:";

internal char f32i8[]  = "cvttss2si eax, xmm0; movsx eax, al";
internal char f32u8[]  = "cvttss2si eax, xmm0; movzx eax, al";
internal char f32i16[] = "cvttss2si eax, xmm0; movsx eax, ax";
internal char f32u16[] = "cvttss2si eax, xmm0; movzx eax, ax";
internal char f32i32[] = "cvttss2si eax, xmm0";
internal char f32u32[] = "cvttss2si rax, xmm0";
internal char f32f64[] = "cvtss2sd xmm0, xmm0";

internal char f64i8[]  = "cvttsd2si eax, xmm0; movsx eax, al";
internal char f64u8[]  = "cvttsd2si eax, xmm0; movzx eax, al";
internal char f64i16[] = "cvttsd2si eax, xmm0; movsx eax, ax";
internal char f64u16[] = "cvttsd2si eax, xmm0; movzx eax, ax";
internal char f64i32[] = "cvttsd2si eax, xmm0";
internal char f64u32[] = "cvttsd2si rax, xmm0";
internal char f64f32[] = "cvtsd2ss xmm0, xmm0";
internal char f64i64[] = "cvttsd2si rax, xmm0";

#define u64f32 i64f32
#define f32i64 f32u32
#define f32u64 f32u32
#define f64u64 f64i64

internal char *cast_table[][10] = {
    // i8    i16     i32     i64     u8     u16     u32     u64     f32     f64
    { NULL,  NULL,   NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64 }, // i8
    { i32i8, NULL,   NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64 }, // i16
    { i32i8, i32i16, NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64 }, // i32
    { i32i8, i32i16, NULL,   NULL,   i32u8, i32u16, NULL,   NULL,   i64f32, i64f64 }, // i64

    { i32i8, NULL,   NULL,   i32i64, NULL,  NULL,   NULL,   i32i64, i32f32, i32f64 }, // u8
    { i32i8, i32i16, NULL,   i32i64, i32u8, NULL,   NULL,   i32i64, i32f32, i32f64 }, // u16
    { i32i8, i32i16, NULL,   u32i64, i32u8, i32u16, NULL,   u32i64, u32f32, u32f64 }, // u32
    { i32i8, i32i16, NULL,   NULL,   i32u8, i32u16, NULL,   NULL,   u64f32, u64f64 }, // u64

    { f32i8, f32i16, f32i32, f32i64, f32u8, f32u16, f32u32, f32u64, NULL,   f32f64 }, // f32
    { f64i8, f64i16, f64i32, f64i64, f64u8, f64u16, f64u32, f64u64, f64f32, NULL   }, // f64
};

internal void cast(Type *from, Type *to)
{
    if (to->kind == TY_VOID) {
        return;
    }

    if (to->kind == TY_BOOL) {
        cmp_zero(from);
        println("  setne al");
        println("  movzx eax, al");
        return;
    }

    int t1 = getTypeId(from);
    int t2 = getTypeId(to);
    if (cast_table[t1][t2]) {
        println("  %s", cast_table[t1][t2]);
    }
}

void gen_expr(Node *node)
{
    switch (node->kind) {
    case ND_NULL_EXPR:
        return;
    case ND_NUM: {
        union { f32 f32; f64 f64; u32 u32; u64 u64; } u;

        switch (node->ty->kind) {
        case TY_FLOAT:
            u.f32 = node->fval;
            println("  mov eax, %u  # float %f", u.u32, node->fval);
            println("  movd xmm0, eax");
            return;
        case TY_DOUBLE:
            u.f64 = node->fval;
            println("  mov rax, %llu  # double %f", (unsigned long long)u.u64, node->fval);
            println("  movq xmm0, rax");
            return;
        default:
            println("  mov rax, %lld", node->val);
            return;
        }
    }
    case ND_NEG:
        gen_expr(node->lhs);

        switch (node->ty->kind) {
        case TY_FLOAT:
            println("  mov rax, 1");
            println("  shl rax, 31");
            println("  movq xmm1, rax");
            println("  xorps xmm0, xmm1");
            return;
        case TY_DOUBLE:
            println("  mov rax, 1");
            println("  shl rax, 63");
            println("  movq xmm1, rax");
            println("  xorpd xmm0, xmm1");
            return;
        default:
            println("  neg rax");
            return;
        }
    case ND_VAR:
    case ND_MEMBER:
        gen_addr(node);
        load(node->ty);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        load(node->ty);
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_expr(node->rhs);
        return;
    case ND_CAST:
        gen_expr(node->lhs);
        cast(node->lhs->ty, node->ty);
        return;
    case ND_MEMZERO:
        // RDI is callee-saved on Win64. Preserve it around REP STOSB.
        println("  push rdi");
        println("  mov rcx, %d", node->var->ty->size);
        println("  lea rdi, [rbp + %d]", node->var->offset);
        println("  xor al, al");
        println("  rep stosb");
        println("  pop rdi");
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        store(node->ty);
        return;
    case ND_STMT_EXPR:
        for (Node *n = node->body; n; n = n->next) {
            gen_stmt(n);
        }
        return;
    case ND_COND: {
        int c = count();
        gen_expr(node->cond);
        cmp_zero(node->cond->ty);
        println("  je .L.else.%d", c);
        gen_expr(node->then);
        println("  jmp .L.end.%d", c);
        println(".L.else.%d:", c);
        gen_expr(node->els);
        println(".L.end.%d:", c);
        return;
    }
    case ND_NOT:
        gen_expr(node->lhs);
        cmp_zero(node->lhs->ty);
        println("  sete al");
        println("  movzx eax, al");
        return;
    case ND_BITNOT:
        gen_expr(node->lhs);
        println("  not rax");
        return;
      case ND_LOGAND: {
          int c = count();
          gen_expr(node->lhs);
          cmp_zero(node->lhs->ty);
          println("  je .L.false.%d", c);
          gen_expr(node->rhs);
          cmp_zero(node->rhs->ty);
          println("  je .L.false.%d", c);
          println("  mov eax, 1");
          println("  jmp .L.end.%d", c);
          println(".L.false.%d:", c);
          println("  xor eax, eax");
          println(".L.end.%d:", c);
          return;
      }
      case ND_LOGOR: {
          int c = count();
          gen_expr(node->lhs);
          cmp_zero(node->lhs->ty);
          println("  jne .L.true.%d", c);
          gen_expr(node->rhs);
          cmp_zero(node->rhs->ty);
          println("  jne .L.true.%d", c);
          println("  xor eax, eax");
          println("  jmp .L.end.%d", c);
          println(".L.true.%d:", c);
          println("  mov eax, 1");
          println(".L.end.%d:", c);
          return;
      }
    case ND_FUNCALL: {
        int stack_struct = 0, stack_arg = 0;
        get_args_stack(node->args, &stack_struct, &stack_arg);

        int stack = stack_struct + stack_arg;
        if ((depth + stack) % 2) {
            println("  sub rsp, 8");
            depth++;
            stack++;
        }

        if (stack_struct > 0) {
            println("  sub rsp, %d", stack_struct * 8);
            depth += stack_struct;
        }
        int struct_offset = 0;
        push_args(node->args, &struct_offset);

        // The call function are stored in rax
        gen_expr(node->lhs);

        int nreg = 0;
        for (Node *arg = node->args; arg && nreg < reg_params_max; arg = arg->next) {
            if (is_flonum(arg->ty)) {
                popf(nreg);
                // Win64 only requires mirroring FP args into GPRs for variadic calls.
                if (node->func_ty->is_variadic) {
                    println("  movq %s, xmm%d", argreg64[nreg], nreg);
                }
            } else {
                pop(argreg64[nreg]);
            }
            nreg++;
        }

        println("  mov r10, rax");
        println("  sub rsp, 32");
        println("  call r10");
        println("  add rsp, 32");

        if (stack) {
            println("  add rsp, %d", stack * 8);
            depth -= stack;
        }

        // It looks like the most significant 48 or 56 bits in RAX may
        // contain garbage if a function return type is short or bool/char,
        // respectively. We clear the upper bits here.
        switch (node->ty->kind) {
        case TY_BOOL:
            println("  movzx eax, al");
            return;
        case TY_CHAR:
            if (node->ty->is_unsigned)
                println("  movzx eax, al");
            else
                println("  movsx eax, al");
            return;
        case TY_SHORT:
            if (node->ty->is_unsigned)
                println("  movzx eax, ax");
            else
                println("  movsx eax, ax");
            return;
        default:
            return;
        }
    }
    default:
        break;
    }

    if (is_flonum(node->lhs->ty)) {
        gen_expr(node->rhs);
        pushf();
        gen_expr(node->lhs);
        popf(1);

        char *sz = (node->lhs->ty->kind == TY_FLOAT) ? "ss" : "sd";

        switch (node->kind) {
        case ND_ADD:
            println("  add%s xmm0, xmm1", sz);
            return;
        case ND_SUB:
            println("  sub%s xmm0, xmm1", sz);
            return;
        case ND_MUL:
            println("  mul%s xmm0, xmm1", sz);
            return;
        case ND_DIV:
            println("  div%s xmm0, xmm1", sz);
            return;
        case ND_EQ:
        case ND_NE:
        case ND_LT:
        case ND_LE:
            println("  ucomi%s xmm1, xmm0", sz);

            if (node->kind == ND_EQ) {
                println("  sete al");
                println("  setnp dl");
                println("  and al, dl");
            } else if (node->kind == ND_NE) {
                println("  setne al");
                println("  setp dl");
                println("  or al, dl");
            } else if (node->kind == ND_LT) {
                println("  seta al");
            } else {
                println("  setae al");
            }

            println("  movzx eax, al");
            return;
        default:
            error_tok(node->tok, "invalid expression");
        }
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("r10");

    char *rax, *r10, *rdx;

    if (node->lhs->ty->kind == TY_LONG || node->lhs->ty->base) {
        rax = "rax";
        r10 = "r10";
        rdx = "rdx";
    } else {
        rax = "eax";
        r10 = "r10d";
        rdx = "edx";
    }

    switch (node->kind) {
    case ND_ADD:
        println("  add %s, %s", rax, r10);
        return;
    case ND_SUB:
        println("  sub %s, %s", rax, r10);
        return;
    case ND_MUL:
        println("  imul %s", r10);
        return;
    case ND_DIV:
    case ND_MOD:
        if (node->ty->is_unsigned) {
            println("  xor %s, %s", rdx, rdx);
            println("  div %s", r10);
        } else {
            if (node->lhs->ty->size == 8) {
                println("  cqo");
            } else {
                println("  cdq");
            }
            println("  idiv  %s", r10);
        }

        if (node->kind == ND_MOD) {
            println("  mov %s, %s", rax, rdx);
        }
        return;
    case ND_BITAND:
        println("  and %s, %s", rax, r10);
        return;
    case ND_BITOR:
        println("  or %s, %s", rax, r10);
        return;
    case ND_BITXOR:
        println("  xor %s, %s", rax, r10);
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        println("  cmp %s, %s", rax, r10);

        if (node->kind == ND_EQ) {
            println("  sete al");
        } else if (node->kind == ND_NE) {
            println("  setne al");
        } else if (node->kind == ND_LT) {
            if (node->lhs->ty->is_unsigned) {
                println("  setb al");
            } else {
                println("  setl al");
            }
        } else if (node->kind == ND_LE) {
            if (node->lhs->ty->is_unsigned) {
                println("  setbe al");
            } else {
                println("  setle al");
            }
        }

        println("  movzx eax, al");
        return;
    case ND_SHL:
        println("  mov rcx, r10");
        println("  shl %s, cl", rax);
        return;
    case ND_SHR:
        println("  mov rcx, r10");
        if (node->lhs->ty->is_unsigned) {
            println("  shr %s, cl", rax);
        } else {
            println("  sar %s, cl", rax);
        }
        return;
    default:
        error_tok(node->tok, "invalid expression");
    }
}

internal void gen_stmt(Node *node)
{
    switch (node->kind) {
    case ND_IF: {
        int c = count();
        gen_expr(node->cond);
        cmp_zero(node->cond->ty);
        println("  je .L.else.%d", c);
        gen_stmt(node->then);
        println("  jmp .L.end.%d", c);
        println(".L.else.%d:", c);
        if (node->els) {
            gen_stmt(node->els);
        }
        println(".L.end.%d:", c);
        return;
    }
    case ND_FOR: {
        int c = count();
        if (node->init) {
            gen_stmt(node->init);
        }
        println(".L.begin.%d:", c);
        if (node->cond) {
            gen_expr(node->cond);
            cmp_zero(node->cond->ty);
            println("  je %s", node->brk_label);
        }
        gen_stmt(node->then);
        println("%s:", node->cont_label);
        if (node->inc) {
            gen_expr(node->inc);
        }
        println("  jmp .L.begin.%d", c);
        println("%s:", node->brk_label);
        return;
    }
    case ND_DO: {
        int c = count();
        println(".L.begin.%d:", c);
        gen_stmt(node->then);
        println("%s:", node->cont_label);
        gen_expr(node->cond);
        cmp_zero(node->cond->ty);
        println("  jne .L.begin.%d", c);
        println("%s:", node->brk_label);
        return;
    }
    case ND_SWITCH:
        gen_expr(node->cond);

        for (Node *n = node->case_next; n; n = n->case_next) {
            char *reg = (node->cond->ty->size == 8) ? "rax" : "eax";
            println("  cmp %s, %lld", reg, n->val);
            println("  je %s", n->label);
        }

        if (node->default_case) {
            println("  jmp %s", node->default_case->label);
        }

        println("  jmp %s", node->brk_label);
        gen_stmt(node->then);
        println("%s:", node->brk_label);
        return;
    case ND_CASE:
        println("%s:", node->label);
        gen_stmt(node->lhs);
        return;
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next) {
            gen_stmt(n);
        }
        return;
    case ND_GOTO:
        println("  jmp %s", node->unique_label);
        return;
    case ND_LABEL:
        println("%s:", node->unique_label);
        gen_stmt(node->lhs);
        return;
    case ND_RETURN:
        if (node->lhs) {
            gen_expr(node->lhs);
        }
        println("  jmp .L.return.%s", current_fn->name);
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    default:
        error_tok(node->tok, "invalid statement");
    }
}

// Assign offsets to local variables.
internal void assign_lvar_offsets(Obj *prog)
{
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (!fn->is_function) {
            continue;
        }

        // If a function has many parameters, some parameters are
        // inevitably passed by stack rather than by register.
        // The first passed-by-stack parameter resides at RBP+48.
        int top = 48;
        int bottom = 0;
        int nargs = 0;

        // Assign offsets to pass-by-stack parameters.
        for (Obj *var = fn->params; var; var = var->next) {
            if (nargs++ < reg_params_max) {
                continue;
            }

            top = align_to(top, 8);
            var->offset = top;
            top += var->ty->size;
        }

        // Assign offsets to pass-by-register parameters and local variables.
        for (Obj *var = fn->locals; var; var = var->next) {
            if (var->offset) {
                continue;
            }

            bottom += var->ty->size;
            bottom = align_to(bottom, var->align);
            var->offset = -bottom;
        }

        fn->stack_size = align_to(bottom, 16);
    }
}

internal void emit_data(Obj *prog)
{
    for (Obj *var = prog; var; var = var->next) {
        if (var->is_function || !var->is_definition) {
            continue;
        }

        if (!var->is_static) {
            println("  .globl %s", var->name);
        }
        println("  .align %d", var->align);

        if (var->init_data) {
            println("  .data");
            println("%s:", var->name);

            Relocation *rel = var->rel;
            int pos = 0;
            while (pos < var->ty->size) {
                if (rel && rel->offset == pos) {
                    println("  .quad %s%+lld", rel->label, rel->addend);
                    rel = rel->next;
                    pos += 8;
                } else {
                    println("  .byte %d", var->init_data[pos++]);
                }
            }
            continue;
        }

        println("  .bss");
        println("%s:", var->name);
        println("  .zero %d", var->ty->size);
    }
}

internal void store_fp(int r, int offset, int sz)
{
    switch (sz) {
    case 4:
        println("  movss [rbp + %d], xmm%d", offset, r);
        return;
    case 8:
        println("  movsd [rbp + %d], xmm%d", offset, r);
        return;
    default:
        m__unreachable();
    }
}

internal void store_gp(int r, int offset, int size)
{
    switch (size) {
    case 1:
        println("  mov [rbp + %d] , %s", offset, argreg8[r]);
        return;
    case 2:
        println("  mov [rbp + %d] , %s", offset, argreg16[r]);
        return;
    case 4:
        println("  mov [rbp + %d] , %s", offset, argreg32[r]);
        return;
    case 8:
        println("  mov [rbp + %d] , %s", offset, argreg64[r]);
        return;
    default:
        for (int i = 0; i < size; i++) {
            println("  mov r10b, byte ptr [%s + %d]", argreg64[r], i);
            println("  mov byte ptr [rbp + %d], r10b", offset + i);
        }
        return;
    }
}

internal void emit_text(Obj *prog)
{
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (!fn->is_function || !fn->is_definition) {
            continue;
        }

        if (!fn->is_static) {
            println("  .globl %s", fn->name);
        }
        println("  .text");
        println("%s:", fn->name);
        current_fn = fn;

        // Prologue
        println("  push rbp");
        println("  mov rbp, rsp");
        if (fn->stack_size >= 4096) {
            println("  mov eax, %d", fn->stack_size);
            println("  call __chkstk");
            println("  sub rsp, rax");
        } else {
            println("  sub rsp, %d", fn->stack_size);
        }

        // Save passed-by-register arguments to the stack
        int i = 0;
        for (Obj *var = fn->params; var; var = var->next) {
            if (var->offset > 0) {
                continue;
            }

            if (is_flonum(var->ty)) {
                store_fp(i++, var->offset, var->ty->size);
            } else {
                store_gp(i++, var->offset, var->ty->size);
            }
        }

        if (fn->va_area) {
            // shadow space stack, base is RBP+16.
            int stack_base = 16;
            for (int r = 0; r < reg_params_max; ++r) {
                println("  mov [rbp + %d], %s", stack_base + (r * 8), argreg64[r]);
            }

            int params_offset = 0;
            for (Obj *var = fn->params; var; var = var->next) {
                params_offset += 8;
            }
            println("  lea rax, [rbp + %d]", stack_base + params_offset);
            println("  mov [rbp + %d], rax", fn->va_area->offset);
        }

        // Emit code
        gen_stmt(fn->body);
        assert(depth == 0);

        // Epilogue
        println(".L.return.%s:", fn->name);
        println("  mov rsp, rbp");
        println("  pop rbp");
        println("  ret");
    }
}

internal char *escape_for_gas(const char *s){
    u64 len = 0;
    for (const char *p = s; *p; ++p) {
        if (*p == '\\' || *p == '"') {
            ++len;
        }
        ++len;
    }

    char *escape = arena_push(1, len + 1);
    len = 0;
    for (const char *p = s; *p; ++p) {
        if (*p == '\\' || *p == '"') {
            escape[len++] = '\\';
        }
        escape[len++] = *p;
    }
    escape[len] = '\0';

    return escape;
}

void codegen(Obj *prog, FILE *out)
{
    output_file = out;

    File **files = get_input_files();
    for (int i = 0; files[i]; i++) {
        println("  .file %d \"%s\"", files[i]->file_no, escape_for_gas(files[i]->name));
    }

    assign_lvar_offsets(prog);
    emit_data(prog);
    emit_text(prog);

    flush_output();
}

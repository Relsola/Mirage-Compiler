#include "mirage.h"

internal FILE *output_file;

internal int depth;

internal char *argreg8[] = { "cl", "dl", "r8b", "r9b" };
internal char *argreg16[] = { "cx", "dx", "r8w", "r9w" };
internal char *argreg32[] = { "ecx", "edx", "r8d", "r9d" };
internal char *argreg64[] = { "rcx", "rdx", "r8", "r9" };

internal Obj *current_fn;

internal void gen_expr(Node *node);
internal void gen_stmt(Node *node);

internal void println(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(output_file, fmt, ap);
    va_end(ap);
    fprintf(output_file, "\n");
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

internal int push_args(Node *arg)
{
    if (!arg) {
        return 0;
    }

    int nargs = push_args(arg->next);
    gen_expr(arg);
    push();
    return nargs + 1;
}

internal int count_args(Node *arg)
{
    int n = 0;
    for (; arg; arg = arg->next) {
        n++;
    }
    return n;
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
    if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
        // If it is an array, do not attempt to load a value to the
        // register because in general we can't load an entire array to a
        // register. As a result, the result of an evaluation of an array
        // becomes not the array itself but the address of the array.
        // This is where "array is automatically converted to a pointer to
        // the first element of the array in C" occurs.
        return;
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

    if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
        for (int i = 0; i < ty->size; i++) {
            println("  mov r8b, byte ptr [rax + %d]", i);
            println("  mov byte ptr [r10 + %d], r8b", i);
        }
        return;
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
    if (is_integer(ty) && ty->size <= 4) {
        println("  cmp eax, 0");
    } else {
        println("  cmp rax, 0");
    }
}

enum { I8, I16, I32, I64, U8, U16, U32, U64 };

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
    default:
        return I64;
    }
}

// The table for type casts
internal char i32i8[]  = "movsx eax, al";
internal char i32u8[]  = "movzx eax, al";
internal char i32i16[] = "movsx eax, ax";
internal char i32u16[] = "movzx eax, ax";
internal char i32i64[] = "movsxd rax, eax";
internal char u32i64[] = "mov eax, eax";

internal char *cast_table[][10] = {
    // i8    i16     i32   i64     u8     u16     u32   u64
    { NULL,  NULL,   NULL, i32i64, i32u8, i32u16, NULL, i32i64 }, // i8
    { i32i8, NULL,   NULL, i32i64, i32u8, i32u16, NULL, i32i64 }, // i16
    { i32i8, i32i16, NULL, i32i64, i32u8, i32u16, NULL, i32i64 }, // i32
    { i32i8, i32i16, NULL, NULL,   i32u8, i32u16, NULL, NULL   }, // i64
    { i32i8, NULL,   NULL, i32i64, NULL,  NULL,   NULL, i32i64 }, // u8
    { i32i8, i32i16, NULL, i32i64, i32u8, NULL,   NULL, i32i64 }, // u16
    { i32i8, i32i16, NULL, u32i64, i32u8, i32u16, NULL, u32i64 }, // u32
    { i32i8, i32i16, NULL, NULL,   i32u8, i32u16, NULL, NULL   }, // u64
};

internal void cast(Type *from, Type *to)
{
    if (to->kind == TY_VOID) {
        return;
    }

    if (to->kind == TY_BOOL) {
        cmp_zero(from);
        println("  setne al");
        println("  movsx eax, al");
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
    case ND_NUM:
        println("  mov rax, %lld", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        println("  neg rax");
        return;
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
        println("  mov rcx, %d", node->var->ty->size);
        println("  lea rdi, [rbp + %d]", node->var->offset);
        println("  mov al, 0");
        println("  rep stosb");
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
        println("  cmp rax, 0");
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
        println("  cmp rax, 0");
        println("  sete al");
        println("  movsx rax, al");
        return;
    case ND_BITNOT:
        gen_expr(node->lhs);
        println("  not rax");
        return;
      case ND_LOGAND: {
          int c = count();
          gen_expr(node->lhs);
          println("  cmp rax, 0");
          println("  je .L.false.%d", c);
          gen_expr(node->rhs);
          println("  cmp rax, 0");
          println("  je .L.false.%d", c);
          println("  mov rax, 1");
          println("  jmp .L.end.%d", c);
          println(".L.false.%d:", c);
          println("  mov rax, 0");
          println(".L.end.%d:", c);
          return;
      }
      case ND_LOGOR: {
          int c = count();
          gen_expr(node->lhs);
          println("  cmp rax, 0");
          println("  jne .L.true.%d", c);
          gen_expr(node->rhs);
          println("  cmp rax, 0");
          println("  jne .L.true.%d", c);
          println("  mov rax, 0");
          println("  jmp .L.end.%d", c);
          println(".L.true.%d:", c);
          println("  mov rax, 1");
          println(".L.end.%d:", c);
          return;
      }
    case ND_FUNCALL: {
        int nargs = count_args(node->args);
        int regargs = nargs < 4 ? nargs : 4;
        int stack_args = nargs - regargs;

        // For Win64, stack arguments must be located right above the 32-byte
        // shadow space. Insert optional 8-byte padding *below* stack args.
        bool needs_pad = ((depth + stack_args) % 2) != 0;
        if (needs_pad) {
            println("  sub rsp, 8");
            depth++;
        }

        push_args(node->args);

        for (int i = 0; i < regargs; i++) {
            pop(argreg64[i]);
        }

        println("  sub rsp, 32");
        println("  mov rax, 0");
        println("  call %s", node->funcname);
        println("  add rsp, 32");

        if (stack_args) {
            println("  add rsp, %d", stack_args * 8);
            depth -= stack_args;
        }

        if (needs_pad) {
            println("  add rsp, 8");
            depth--;
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
        println("  imul %s, %s", rax, r10);
        return;
    case ND_DIV:
    case ND_MOD:
        if (node->ty->is_unsigned) {
            println("  mov %s, 0", rdx);
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
            println("  mov rax, rdx");
        }
        return;
    case ND_BITAND:
        println("  and rax, r10");
        return;
    case ND_BITOR:
        println("  or rax, r10");
        return;
    case ND_BITXOR:
        println("  xor rax, r10");
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

        println("  movzx rax, al");
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
        println("  cmp rax, 0");
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
            println("  cmp rax, 0");
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
        println("  cmp rax, 0");
        println("  jne .L.begin.%d", c);
        println("%s:", node->brk_label);
        return;
    }
    case ND_SWITCH:
        gen_expr(node->cond);

        for (Node *n = node->case_next; n; n = n->case_next) {
            char *reg = (node->cond->ty->size == 8) ? "rax" : "eax";
            println("  cmp %s, %ld", reg, n->val);
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

        int offset = 0;
        for (Obj *var = fn->locals; var; var = var->next) {
            offset += var->ty->size;
            offset = align_to(offset, var->align);
            var->offset = -offset;
        }
        fn->stack_size = align_to(offset, 16);
    }
}

internal void emit_data(Obj *prog)
{
    for (Obj *var = prog; var; var = var->next) {
        if (var->is_function || !var->is_definition) {
            continue;
        }

        if (var->is_static) {
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
    }
    m__unreachable();
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
        println("  sub rsp, %d", fn->stack_size);

        // Save passed-by-register arguments to the stack
        int i = 0;
        for (Obj *var = fn->params; var; var = var->next) {
            store_gp(i++, var->offset, var->ty->size);
        }

        if (fn->va_area) {
            for (int r = 0; r < 4; r++) {
                // call and rbp use 16 byte
                println("  mov [rbp + %d], %s", 16 + (r * 8), argreg64[r]);
            }
            println("  lea rax, [rbp + %d]", fn->va_start_offset);
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

void codegen(Obj *prog, FILE *out)
{
    output_file = out;

    assign_lvar_offsets(prog);
    emit_data(prog);
    emit_text(prog);
}

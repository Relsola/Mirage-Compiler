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
            println("  lea rax, [rbp - %d]", node->var->offset);
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

    // When we load a char or a short value to a register, we always
    // extend them to the size of int, so we can assume the lower half of
    // a register always contains a valid value. The upper half of a
    // register for char, short and int may contain garbage. When we load
    // a long value to a register, it simply occupies the entire register.
    if (ty->size == 1) {
        println("  movsx eax, byte ptr [rax]");
    } else if (ty->size == 2) {
        println("  movsx eax, word ptr [rax]");
    } else if (ty->size == 4) {
        println("  movsxd rax, dword ptr [rax]");
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

enum { I8, I16, I32, I64 };

internal int getTypeId(Type *ty)
{
    switch (ty->kind) {
    case TY_CHAR:
        return I8;
    case TY_SHORT:
        return I16;
    case TY_INT:
        return I32;
    default:
        return I64;
    }
}

// The table for type casts
internal char i32i8[] = "movsx eax, al";
internal char i32i16[] = "movsx eax, ax";
internal char i32i64[] = "movsxd rax, eax";

internal char *cast_table[][10] = {
    // i8    i16     i32   i64
    { NULL,  NULL,   NULL, i32i64 },   // i8
    { i32i8, NULL,   NULL, i32i64 },   // i16
    { i32i8, i32i16, NULL, i32i64 },   // i32
    { i32i8, i32i16, NULL, NULL   },   // i64
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
    case ND_FUNCALL: {
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            push();
            nargs++;
        }

        for (int i = nargs - 1; i >= 0; --i) {
            pop(argreg64[i]);
        }

        println("  sub rsp, 32");
        println("  mov rax, 0");
        println("  call %s", node->funcname);
        println("  add rsp, 32");
        return;
    }
    default:
        break;
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("r10");

    char *rax, *r10;

    if (node->lhs->ty->kind == TY_LONG || node->lhs->ty->base) {
        rax = "rax";
        r10 = "r10";
    } else {
        rax = "eax";
        r10 = "r10d";
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
        if (node->lhs->ty->size == 8) {
            println("  cqo");
        } else {
            println("  cdq");
        }
        println("  idiv  %s", r10);
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LE:
    case ND_LT:
        println("  cmp %s, %s", rax, r10);

        if (node->kind == ND_EQ) {
            println("  sete al");
        } else if (node->kind == ND_NE) {
            println("  setne al");
        } else if (node->kind == ND_LT) {
            println("  setl al");
        } else if (node->kind == ND_LE) {
            println("  setle al");
        }

        println("  movzx rax, al");
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
            println("  je .L.end.%d", c);
        }
        gen_stmt(node->then);
        if (node->inc) {
            gen_expr(node->inc);
        }
        println("  jmp .L.begin.%d", c);
        println(".L.end.%d:", c);
        return;
    }
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next) {
            gen_stmt(n);
        }
        return;
    case ND_RETURN:
        gen_expr(node->lhs);
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
            offset = align_to(offset, var->ty->align);
            var->offset = offset;
        }
        fn->stack_size = align_to(offset, 16);
    }
}

internal void emit_data(Obj *prog)
{
    for (Obj *var = prog; var; var = var->next) {
        if (var->is_function) {
            continue;
        }

        println("  .data");
        println("  .globl %s", var->name);
        println("%s:", var->name);

        if (var->init_data) {
            for (int i = 0; i < var->ty->size; i++) {
                println("  .byte %d", var->init_data[i]);
            }
        } else {
            println("  .zero %d", var->ty->size);
        }
    }
}

internal void store_gp(int r, int offset, int size)
{
    switch (size) {
    case 1:
        println("  mov [rbp - %d] , %s", offset, argreg8[r]);
        return;
    case 2:
        println("  mov [rbp - %d] , %s", offset, argreg16[r]);
        return;
    case 4:
        println("  mov [rbp - %d] , %s", offset, argreg32[r]);
        return;
    case 8:
        println("  mov [rbp - %d] , %s", offset, argreg64[r]);
        return;
    }
    unreachable();
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

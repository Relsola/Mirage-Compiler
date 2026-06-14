#include "mirage.h"

internal int depth;
internal char *argreg[] = {"rcx", "rdx", "r8", "r9"};
internal Obj *current_fn;

internal void gen_expr(Node *node);

internal int count(void)
{
    local_persist int i = 0;
    return ++i;
}

internal void push(void)
{
    printf("  push rax\n");
    ++depth;
}

internal void pop(char *arg)
{
    printf("  pop %s\n", arg);
    --depth;
}


// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
internal int align_to(int n, int align)
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
        int offset = node->var->offset;
        printf("  lea rax, [rbp - %d]\n", offset);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    default:
        error_tok(node->tok, "not an lvalue");
    }
}

// Load a value from where %rax is pointing to.
internal void load(Type *ty)
{
    if (ty->kind == TY_ARRAY) {
        // If it is an array, do not attempt to load a value to the
        // register because in general we can't load an entire array to a
        // register. As a result, the result of an evaluation of an array
        // becomes not the array itself but the address of the array.
        // This is where "array is automatically converted to a pointer to
        // the first element of the array in C" occurs.
        return;
    }

    printf("  mov rax, [rax]\n");
}

// Store %rax to an address that the stack top is pointing to.
static void store(void)
{
    pop("r10");
    printf("  mov [r10], rax\n");
}

void gen_expr(Node *node)
{
    switch (node->kind) {
    case ND_NUM:
        printf("  mov rax, %d\n", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        printf("  neg rax\n");
        return;
    case ND_VAR:
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
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        store();
        return;
    case ND_FUNCALL: {
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            push();
            nargs++;
        }

        for (int i = nargs - 1; i >= 0; --i) {
            pop(argreg[i]);
        }

        printf("  sub rsp, 32\n");
        printf("  mov rax, 0\n");
        printf("  call %s\n", node->funcname);
        printf("  add rsp, 32\n");
        return;
    }
    default:
        break;
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("r10");

    switch (node->kind) {
    case ND_ADD:
        printf("  add rax, r10\n");
        return;
    case ND_SUB:
        printf("  sub rax, r10\n");
        return;
    case ND_MUL:
        printf("  imul rax, r10\n");
        return;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv  r10\n");
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LE:
    case ND_LT:
        printf("  cmp rax, r10\n");

        if (node->kind == ND_EQ) {
            printf("  sete al\n");
        } else if (node->kind == ND_NE) {
            printf("  setne al\n");
        } else if (node->kind == ND_LT) {
            printf("  setl al\n");
        } else if (node->kind == ND_LE) {
            printf("  setle al\n");
        }

        printf("  movzx rax, al\n");
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
        printf("  cmp rax, 0\n");
        printf("  je .L.else.%d\n", c);
        gen_stmt(node->then);
        printf("  jmp .L.end.%d\n", c);
        printf(".L.else.%d:\n", c);
        if (node->els) {
            gen_stmt(node->els);
        }
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_FOR: {
        int c = count();
        if (node->init) {
            gen_stmt(node->init);
        }
        printf(".L.begin.%d:\n", c);
        if (node->cond) {
            gen_expr(node->cond);
            printf("  cmp rax, 0\n");
            printf("  je .L.end.%d\n", c);
        }
        gen_stmt(node->then);
        if (node->inc) {
            gen_expr(node->inc);
        }
        printf("  jmp .L.begin.%d\n", c);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next) {
            gen_stmt(n);
        }
        return;
    case ND_RETURN:
        gen_expr(node->lhs);
        printf("  jmp .L.return.%s\n", to_c_str(current_fn->name));
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
            var->offset = offset;
        }
        fn->stack_size = align_to(offset, 16);
    }
}

void codegen(Obj *prog)
{
    assign_lvar_offsets(prog);

    printf("  .intel_syntax noprefix\n");

    for (Obj *fn = prog; fn; fn = fn->next) {
        if (!fn->is_function) {
            continue;
        }

        printf("  .globl %s\n", to_c_str(fn->name));
        printf("  .text\n");
        printf("%s:\n", to_c_str(fn->name));
        current_fn = fn;

        // Prologue
        printf("  push rbp\n");
        printf("  mov rbp, rsp\n");
        printf("  sub rsp, %d\n", fn->stack_size);

        // Save passed-by-register arguments to the stack
        int i = 0;
        for (Obj *var = fn->params; var; var = var->next) {
            printf("  mov [rbp - %d], %s\n", var->offset, argreg[i++]);
        }

        // Emit code
        gen_stmt(fn->body);
        assert(depth == 0);

        // Epilogue
        printf(".L.return.%s:\n", to_c_str(fn->name));
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
    }
}

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#define internal        static
#define local_persist   static
#define global_variable static

#ifndef __clang__
#define __attribute__(x)
#endif

typedef struct String String;
typedef struct Type Type;
typedef struct Node Node;

//
// tokenize.c
//

// Token
typedef enum
{
    TK_IDENT,   // Identifiers
    TK_PUNCT,   // Punctuators
    TK_KEYWORD, // Keywords
    TK_NUM,     // Numeric literals
    TK_EOF,     // End-of-file markers
} TokenKind;

// Token type
typedef struct Token Token;
struct Token
{
    TokenKind kind; // Token kind
    String *name;   // Token name
    Token *next;    // Next token
    int val;        // If kind is TK_NUM, it's value
};

__attribute__((noreturn)) void error(char *fmt, ...);
__attribute__((noreturn)) void error_at(char *loc, char *fmt, ...);
__attribute__((noreturn)) void error_tok(Token *tok, char *fmt, ...);

bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *s);
bool consume(Token **rest, Token *tok, char *str);
Token *tokenize(char *input);

//
// parse.c
//

// Variable or function
typedef struct Obj Obj;
struct Obj
{
    Obj *next;
    String *name;  // Variable name
    Type *ty;      // Type
    bool is_local; // local or global/function

    // Local variable
    int offset;

    // Global variable or function
    bool is_function;
    Obj *params;

    Node *body;
    Obj *locals;
    int stack_size;
};

// AST node
typedef enum
{
    ND_ADD,       // +
    ND_SUB,       // -
    ND_MUL,       // *
    ND_DIV,       // /
    ND_NEG,       // unary -
    ND_EQ,        // ==
    ND_NE,        // !=
    ND_LT,        // <
    ND_LE,        // <=
    ND_ASSIGN,    // =
    ND_ADDR,      // unary &
    ND_DEREF,     // unary *
    ND_RETURN,    // "return"
    ND_IF,        // "if"
    ND_FOR,       // "for" or "while"
    ND_BLOCK,     // { ... }
    ND_FUNCALL,   // Function call
    ND_EXPR_STMT, // Expression statement
    ND_VAR,       // Variable
    ND_NUM,       // Integer
} NodeKind;

// AST node type
// TODO use union
struct Node
{
    NodeKind kind; // Node kind
    Node *next;    // Next node
    Type *ty;      // Type, e.g. int or pointer to int
    Token *tok;    // Representative token

    Node *rhs;  // Left-hand side
    Node *lhs;  // Right-hand side
    Node *body; // Block

    // "if" or "for" statement
    Node *init;
    Node *cond;
    Node *then;
    Node *els;
    Node *inc;

    // Function call
    char *funcname;
    Node *args;

    Obj *var; // Used if kind == ND_VAR
    int val;  // Used if  kind == ND_NUM
};

Obj *parse(Token *tok);

//
// type.c
//

typedef enum
{
    TY_FUNC,
    TY_INT,
    TY_PTR,
    TY_ARRAY,
} TypeKind;

struct Type
{
    TypeKind kind;
    int size; // sizeof() value

    // Pointer-to or array-of type. We intentionally use the same member
    // to represent pointer/array duality in C.
    //
    // In many contexts in which a pointer is expected, we examine this
    // member instead of "kind" member to determine whether a type is a
    // pointer or not. That means in many contexts "array of T" is
    // naturally handled as if it were "pointer to T", as required by
    // the C spec.
    Type *base;

    // Declaration
    Token *name;

    // Array
    int array_len;

    // Function type
    Type *return_ty;
    Type *params;
    Type *next;
};

extern Type *ty_int;

bool is_integer(Type *ty);
Type *copy_type(Type *ty);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *array_of(Type *base, int length);
void add_type(Node *node);

//
// codegen.c
//

void codegen(Obj *prog);

//
// string.s
//

struct String
{
    char *loc;   // Token location
    int len;     // Token length
    char *c_str; // C string
};

String *new_string(char *start, char *end);
bool string_equal(String *s1, String *s2);
char *to_c_str(String *s);

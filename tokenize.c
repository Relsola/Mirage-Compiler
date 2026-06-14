#include "mirage.h"

// Input string
internal char *current_input;

// Reports an error location and exit
__attribute__((noreturn)) internal void verror_at(char *loc, char *fmt, va_list ap)
{
    int pos = loc - current_input;
    fprintf(stderr, "%s\n", current_input);
    fprintf(stderr, "%*s", pos, ""); // print pos spaces.
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Reports an error and exit
void error(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
}

void error_tok(Token *tok, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->name->loc, fmt, ap);
}

bool equal(Token *tok, char *op)
{
    String *name = tok->name;
    bool result = memcmp(name->loc, op, name->len) == 0 && op[name->len] == '\0';
    return result;
}

Token *skip(Token *tok, char *s)
{
    if (!equal(tok, s)) {
        error_tok(tok, "expected '%s'", s);
    }
    return tok->next;
}

bool consume(Token **rest, Token *tok, char *str)
{
    if (equal(tok, str)) {
        *rest = tok->next;
        return true;
    }
    *rest = tok;
    return false;
}

internal Token *new_token(TokenKind kind, char *start, char *end)
{
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->name = new_string(start, end);
    return tok;
}

internal bool startswith(char *p, char *q)
{
    bool result = strncmp(p, q, strlen(q)) == 0;
    return result;
}

// Returns true if c is valid as the first character of an identifier.
internal bool is_ident1(char c)
{
    bool result = ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
    return result;
}

// Returns true if c is valid as a non-first character of an identifier.
internal bool is_ident2(char c)
{
    bool result = is_ident1(c) || ('0' <= c && c <= '9');
    return result;
}

// Read a punctuator token from p and returns its length.
internal int read_punct(char *p)
{
    if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") || startswith(p, ">=")) {
        return 2;
    }

    return ispunct(*p) ? 1 : 0;
}

internal bool is_keyword(Token *tok)
{
    local_persist char *kw[] = {"return", "if", "else", "for", "while", "int"};

    for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
        if (equal(tok, kw[i])) {
            return true;
        }
    }
    return false;
}

internal void convert_keywords(Token *tok)
{
    for (Token *t = tok; t->kind != TK_EOF; t = t->next) {
        if (is_keyword(t)) {
            t->kind = TK_KEYWORD;
        }
    }
}

// Tokenize a given string and returns new tokens.
Token *tokenize(char *p)
{
    current_input = p;
    Token head = {};
    Token *cur = &head;

    while (*p) {
        // Skip whitespace characters
        if (isspace(*p)) {
            p++;
            continue;
        }

        // Numeric literal
        if (isdigit(*p)) {
            char *start = p;
            int val = strtoul(p, &p, 10);
            cur = cur->next = new_token(TK_NUM, start, p);
            cur->val = val;
            continue;
        }

        // Identifier or keyword
        if (is_ident1(*p)) {
            char *start = p;
            do {
                ++p;
            } while (is_ident2(*p));
            cur = cur->next = new_token(TK_IDENT, start, p);
            continue;
        }

        // Punctuators
        int punct_len = read_punct(p);
        if (punct_len) {
            cur = cur->next = new_token(TK_PUNCT, p, p + punct_len);
            p += punct_len;
            continue;
        }

        error_at(p, "invalid token");
    }

    cur = cur->next = new_token(TK_EOF, p, p);
    convert_keywords(head.next);
    return head.next;
}

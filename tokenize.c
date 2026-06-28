#include "mirage.h"

// Input filename
internal char *current_filename;

// Input string
internal char *current_input;

// Reports an error message in the following format.
//
// foo.c:10: x = y + 1;
//               ^ <error message here>
internal void verror_at(int line_no, char *loc, char *fmt, va_list ap)
{
    // Find a line containing `loc`.
    char *line = loc;
    while (current_input < line && line[-1] != '\n') {
        --line;
    }

    char *end = loc;
    while (*end != '\n') {
        ++end;
    }

    // Print out the line.
    int indent = fprintf(stderr, "%s:%d: ", current_filename, line_no);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    // Show the error message.
    int pos = loc - line + indent;

    fprintf(stderr, "%*s", pos, ""); // print pos spaces.
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
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
    int line_no = 1;
    for (char *p = current_input; p < loc; p++) {
        if (*p == '\n') {
            line_no++;
        }
    }

    va_list ap;
    va_start(ap, fmt);
    verror_at(line_no, loc, fmt, ap);
    exit(1);
}

void error_tok(Token *tok, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->line_no, tok->loc, fmt, ap);
    exit(1);
}

bool equal(Token *tok, char *op)
{
    bool result = memcmp(tok->loc, op, tok->len) == 0 && op[tok->len] == '\0';
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
    tok->loc = start;
    tok->len = end - start;
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
    local_persist char *kw[] = {
        "<<=", ">>=", "...", "==", "!=", "<=", ">=", "->", "+=", "-=", "*=", "/=",
        "++", "--", "%=", "&=", "|=", "^=", "&&", "||", "<<", ">>"
    };

    for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
        if (startswith(p, kw[i])) {
            return strlen(kw[i]);
        }
    }
    return ispunct(*p) ? 1 : 0;
}

internal bool is_keyword(Token *tok)
{
    local_persist char *kw[] = {
        "void", "char", "short", "long", "int", "struct", "union", "_Bool",
        "return", "if", "else", "for", "while", "sizeof", "typedef", "enum",
        "static", "goto", "break", "continue", "switch", "case", "default",
        "extern", "_Alignof", "_Alignas", "do", "signed", "unsigned", "const",
        "volatile", "auto", "register", "restrict", "__restrict", "__restrict__",
        "_Noreturn", "float", "double"
    };

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

internal int from_hex(char c)
{
    if ('0' <= c && c <= '9') {
        return c - '0';
    }
    if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    }
    return c - 'A' + 10;
}

internal int read_escaped_char(char **new_pos, char *p)
{
    if ('0' <= *p && *p <= '7') {
        // Read an octal number.
        int c = *p++ - '0';
        if ('0' <= *p && *p <= '7') {
            c = (c << 3) + (*p++ - '0');
            if ('0' <= *p && *p <= '7') {
                c = (c << 3) + (*p++ - '0');
            }
        }
        *new_pos = p;
        return c;
    }

    if (*p == 'x') {
        // Read a hexadecimal number.
        p++;
        if (!isxdigit(*p)) {
            error_at(p, "invalid hex escape sequence");
        }

        int c = 0;
        for (; isxdigit(*p); p++) {
            c = (c << 4) + from_hex(*p);
        }
        *new_pos = p;
        return c;
    }

    *new_pos = p + 1;
    // Escape sequences are defined using themselves here. E.g.
    // '\n' is implemented using '\n'. This tautological definition
    // works because the compiler that compiles our compiler knows
    // what '\n' actually is. In other words, we "inherit" the ASCII
    // code of '\n' from the compiler that compiles our compiler,
    // so we don't have to teach the actual code here.
    //
    // This fact has huge implications not only for the correctness
    // of the compiler but also for the security of the generated code.
    // For more info, read "Reflections on Trusting Trust" by Ken Thompson.
    // https://github.com/rui314/chibicc/wiki/thompson1984.pdf
    switch (*p) {
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'v':
        return '\v';
    case 'f':
        return '\f';
    case 'r':
        return '\r';
    default:
        return *p;
    }
}

// Find a closing double-quote.
internal char *string_literal_end(char *p)
{
    char *start = p;
    for (; *p != '"'; p++) {
        if (*p == '\n' || *p == '\0') {
            error_at(start, "unclosed string literal");
        }
        if (*p == '\\') {
            p++;
        }
    }
    return p;
}

internal Token *read_string_literal(char *start)
{
    char *end = string_literal_end(start + 1);
    char *buf = calloc(1, end - start);
    int len = 0;

    for (char *p = start + 1; p < end;) {
        if (*p == '\\') {
            buf[len++] = read_escaped_char(&p, p + 1);
        } else {
            buf[len++] = *p++;
        }
    }

    Token *tok = new_token(TK_STR, start, end + 1);
    tok->ty = array_of(ty_char, len + 1);
    tok->str = buf;
    return tok;
}

internal Token *read_char_literal(char *start)
{
    char *p = start + 1;
    if (*p == '\0') {
        error_at(start, "unclosed char literal");
    }

    char c;
    if (*p == '\\') {
        c = read_escaped_char(&p, p + 1);
    } else {
        c = *p++;
    }

    char *end = strchr(p, '\'');
    if (!end) {
        error_at(p, "unclosed char literal");
    }

    Token *tok = new_token(TK_NUM, start, end + 1);
    tok->val = c;
    tok->ty = ty_int;
    return tok;
}

internal Token *read_int_literal(char *start)
{
    char *p = start;

    // Read a binary, octal, decimal or hexadecimal number.
    int base = 10;
    if (!_strnicmp(p, "0x", 2) && isxdigit(p[2])) {
        p += 2;
        base = 16;
    } else if (!_strnicmp(p, "0b", 2) && (p[2] == '0' || p[2] == '1')) {
        p += 2;
        base = 2;
    } else if (*p == '0') {
        base = 8;
    }

    u64 val = strtoull(p, &p, base);

    // Read U, L or LL suffixes.
    bool l = false;
    bool u = false;

    if (startswith(p, "LLU") || startswith(p, "LLu") ||
        startswith(p, "llU") || startswith(p, "llu") ||
        startswith(p, "ULL") || startswith(p, "Ull") ||
        startswith(p, "uLL") || startswith(p, "ull"))
    {
        p += 3;
        l = u = true;
    } else if (!_strnicmp(p, "lu", 2) || !_strnicmp(p, "ul", 2)) {
        p += 2;
        l = u = true;
    } else if (startswith(p, "LL") || startswith(p, "ll")) {
        p += 2;
        l = true;
    } else if (*p == 'L' || *p == 'l') {
        p++;
        l = true;
    } else if (*p == 'U' || *p == 'u') {
        p++;
        u = true;
    }

    // Infer a type.
    Type *ty;
    if (base == 10) {
        if (l && u)         { ty = ty_ulong;                         }
        else if (l)         { ty = ty_long;                          }
        else if (u)         { ty = (val >> 32) ? ty_ulong : ty_uint; }
        else                { ty = (val >> 31) ? ty_long : ty_int;   }
    } else {
        if (l && u)         { ty = ty_ulong;                         }
        else if (l)         { ty = (val >> 63) ? ty_ulong : ty_long; }
        else if (u)         { ty = (val >> 32) ? ty_ulong : ty_uint; }
        else if (val >> 63) { ty = ty_ulong;                         }
        else if (val >> 32) { ty = ty_long;                          }
        else if (val >> 31) { ty = ty_uint;                          }
        else                { ty = ty_int;                           }
    }

    Token *tok = new_token(TK_NUM, start, p);
    tok->val = val;
    tok->ty = ty;
    return tok;
}

internal Token *read_number(char *start)
{
    // Try to parse as an integer constant.
    Token *tok = read_int_literal(start);
    if (!strchr(".eEfF", start[tok->len])) {
        return tok;
    }

    // If it's not an integer, it must be a floating point constant.
    char *end;
    f64 val = strtod(start, &end);

    Type *ty;
    if (*end == 'f' || *end == 'F') {
        ty = ty_float;
        end++;
    } else if (*end == 'l' || *end == 'L') {
        ty = ty_double;
        end++;
    } else {
        ty = ty_double;
    }

    tok = new_token(TK_NUM, start, end);
    tok->fval = val;
    tok->ty = ty;
    return tok;
}

// Initialize line info for all tokens.
internal void add_line_numbers(Token *tok)
{
    char *p = current_input;
    int n = 1;

    do {
        if (p == tok->loc) {
            tok->line_no = n;
            tok = tok->next;
        }
        if (*p == '\n') {
            ++n;
        }
    } while (*p++);
}

// Tokenize a given string and returns new tokens.
internal Token *tokenize(char *filename, char *p)
{
    current_filename = filename;
    current_input = p;
    Token head = {};
    Token *cur = &head;

    while (*p) {
        // Skip line comments.
        if (startswith(p, "//")) {
            p += 2;
            while (*p != '\n') {
                p++;
            }
            continue;
        }

        // Skip block comments.
        if (startswith(p, "/*")) {
            char *q = strstr(p + 2, "*/");
            if (!q) {
                error_at(p, "unclosed block comment");
            }
            p = q + 2;
            continue;
        }

        // Skip whitespace characters
        if (isspace(*p)) {
            p++;
            continue;
        }

        // Numeric literal
        if (isdigit(*p) || (*p == '.' && isdigit(p[1]))) {
            cur = cur->next = read_number(p);
            p += cur->len;
            continue;
        }

        // String literal
        if (*p == '"') {
            cur = cur->next = read_string_literal(p);
            p += cur->len;
            continue;
        }

        // Character literal
        if (*p == '\'') {
            cur = cur->next = read_char_literal(p);
            p += cur->len;
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
    add_line_numbers(head.next);
    convert_keywords(head.next);
    return head.next;
}

// Returns the contents of a given file.
internal char *read_file(char *path)
{
    FILE *fp;

    if (strcmp(path, "-") == 0) {
        // By convention, read from stdin if a given filename is "-".
        fp = stdin;
    } else {
        errno_t err = fopen_s(&fp, path, "rb");
        if (err || !fp) {
            char err_buf[256];
            strerror_s(err_buf, sizeof(err_buf), err);
            error("cannot open %s: %s", path, err_buf);
        }
    }

    char *buf = NULL;
    u64 len = 0;

    if (fp != stdin) {
        fseek(fp, 0, SEEK_END);
        i64 size = ftell(fp);
        if (size < 0) {
            error("read failed");
        }

        rewind(fp);
        buf = malloc(size + 2);
        if (!buf) {
            error("out of memory");
        }

        u64 nread = fread(buf, 1, size, fp);
        if (nread != size && ferror(fp)) {
            error("read failed");
        }

        fclose(fp);
        len = nread;
        goto __terminate_source;
    }

    u64 cap = 4096;
    buf = malloc(cap);
    for (;;) {
        if (len == cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }

        u64 n = fread(buf + len, 1, cap - len, fp);
        len += n;

        if (n == 0) {
            break;
        }
    }

    if (len + 2 > cap) {
        buf = realloc(buf, len + 2);
    }

__terminate_source:
    if (len == 0 || buf[len - 1] != '\n') {
        buf[len++] = '\n';
    }
    buf[len] = '\0';
    return buf;
}

Token *tokenize_file(char *path)
{
    return tokenize(path, read_file(path));
}

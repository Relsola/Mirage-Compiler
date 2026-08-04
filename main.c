#include <windows.h>
#include "mirage.h"

StringArray include_paths;

internal char *opt_o;
internal bool opt_S;
internal bool opt_E;

internal StringArray input_paths;

internal StringArray tmpfiles;

#if PERF
internal LARGE_INTEGER perf_freq;

typedef struct ScopedTimer
{
    const char *name;
    LARGE_INTEGER start;
} ScopedTimer;

internal ScopedTimer timer_start(const char *name)
{
    ScopedTimer t;
    t.name = name;
    QueryPerformanceCounter(&t.start);
    return t;
}

internal void timer_stop(ScopedTimer *t)
{
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);

    f64 ms = (f64)(end.QuadPart - t->start.QuadPart) * 1000.0 / perf_freq.QuadPart;
    printf("%-12s %.3f ms\n", t->name, ms);
}
#endif

internal void usage(int status)
{
    fprintf(stderr, "mirage [ -o <path> ] <file>\n");
    exit(status);
}

internal void add_default_include_paths(char *argv0)
{
    char drive[_MAX_DRIVE], dir[_MAX_DIR];
    _splitpath_s(argv0, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);
    char *path = format("%s%sinclude", drive, dir);
    strarray_push(&include_paths, path);

    // TODO Add standard include paths.
}

internal void define(char *str)
{
    char *eq = strchr(str, '=');
    if (eq) {
        define_macro(strndup(str, eq - str), eq + 1);
    } else {
        define_macro(str, "1");
    }
}

internal void parse_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) {
            usage(0);
        }

        if (!strcmp(argv[i], "-o")) {
            if (!argv[++i]) {
                usage(1);
            }
            opt_o = argv[i];
            continue;
        }

        if (!strncmp(argv[i], "-o", 2)) {
            opt_o = argv[i] + 2;
            continue;
        }

        if (!strcmp(argv[i], "-S")) {
            opt_S = true;
            continue;
        }

        if (!strcmp(argv[i], "-E")) {
            opt_E = true;
            continue;
        }

        if (!strncmp(argv[i], "-I", 2)) {
            strarray_push(&include_paths, argv[i] + 2);
            continue;
        }

        if (!strcmp(argv[i], "-D")) {
            define(argv[++i]);
            continue;
        }

        if (!strncmp(argv[i], "-D", 2)) {
            define(argv[i] + 2);
            continue;
        }

        if (!strcmp(argv[i], "-U")) {
            undef_macro(argv[++i]);
            continue;
        }

        if (!strncmp(argv[i], "-U", 2)) {
            undef_macro(argv[i] + 2);
            continue;
        }

        // These options are ignored for now.
        if (!strncmp(argv[i], "-O", 2) ||
            !strncmp(argv[i], "-W", 2) ||
            !strncmp(argv[i], "-c", 2) ||
            !strncmp(argv[i], "-g", 2) ||
            !strncmp(argv[i], "-std=", 5) ||
            !strcmp(argv[i], "-w")) {
            continue;
        }

        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            error("unknown argument: %s", argv[i]);
        }

        strarray_push(&input_paths, argv[i]);
    }

    if (input_paths.len == 0) {
        error("no input files");
    }
}

internal FILE *open_file(const char *path)
{
    if (!path || strcmp(path, "-") == 0) {
        return stdout;
    }

    FILE *out;
    errno_t err = fopen_s(&out, path, "w");
    if (err || !out) {
        char err_buf[256];
        strerror_s(err_buf, sizeof(err_buf), err);
        error("cannot open output file: %s: %s", path, err_buf);
    }
    return out;
}

internal bool endswith(char *p, char *q)
{
    int len1 = strlen(p);
    int len2 = strlen(q);
    return (len1 >= len2) && !strcmp(p + len1 - len2, q);
}

// Replace file extension
internal char *replace_extension(char *filename, char *extension)
{
    char *tmp_filename = _strdup(filename);

    char *p = strrchr(tmp_filename, '\\');
    if (p) {
        tmp_filename = p + 1;
    }

    p = strrchr(tmp_filename, '/');
    if (p) {
        tmp_filename = p + 1;
    }

    char *dot = strrchr(tmp_filename, '.');
    if (dot) {
        *dot = '\0';
    }

    return format("%s%s", tmp_filename, extension);
}

internal void cleanup(void)
{
    for (int i = 0; i < tmpfiles.len; i++) {
        _unlink(tmpfiles.data[i]);
    }
}

internal char *create_tmpfile(void)
{
    char tempPath[MAX_PATH];
    char tempFile[MAX_PATH];

    GetTempPath2A(MAX_PATH, tempPath);

    if (!GetTempFileNameA(tempPath, "ccb", 0, tempFile)) {
        error("GetTempFileName failed");
    }

    char *tmp_filename = _strdup(tempFile);
    strarray_push(&tmpfiles, tmp_filename);
    return tmp_filename;
}

// Returns true if a given file exists.
bool file_exists(char *path) {
  return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

// Print tokens to stdout. Used for -E.
internal void print_tokens(Token *tok)
{
    FILE *out = open_file(opt_o ? opt_o : "-");

    int line = 1;
    for (; tok->kind != TK_EOF; tok = tok->next) {
        if (line > 1 && tok->at_bol) {
            fprintf(out, "\n");
        }
        if (tok->has_space && !tok->at_bol) {
            fprintf(out, " ");
        }
        fprintf(out, "%.*s", tok->len, tok->loc);
        line++;
    }
    fprintf(out, "\n");
}

internal void compiler_to_asm(char *input_file, char *output_file)
{
#if PERF
    ScopedTimer timer;
    timer = timer_start("tokenize_file");
#endif

    // Tokenize and parse.
    Token *tok = tokenize_file(input_file);
    if (!tok) {
        char err_buf[256];
        strerror_s(err_buf, sizeof(err_buf), errno);
        error("%s: %s", input_file, err_buf);
    }

#if PERF
    timer_stop(&timer);
    timer = timer_start("preprocess");
#endif

    tok = preprocess(tok);

#if PERF
    timer_stop(&timer);
    timer = timer_start("parse");
#endif

    // If -E is given, print out preprocessed C code as a result.
    if (opt_E) {
        print_tokens(tok);
        return;
    }

    Obj *prog = parse(tok);

#if PERF
    timer_stop(&timer);
    timer = timer_start("codegen");
#endif

    // Traverse the AST to emit assembly.
    FILE *out = open_file(output_file);
    fprintf(out, "  .intel_syntax noprefix\n");
    codegen(prog, out);

    if (out != stdout) {
        fclose(out);
    }

#if PERF
    timer_stop(&timer);
#endif
}

// TODO error handler
internal void run_subprocess(char *cmdline)
{
    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };

    si.cb = sizeof(si);

    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    CreateProcessA(NULL, cmdline, NULL, NULL, true, 0, NULL, NULL, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

internal void compiler_to_obj(char *input_file, char *output_file)
{
    char *cmd = format(
        "clang -c -x assembler \"%s\" -o \"%s\"",
        input_file,
        output_file);

    run_subprocess(cmd);
}

internal void run_linker(StringArray *inputs, char *output)
{
    char *cmd = "";
    for (int i = 0; i < inputs->len; ++i) {
        cmd = format("%s %s", cmd, inputs->data[i]);
    }
    // TODO linker params
    cmd = format("clang %s -o %s -Wl,/defaultlib:legacy_stdio_definitions.lib", cmd, output);
    run_subprocess(cmd);
}

int main(int argc, char **argv)
{
    atexit(cleanup);
#if PERF
    QueryPerformanceFrequency(&perf_freq);
#endif

    init_macros();
    parse_args(argc, argv);

    if (input_paths.len > 1 && opt_o && (opt_S | opt_E)) {
        error("cannot specify '-o' with '-S' or '-E' with multiple files");
    }

    add_default_include_paths(argv[0]);

    StringArray ld_args = {};
    for (int i = 0; i < input_paths.len; ++i) {
        char *input_file = input_paths.data[i];

        char *output_file;
        if (opt_o) {
            output_file = opt_o;
        } else if (opt_S) {
            output_file = replace_extension(input_file, ".s");
        } else {
            output_file = replace_extension(input_file, ".o");
        }

        // Handle .o
        if (endswith(input_file, ".o")) {
            strarray_push(&ld_args, input_file);
            continue;
        }

#if PERF
        printf("start compiler %s >>\n", input_file);
#endif
        if (opt_S || opt_E) {
            compiler_to_asm(input_file, output_file);
            continue;
        }

        // Compile
        char *tmp1 = create_tmpfile();
        char *tmp2 = create_tmpfile();
        compiler_to_asm(input_file, tmp1);
        compiler_to_obj(tmp1, tmp2);
        strarray_push(&ld_args, tmp2);
    }

    if (ld_args.len > 0) {
        run_linker(&ld_args, opt_o ? opt_o : "main.exe");
    }

    return 0;
}

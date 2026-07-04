#include <windows.h>
#include "mirage.h"

internal char *opt_o;
internal bool opt_S;
internal bool opt_c;

internal StringArray input_paths;

internal StringArray tmpfiles;

internal void usage(int status)
{
    fprintf(stderr, "mirage [ -o <path> ] <file>\n");
    exit(status);
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

        if (!strcmp(argv[i], "-c")) {
            opt_c = true;
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

internal void compiler_to_asm(const char *input, const char *output)
{
    // Tokenize and parse.
    Token *tok = tokenize_file(input);
    tok = preprocess(tok);
    Obj *prog = parse(tok);

    // Traverse the AST to emit assembly.
    FILE *out = open_file(output);
    fprintf(out, "  .intel_syntax noprefix\n");
    fprintf(out, ".file 1 \"%s\"\n", input);
    codegen(prog, out);

    if (out != stdout) {
        fclose(out);
    }
}

internal void run_subprocess(char *cmdline)
{
    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi = { 0 };

    si.cb = sizeof(si);

    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

    if (!ok) {
        error("CreateProcess failed");
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitcode = 0;
    GetExitCodeProcess(pi.hProcess, &exitcode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (exitcode != 0) {
        exit(exitcode);
    }
}

internal void compiler_to_obj(char *input, char *output)
{
    char *cmd = format(
        "clang -c -x assembler \"%s\" -o \"%s\"",
        input,
        output);

    run_subprocess(cmd);
}

int main(int argc, char **argv)
{
    atexit(cleanup);
    parse_args(argc, argv);

    if (input_paths.len > 1 && opt_o && (opt_c || opt_S)) {
        error("cannot specify '-o' with '-c' or '-S' with multiple files");
    }

    for (int i = 0; i < input_paths.len; ++i) {
        char *input = input_paths.data[i];

        char *output;
        if (opt_o) {
            output = opt_o;
        } else if (opt_S) {
            output = replace_extension(input, ".s");
        } else {
            output = replace_extension(input, ".obj");
        }

        if (opt_S) {
            compiler_to_asm(input, output);
            return 0;
        }

        // Traverse the AST to emit assembly.
        char *tmpfile = create_tmpfile();
        compiler_to_asm(input, tmpfile);
        compiler_to_obj(tmpfile, output);
    }

    return 0;
}

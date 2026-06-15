#include "mirage.h"

internal char *opt_o;

internal char *input_path;

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

        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            error("unknown argument: %s", argv[i]);
        }

        input_path = argv[i];
    }

    if (!input_path) {
        error("no input files");
    }
}

internal FILE *open_file(char *path)
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

int main(int argc, char **argv)
{
    parse_args(argc, argv);

    // Tokenize and parse.
    Token *tok = tokenize_file(input_path);
    Obj *prog = parse(tok);

    // Traverse the AST to emit assembly.
    FILE *out = open_file(opt_o);
    fprintf(out, "  .intel_syntax noprefix\n");
    fprintf(out, ".file 1 \"%s\"\n", input_path);
    codegen(prog, out);

    return 0;
}

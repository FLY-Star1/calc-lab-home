// Giyosiddin Kutbiddinov 251ADB137
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: course assignment; calc-tests.zip; https://www.geeksforgeeks.org/c/basics-file-handling-c/ ; 
// https://www.geeksforgeeks.org/c/eof-and-feof-in-c/; https://claude.ai/share/d1fc4530-18bd-4e4c-b1d1-f2d3bafd6aed

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

int calculate(FILE *input, FILE *output)
{
    int ch;
    long long result = 0;
    int operation = '+';
    int expect_number = 1;
    int has_number = 0;

    while ((ch = fgetc(input)) != EOF) {
        if (ch == ' ' || ch == '\t') {
            continue;
        }
        if (ch == '\n') {
            if (has_number) {
                if (expect_number) {
                    fprintf(stderr, "Missing number at the end of a line\n");
                    return 1;
                }
                if (fprintf(output, "%lld\n", result) < 0) {
                    perror("Writing result file");
                    return 1;
                }
            }
            result = 0;
            operation = '+';
            expect_number = 1;
            has_number = 0;
            continue;
        }
        if (isdigit(ch)) {
            if (!expect_number) {
                fprintf(stderr, "Expected + or - between numbers\n");
                return 1;
            }
            long long number = 0;
            do {
                // Appending a digit moves the existing digits one place left.
                number = number * 10 + (ch - '0');
                ch = fgetc(input);
            } while (ch != EOF && isdigit(ch));

            if (operation == '+') {
                result += number;
            } else {
                result -= number;
            }

            expect_number = 0;
            has_number = 1;
            if (ch != EOF && ungetc(ch, input) == EOF) {
                fprintf(stderr, "Could not return the next character\n");
                return 1;
            }
            continue;
        }

        if (ch == '+' || ch == '-') {
            if (expect_number) {
                fprintf(stderr, "Expected a number before the operator\n");
                return 1;
            }
            operation = ch;
            expect_number = 1;
        } else {
            fprintf(stderr, "Unrecognized character: %c\n", ch);
            return 1;
        }
    }

    if (ferror(input)) {
        perror("Reading input file");
        return 1;
    }
    if (has_number) {
        if (expect_number) {
            fprintf(stderr, "Missing number at the end of the file\n");
            return 1;
        }
        if (fprintf(output, "%lld\n", result) < 0) {
            perror("Writing result file");
            return 1;
        }
    }
    return 0;
}

// output-directory creation; AI(Claude)
int ensure_output_directory(const char *directory)
{
    if (mkdir(directory, 0777) == 0) {
        return 0;
    }
    if (errno == EEXIST) {
        struct stat information;
        if (stat(directory, &information) == 0 &&
            S_ISDIR(information.st_mode)) {
            return 0;
        }
    }
    perror(directory);
    return 1;
}

// required output filename and dynamic allocation; AI(Claude)
char *make_output_path(const char *directory, const char *input_path)
{
    const char *base = strrchr(input_path, '/');
    base = base == NULL ? input_path : base + 1;

    size_t base_length = strlen(base);
    if (base_length >= 4 && strcmp(base + base_length - 4, ".txt") == 0) {
        base_length -= 4;
    }

    const char suffix[] = "_giyosiddin_kutbiddinov_251ADB137.txt";
    size_t directory_length = strlen(directory);

    size_t capacity = directory_length + 1 + base_length + sizeof(suffix);
    char *path = malloc(capacity);
    if (path == NULL) {
        fprintf(stderr, "Not enough memory for the output filename\n");
        return NULL;
    }

    memcpy(path, directory, directory_length);
    path[directory_length] = '/';
    memcpy(path + directory_length + 1, base, base_length);
    memcpy(path + directory_length + 1 + base_length, suffix, sizeof(suffix));
    return path;
}

// command-line handling and file orchestration; AI(Claude)
int main(int argc, char *argv[])
{
    if (argc != 4 || strcmp(argv[1], "-o") != 0) {
        fprintf(stderr, "Usage: %s -o OUTDIR input.txt\n", argv[0]);
        return 1;
    }
    const char *output_directory = argv[2];
    const char *input_path = argv[3];
    FILE *input = fopen(input_path, "rb");
    if (input == NULL) {
        perror(input_path);
        return 1;
    }
    if (ensure_output_directory(output_directory) != 0) {
        fclose(input);
        return 1;
    }
    char *output_path = make_output_path(output_directory, input_path);
    if (output_path == NULL) {
        fclose(input);
        return 1;
    }
    FILE *output = fopen(output_path, "wb");
    if (output == NULL) {
        perror(output_path);
        free(output_path);
        fclose(input);
        return 1;
    }
    int status = calculate(input, output);

    if (fclose(input) == EOF) {
        perror("Closing input file");
        status = 1;
    }
    if (fclose(output) == EOF) {
        perror(output_path);
        status = 1;
    }
    free(output_path);
    return status;
}

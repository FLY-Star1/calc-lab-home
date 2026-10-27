// Giyosiddin Kutbiddinov 251ADB137
// Arithmetic Parser in C
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: course assignment; calc-tests.zip; Claude;
// https://www.geeksforgeeks.org/c/basics-file-handling-c/
// https://www.geeksforgeeks.org/c/eof-and-feof-in-c/
// https://claude.ai/share/d1fc4530-18bd-4e4c-b1d1-f2d3bafd6aed//
// https://ftp.gnu.org/old-gnu/Manuals/glibc-2.2.3/html_node/libc_276.html

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#include <limits.h>
#include <dirent.h>

enum { END, NUMBER = 256, POWER, INVALID };

typedef struct {
    long long integer;
    double decimal;
    int is_float;
    size_t position;
} Number;
typedef struct {
    char *text;
    size_t length, index, position, error;
    int token, memory_error;
    Number number;
} Parser;
typedef struct {
    Number right;
    size_t position;
} PowerStep;
// dynamic buffers
void *grow(void *data, size_t *capacity, size_t needed, size_t item_size)
{
    if (needed <= *capacity) {
        return data;
    }
    size_t bigger = *capacity == 0 ? 64 : *capacity;
    while (bigger < needed) {
        if (bigger > (size_t)-1 / item_size / 2) {
            fprintf(stderr, "Input is too large\n");
            return NULL;
        }
        bigger *= 2;
    }
    void *new_data = realloc(data, bigger * item_size);
    if (new_data == NULL) {
        fprintf(stderr, "Not enough memory\n");
    } else {
        *capacity = bigger;
    }
    return new_data;
}

// tokenizer and positions;
void next_token(Parser *parser)
{
    while (parser->index < parser->length &&
           (parser->text[parser->index] == ' ' || parser->text[parser->index] == '\t')) {
        ++parser->index;
    }
    size_t start = parser->index;
    parser->position = start + 1;
    if (start == parser->length) {
        parser->token = END;
        return;
    }
    int ch = (unsigned char)parser->text[parser->index++];
    parser->token = ch;
    if (isdigit(ch)) {
        while (isdigit((unsigned char)parser->text[parser->index])) {
            ++parser->index;
        }
        parser->number = (Number){0};
        parser->number.position = start + 1;
        if (parser->text[parser->index] == '.') {
            size_t dot = parser->index++;
            if (!isdigit((unsigned char)parser->text[parser->index])) {
                parser->token = INVALID;
                parser->position = dot + 1;
                return;
            }
            parser->number.is_float = 1;
            while (isdigit((unsigned char)parser->text[parser->index])) {
                ++parser->index;
            }
        }
        char saved = parser->text[parser->index];
        parser->text[parser->index] = '\0';
        errno = 0;
        parser->token = NUMBER;
        if (parser->number.is_float) {
            parser->number.decimal = strtod(parser->text + start, NULL);
            if (!isfinite(parser->number.decimal)) parser->token = INVALID;
        } else {
            parser->number.integer = strtoll(parser->text + start, NULL, 10);
            if (errno == ERANGE) parser->token = INVALID;
        }
        parser->text[parser->index] = saved;
    } else if (ch == '*' && parser->text[parser->index] == '*') {
        ++parser->index;
        parser->token = POWER;
    } else if (ch == 0 || strchr("+-*/()\n#", ch) == NULL) {
        parser->token = INVALID;
    }
}
// arithmetic and errors; 
int multiply(long long a, long long b, long long *result)
{
    if (a > 0 && ((b > 0 && a > LLONG_MAX / b) ||
                  (b < 0 && b < LLONG_MIN / a))) return 1;
    if (a < 0 && ((b > 0 && a < LLONG_MIN / b) ||
                  (b < 0 && a < LLONG_MAX / b))) return 1;
    *result = a * b;
    return 0;
}

Number apply_operation(Parser *parser, int operation, size_t position, Number left, Number right)
{
    Number result = {0};
    result.position = left.position;
    result.is_float = left.is_float || right.is_float;
    if (operation == '/' && (right.is_float ? right.decimal == 0 : right.integer == 0)) {
        parser->error = right.position;
    } else if (result.is_float) {
        double a = left.is_float ? left.decimal : (double)left.integer;
        double b = right.is_float ? right.decimal : (double)right.integer;
        switch (operation) {
            case '+': result.decimal = a + b; break;
            case '-': result.decimal = a - b; break;
            case '*': result.decimal = a * b; break;
            case '/': result.decimal = a / b; break;
            case POWER: result.decimal = pow(a, b); break;
        }
        if (!isfinite(result.decimal)) parser->error = position;
    } else {
        long long a = left.integer, b = right.integer;
        int overflow = 0;
        switch (operation) {
            case '+':
                overflow = (b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b);
                if (!overflow) result.integer = a + b;
                break;
            case '-':
                overflow = (b < 0 && a > LLONG_MAX + b) || (b > 0 && a < LLONG_MIN + b);
                if (!overflow) result.integer = a - b;
                break;
            case '*': overflow = multiply(a, b, &result.integer); break;
            case '/':
                overflow = a == LLONG_MIN && b == -1;
                if (!overflow) result.integer = a / b;
                break;
            case POWER:
                if (b < 0) parser->error = right.position;
                result.integer = 1;  // Also gives 0 ** 0 == 1.
                // Squaring is fast even for very large exponents.
                while (b > 0 && !overflow) {
                    if (b % 2) overflow = multiply(result.integer, a, &result.integer);
                    b /= 2;
                    if (b > 0 && !overflow) overflow = multiply(a, a, &a);
                }
                break;
        }
        if (overflow) parser->error = position;
    }
    return result;
}

// parsing helpers; AI (Claude)
Number parse_expression(Parser *parser);

Number parse_primary(Parser *parser)
{
    Number result = {0};
    if (parser->token == NUMBER) {
        result = parser->number;
        next_token(parser);
    } else if (parser->token == '(') {
        size_t opening = parser->position;
        next_token(parser);
        result = parse_expression(parser);
        if (parser->error || parser->memory_error) {
            return result;
        }
        if (parser->token != ')') {
            parser->error = parser->position;
        } else {
            next_token(parser);
        }
        result.position = opening;
    } else {
        parser->error = parser->position;
    }
    return result;
}

Number parse_power(Parser *parser)
{
    Number base = parse_primary(parser), result = base;
    PowerStep *steps = NULL;
    size_t count = 0, capacity = 0;
    while (!parser->error && !parser->memory_error && parser->token == POWER) {
        size_t position = parser->position;
        next_token(parser);
        Number right = parse_primary(parser);
        if (parser->error || parser->memory_error) {
            break;
        }
        PowerStep *larger = grow(steps, &capacity, count + 1, sizeof(PowerStep));
        if (larger == NULL) {
            parser->memory_error = 1;
            break;
        }
        steps = larger;
        steps[count++] = (PowerStep){right, position};
    }
    if (count > 0 && !parser->error && !parser->memory_error) {
        result = steps[count - 1].right;
        for (size_t i = count; i > 0 && !parser->error; --i) {
            Number left = i == 1 ? base : steps[i - 2].right;
            result = apply_operation(parser, POWER, steps[i - 1].position, left, result);
        }
    }
    free(steps);
    return result;
}

Number parse_term(Parser *parser)
{
    Number result = parse_power(parser);
    while (!parser->error && !parser->memory_error && (parser->token == '*' || parser->token == '/')) {
        int operation = parser->token;
        size_t position = parser->position;
        next_token(parser);
        Number right = parse_power(parser);
        if (!parser->error && !parser->memory_error) {
            result = apply_operation(parser, operation, position, result, right);
        }
    }
    return result;
}

Number parse_expression(Parser *parser)
{
    Number result = parse_term(parser);
    while (!parser->error && !parser->memory_error && (parser->token == '+' || parser->token == '-')) {
        int operation = parser->token;
        size_t position = parser->position;
        next_token(parser);
        Number right = parse_term(parser);
        if (!parser->error && !parser->memory_error) {
            result = apply_operation(parser, operation, position, result, right);
        }
    }
    return result;
}

// expression lines; AI (Claude)
int evaluate(Parser *parser, FILE *output)
{
    next_token(parser);
    while (parser->token != END) {
        if (parser->token == '#') {
            while (parser->index < parser->length && parser->text[parser->index] != '\n') {
                ++parser->index;
            }
        } else if (parser->token != '\n') {
            Number result = parse_expression(parser);
            if (parser->memory_error) {
                return 1;
            }
            if (parser->error) {
                break;
            }
            if (parser->token != END && parser->token != '\n') {
                parser->error = parser->position;  
                break;
            }
            if (output != NULL) {
                int written = result.is_float ? fprintf(output, "%.15g\n", result.decimal)
                                              : fprintf(output, "%lld\n", result.integer);
                if (written < 0) {
                    perror("Writing result file");
                    return 1;
                }
            }
        }
        next_token(parser);
    }
    return 0;  
}
// file calculation and error output; AI (Claude)
int calculate(FILE *input, FILE *output)
{
    int ch;
    char *text = NULL;
    size_t length = 0;
    size_t capacity = 0;
    while ((ch = fgetc(input)) != EOF) {
        char *larger = grow(text, &capacity, length + 2, sizeof(char));
        if (larger == NULL) {
            free(text);
            return 1;
        }
        text = larger;
        text[length++] = (char)ch;
    }
    if (ferror(input)) {
        perror("Reading input file");
        free(text);
        return 1;
    }
    if (text == NULL) {
        text = grow(NULL, &capacity, 1, sizeof(char));
        if (text == NULL) {
            return 1;
        }
    }
    text[length] = '\0';
    Parser parser = {0};
    parser.text = text;
    parser.length = length;
    int status = evaluate(&parser, NULL);
    if (status == 0) {
        if (parser.error != 0) {
            if (fprintf(output, "ERROR:%zu\n", parser.error) < 0) {
                perror("Writing error result");
                status = 1;
            }
        } else {
            parser.index = 0;
            status = evaluate(&parser, output);
        }
    }
    free(text);
    return status;
}

// output-directory creation; AI (Claude)
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

// required output filename and dynamic allocation; AI (Claude)
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

// file-opening code moved from main; AI (Claude)
int process_file(const char *output_directory, const char *input_path)
{
    FILE *input = fopen(input_path, "rb");
    if (input == NULL) {
        perror(input_path);
        return 1;
    }
    char *output_path = make_output_path(output_directory, input_path);
    if (output_path == NULL) {
        fclose(input);
        return 1;
    }
    struct stat source, target;
    if (stat(input_path, &source) == 0 && stat(output_path, &target) == 0 &&
        source.st_dev == target.st_dev && source.st_ino == target.st_ino) {
        fprintf(stderr, "%s: output refers to input\n", output_path);
        free(output_path);
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

// directory mode; AI (Claude)
int process_directory(const char *output_directory, const char *input_directory)
{
    DIR *directory = opendir(input_directory);
    if (directory == NULL) {
        perror(input_directory);
        return 1;
    }
    char **files = NULL;
    size_t count = 0, capacity = 0;
    int status = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0) {
                perror(input_directory);
                status = 1;
            }
            break;
        }
        size_t length = strlen(entry->d_name);
        if (length < 4 || strcmp(entry->d_name + length - 4, ".txt") != 0) {
            continue;
        }
        size_t size = strlen(input_directory) + length + 2;
        char *path = malloc(size);
        if (path == NULL) {
            fprintf(stderr, "Not enough memory for input path\n");
            status = 1;
            break;
        }
        snprintf(path, size, "%s/%s", input_directory, entry->d_name);
        struct stat information;
        if (stat(path, &information) != 0) {
            perror(path);
            free(path);
            status = 1;
            break;
        }
        if (!S_ISREG(information.st_mode)) {
            free(path);
            continue;
        }
        char **larger = grow(files, &capacity, count + 1, sizeof(char *));
        if (larger == NULL) {
            free(path);
            status = 1;
            break;
        }
        files = larger;
        files[count++] = path;
    }
    if (closedir(directory) != 0) {
        perror("Closing input directory");
        status = 1;
    }
    for (size_t i = 0; i < count; ++i) {
        if (process_file(output_directory, files[i]) != 0) {
            status = 1;
        }
        free(files[i]);
    }
    free(files);
    return status;
}

// default folder name;
char *make_default_directory(const char *input_path, int directory_mode)
{
    size_t end = strlen(input_path);
    while (end > 0 && input_path[end - 1] == '/') {
        --end;
    }
    size_t start = end;
    while (start > 0 && input_path[start - 1] != '/') {
        --start;
    }
    if (!directory_mode && end - start >= 4 && memcmp(input_path + end - 4, ".txt", 4) == 0) {
        end -= 4;
    }
    const char suffix[] = "_giyosiddin_251ADB137";
    size_t length = end - start;
    if (directory_mode && length == 0) {
        input_path = "root";
        start = 0;
        length = 4;
    }
    char *directory = malloc(length + sizeof(suffix));
    if (directory == NULL) {
        fprintf(stderr, "Not enough memory for output directory\n");
        return NULL;
    }
    memcpy(directory, input_path + start, length);
    memcpy(directory + length, suffix, sizeof(suffix));
    return directory;
}

int usage(const char *program)
{
    fprintf(stderr, "Usage: %s [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n", program);
    return 1;
}

// command-line handling; AI (Claude)
int main(int argc, char *argv[])
{
    const char *output_directory = NULL;
    const char *input_path = NULL;
    int directory_mode = 0;
    int options_finished = 0;
    for (int i = 1; i < argc; ++i) {
        if (!options_finished && strcmp(argv[i], "--") == 0) {
            options_finished = 1;
        } else if (!options_finished &&
                   (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output-dir") == 0)) {
            if (output_directory != NULL || i + 1 == argc) {
                return usage(argv[0]);
            }
            output_directory = argv[++i];
        } else if (!options_finished &&
                   (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0)) {
            if (input_path != NULL || i + 1 == argc) {
                return usage(argv[0]);
            }
            input_path = argv[++i];
            directory_mode = 1;
        } else {
            if (input_path != NULL || (!options_finished && argv[i][0] == '-')) {
                return usage(argv[0]);
            }
            input_path = argv[i];
        }
    }
    if (input_path == NULL || input_path[0] == '\0' ||
        (output_directory != NULL && output_directory[0] == '\0')) {
        return usage(argv[0]);
    }
    struct stat information;
    if (stat(input_path, &information) != 0) {
        perror(input_path);
        return 1;
    }
    if (directory_mode ? !S_ISDIR(information.st_mode) : !S_ISREG(information.st_mode)) {
        fprintf(stderr, "%s: wrong input type\n", input_path);
        return 1;
    }
    char *default_directory = NULL;
    if (output_directory == NULL) {
        default_directory = make_default_directory(input_path, directory_mode);
        if (default_directory == NULL) {
            return 1;
        }
        output_directory = default_directory;
    }
    int status = ensure_output_directory(output_directory);
    if (status == 0) {
        if (directory_mode) {
            status = process_directory(output_directory, input_path);
        } else {
            status = process_file(output_directory, input_path);
        }
    }
    free(default_directory);
    return status;
}
// Giyosiddin Kutbiddinov 251ADB137
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: course assignment; calc-tests.zip; https://www.geeksforgeeks.org/c/basics-file-handling-c/ ; https://www.geeksforgeeks.org/c/eof-and-feof-in-c/  

#include <stdio.h>
#include <ctype.h>

int calculate(FILE *input)
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
                printf("%lld\n", result);
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
        printf("%lld\n", result);
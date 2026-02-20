#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// --- STRUCTURES ---

typedef struct {
    char type[20];
    char* value;
    int line;
    int column;
} Token;

typedef struct {
    Token* items;
    int count;
    int capacity;
} TokenList;

typedef struct {
    char* message;
    int line;
    int column;
} Error;

typedef struct {
    Error* items;
    int count;
    int capacity;
} ErrorList;

// --- CONSTANTS ---

const char* KEYWORDS[] = {
    "programa", "inicio", "fim", "inteiro", "flutuante", "se", "entao", "senao",
    "fimse", "para", "de", "ate", "passo", "faca", "fimpara", "enquanto",
    "fimenquanto", "e", "ou", "nao", "leia", "escreva", "escreval",
    "procedimento", "retorna", "vazio", "div"
};
const int KEYWORD_COUNT = 27;

// --- UTILS ---

void init_token_list(TokenList* list) {
    list->count = 0;
    list->capacity = 10;
    list->items = malloc(sizeof(Token) * list->capacity);
}

void add_token(TokenList* list, const char* type, const char* value, int line, int col) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->items = realloc(list->items, sizeof(Token) * list->capacity);
    }
    strncpy(list->items[list->count].type, type, 19);
    list->items[list->count].type[19] = '\0';
    list->items[list->count].value = strdup(value);
    list->items[list->count].line = line;
    list->items[list->count].column = col;
    list->count++;
}

void init_error_list(ErrorList* list) {
    list->count = 0;
    list->capacity = 10;
    list->items = malloc(sizeof(Error) * list->capacity);
}

void add_error(ErrorList* list, const char* message, int line, int col) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->items = realloc(list->items, sizeof(Error) * list->capacity);
    }
    list->items[list->count].message = strdup(message);
    list->items[list->count].line = line;
    list->items[list->count].column = col;
    list->count++;
}

bool is_keyword(const char* lexeme) {
    for (int i = 0; i < KEYWORD_COUNT; i++) {
        if (strcmp(lexeme, KEYWORDS[i]) == 0) return true;
    }
    return false;
}

// --- LEXER LOGIC ---

void skip_ignorable(const char* src, int* pos, int* line, int* col, ErrorList* errors) {
    while (src[*pos] != '\0') {
        char ch = src[*pos];
        if (ch == ' ' || ch == '\t') {
            (*pos)++;
            (*col)++;
        } else if (ch == '\n') {
            (*pos)++;
            (*line)++;
            *col = 1;
        } else if (ch == '/' && src[*pos + 1] == '/') {
            while (src[*pos] != '\0' && src[*pos] != '\n') {
                (*pos)++;
            }
            // We don't increment *col here because we'll hit \n or \0 next
        } else if (ch == '/' && src[*pos + 1] == '*') {
            int start_line = *line;
            int start_col = *col;
            (*pos) += 2;
            (*col) += 2;
            bool closed = false;
            while (src[*pos] != '\0') {
                if (src[*pos] == '*' && src[*pos + 1] == '/') {
                    (*pos) += 2;
                    (*col) += 2;
                    closed = true;
                    break;
                }
                if (src[*pos] == '\n') {
                    (*line)++;
                    *col = 1;
                } else {
                    (*col)++;
                }
                (*pos)++;
            }
            if (!closed) {
                add_error(errors, "Unclosed multi-line comment", start_line, start_col);
            }
        } else {
            break;
        }
    }
}

Token ler_palavra(const char* src, int* pos, int line, int col) {
    int start = *pos;
    while (isalnum(src[*pos]) || src[*pos] == '_') (*pos)++;

    int len = *pos - start;
    char* lexeme = malloc(len + 1);
    strncpy(lexeme, src + start, len);
    lexeme[len] = '\0';

    Token t;
    t.line = line;
    t.column = col;
    t.value = lexeme;
    strcpy(t.type, is_keyword(lexeme) ? "KEYWORD" : "IDENTIFIER");
    return t;
}

Token ler_numero(const char* src, int* pos, int line, int col) {
    int start = *pos;
    bool is_float = false;

    while (isdigit(src[*pos])) (*pos)++;

    if (src[*pos] == '.' && isdigit(src[*pos + 1])) {
        is_float = true;
        (*pos)++;
        while (isdigit(src[*pos])) (*pos)++;
    }

    if (src[*pos] == 'e' || src[*pos] == 'E') {
        int e_pos = *pos;
        (*pos)++;
        if (src[*pos] == '+' || src[*pos] == '-') (*pos)++;

        if (isdigit(src[*pos])) {
            is_float = true;
            while (isdigit(src[*pos])) (*pos)++;
        } else {
            // Backtrack if 'e' is not followed by digits
            *pos = e_pos;
        }
    }

    int len = *pos - start;
    char* lexeme = malloc(len + 1);
    strncpy(lexeme, src + start, len);
    lexeme[len] = '\0';

    Token t;
    t.line = line;
    t.column = col;
    t.value = lexeme;
    strcpy(t.type, is_float ? "FLOAT" : "INTEGER");
    return t;
}

Token ler_string(const char* src, int* pos, int line, int col, ErrorList* errors) {
    int start = *pos;
    int start_col = col;
    (*pos)++; // skip "

    while (src[*pos] != '\0' && src[*pos] != '"') {
        if (src[*pos] == '\\') {
            if (src[*pos + 1] != '\0') (*pos) += 2;
            else (*pos)++;
        } else if (src[*pos] == '\n') {
            add_error(errors, "Unclosed string literal", line, start_col);
            Token t;
            t.type[0] = '\0';
            t.value = NULL;
            return t;
        } else {
            (*pos)++;
        }
    }

    if (src[*pos] == '\0') {
        add_error(errors, "Unclosed string literal", line, start_col);
        Token t;
        t.type[0] = '\0';
        t.value = NULL;
        return t;
    }

    (*pos)++; // skip " closing

    int len = *pos - start;
    char* lexeme = malloc(len + 1);
    strncpy(lexeme, src + start, len);
    lexeme[len] = '\0';

    Token t;
    t.line = line;
    t.column = start_col;
    t.value = lexeme;
    strcpy(t.type, "STRING");
    return t;
}

void tokenize(const char* source, TokenList* tokens, ErrorList* errors) {
    int pos = 0, line = 1, col = 1;

    while (source[pos] != '\0') {
        skip_ignorable(source, &pos, &line, &col, errors);
        if (source[pos] == '\0') break;

        char ch = source[pos];
        Token t;
        t.type[0] = '\0';
        t.value = NULL;
        bool found = false;

        if (isalpha(ch) || ch == '_') {
            t = ler_palavra(source, &pos, line, col);
            found = true;
        } else if (isdigit(ch)) {
            t = ler_numero(source, &pos, line, col);
            found = true;
        } else if (ch == '"') {
            t = ler_string(source, &pos, line, col, errors);
            if (t.type[0] != '\0') {
                found = true;
            } else {
                // Error already recorded. The string was likely unclosed.
                // ler_string already moved pos to either \n or \0.
                continue;
            }
        } else if (strchr("+-*/%=!<>|&", ch)) {
            char val[3] = {ch, '\0', '\0'};
            int start_col = col;
            if ((ch == '=' || ch == '!' || ch == '<' || ch == '>') && source[pos+1] == '=') {
                val[1] = '='; pos += 2; col += 2;
            } else if (ch == '&' && source[pos+1] == '&') {
                val[1] = '&'; pos += 2; col += 2;
            } else if (ch == '|' && source[pos+1] == '|') {
                val[1] = '|'; pos += 2; col += 2;
            } else {
                pos++; col++;
            }
            add_token(tokens, "OPERATOR", val, line, start_col);
            continue;
        } else if (strchr("(){};,", ch)) {
            char val[2] = {ch, '\0'};
            add_token(tokens, "DELIMITER", val, line, col);
            pos++; col++;
            continue;
        } else {
            char msg[50];
            sprintf(msg, "Unexpected character '%c'", ch);
            add_error(errors, msg, line, col);
            pos++; col++;
            continue;
        }

        if (found) {
            add_token(tokens, t.type, t.value, t.line, t.column);
            col += strlen(t.value);
            free(t.value);
        }
    }
    add_token(tokens, "EOF", "", line, col);
}

char* read_file_to_string(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = malloc(length + 1);
    if (buffer) {
        fread(buffer, 1, length, f);
        buffer[length] = '\0';
    }
    fclose(f);
    return buffer;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    char* source = read_file_to_string(argv[1]);
    if (!source) {
        fprintf(stderr, "Error: Could not open input file '%s'\n", argv[1]);
        return 1;
    }

    TokenList tokens;
    init_token_list(&tokens);
    ErrorList errors;
    init_error_list(&errors);

    tokenize(source, &tokens, &errors);

    if (errors.count > 0) {
        for (int i = 0; i < errors.count; i++) {
            fprintf(stderr, "Lexical Error at %d:%d: %s\n", errors.items[i].line, errors.items[i].column, errors.items[i].message);
            free(errors.items[i].message);
        }
        free(errors.items);
        for (int i = 0; i < tokens.count; i++) {
            free(tokens.items[i].value);
        }
        free(tokens.items);
        free(source);
        return 1;
    }

    FILE* out = fopen(argv[2], "w");
    if (!out) {
        fprintf(stderr, "Error: Could not open output file '%s' for writing\n", argv[2]);
        for (int i = 0; i < tokens.count; i++) {
            free(tokens.items[i].value);
        }
        free(tokens.items);
        free(errors.items);
        free(source);
        return 1;
    }

    fprintf(out, "%-15s | %-15s | %-5s | %-5s\n", "TYPE", "VALUE", "LINE", "COL");
    fprintf(out, "----------------------------------------------------------\n");
    for (int i = 0; i < tokens.count; i++) {
        fprintf(out, "%-15s | %-15s | %-5d | %-5d\n", tokens.items[i].type, tokens.items[i].value, tokens.items[i].line, tokens.items[i].column);
        free(tokens.items[i].value);
    }

    fclose(out);
    free(tokens.items);
    free(errors.items);
    free(source);

    return 0;
}

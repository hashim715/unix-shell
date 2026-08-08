#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct redirect {
    char type; // < >
    char* filename; // any filename
};

struct cmd {
    char** argv; // NULL-terminated array of words, e.g. {"grep", "foo", NULL}
    int argc;
    struct redirect* redirects;
    int redirect_count;
};

char** tokenize(size_t* token_count, char* buffer, ssize_t* characters_read) {
    char** token_list = (char**)malloc((*token_count) * sizeof(char*));

    if (token_list == NULL) {
        return NULL; // Always check if allocation failed
    };

    bool in_word = false;
    int token_index = 0;

    for (int i = 0; i < (*characters_read - 1); i++) {
        if (buffer[i] == ' ') {
            buffer[i] = '\0';
            in_word = false;
        }
        else if (buffer[i] == '<' || buffer[i] == '>') {
            token_list[token_index++] = (buffer[i] == '<') ? strdup("<") : strdup(">"); // operator is its own token
            if (in_word) {
                buffer[i] = '\0'; // end current word before the operator
            };
            in_word = false;
        }
        else if (in_word == false) {
            token_list[token_index++] = &buffer[i]; // start of a new word
            in_word = true;
        };
    };

    if (token_index + 1 >= *token_count) {
        token_list = realloc(token_list, *((token_count) + 1) * sizeof(char*));
    };

    token_list[token_index] = NULL; // sentinel, like argv

    return token_list;
};

char** tokenize2(char* buffer, ssize_t* characters_read) {
    int capacity = 8;
    char** token_list = (char**)malloc(capacity * sizeof(char*));
    int token_index = 0;

    bool in_word = false;

    for (int i = 0; i < (*characters_read - 1); i++) {
        if (buffer[i] == ' ') {
            buffer[i] = '\0';
            in_word = false;
        }
        else if (buffer[i] == '<' || buffer[i] == '>') {
            if (token_index + 1 >= capacity) { // +1 to always leave room for NULL
                capacity *= 2;
                token_list = realloc(token_list, capacity * sizeof(char*));
            };
            token_list[token_index++] = (buffer[i] == '<') ? strdup("<") : strdup(">"); // operator is its own token
            if (in_word) {
                buffer[i] = '\0'; // end current word before the operator
            };
            in_word = false;
        }
        else if (in_word == false) {
            if (token_index + 1 >= capacity) { // +1 to always leave room for NULL
                capacity *= 2;
                token_list = realloc(token_list, capacity * sizeof(char*));
            };
            token_list[token_index++] = &buffer[i]; // start of a new word
            in_word = true;
        };
    };

    if (token_index + 1 >= capacity) {
        capacity += 1;
        token_list = realloc(token_list, capacity * sizeof(char*));
    };

    token_list[token_index] = NULL; // sentinel, like argv
    return token_list;
};

size_t count_tokens(char* buffer, ssize_t* characters_read) {
    size_t token_count = 0;
    bool in_word = false;

    for (int i = 0; i < (*characters_read - 1); i++) {
        if (buffer[i] == ' ') {
            in_word = false;
        }
        else if (buffer[i] == '<' || buffer[i] == '>') {
            token_count += 1;
            in_word = false;
        }
        else if (in_word == false) {
            token_count += 1;
            in_word = true;
        };
    };

    printf("total tokens count is: %zu\n", token_count);

    return token_count;
};

int main(int argc, char** argv) {
    char* buffer = NULL;
    size_t buff_size = 0;
    ssize_t characters_read;

    while (1) {
        printf("Please enter a command: ");
        // Memory is dynamically allocated inside this call
        characters_read = getline(&buffer, &buff_size, stdin);

        if (characters_read != -1) {
            if (characters_read > 0 && buffer[characters_read - 1] == '\n') {
                buffer[characters_read - 1] = '\0';
            };

            if (strcmp(buffer, ".exit") == 0) {
                free(buffer);
                exit(EXIT_SUCCESS);
            };

            printf("Buffer dynamically allocated to %zu bytes\n", buff_size);
            printf("Characters read: %zd\n", characters_read);
            printf("You entered: %s\n", buffer);

            size_t total_tokens = count_tokens(buffer, &characters_read);

            // char** tokens = tokenize(&total_tokens, buffer, &characters_read);
            char** tokens = tokenize2(buffer, &characters_read);

            if (tokens == NULL) {
                free(buffer);
                printf("tokenization failed please try again..");
                continue;
            };

            int i = 0;
            while (tokens[i] != NULL) {
                printf("Argument %d: %s\n", i, tokens[i]);
                i++;
            };

            free(tokens);
            tokens = NULL;
        } else {
            if (feof(stdin)) {
                // EOF: user hit Ctrl+D or piped file ended — normal shell exit
                free(buffer);
                exit(EXIT_SUCCESS);
            }
            free(buffer);
            exit(EXIT_FAILURE);
        };

        // Crucial step: Prevent a memory leak
        free(buffer);
        buffer = NULL;
        buff_size = 0;
    };

    return 0;
};
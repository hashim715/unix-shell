#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct {
    char type; // < >
    char* filename; // any filename
} redirect;

typedef struct {
    char** argv; // NULL-terminated array of words, e.g. {"grep", "foo", NULL}
    int argc;
    redirect* redirects;
    int redirect_count;
} cmd;

void run_command(cmd* commands, int cmd_count) {
    pid_t pids[cmd_count];
    int pipes[cmd_count - 1][2]; // one pipe (2 fds) between each adjacent pair

    // create all pipes up front, before any forking
    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe failed");
            exit(EXIT_FAILURE);
        };
    };

    for (int i = 0; i < cmd_count; i++) {
        // (set up any pipes needed between commands[i-1] and commands[i] here)
        pids[i] = fork();

        if (pids[i] == 0) {
            // CHILD for commands[i]

            // if not the first command, stdin comes from the previous pipe's read end
            if (i > 0) {
                dup2(pipes[i-1][0], 0);
            };

            // if not the last command, stdout goes to this pipe's write end
            if (i < cmd_count - 1) {
                dup2(pipes[i][1], 1);
            };

            // close EVERY pipe fd this child inherited — used or not
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            };

            // (redirection handling for commands[i].redirects goes here, after pipe dup2s)
            for (int j = 0; j < commands[i].redirect_count; j++) {
                if (commands[i].redirects[j].type == '>') {
                    int fd = open(commands[i].redirects[j].filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    };
                    dup2(fd, 1); // make stdout (fd 1) point to this file
                    close(fd); // don't need the original descriptor anymore
                } else if (commands[i].redirects[j].type == '<') {
                    int fd = open(commands[i].redirects[j].filename, O_RDONLY);
                    if (fd == -1) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    };
                    dup2(fd, 0); // make stdin (fd 0) point to this file
                    close(fd);
                };
            };

            // ADD THIS CHECK:
            if (commands[i].argv == NULL || commands[i].argv[0] == NULL) {
                fprintf(stderr, "Empty command\n");
                exit(EXIT_FAILURE);
            };

            execvp(commands[i].argv[0], commands[i].argv);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        } else if(pids[i] < 0){
            perror("fork failed");

            // clean up: close all pipe fds in the parent
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            };

            // wait for whichever children were already successfully forked (0 .. i-1)
            for (int j = 0; j < i; j++) {
                waitpid(pids[j], NULL, 0);
            };

            exit(EXIT_FAILURE); // or: return, if you don't want to kill the whole shell
        };
    };

    // PARENT: close all pipe fds too — the parent doesn't need any of them,
    // only the children communicate through them
    for (int i = 0; i < cmd_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    };

    for (int i = 0; i < cmd_count; i++) {
        waitpid(pids[i], NULL, 0);
    };
};

void free_commands(cmd* commands, int built_count) {
    // Free all fully-finished commands (0 .. built_count - 1)
    for (int i = 0; i < built_count; i++) {
        if (commands[i].argv != NULL) {
            for (int j = 0; commands[i].argv[j] != NULL; j++) {
                free(commands[i].argv[j]);
            };
            free(commands[i].argv);
        };

        if (commands[i].redirects != NULL) {
            for (int j = 0; j < commands[i].redirect_count; j++) {
                free(commands[i].redirects[j].filename);
            };
            free(commands[i].redirects);
        };
    };
    free(commands);
    commands = NULL;
};

void free_commands_at_malformed(cmd* commands, int built_count, int current_argc, int current_redirect_count) {
    // free fully-finished commands (0 .. cmd_index - 1)
    for (int i = 0; i < built_count; i++) {
        if (commands[i].argv != NULL) {
            for (int j = 0; commands[i].argv[j] != NULL; j++) {
                free(commands[i].argv[j]);
            };
            free(commands[i].argv);
        };

        if (commands[i].redirects != NULL) {
            for (int j = 0; j < commands[i].redirect_count; j++) {
                free(commands[i].redirects[j].filename);
            };
            free(commands[i].redirects);
        };
    };

    // free the in-progress command at cmd_index, using argc/redirect_count as bounds
    if (commands[built_count].argv != NULL) {
        for (int j = 0; j < current_argc; j++) {
            free(commands[built_count].argv[j]);
        };
        free(commands[built_count].argv);
    };

    if (commands[built_count].redirects != NULL) {
        for (int j = 0; j < current_redirect_count; j++) {
            free(commands[built_count].redirects[j].filename);
        };
        free(commands[built_count].redirects);
    };
    free(commands);
    commands = NULL;
};

cmd* parse_tokens(char** token_list, int* cmd_count) {
    int capacity = 5;
    cmd* commands = (cmd*)malloc(capacity * sizeof(cmd));

    if (commands == NULL) {
        printf("Memory allocation failed!\n");
        return NULL; // Always check if allocation failed
    };

    int index = 0;
    int cmd_index = 0;
    int argc = 0;
    int redirect_count = 0;
    int redirect_capacity = 2;
    int arg_capacity = 4;
    int cmd_capacity = 4;

    commands[0].redirects = malloc(redirect_capacity * sizeof(redirect));
    commands[0].argv = malloc(arg_capacity * sizeof(char*));
    commands[0].argc = 0;
    commands[0].redirect_count = 0;

    while (token_list[index] != NULL) {
        if (strcmp(token_list[index], "|") == 0) {
            if (argc + 1 >= arg_capacity) {
                arg_capacity += 1;
                commands[cmd_index].argv = realloc(commands[cmd_index].argv, arg_capacity * sizeof(char*));
            };

            commands[cmd_index].argv[argc] = NULL; // sentinel, like argv

            // ADD THIS CHECK:
            if (argc == 0) {
                free_commands_at_malformed(commands, cmd_index, argc, redirect_count);
                printf("Invalid command: empty command in pipeline\n");
                return NULL;
            };

            if (cmd_index + 1 >= cmd_capacity) {
                cmd_capacity *= 2;
                commands = (cmd*)realloc(commands, cmd_capacity * sizeof(cmd));
            };

            *cmd_count = cmd_index + 1;
            cmd_index++;
            redirect_count = 0;
            argc = 0;
            arg_capacity = 4;
            redirect_capacity = 2;
            commands[cmd_index].redirects = malloc(redirect_capacity * sizeof(redirect)); // capacity 2, allocated now
            commands[cmd_index].argv = malloc(arg_capacity * sizeof(char*));
            // capacity 4, allocated now
            commands[cmd_index].argc = argc;
            commands[cmd_index].redirect_count = redirect_count;
        } 
        else if (strcmp(token_list[index], ">") == 0)  {
            if (redirect_count >= redirect_capacity) {
                redirect_capacity *= 2;
                commands[cmd_index].redirects = realloc(commands[cmd_index].redirects,redirect_capacity * sizeof(redirect));
            };

            if (token_list[index + 1] == NULL ||
                strcmp(token_list[index + 1], ">") == 0 ||
                strcmp(token_list[index + 1], "<") == 0 ||
                strcmp(token_list[index + 1], "|") == 0) {
                free_commands_at_malformed(commands, cmd_index, argc, redirect_count);
                printf("Invalid command parsing failed!\n");
                return NULL;
            };

            commands[cmd_index].redirects[redirect_count].filename = strdup(token_list[index + 1]);
         
            commands[cmd_index].redirects[redirect_count].type = '>';

            commands[cmd_index].redirect_count = redirect_count + 1;
            redirect_count++;
            index++;
            index++;
            continue;
        } else if (strcmp(token_list[index], "<") == 0) {
            if (redirect_count >= redirect_capacity) {
                redirect_capacity *= 2;
                commands[cmd_index].redirects = realloc(commands[cmd_index].redirects,redirect_capacity * sizeof(redirect));
            };

            if (token_list[index + 1] == NULL ||
                strcmp(token_list[index + 1], ">") == 0 ||
                strcmp(token_list[index + 1], "<") == 0 ||
                strcmp(token_list[index + 1], "|") == 0) {
                free_commands_at_malformed(commands, cmd_index, argc, redirect_count);
                printf("Invalid command parsing failed!\n");
                return NULL;
            };

            commands[cmd_index].redirects[redirect_count].filename = strdup(token_list[index + 1]);
         
            commands[cmd_index].redirects[redirect_count].type = '<';
            
            commands[cmd_index].redirect_count = redirect_count + 1;
            redirect_count++;
            index++;
            index++;
            continue;
        } else {
            if (argc + 1 >= arg_capacity) {
                arg_capacity *= 2;
                commands[cmd_index].argv = realloc(commands[cmd_index].argv, arg_capacity * sizeof(char*));
            }

            commands[cmd_index].argv[argc] = strdup(token_list[index]);
            commands[cmd_index].argc = argc + 1;
            argc++;
        };
        index++;
    };

    if (argc + 1 >= arg_capacity) {
        arg_capacity += 1;
        commands[cmd_index].argv = realloc(commands[cmd_index].argv, arg_capacity * sizeof(char*));
    };

    commands[cmd_index].argv[argc] = NULL; // sentinel, like argv
    commands[cmd_index].argc = argc;
    commands[cmd_index].redirect_count = redirect_count;

    // ADD THIS CHECK:
    if (argc == 0) {
        free_commands_at_malformed(commands, cmd_index, argc, redirect_count);
        printf("Invalid command: empty command in pipeline\n");
        return NULL;
    };

    *cmd_count = cmd_index + 1;   // <-- add this, count the final command too

    return commands;
};

char** tokenize(char* buffer, ssize_t* characters_read) {
    int capacity = 8;
    char** token_list = (char**)malloc(capacity * sizeof(char*));
    int token_index = 0;

    bool in_word = false;
    int word_start = -1;

    for (int i = 0; i < (*characters_read - 1); i++) {
        if (buffer[i] == ' ') {
            if (in_word) {
                buffer[i] = '\0';
                if (token_index + 1 >= capacity) {
                    capacity *= 2;
                    token_list = realloc(token_list, capacity * sizeof(char*));
                };
                token_list[token_index++] = strdup(&buffer[word_start]);
            };
            in_word = false;
        }
        else if (buffer[i] == '<' || buffer[i] == '>' || buffer[i] == '|') {
            char operator = buffer[i];

            if (in_word) {
                buffer[i] = '\0';
                if (token_index + 1 >= capacity) {
                    capacity *= 2;
                    token_list = realloc(token_list, capacity * sizeof(char*));
                };
                token_list[token_index++] = strdup(&buffer[word_start]);
                in_word = false;
            };

            if (token_index + 1 >= capacity) { // +1 to always leave room for NULL
                capacity *= 2;
                token_list = realloc(token_list, capacity * sizeof(char*));
            };

            switch (operator) {
                case '<':
                    token_list[token_index++] = strdup("<");
                    break;
                case '>':
                    token_list[token_index++] = strdup(">");
                    break;
                case '|':
                    token_list[token_index++] = strdup("|");
                    break;
                default:
                    printf("Invalid token found\n");
            };
        }
        else if (!in_word) {
            word_start = i;
            in_word = true;
        };
    };

       // handle a word that runs to the very end of the string
    if (in_word) {
        if (token_index + 1 >= capacity) {
            capacity += 1;
            token_list = realloc(token_list, capacity * sizeof(char*));
        };
        token_list[token_index++] = strdup(&buffer[word_start]);
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
        else if (buffer[i] == '<' || buffer[i] == '>' || buffer[i] == '|') {
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
                buffer = NULL;
                exit(EXIT_SUCCESS);
            };

            printf("Buffer dynamically allocated to %zu bytes\n", buff_size);
            printf("Characters read: %zd\n", characters_read);
            printf("You entered: %s\n", buffer);

            char** tokens = tokenize(buffer, &characters_read);

            if (tokens == NULL) {
                free(buffer);
                buffer = NULL;
                printf("tokenization failed please try again..");
                continue;
            };

            int cmd_count;
            cmd* commands = parse_tokens(tokens, &cmd_count);

            if (commands == NULL) {
                free(buffer);
                buffer = NULL;
                for (int i = 0; tokens[i] != NULL; i++) {
                    free(tokens[i]);
                };

                free(tokens);
                tokens = NULL;

                printf("tokenization failed please try again..\n");
                continue;
            };

            printf("total commands: %d\n", cmd_count);

            for (int i = 0; i < cmd_count; i++) {
                printf("argc: %d\n", commands[i].argc);

                if (commands[i].argv != NULL) {
                    for (int j = 0; commands[i].argv[j] != NULL; j++) {
                        printf("argument no: %d and argument is: %s\n", j ,commands[i].argv[j]);
                    };
                };

                printf("redirect count: %d\n", commands[i].redirect_count);

                if (commands[i].redirects != NULL) {
                    for (int j = 0; j < commands[i].redirect_count; j++) {
                        printf("redirect no: %d and redirect type is: %c and redirect filename is: %s\n", j, commands[i].redirects[j].type ,commands[i].redirects[j].filename);
                    };
                };
            };

            run_command(commands, cmd_count);

            for (int i = 0; tokens[i] != NULL; i++) {
                free(tokens[i]);
            };

            free(tokens);
            free_commands(commands, cmd_count);
            
            tokens = NULL;
        } else {
            if (feof(stdin)) {
                // EOF: user hit Ctrl+D or piped file ended — normal shell exit
                free(buffer);
                buffer = NULL;
                exit(EXIT_SUCCESS);
            };
            free(buffer);
            buffer = NULL;
            exit(EXIT_FAILURE);
        };

        // Crucial step: Prevent a memory leak
        free(buffer);
        buffer = NULL;
        buff_size = 0;
    };

    return 0;
};
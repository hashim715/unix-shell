# unix_shell_from_scratch

A Unix shell implemented from scratch in C, built as a learning project to understand how shells read, parse, and (eventually) execute commands.

## Status

Work in progress. Currently implemented:

- A REPL loop that reads a line of input via `getline`.
- Tokenization of the input line into words and `<`/`>` redirect operators, with `<`/`>` treated as standalone tokens (e.g. `cat<file` tokenizes to `{"cat", "<", "file"}`).
- A `.exit` command to quit the shell.

Not yet implemented:

- Actually executing commands (`fork`/`exec`).
- Applying `<`/`>` redirects to child process file descriptors.
- Populating the `struct cmd` / `struct redirect` types beyond tokenizing.

## Building

```sh
clang -o shell shell.c
```

## Running

```sh
./shell
```

Type a command and press enter. Type `.exit` or press Ctrl+D to quit.

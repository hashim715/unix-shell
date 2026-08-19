# unix_shell_from_scratch

A Unix shell implemented from scratch in C, built as a learning project to understand how shells read, parse, and (eventually) execute commands.

## Status

Work in progress. Currently implemented:

- A REPL loop that reads a line of input via `getline`.
- Tokenization of the input line into words and `<`, `>`, `|` operators, each treated as a standalone token (e.g. `cat<file` tokenizes to `{"cat", "<", "file"}`).
- `parse_tokens`, which turns the token list into an array of `cmd` structs: `|` starts a new command, `<`/`>` attach a redirect to the current command, everything else is appended to `argv`. Malformed input (a redirect with no filename, or an empty command in a pipeline) is rejected and any partially-built state is freed.
- `run_command`, which executes the parsed pipeline for real: one `pipe()` per adjacent command pair, `fork()` per command, `dup2` to wire stdin/stdout through the pipes, `<`/`>` redirects applied via `open`+`dup2` after the pipe setup, and `execvp` to run the program. The parent closes its pipe fds and waits on every child.
- A `.exit` command to quit the shell.
- `run_shell_tests.sh`, a self-checking test harness that feeds command sequences into the shell binary via stdin and checks the resulting files/stdout against expected output.

Not yet implemented:

- Background jobs (`&`), job control, signal handling (Ctrl+C/Ctrl+Z).
- Built-in commands beyond `.exit` (e.g. `cd`).
- Quoting/escaping in the tokenizer.

## Building

```sh
clang -o shell shell.c
```

## Running

```sh
./shell
```

Type a command and press enter — pipes (`|`) and redirects (`<`, `>`) are supported, e.g. `grep foo < input.txt | wc -l > out.txt`. Type `.exit` or press Ctrl+D to quit.

## Testing

```sh
./run_shell_tests.sh ./shell
```

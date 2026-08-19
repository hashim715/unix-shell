#!/usr/bin/env bash
#
# run_shell_tests.sh — a self-checking test harness for your CS111-style shell.
#
# Usage:
#   ./run_shell_tests.sh /path/to/your/shell_binary
#
# It creates a scratch directory, feeds command sequences into your shell
# via stdin (the same "./sh111 < commands.txt" trick from the assignment),
# and checks the resulting file contents / captured stdout against what's
# expected. It does NOT try to parse your shell's own debug printf output
# (like "argc: 2" etc) — it only checks real side effects: files created,
# and what the underlying programs (wc, grep, sort...) actually printed.
#
# Exit code: 0 if everything passed, 1 if anything failed.

set -u

if [ $# -ne 1 ]; then
    echo "Usage: $0 /path/to/your/shell_binary"
    exit 1
fi

SHELL_BIN="$1"

if [ ! -x "$SHELL_BIN" ]; then
    echo "Error: '$SHELL_BIN' does not exist or is not executable."
    echo "Did you run 'make' first?"
    exit 1
fi

# Resolve to an absolute path BEFORE we cd into the scratch dir
SHELL_BIN="$(cd "$(dirname "$SHELL_BIN")" && pwd)/$(basename "$SHELL_BIN")"

SCRATCH_DIR="$(mktemp -d)"
PASS_COUNT=0
FAIL_COUNT=0

cleanup() {
    rm -rf "$SCRATCH_DIR"
}
trap cleanup EXIT

pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "  PASS: $1"
}

fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo "  FAIL: $1"
}

# run_shell <commands_file> <captured_output_file>
# Feeds commands_file into the shell's stdin, captures combined stdout+stderr.
run_shell() {
    local cmds_file="$1"
    local out_file="$2"
    ( cd "$SCRATCH_DIR" && "$SHELL_BIN" < "$cmds_file" ) > "$out_file" 2>&1
}

echo "=================================================="
echo "Testing shell binary: $SHELL_BIN"
echo "Scratch directory:    $SCRATCH_DIR"
echo "=================================================="

# ---------------------------------------------------------------------------
echo ""
echo "[1] Single simple command (no redirection, no pipe)"
# ---------------------------------------------------------------------------
cat > "$SCRATCH_DIR/t1_cmds.txt" <<'EOF'
echo hello_world
EOF
run_shell "$SCRATCH_DIR/t1_cmds.txt" "$SCRATCH_DIR/t1_out.txt"
if grep -q "hello_world" "$SCRATCH_DIR/t1_out.txt"; then
    pass "echo hello_world produced expected output"
else
    fail "echo hello_world did NOT appear in shell output (see $SCRATCH_DIR/t1_out.txt)"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[2] Output redirection with >"
# ---------------------------------------------------------------------------
cat > "$SCRATCH_DIR/t2_cmds.txt" <<'EOF'
echo redirected_content > t2_out.txt
EOF
run_shell "$SCRATCH_DIR/t2_cmds.txt" "$SCRATCH_DIR/t2_shell_log.txt"
if [ -f "$SCRATCH_DIR/t2_out.txt" ] && grep -q "redirected_content" "$SCRATCH_DIR/t2_out.txt"; then
    pass "t2_out.txt was created and contains expected content"
else
    fail "t2_out.txt missing or wrong content"
fi
# It should NOT have printed "redirected_content" as a STANDALONE line in the
# terminal/log (i.e. echo's own output line), since it was redirected to a file.
# NOTE: we deliberately only match a line that is EXACTLY the marker word, not
# any line merely containing it — otherwise this would false-trigger on your
# shell's own debug output, e.g. a line like:
#   You entered: echo redirected_content > t2_out.txt
# which legitimately contains the same word but is not a leak of echo's output.
if grep -qx "redirected_content" "$SCRATCH_DIR/t2_shell_log.txt"; then
    fail "redirected_content leaked to terminal output — > redirection did not actually redirect stdout"
else
    pass "redirected_content did not leak to terminal (correctly went to file only)"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[3] Input redirection with <"
# ---------------------------------------------------------------------------
printf "line one\nline two\nline three\n" > "$SCRATCH_DIR/t3_in.txt"
cat > "$SCRATCH_DIR/t3_cmds.txt" <<'EOF'
wc -l < t3_in.txt
EOF
run_shell "$SCRATCH_DIR/t3_cmds.txt" "$SCRATCH_DIR/t3_out.txt"
if grep -Eq "^\s*3\s*$" "$SCRATCH_DIR/t3_out.txt"; then
    pass "wc -l < t3_in.txt correctly reported 3 lines"
else
    fail "wc -l < t3_in.txt did not report 3 (see $SCRATCH_DIR/t3_out.txt)"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[4] Both redirects on one command (< and >)"
# ---------------------------------------------------------------------------
printf "banana\napple\ncherry\n" > "$SCRATCH_DIR/t4_in.txt"
cat > "$SCRATCH_DIR/t4_cmds.txt" <<'EOF'
sort < t4_in.txt > t4_out.txt
EOF
run_shell "$SCRATCH_DIR/t4_cmds.txt" "$SCRATCH_DIR/t4_shell_log.txt"
EXPECTED_SORTED=$(printf "apple\nbanana\ncherry")
ACTUAL_SORTED=$(cat "$SCRATCH_DIR/t4_out.txt" 2>/dev/null)
if [ "$ACTUAL_SORTED" == "$EXPECTED_SORTED" ]; then
    pass "sort < in > out produced correctly sorted file"
else
    fail "sort < in > out did not produce expected sorted content"
    echo "    expected: $(echo "$EXPECTED_SORTED" | tr '\n' '|')"
    echo "    actual:   $(echo "$ACTUAL_SORTED" | tr '\n' '|')"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[5] Two-command pipeline"
# ---------------------------------------------------------------------------
mkdir -p "$SCRATCH_DIR/t5_dir"
touch "$SCRATCH_DIR/t5_dir/a" "$SCRATCH_DIR/t5_dir/b" "$SCRATCH_DIR/t5_dir/c"
cat > "$SCRATCH_DIR/t5_cmds.txt" <<'EOF'
ls t5_dir | wc -l
EOF
run_shell "$SCRATCH_DIR/t5_cmds.txt" "$SCRATCH_DIR/t5_out.txt"
if grep -Eq "^\s*3\s*$" "$SCRATCH_DIR/t5_out.txt"; then
    pass "ls t5_dir | wc -l correctly reported 3 files"
else
    fail "ls t5_dir | wc -l did not report 3 (see $SCRATCH_DIR/t5_out.txt)"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[6] Three-command (N-length) pipeline"
# ---------------------------------------------------------------------------
mkdir -p "$SCRATCH_DIR/t6_dir"
touch "$SCRATCH_DIR/t6_dir/shell_notes.txt" "$SCRATCH_DIR/t6_dir/other.txt" "$SCRATCH_DIR/t6_dir/shell_more.txt"
cat > "$SCRATCH_DIR/t6_cmds.txt" <<'EOF'
ls t6_dir | grep shell | wc -l
EOF
run_shell "$SCRATCH_DIR/t6_cmds.txt" "$SCRATCH_DIR/t6_out.txt"
if grep -Eq "^\s*2\s*$" "$SCRATCH_DIR/t6_out.txt"; then
    pass "ls | grep | wc -l three-stage pipeline correctly reported 2"
else
    fail "three-stage pipeline did not report 2 (see $SCRATCH_DIR/t6_out.txt)"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[7] Redirection takes priority over pipe (spec's own example)"
# ---------------------------------------------------------------------------
printf "std::string foo;\nint x = 5;\nstd::string bar;\n" > "$SCRATCH_DIR/t7_shell.cc"
cat > "$SCRATCH_DIR/t7_cmds.txt" <<'EOF'
grep std::string t7_shell.cc > t7_grep.out | wc
EOF
run_shell "$SCRATCH_DIR/t7_cmds.txt" "$SCRATCH_DIR/t7_shell_log.txt"

# grep.out should contain the 2 matching lines
if [ -f "$SCRATCH_DIR/t7_grep.out" ] && [ "$(grep -c 'std::string' "$SCRATCH_DIR/t7_grep.out")" == "2" ]; then
    pass "t7_grep.out contains the 2 expected matching lines"
else
    fail "t7_grep.out missing or wrong content (redirection may not have worked)"
fi

# wc should report all zeros, since grep's stdout was redirected away from the pipe
if grep -Eq "^\s*0\s+0\s+0\s*$" "$SCRATCH_DIR/t7_shell_log.txt"; then
    pass "wc correctly reported '0 0 0' (redirection correctly took priority over the pipe)"
else
    fail "wc did NOT report all zeros — redirection may not be taking priority over the pipe"
    echo "    shell output was:"
    sed 's/^/      /' "$SCRATCH_DIR/t7_shell_log.txt"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[8] Malformed input: dangling redirect (should not crash)"
# ---------------------------------------------------------------------------
cat > "$SCRATCH_DIR/t8_cmds.txt" <<'EOF'
ls >
echo still_alive_after_error
EOF
run_shell "$SCRATCH_DIR/t8_cmds.txt" "$SCRATCH_DIR/t8_out.txt"
SHELL_EXIT=$?
if grep -q "still_alive_after_error" "$SCRATCH_DIR/t8_out.txt"; then
    pass "shell survived malformed 'ls >' and kept processing later commands"
else
    fail "shell did not recover from malformed 'ls >' (crashed or hung?)"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[9] Nonexistent command (should error gracefully, not crash)"
# ---------------------------------------------------------------------------
cat > "$SCRATCH_DIR/t9_cmds.txt" <<'EOF'
this_command_does_not_exist_xyz
echo still_alive_after_bad_command
EOF
run_shell "$SCRATCH_DIR/t9_cmds.txt" "$SCRATCH_DIR/t9_out.txt"
if grep -q "still_alive_after_bad_command" "$SCRATCH_DIR/t9_out.txt"; then
    pass "shell survived nonexistent command and kept processing later commands"
else
    fail "shell did not recover from nonexistent command (crashed or hung?)"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[10] Empty/double-pipe segment (edge case — should not crash)"
# ---------------------------------------------------------------------------
cat > "$SCRATCH_DIR/t10_cmds.txt" <<'EOF'
ls || wc
echo still_alive_after_double_pipe
EOF
run_shell "$SCRATCH_DIR/t10_cmds.txt" "$SCRATCH_DIR/t10_out.txt"
if grep -q "still_alive_after_double_pipe" "$SCRATCH_DIR/t10_out.txt"; then
    pass "shell survived 'ls || wc' (empty pipeline segment) without crashing"
else
    fail "shell did not recover from 'ls || wc' (crashed or hung?)"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[11] File descriptor cleanliness check"
# ---------------------------------------------------------------------------
# Compile a tiny helper that just reports how many fds are open above 2.
# This works cross-platform (Linux /proc or macOS fcntl probing) without
# relying on /proc/self/fd, which doesn't exist on macOS.
cat > "$SCRATCH_DIR/fdcheck.c" <<'EOF'
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int main(void) {
    int extra_open = 0;
    // Check a generous range of fd numbers beyond stderr.
    for (int fd = 3; fd < 256; fd++) {
        errno = 0;
        int flags = fcntl(fd, F_GETFD);
        if (flags != -1) {
            extra_open++;
            fprintf(stderr, "fdcheck: unexpected open fd = %d\n", fd);
        }
    }
    printf("EXTRA_OPEN_FDS=%d\n", extra_open);
    return 0;
}
EOF
if cc -o "$SCRATCH_DIR/fdcheck" "$SCRATCH_DIR/fdcheck.c" 2>"$SCRATCH_DIR/fdcheck_compile_err.txt"; then
    cat > "$SCRATCH_DIR/t11_cmds.txt" <<EOF
$SCRATCH_DIR/fdcheck
EOF
    run_shell "$SCRATCH_DIR/t11_cmds.txt" "$SCRATCH_DIR/t11_out.txt"
    if grep -q "EXTRA_OPEN_FDS=0" "$SCRATCH_DIR/t11_out.txt"; then
        pass "child process had no unexpected open file descriptors beyond 0/1/2"
    else
        fail "child process has extra open file descriptors (see $SCRATCH_DIR/t11_out.txt) — you likely need more close() calls"
    fi
else
    echo "  SKIP: could not compile fdcheck.c helper, skipping fd cleanliness test"
    cat "$SCRATCH_DIR/fdcheck_compile_err.txt"
fi

# ---------------------------------------------------------------------------
echo ""
echo "[12] Pipeline with a dangling/invalid inner redirect (should not crash whole shell)"
# ---------------------------------------------------------------------------
cat > "$SCRATCH_DIR/t12_cmds.txt" <<'EOF'
ls | grep < 
echo still_alive_after_invalid_pipeline
EOF
run_shell "$SCRATCH_DIR/t12_cmds.txt" "$SCRATCH_DIR/t12_out.txt"
if grep -q "still_alive_after_invalid_pipeline" "$SCRATCH_DIR/t12_out.txt"; then
    pass "shell survived an invalid mid-pipeline redirect without crashing"
else
    fail "shell did not recover from invalid mid-pipeline redirect (crashed or hung?)"
fi

# ---------------------------------------------------------------------------
echo ""
echo "=================================================="
echo "RESULTS: $PASS_COUNT passed, $FAIL_COUNT failed"
echo "=================================================="

if [ "$FAIL_COUNT" -gt 0 ]; then
    exit 1
fi
exit 0
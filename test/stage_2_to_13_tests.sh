#!/bin/sh

set -eu

MINISHELL=${1:-./minishell}
TMPDIR=${TMPDIR:-/tmp}
WORKDIR="$TMPDIR/minishell_stage_tests_$$"
PASS=0
FAIL=0
SKIP=0

mkdir -p "$WORKDIR"
cd "$WORKDIR"

cleanup() {
	cd / >/dev/null 2>&1 || true
	rm -rf "$WORKDIR"
}
trap cleanup EXIT HUP INT TERM

run_shell() {
	printf '%s\n' "$1" | "$MINISHELL" 2>&1
}

expect_status() {
	name=$1
	input=$2
	want=$3
	set +e
	printf '%s\n' "$input" | "$MINISHELL" >/tmp/minishell_test_out_$$ 2>/tmp/minishell_test_err_$$
	got=$?
	set -e
	rm -f /tmp/minishell_test_out_$$ /tmp/minishell_test_err_$$
	if [ "$got" -eq "$want" ]; then
		PASS=$((PASS + 1))
		printf 'ok - %s\n' "$name"
	else
		FAIL=$((FAIL + 1))
		printf 'not ok - %s: expected status %s, got %s\n' "$name" "$want" "$got"
	fi
}

expect_stdout() {
	name=$1
	input=$2
	want=$3
	set +e
	got=$(run_shell "$input")
	status=$?
	set -e
	if [ "$status" -eq 0 ] && [ "$got" = "$want" ]; then
		PASS=$((PASS + 1))
		printf 'ok - %s\n' "$name"
	else
		FAIL=$((FAIL + 1))
		printf 'not ok - %s\nexpected: [%s]\ngot:      [%s]\nstatus:   [%s]\n' \
			"$name" "$want" "$got" "$status"
	fi
}

expect_file() {
	name=$1
	input=$2
	file=$3
	want=$4
	set +e
	printf '%s\n' "$input" | "$MINISHELL" >/tmp/minishell_test_out_$$ 2>/tmp/minishell_test_err_$$
	status=$?
	set -e
	if [ -f "$file" ]; then
		got=$(cat "$file")
	else
		got="__missing__"
	fi
	rm -f /tmp/minishell_test_out_$$ /tmp/minishell_test_err_$$
	if [ "$status" -eq 0 ] && [ "$got" = "$want" ]; then
		PASS=$((PASS + 1))
		printf 'ok - %s\n' "$name"
	else
		FAIL=$((FAIL + 1))
		printf 'not ok - %s\nexpected file: [%s]\ngot:           [%s]\nstatus:        [%s]\n' \
			"$name" "$want" "$got" "$status"
	fi
}

skip_manual() {
	SKIP=$((SKIP + 1))
	printf 'skip - %s\n' "$1"
}

# Stage 2: syntax validation
expect_status 'reject pipe at beginning' '| cat' 2
expect_status 'reject pipe at end' 'echo hello |' 2
expect_status 'reject consecutive pipes' 'echo hello || cat' 2
expect_status 'reject separated pipes' 'echo hello | | cat' 2
expect_status 'reject output redirection without target' 'echo hello >' 2
expect_status 'reject input redirection without target' 'cat <' 2
expect_status 'reject heredoc without delimiter' 'cat <<' 2
expect_status 'reject redirection followed by redirection' 'echo hello > > out' 2
expect_status 'reject redirection followed by pipe' 'cat < | wc' 2
expect_status 'reject unsupported and-if' 'true && echo no' 2
expect_status 'reject unsupported or-if' 'false || echo no' 2
expect_status 'reject unsupported semicolon' 'echo one ; echo two' 2
expect_status 'reject unsupported subshell open paren' '( echo hi )' 2

# Stage 3 and 4: parser structure, quote removal, and expansion through behavior
expect_stdout 'empty input is no-op' '' ''
expect_stdout 'spaces-only input is no-op' '     ' ''
expect_stdout 'single quoted string stays one arg' "echo 'hello world'" 'hello world'
expect_stdout 'double quoted string stays one arg' 'echo "hello world"' 'hello world'
expect_stdout 'mixed quoted word joins into one token' 'echo hello"world"' 'helloworld'
expect_stdout 'single quotes suppress variable expansion' "echo '\$HOME'" '$HOME'
expect_stdout 'double quotes allow variable expansion' 'HOME=/tmp echo "$HOME"' '/tmp'
expect_stdout 'empty quoted argument is preserved' 'echo "" x' ' x'
expect_stdout 'unset variable expands to empty' 'echo "$NOT_EXIST"' ''
expect_stdout 'variable adjacent to text expands' 'USER=me echo abc$USER.txt' 'abcme.txt'
expect_stdout 'unquoted variable field splitting' 'X="a b c"; echo $X' 'a b c'
expect_stdout 'quoted variable prevents field splitting' 'X="a b c"; echo "$X"' 'a b c'

# Stage 5: builtins
expect_stdout 'echo prints newline by default' 'echo hello' 'hello'
expect_stdout 'echo supports -n' 'echo -n hello' 'hello'
expect_stdout 'echo supports repeated -n flags' 'echo -n -nn hello' 'hello'
expect_stdout 'echo treats invalid -n form as text' 'echo -n-n hello' '-n-n hello'
expect_stdout 'pwd prints current directory' 'pwd' "$WORKDIR"
expect_stdout 'cd changes shell cwd before next command' 'cd /tmp
pwd' '/tmp'
expect_stdout 'cd without arg uses HOME' 'HOME=/tmp cd
pwd' '/tmp'
expect_stdout 'cd updates OLDPWD' 'cd /tmp
echo $OLDPWD' "$WORKDIR"
expect_stdout 'cd - returns to previous directory' 'cd /tmp
cd - >/dev/null
pwd' "$WORKDIR"
expect_stdout 'export sets variable' 'export A=a=b=c
echo "$A"' 'a=b=c'
expect_stdout 'export sets multiple variables' 'export A=1 B=2
echo "$A $B"' '1 2'
expect_status 'export rejects leading digit' 'export 1ABC=x' 1
expect_status 'export rejects dash in identifier' 'export A-B=x' 1
expect_status 'export rejects empty key' 'export =x' 1
expect_stdout 'unset removes variable' 'export A=ok
unset A
echo "$A"' ''
expect_stdout 'unset removes multiple variables' 'export A=ok B=ok
unset A B
echo "$A$B"' ''
expect_stdout 'unset ignores missing variables' 'unset DOES_NOT_EXIST
echo ok' 'ok'
expect_stdout 'env prints assigned variables' 'export A=ok
env | grep "^A="' 'A=ok'
expect_status 'exit numeric status wraps to 0-255' 'exit 257' 1
expect_status 'exit non-numeric status is error' 'exit abc' 2

# Stage 6: redirection execution
expect_file 'output redirection creates and truncates' 'echo hello > out' out 'hello'
printf 'old\n' > append.out
expect_file 'append redirection preserves content' 'echo new >> append.out' append.out 'old
new'
printf 'input\n' > in
expect_stdout 'input redirection feeds stdin' 'cat < in' 'input'
expect_stdout 'input redirection only is no-op' '< in' ''
expect_file 'redirection-only command creates file' '> only_redir' only_redir ''
expect_status 'missing input redirection fails' 'cat < missing' 1
expect_file 'last output redirection wins' 'echo hello > first > second' second 'hello'
test -f first && PASS=$((PASS + 1)) && printf 'ok - earlier output redirection still creates file\n' \
	|| { FAIL=$((FAIL + 1)); printf 'not ok - earlier output redirection still creates file\n'; }
mkdir -p redir_dir
expect_status 'output redirection to directory fails' 'echo hi > redir_dir' 1
expect_status 'input redirection from directory fails' 'cat < redir_dir' 1

# Stage 7 and 8: external commands and pipelines
expect_status 'command not found returns 127' 'definitely_not_a_command' 127
expect_status 'directory execution returns 126' '/tmp' 126
expect_stdout 'simple pipeline connects stdout to stdin' 'echo hello | cat' 'hello'
expect_stdout 'pipeline to wc works' 'echo hello | wc -c' '6'
expect_status 'pipeline status is last command success' 'false | true' 0
expect_status 'pipeline status is last command failure' 'true | false' 1
expect_file 'redirection overrides pipeline stdout' 'echo hello > piped_out | cat' piped_out 'hello'

# Stage 9: heredoc
expect_stdout 'basic heredoc feeds stdin' 'cat << EOF
hello
EOF' 'hello'
expect_stdout 'unquoted heredoc delimiter expands body' 'A=ok cat << EOF
$A
EOF' 'ok'
expect_stdout 'quoted heredoc delimiter keeps body literal' "A=ok cat << 'EOF'
\$A
EOF" '$A'
expect_stdout 'heredoc into pipeline' 'cat << EOF | wc -l
one
two
EOF' '2'
expect_stdout 'multiple heredocs: last wins' 'cat << EOF << END
first
EOF
second
END' 'second'

# Stage 10 to 12: environment, signals, exit status
expect_status 'missing PATH handled as command not found' 'unset PATH
no_such_command' 127
expect_stdout 'exit status expands after false' 'false
echo $?' '1'
expect_stdout 'exit status expands after true' 'true
echo $?' '0'
skip_manual 'Ctrl+C at prompt should redraw prompt and keep shell alive'
skip_manual 'Ctrl+D at prompt should exit shell'
skip_manual 'Ctrl+\ at prompt should be ignored'
skip_manual 'Ctrl+C during cat/sleep should set status 130'
skip_manual 'Ctrl+\ during foreground command should set status 131 if matching Bash'
skip_manual 'Ctrl+C during heredoc should abort heredoc and skip execution'

printf '\nsummary: %s passed, %s failed, %s skipped\n' "$PASS" "$FAIL" "$SKIP"
if [ "$FAIL" -ne 0 ]; then
	exit 1
fi
*This project has been created as part of the 42 curriculum by lkai-yua, kong.*

# minishell

## Description

`minishell` is a simplified re-implementation of a POSIX-like shell (bash-inspired),
built from scratch in C as part of the 42 core curriculum. The goal of the project is
to understand, at a low level, how a shell actually works: reading a line of input,
turning it into tokens, parsing it into a command structure, expanding variables and
quotes, and finally executing the result as one or more processes connected by pipes
and redirections.

The shell supports:

- An interactive prompt built on [GNU Readline](https://tiswww.case.edu/php/chet/readline/rltop.html) (with history navigation).
- Simple commands and pipelines (`cmd1 | cmd2 | cmd3`).
- Redirections: `<`, `>`, `>>`, and heredocs (`<<`).
- Single (`'...'`) and double (`"..."`) quoting, with correct expansion rules for each.
- Environment variable expansion (`$VAR`, `$?`) and unquoted word splitting.
- Built-in commands implemented internally: `echo`, `cd`, `pwd`, `export`, `unset`,
  `env`, `exit`.
- External commands resolved and executed via `PATH`, with proper `fork`/`execve` handling.
- Signal handling that mimics bash's behaviour for `Ctrl-C`, `Ctrl-\`, and `Ctrl-D`,
  both at the prompt and while a child process is running.

`&&`, `||`, `;` and subshells `( )` are intentionally out of scope for this project.

## Instructions

### Requirements

- A C compiler (`cc`/`gcc`/`clang`).
- GNU `make`.
- The `readline` development library (`libreadline-dev` on Debian/Ubuntu,
  `readline` via Homebrew on macOS).
- Optional, for the test suite: [Criterion](https://github.com/Snaipe/Criterion).

### Build

```sh
make        # builds the ./minishell executable
make re     # full rebuild
make clean  # remove object files
make fclean # remove object files and the executable
```

### Run

```sh
./minishell
```

This opens an interactive prompt. Examples:

```sh
minishell$ echo "Hello, $USER" | wc -c
minishell$ cat < input.txt | grep foo >> output.txt
minishell$ export MY_VAR=42 && echo $MY_VAR   # note: && is not supported, use two lines
```

Exit the shell with `exit`, or `Ctrl-D` on an empty line.

### Tests

Unit tests (built with [Criterion](https://github.com/Snaipe/Criterion)) cover the
lexer, parser, AST, expander, environment handling, builtins, and redirections:

```sh
make test
```

An interactive signal-behaviour test (spawns the shell via a pty and asserts on
`Ctrl-C`/`Ctrl-\` behaviour) is also available:

```sh
python3 test/test_signals_interactive.py
```

## Technical Overview

Every line of input flows through the same pipeline: **Lexer → Validator → Parser →
Expander → Executor**. See [`_resource/ARCHITECTURE.md`](_resource/ARCHITECTURE.md)
for a deep dive with diagrams; the summary below is organized by module.

### Lexer

Converts the raw input line into a singly-linked list of tokens (`t_tokensll`),
character by character. `build_tokensll` (`lexer.c`) walks the string, skips
whitespace, and calls `build_token` once per token, which dispatches into
`utils_lexer.c`:

- **Words** (`TOK_STR`) — `string_build` glues runs of quoted and unquoted
  characters into a single token, stopping only at whitespace or an unquoted
  operator character. A quoted span is consumed in one shot by
  `quotes_build`/`_quotes_build_helper` (`utils_lexer_build.c`), which scan only
  for the matching closing quote — anything in between, including whitespace or
  `|`/`<`/`>`, is swallowed as literal content rather than treated as a token
  boundary (e.g. `echo "a|b"` lexes to two tokens, not three, and produces no
  `TOK_PIPE`). An unclosed quote is reported as a syntax error (`ERR_QUOTE`).
- **Operators** (`utils_lexer_token.c`) — `|`, `<`, `>`, `>>`, `<<`, recognized by
  `special_build` even when not separated by whitespace (e.g. `echo hi>out`).
- **Escapes** — an unquoted `\` (`backslash_build`) consumes itself and appends the
  following character literally, stripping its special meaning, matching bash for
  the unquoted case. A trailing `\` with nothing after it is instead kept as a
  literal backslash, a simplification — real bash would prompt for a continuation
  line here. Backslash gets no special handling inside quotes at the lexer stage.

Quotes and `$` are kept **raw** in each token's value at this stage — the quote
characters themselves are copied verbatim into `token->val` — nothing is expanded
or stripped yet; that happens later in the Expander (`handle_quotes` strips the
quote characters while `expand_word` interprets `$`). A malloc failure anywhere in
the pipeline frees what's been built so far and calls `hardexit`; a quote error
instead frees and returns `NULL`, leaving the syntax-error reporting to the caller.
List bookkeeping — `init_token`, `freetoken`, `freetokensll`, and the `t_sll_ops`
traversal helpers used while linking tokens together — lives in `utils_sll.c`.

A dedicated validation pass then walks the finished token list and rejects illegal
grammar before any parsing happens: `is_valid_pipe`/`is_valid_redir`
(`utils_validator_tokens.c`) check leading/trailing/doubled pipes, redirections
with no target, and a redirection immediately followed by a pipe or another
redirection; `validate_tokensll` (`validator_tokens.c`) drives the walk and also
rejects unsupported operators (`;`, `&`, `&&`, `(`, `)`) that got lexed as plain
strings, since this shell doesn't implement them. Any failure sets the shell's exit
code to `2`, matching bash.

### AST

The parser output is a small binary tree of two node kinds:

- `NODE_CMD` — one command: a `NULL`-terminated `argv`, plus a singly-linked list of
  `t_redir` (type, target, resolved fd) in encounter order.
- `NODE_BINOP` — currently only `BIN_PIPE` is produced by the parser (`BIN_AND`,
  `BIN_OR`, `BIN_SEQ` exist in the enum for future extension but are not wired up),
  connecting a `left`/`right` subtree.

Each node carries a `t_span {start, end}` — token indices, not byte offsets — for
error reporting. It's populated as a byproduct of how far the parser's cursor moved
while building the node, not computed separately. The whole tree is freed recursively
via `ast_free`.

### Parser

A small recursive-descent parser (`t_parser` cursor over the token list) turns the
validated token stream into an AST:

```
parse_tokens()
  └─ parse_pipeline()
       └─ parse_simple_command()   → one NODE_CMD (argv + redirs)
       while next token is '|': wrap left/right in a NODE_BINOP(BIN_PIPE)
```

**Building a command node.** `parse_simple_command` keeps consuming tokens as long as
the current one is a word (`TOK_STR`) or one of the four redirection operators — that
whitelist, not a specific check for `|`, is what ends the loop; `|` (or end of input)
simply isn't in it. Each iteration re-checks the current token and dispatches:

- a word → pushed onto a growable `argv` array (`t_argv_builder`: starts at capacity
  4, doubles on demand, always keeps a trailing `NULL` so it's a valid
  `execve`-ready array at every point, not just at the end);
- a redirection operator → its type is captured, the cursor advances, and the *next*
  token must be a word (its value becomes the redirect's target) or parsing fails;
  the resulting `t_redir` is appended to the tail of the command's redir list.

Because both cases live in the same loop, arguments and redirections can interleave
freely in the source (`cat < in out > final` is legal), rather than needing separate
argument/redirection phases.

**Two distinct ways this returns nothing.** A single failed word or redirect mid-loop
aborts immediately. But even a loop that completes normally produces `NULL` if it
collected zero words and zero redirects — this is what turns `cmd |` (or `| cmd`)
into a syntax error: after consuming the pipe, the next call to
`parse_simple_command` sees no valid tokens to consume, returns `NULL`, and the
pipeline builder above propagates that failure instead of accepting an empty side.

**Folding the pipeline, left to right.** `parse_pipeline` builds the first command as
`left`, then loops while it sees `TOK_PIPE`: advance past it, parse the next command
as `right`, and immediately wrap `(left, right)` in a new `BIN_PIPE` node that
becomes `left` for the next iteration. This is iterative rather than recursive
specifically so `a | b | c` folds as `(a | b) | c` — left-associative, matching how a
pipeline is conceptually a flat left-to-right chain rather than right-nested.

**Redirections as a plain linked list, not a growable array like argv.**
Linked list also preserves encounter order for free, which matters because later redirects override earlier ones on the same fd (`cmd > a > b` — `b` wins), and the same `t_redir` node carries the `fd` field used later at execution time.

### Expander

_`src/expander/`_

Walks the AST and rewrites every raw word into its final form. `expand_ast` recurses
through `NODE_BINOP`s the same way the AST itself is shaped, and for each `NODE_CMD`
calls `expand_cmd_node`, which runs three passes over the command, in this fixed
order:

1. `process_assignments` (`expand_assignment.c`) — peels leading `NAME=value` tokens
   off `argv` and applies them straight to the shell's environment.
2. `expand_argv` — expands and field-splits every remaining argv word.
3. `expand_redirs` — expands every redirection target, collapsing it back down to
   exactly one word.

**Leading assignments (`expand_assignment.c`).** `assignment_prefix_count` (built on
`is_assignment_word`, POSIX identifier rules) scans `argv` for a run of leading
`NAME=value` words; `process_assignments` then branches on what follows that run:

- **Nothing follows** (`A=1 B=2` alone, no command) — each assignment is applied
  straight to the shell's real environment (`env_set` on `app->env_list`, then
  `update_env_array`) — permanent, exactly like a plain `export`.
- **A command follows** (`A=1 B=2 cmd`) — the assignments are applied instead to a
  *disposable* clone of the environment (`env_init(app->envp)` into a throwaway
  `t_env` list), flattened into an array, and attached to that command's own AST node
  (`t_cmd_node.envp`); the shell's real `app->env_list`/`app->envp` are never touched.
  `exec_builtinproc`/`do_exec` (`exec_dispatch.c`, `exec_pipeline_run.c`) swap
  `app->envp` to this array only for the duration of running that one command (builtin
  call or `execve`), then restore the original array right after. This is what fixes
  the shell-leak deviation from bash that used to exist here: `FOO=bar echo hi` no
  longer leaves `FOO=bar` set in the shell afterward.

One quirk, unrelated to scoping: a raw value ending in a literal, unquoted `;` still has
that trailing `;` silently stripped (`FOO=bar;` sets `FOO` to `bar`, not `bar;`) — a
workaround for this shell not lexing `;` as a statement separator at all, so a semicolon
typed at the end of a bash-style line doesn't leak into the value.

**The scoped environment only reaches code that actually reads `app->envp`.**
Matching real bash, a prefix assignment is *not* visible to that same command's own
argv/redirect/heredoc expansion (`A=hello echo $A` prints empty in bash too — the
shell expands `$A` against its pre-assignment environment, before the temporary value
is applied for the command it's attached to) — so `expand_cmd_node` reading
`app->envp` there is correct, not a bug. Where the temporary environment *should* be
visible is inside the command actually being run, and that's inconsistent across
builtins: `exec_builtinproc` swaps `app->envp` to `cmdnode->envp` before calling
`exec_builtin`, and `builtin_env` was rewritten to iterate `app->envp` specifically so
it'd pick that up — but `cd` (`builtin_cd.c`), `export`, `unset`, and `exit` all read
`app->env_list` (the permanent linked list) instead, which the swap never touches. So
`HOME=/tmp cd` doesn't move to `/tmp` the way bash's does, even though `HOME=/tmp env`
correctly shows the scoped value.

**Core primitive: `expand_word`.** Everything above funnels through here — it's
called once per remaining argv word, once per redirect target, and once per
assignment's raw value. It runs a character-by-character state machine
(`t_expand_ctx`, `expand_step`) over the raw token, tracking quote state (`t_qstate`:
`Q_NONE` / `Q_SQUOTE` / `Q_DQUOTE`), an accumulating string buffer (`t_strbuf`) for
the word currently being built, and a `t_wordlist` for words already completed. Each
`expand_step` call tries, in order: `handle_whitespace` → `handle_quotes` →
`handle_backslash` → `handle_variable` → fall through to a literal copy of the
current byte. These checks are mutually exclusive on the byte itself (whitespace vs.
quote char vs. `\` vs. `$`), so the ordering is really about layering — boundary
detection, then quote-mode toggles, then escapes, then substitution, with literal
copy as the catch-all.

- **Quote toggling** (`handle_quotes`) — `'`/`"` are never copied, they flip
  `ctx->state` (guarded so a quote char of the *other* kind is ignored while already
  inside a quote, e.g. `"it's"` stays intact). Single quotes make everything until
  the closing `'` fully literal — no `$`, no backslash escapes. Double quotes still
  expand `$`, and additionally recognize backslash escapes for `\`, `"`, `$` only
  (`handle_backslash`); any other backslash is just a literal character.
- **Whitespace** (`handle_whitespace`) only splits a word when `ctx->state ==
  Q_NONE`. In practice this branch never fires during the raw token scan: the lexer
  already splits on every unquoted whitespace when building tokens, so any
  whitespace still present inside a single token got there *because* it was inside
  quotes — meaning `ctx->state` is never `Q_NONE` at that point. The only place
  unquoted-whitespace splitting actually happens is a second, independent path — see
  field splitting below.
- **Variable expansion** (`handle_variable`) — dispatch depends on both what follows
  the `$` and the current quote state:

  | Input pattern (at `ctx->i`) | Guard | Action | Unquoted (`Q_NONE`) | Double-quoted (`Q_DQUOTE`) | Single-quoted (`Q_SQUOTE`) |
  |---|---|---|---|---|---|
  | `$?` | `state != Q_SQUOTE` | `handle_status` — renders `last_status` as digits | Splits like any expanded text (status is always digits, so effectively still one word) | Appended literally via `sb_push_str`, no split | Never reached — literal `$?` chars copied as-is |
  | `$0`–`$9` | `state != Q_SQUOTE` | Consumes `$` + digit, appends **nothing** | Silently disappears (no positional params) | Same — silently disappears | Never reached — literal chars |
  | `$NAME` (valid identifier start) | `state != Q_SQUOTE` | `handle_env_var` — `env_lookup`, empty string `""` if unset | Value is field-split via `append_unquoted_text` (spaces in value → multiple words; unset → contributes nothing) | Value appended literally via `sb_push_str`; unset still forces `word_in_progress = 1` (empty-string arg, matches bash's `"$UNSET"` → empty arg) | Never reached — literal `$NAME` chars |
  | `$` followed by anything else (e.g. `$$`, `$ `, `$@`) | `state != Q_SQUOTE` | Not `?`, not digit, not name-start → falls to the last branch inside `handle_variable` | Literal `$` char pushed, next char handled on the following iteration | Same — literal `$` | Never reached — literal `$` |
  | Any `$...` | `state == Q_SQUOTE` | Guard fails entirely, `handle_variable` returns 0 immediately | — | — | Falls through to raw-copy default: `$` copied as a plain character, no special meaning at all |

- **Field splitting has two separate mechanisms, not one:**
  - Ordinary/escaped/quoted characters accumulate into the string buffer one at a
    time and only get flushed to the `t_wordlist` at the end of the token (or when
    whitespace is hit in `Q_NONE`, which — per above — doesn't actually happen
    mid-token on raw input).
  - An **expanded variable's value**, when substituted in `Q_NONE`, is instead
    pushed through `append_unquoted_text`, which re-scans *that value* for
    whitespace and flushes a completed word to the `t_wordlist` immediately at each
    space — this is what lets `echo $VAR` (where `$VAR="a b"`) become two argv
    entries. Whitespace found this way is judged purely on the substituted text; if
    the value happens to contain a literal quote character, that's just data to this
    function, not a delimiter. Inside double quotes, the same value is instead
    appended as one literal blob (`sb_push_str`, no splitting) — even an
    unset/empty `"$VAR"` still counts as one empty-string word, matching bash.

`expand_word` reports its result in a single `t_wordlist *out` (`{items, count}`),
which is structurally identical to the parser's `t_argv_builder` — kept as a separate
type purely to keep "fragments from splitting one word" (the expander's concern)
distinct from "the whole command's final argv" (the parser/exec's concern), not
because the growth logic differs.

`expand_argv` calls `expand_word` once per remaining argv word and flattens every
resulting `t_wordlist` into the command's new argv via `push_words_to_builder`.
`expand_redirs` calls it once per redirection target and additionally enforces that
the result collapses to **exactly one**, non-empty word — anything else is reported
as an ambiguous redirect (`ERR_CMDNEXEC`), matching bash.

### Envp

The environment has **two representations kept deliberately in sync, not one**:
a linked list (`t_env`, `key`/`value`/`next`) that's the mutable source of truth
for builtins, and a flat `char **app->envp` array rebuilt from it whenever it
changes. Three files split the responsibility along that line:

- `envp_utils.c` — list construction/destruction: `env_new_node`, `env_init`,
  `env_free`.
- `envp_accessor.c` — list mutation/lookup used by builtins: `env_get`, `env_set`,
  `env_unset`.
- `envp.c` — conversion to the flat array: `env_to_array`, `update_env_array`.

**Export-only variables: `value == NULL` is a distinct state from `value ==
""`.** `env_new_node` stores `NULL` when no value is given, which is how `export
NAME` (no `=`) is represented internally — the variable exists for `export`'s own
listing, but must not show up for a plain `$NAME` expansion or in a child
process's environment. Every consumer that walks to the array checks this
explicitly:  `env_count`/`env_to_array` (`envp.c`) skip any node whose `value` is
`NULL`, so a valueless `export FOO` never reaches `app->envp` or `execve`.

**`env_init` — skip malformed entries, abort on real failure.** Walking the
incoming `envp[]`, a missing `=` in an entry is treated as "not a variable,
move on" (`env_parse_entry` is simply not called, the index just advances) —
not an error, since a shell shouldn't refuse to start over one odd entry it
inherited. A malloc failure partway through, by contrast, unwinds everything
built so far via `env_free(head)` and returns `NULL` — the two failure shapes
are handled differently on purpose: one is recoverable per-entry, the other
means memory is exhausted and there's nothing sensible left to build.

**`env_set` — search first, then decide between two branches:**
- **Found + `value` non-`NULL`** → `env_update_value` replaces `cur->value` in
  place (old value freed, new one duplicated in).
- **Found + `value == NULL`** → falls straight through as a no-op, the existing
  value is left untouched. This is what makes `export FOO` on an already-set
  `FOO=bar` safe: it must not wipe `bar` back to unset.
- **Not found** → a new node is allocated and **prepended** to the list
  (`node->next = *list; *list = node`), not appended — O(1) insert, at the cost
  of new variables appearing before older ones if the list were ever iterated
  in order-of-declaration (nothing currently depends on that order).

**`env_unset` — removal is a plain linked-list splice** (`prev`/`cur` walk,
patch whichever of `*list` or `prev->next` pointed at the removed node), and
**returns `0` even when the key was never found** — `unset` on a non-existent
variable is a silent success in POSIX shells, not an error.

**The array is a derived, read-only snapshot — it is never mutated directly,
only regenerated.** `update_env_array` calls `env_to_array` to build a
complete new array first, and only frees the *old* `app->envp` after the new
one is confirmed non-`NULL` — so a mid-rebuild malloc failure leaves the
previous, still-valid array in place instead of leaving `app->envp` pointing at
freed memory.

**Why the array matters as much as the list.** Builtins (`env_get`/`env_set`/
`env_unset` in `builtin_cd.c`, `builtin_export.c`, `builtin_unset.c`) only ever
touch the linked list. But variable expansion (`env_lookup`, called from
`handle_env_var` in the Expander) and `execve` itself both read from
`app->envp` — the flat array — not the list. `update_env_array` is therefore
the one synchronization point between "what builtins changed" and "what
expansion/exec can see," and it's called from three places: once at startup
(`main.c`, right after `env_init`), once per leading assignment inside
`apply_assignment` (`expand_assignment.c` — this is *why* `A=1 B=$A cmd`
resolves `B` correctly, per the Expander section above), and twice per prompt
line in `process_prompt` (`minishell.c`) — once before `expand_ast` and once
after `execute_ast` — to pick up whatever a builtin like `export`/`cd`/`unset`
changed on the *previous* line before the *next* line's expansion runs.
**Sample case:** `export FOO=bar` mutates only `app->env_list`; typing `echo
$FOO` as a *separate* command afterward works because `process_prompt`
resyncs `app->envp` from the list before that second line is expanded — not
because `export` updated the array itself.

### Exec

_`src/exec/`_

Turns the expanded AST into real processes. Every command line funnels through
`execute_ast` (`exec_dispatch.c`), which decides *before* touching any process whether
it's even looking at a pipeline.

Heredocs are collected first, once, over the whole AST — `collect_heredocs`
(`exec_fd.c`) walks every `NODE_CMD` in the tree (both sides of every pipe) and reads
all `<<` input up front, before any `fork`, to avoid a deadlock between parent and
child on the heredoc pipe. (Kept at a surface level here on purpose — heredoc
collection itself is dense enough to deserve its own pass.)

**The one-command shortcut: `NODE_CMD` vs. everything else.** `execute_ast` only
special-cases a *bare*, single `NODE_CMD` — the moment the AST has a `NODE_BINOP` (any
pipe), it skips straight to `exec_allcmds` regardless of what's inside. For a bare
`NODE_CMD` it branches three ways on `argv`:

- **No `argv[0]` at all, but redirects present** — bash still creates/truncates the
  target file even though there's no command to run (`> file` with nothing before it).
  `open_redirs` + `close_redirsfd` run and the function returns immediately — no fork,
  no `exec_allcmds`.
- **`argv[0]` is a builtin** — `exec_builtinproc` runs it *in the parent shell process
  itself*, no fork at all. This is the only path where `cd`, `export`, `unset`, `exit`
  can actually mutate the running shell; the moment the same builtin appears anywhere in
  a pipe (`cd /tmp | true`), it falls through to `exec_allcmds` instead and runs in a
  forked child via `do_exec`'s own, second `is_builtin` check — so the directory/env
  change is thrown away when that child exits. Two `is_builtin` checks exist for exactly
  this reason: one gates the in-process fast path, the other is the correctness fallback
  inside a pipeline.
- **Anything else (an external command, alone)** — falls through to `exec_allcmds`,
  which still forks it as a one-command "pipeline."

**Running a builtin in-process still has to fake a subshell's fd isolation.**
`exec_builtinproc` only touches fds if `cmdnode->redirs` is non-empty: it `dup`s the
current stdin/stdout aside, applies the redirects with `open_redirs`/`do_dup2redirs`,
runs the builtin, then `restore_fd`s the originals. Skipping this dance when there are
no redirects avoids two needless `dup` calls on the hot path (a bare `cd`/`export` with
no `>`/`<`). Without the restore step, `pwd > log.txt` would permanently redirect the
*shell's own* stdout to `log.txt` after the command returned, since there's no child
process boundary to contain the change.

**Building the flat command array uses two different tree walks for two different
jobs.** `count_cmds` (`exec_pipeline_utils.c`) only ever descends `->left`,
incrementing once per node and stopping at the first non-`NODE_BINOP` — that's enough
to get the right count *only* because of how the parser folds a pipeline (`a | b | c`
→ `((a,b),c)`, one extra `BINOP` layer per additional command along that same left
spine), not because it's actually visiting every command node. The real traversal,
`_flatten_cmds`, recurses into *both* children for a `BINOP` and only records a pointer
at a leaf — that's what puts `[cat, grep, wc]` in the array in left-to-right source
order despite the tree being right-heavy. `count_cmds` runs first purely to size the
array before `_flatten_cmds` fills it in.

**Per-stage fd plumbing (`start_pipeline`, `exec_pipeline.c`).** For each command in
the flattened array, in order: `setup_pipe` opens a fresh pipe *unless this is the last
command* (`iter_i < n_cmd - 1`); a `fork` follows. In the child, `do_childproc` closes
the unused read end of the pipe it just helped create (it only ever needs the write
end), then `dup2`s `prevfdin` → stdin (skipped for the first command) and the new
pipe's write end → stdout (skipped for the last), *then* applies the command's own
redirects on top — meaning an explicit `>`/`<` on a piped command always wins over the
pipe wiring, since `do_dup2redirs` runs after and simply `dup2`s over whatever the pipe
just set. In the parent, `update_pipeops` immediately closes the previous read end and
the just-forked write end — closing the write end in the parent is what lets the
*next* reader ever see EOF; holding it open in a still-running parent is the classic
way to deadlock a pipeline. If a `fork()` itself fails partway through, the caller
closes whatever fds the struct is still holding and reaps everyone forked so far
(`free_pipeops(pipeops, i - 1)`) — closing the previous pipe's read end this way is
enough to make the already-running earlier command fail on its next write (`SIGPIPE`)
and unwind on its own, no explicit kill needed.

**Sample case:** `cat file.txt | grep foo | wc -l` — three commands, so two pipes.
`cat` gets a fresh pipe, no `prevfdin` (nothing to read), and writes into it; `grep`
reads that pipe, gets its own fresh pipe, and writes into *that*; `wc`, being last,
gets no pipe of its own — its stdout falls through untouched to the terminal. As each
child forks, the parent closes its own copies of the read end just handed off and the
write end just created, so `grep` sees EOF the instant `cat` finishes, and `wc` sees
EOF the instant `grep` finishes, rather than hanging forever waiting for a reader/writer
that's already gone.

**Exit status is the last command's, everyone else is just reaped.** After the fork
loop, `get_lastcmdstatus` specifically `waitpid`s on `pids[n_cmd - 1]` to get the exit
code (translating a signal death into `128 + signum`, with the `Quit (core
dumped)`/newline side effects) — matching bash's rule that a pipeline's status is its
last command's. `free_pipeops` then reaps everyone *before* that, discarding their
statuses, purely to avoid zombies.

**PATH resolution (`resolvecmdpath`, `exec_path.c`) short-circuits on a literal `/`.**
If `argv[0]` contains a `/` (`./a.out`, `/bin/ls`), `PATH` is never consulted at all —
`cmdwithpath` just `access(X_OK)`-checks that exact path, matching POSIX/bash exactly.
Otherwise `getrawpathlst` splits `PATH` on `:` (substituting `.` for an empty entry —
a leading/trailing/doubled `:`, per POSIX) and `matchcmdpath` tries each directory in
order. The search loop's break condition matters: it stops not only on success, but
also the *first* time a candidate exists but isn't executable (`EX_CMD_NEXEC`) — so a
non-executable match earlier on `PATH` reports "permission denied" (126) instead of
silently trying later directories for another file of the same name, matching bash's
"first match on the filename wins" rather than "first *working* match wins." A subtler
point: every failed candidate along the way — even ones that don't stop the loop —
still sets `app->exitcode` via `setexecerrno`, so once a later directory *does*
succeed, `resolvecmdpath` explicitly resets `app->exitcode` to `EX_OK` before returning;
otherwise a stray "not found in an earlier directory" code could leak through even
though the command was ultimately found and hasn't even run yet.

Exit statuses follow POSIX conventions (`t_exitcode` in `include/minishell.h`):
`126` for found-but-not-executable, `127` for command-not-found, `128 + signum` for a
child killed by a signal.

### Builtins

Builtins are dispatched by `exec_builtin`/`is_builtin` (`builtin_dispatch.c`) and each
lives in its own file:

| Builtin  | Notes |
|----------|-------|
| `echo`   | <ul><li>Prints its arguments space-separated with a trailing newline.</li><li>Accepts a leading run of valid `-n` flags (`-n`, `-nn`, `-nnn`, ...) to suppress that newline.</li><li>`-na` and similar are *not* recognized as flags — printed as a literal argument instead, matching bash.</li></ul> |
| `cd`     | <ul><li>No args → `$HOME` (error if unset).</li><li>`-` → `$OLDPWD` (error if unset); like bash, the resolved path is printed to stdout.</li><li>Otherwise `chdir`s straight to the given argument.</li><li>Rejects `cd a b` with "too many arguments" and leaves the directory unchanged.</li><li>After a successful `chdir`, sets `OLDPWD` to the previous `getcwd()` result and `PWD` to the new one.</li></ul> |
| `pwd`    | <ul><li>Prints `getcwd()` followed by a newline.</li><li>On failure (e.g. the cwd was deleted out from under the shell), reports `strerror(errno)` and returns an error status instead.</li></ul> |
| `export` | <ul><li>No args → prints every variable, sorted alphabetically, as `declare -x NAME="value"` (or bare `declare -x NAME` if it has no value).</li><li>`NAME=value` sets/updates the variable via `env_set`.</li><li>`NAME` alone marks it exported without a value, but only if it doesn't already exist — an existing value is left untouched.</li><li>Identifiers are validated via `is_valid_identifier` (letter/`_` first, then alnum/`_`); invalid ones report an error and are skipped, but the remaining arguments are still processed.</li></ul> |
| `unset`  | <ul><li>Removes each named variable via `env_unset`.</li><li>Invalid identifiers report an error and set the exit status to `1`, but don't stop the rest of the arguments from being processed.</li><li>Unsetting a variable that doesn't exist is a silent no-op, matching bash.</li></ul> |
| `env`    | <ul><li>Prints every variable that currently has a (non-`NULL`) value as `KEY=value`.</li><li>Variables created via a valueless `export NAME` are excluded, mirroring real env's "exported but unset" behaviour.</li></ul> |
| `exit`   | <ul><li>Always prints `exit` to stderr first, like bash.</li><li>No args → exits with the shell's last recorded exit status.</li><li>`exit N M` → "too many arguments", returns `1` *without* exiting.</li><li>A non-numeric argument → "numeric argument required", exits with status `2`.</li><li>A numeric argument exits with `(unsigned char)value`, i.e. wraps mod 256 (`exit 300` → status `44`, `exit -1` → status `255`).</li></ul> |

The environment itself is kept as a linked list (`t_env` in `src/envp/envp.c`), rebuilt
into a flat `char **envp` array (`update_env_array`) whenever it changes, so that both
`execve` and the builtins always see a consistent view of the environment.

### Signals

_`src/signals.c`_

Signal handling switches disposition depending on what the shell is currently doing,
mirroring bash's own behaviour:

| Context                              | `Ctrl-C` (SIGINT)                                   | `Ctrl-\` (SIGQUIT) |
|---------------------------------------|------------------------------------------------------|--------------------|
| At the prompt (`signals_at_prompt`)   | Cancel the current line, print a newline, redraw a fresh prompt (`handlesig_prompt` uses `rl_replace_line`/`rl_on_new_line`/`rl_redisplay`) | Ignored |
| Executing a pipeline (`signals_ignore` in parent, `signals_default` in each child) | Parent ignores it; the child (which has default disposition) is killed by the kernel, and the parent reports `128 + SIGINT` via `waitpid` | Child dumps core and prints `Quit (core dumped)`; parent is unaffected |
| Reading a heredoc                     | Interrupts collection, discards what was read, returns to a fresh prompt | Ignored |

`g_signal` is the single `volatile sig_atomic_t` global allowed by the subject; it only
records *that* a signal was caught so the main loop can react (`handle_sigint_in_main`)
— all actual state mutation happens outside the handler.

## Resources

### Classic references

- [Bash Reference Manual](https://www.gnu.org/software/bash/manual/bash.html) — ground truth for quoting, expansion, and exit-status rules.
- [GNU Readline Library manual](https://tiswww.case.edu/php/chet/readline/readline.html) — prompt/history API used for the REPL.
- [POSIX Shell & Utilities specification](https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html) — the formal grammar minishell approximates.
- `man 2 fork`, `man 2 execve`, `man 2 pipe`, `man 2 dup2`, `man 2 waitpid`, `man 3 readline`, `man 7 signal` — the syscalls/library calls the executor and signal handling are built on.
- [Writing a simple shell in C](https://brennan.io/2015/01/16/write-a-shell-in-c/) — a widely-referenced introductory walkthrough of the fork/exec/wait shell loop.

### Use of AI

Claude (Anthropic) was used throughout this project as a learning and review tool. Specific uses:

- **Conceptual explanation** — understanding the overall scope of the project (what a POSIX shell is actually expected to do end to end: tokenizing, parsing, expansion, process/pipeline management, signal handling) and the theory behind unfamiliar mechanics — signal disposition across `fork`, why heredocs need to be collected before the pipeline starts to avoid a deadlock, quoting/expansion/field-splitting rules — before writing any code against them.
- **Architecture review** — discussing how to split the project into modules (lexer/parser/AST/expander/exec/builtins/signals), sanity-checking the data flow between them, and reviewing whether a given design decision (e.g. running builtins in-process vs. forking, where to keep environment state) fit the project's scope and constraints.
- **Debugging assistance** — helping narrow down the root cause of crashes, hangs, and incorrect exit codes during development.

All core logic, implementation, and final decisions were written and verified manually. Claude was used to ask questions, review drafts, and reason through edge cases.
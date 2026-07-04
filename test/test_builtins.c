#include <criterion/criterion.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "minishell.h"

static char *read_all(int fd)
{
	char	buf[256];
	char	*out;
	size_t	cap;
	size_t	len;
	ssize_t	n;

	cap = 256;
	len = 0;
	out = malloc(cap);
	if (!out)
		return (NULL);
	while ((n = read(fd, buf, sizeof(buf))) > 0)
	{
		if (len + (size_t)n + 1 > cap)
		{
			cap = (len + (size_t)n + 1) * 2;
			out = realloc(out, cap);
			if (!out)
				return (NULL);
		}
		memcpy(out + len, buf, (size_t)n);
		len += (size_t)n;
	}
	out[len] = '\0';
	close(fd);
	return (out);
}

static char *capture_stdout_builtin(int (*fn)(char **, t_app *),
		char **argv, t_app *app, int *ret)
{
	int	fds[2];
	int	saved;

	cr_assert_eq(pipe(fds), 0);
	saved = dup(STDOUT_FILENO);
	dup2(fds[1], STDOUT_FILENO);
	close(fds[1]);
	*ret = fn(argv, app);
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	return (read_all(fds[0]));
}

static char *capture_stdout_env(int (*fn)(t_app *), t_app *app, int *ret)
{
	int	fds[2];
	int	saved;

	cr_assert_eq(pipe(fds), 0);
	saved = dup(STDOUT_FILENO);
	dup2(fds[1], STDOUT_FILENO);
	close(fds[1]);
	*ret = fn(app);
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	return (read_all(fds[0]));
}

static char *capture_stdout_echo(int (*fn)(char **), char **argv, int *ret)
{
	int	fds[2];
	int	saved;

	cr_assert_eq(pipe(fds), 0);
	saved = dup(STDOUT_FILENO);
	dup2(fds[1], STDOUT_FILENO);
	close(fds[1]);
	*ret = fn(argv);
	fflush(stdout);
	dup2(saved, STDOUT_FILENO);
	close(saved);
	return (read_all(fds[0]));
}

static t_app make_app(void)
{
	t_app app;

	app.env_list = NULL;
	app.envp = NULL;
	app.tokensll = NULL;
	app.ast = NULL;
	app.exitcode = 0;
	return (app);
}

Test(builtins, is_builtin_recognizes_known)
{
	cr_assert_eq(is_builtin("echo"), 1);
	cr_assert_eq(is_builtin("cd"), 1);
	cr_assert_eq(is_builtin("pwd"), 1);
	cr_assert_eq(is_builtin("export"), 1);
	cr_assert_eq(is_builtin("unset"), 1);
	cr_assert_eq(is_builtin("env"), 1);
	cr_assert_eq(is_builtin("exit"), 1);
	cr_assert_eq(is_builtin("nope"), 0);
}

Test(builtins, export_prints_sorted)
{
	t_app	app;
	char	*argv[] = {"export", NULL};
	char	*out;
	int	ret;

	app = make_app();
	env_set(&app.env_list, "B", "2");
	env_set(&app.env_list, "A", "1");
	env_set(&app.env_list, "C", NULL);

	out = capture_stdout_builtin(builtin_export, argv, &app, &ret);
	cr_assert_eq(ret, 0);
	cr_assert_not_null(out);
	cr_assert_str_eq(out,
		"declare -x A=\"1\"\n"
		"declare -x B=\"2\"\n"
		"declare -x C\n");
	free(out);
	env_free(app.env_list);
}

Test(builtins, export_invalid_identifier_returns_error)
{
	t_app	app;
	char	*argv[] = {"export", "1BAD=1", "OK=2", NULL};
	int	ret;

	app = make_app();
	ret = builtin_export(argv, &app);
	cr_assert_eq(ret, 1);
	cr_assert_str_eq(env_get(app.env_list, "OK"), "2");
	env_free(app.env_list);
}

Test(builtins, export_name_only_preserves_existing_value)
{
	t_app	app;
	char	*argv[] = {"export", "A", NULL};
	int	ret;

	app = make_app();
	env_set(&app.env_list, "A", "1");
	ret = builtin_export(argv, &app);
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(env_get(app.env_list, "A"), "1");
	env_free(app.env_list);
}

Test(builtins, unset_invalid_identifier_returns_error)
{
	t_app	app;
	char	*argv[] = {"unset", "BAD-NAME", "A", NULL};
	int	ret;

	app = make_app();
	env_set(&app.env_list, "A", "1");
	ret = builtin_unset(argv, &app);
	cr_assert_eq(ret, 1);
	cr_assert_null(env_get(app.env_list, "A"));
	env_free(app.env_list);
}

Test(builtins, unset_continues_after_invalid_identifier)
{
	t_app	app;
	char	*argv[] = {"unset", "A", "BAD-NAME", "B", NULL};
	int	ret;

	app = make_app();
	env_set(&app.env_list, "A", "1");
	env_set(&app.env_list, "B", "2");
	ret = builtin_unset(argv, &app);
	cr_assert_eq(ret, 1);
	cr_assert_null(env_get(app.env_list, "A"));
	cr_assert_null(env_get(app.env_list, "B"));
	env_free(app.env_list);
}

Test(builtins, cd_missing_home_or_oldpwd)
{
	t_app	app;
	char	*argv_home[] = {"cd", NULL};
	char	*argv_oldpwd[] = {"cd", "-", NULL};
	int	ret;

	app = make_app();
	ret = builtin_cd(argv_home, &app);
	cr_assert_eq(ret, 1);
	ret = builtin_cd(argv_oldpwd, &app);
	cr_assert_eq(ret, 1);
	env_free(app.env_list);
}

Test(builtins, env_skips_null_values)
{
	t_app	app;
	char	*out;
	int	ret;

	app = make_app();
	env_set(&app.env_list, "A", "1");
	env_set(&app.env_list, "B", NULL);
	env_set(&app.env_list, "C", "3");

	out = capture_stdout_env(builtin_env, &app, &ret);
	cr_assert_eq(ret, 0);
	cr_assert_not_null(out);
	cr_assert_str_eq(out, "C=3\nA=1\n");
	free(out);
	env_free(app.env_list);
}

Test(builtins, echo_n_flags)
{
	char	*argv1[] = {"echo", "-n", "hi", NULL};
	char	*argv2[] = {"echo", "-nn", "hi", NULL};
	char	*argv3[] = {"echo", "-n-no", "hi", NULL};
	char	*out;
	int	ret;

	out = capture_stdout_echo(builtin_echo, argv1, &ret);
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(out, "hi");
	free(out);

	out = capture_stdout_echo(builtin_echo, argv2, &ret);
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(out, "hi");
	free(out);

	out = capture_stdout_echo(builtin_echo, argv3, &ret);
	cr_assert_eq(ret, 0);
	cr_assert_str_eq(out, "-n-no hi\n");
	free(out);
}

static int run_exit_and_get_status(char **argv, int last_status)
{
	pid_t	pid;
	int	status;
	t_app	app;

	pid = fork();
	cr_assert(pid >= 0);
	if (pid == 0)
	{
		app = make_app();
		app.exitcode = last_status;
		builtin_exit(argv, &app);
		_exit(255);
	}
	waitpid(pid, &status, 0);
	cr_assert(WIFEXITED(status));
	return (WEXITSTATUS(status));
}

Test(builtins, exit_numeric_and_non_numeric)
{
	char	*argv_num[] = {"exit", "42", NULL};
	char	*argv_bad[] = {"exit", "abc", NULL};
	char	*argv_bad_extra[] = {"exit", "abc", "2", NULL};
	char	*argv_none[] = {"exit", NULL};

	cr_assert_eq(run_exit_and_get_status(argv_num, 7), 42);
	cr_assert_eq(run_exit_and_get_status(argv_bad, 7), 2);
	cr_assert_eq(run_exit_and_get_status(argv_bad_extra, 7), 2);
	cr_assert_eq(run_exit_and_get_status(argv_none, 7), 7);
}

Test(builtins, exit_too_many_args_returns_error)
{
	t_app	app;
	char	*argv[] = {"exit", "1", "2", NULL};
	int	ret;

	app = make_app();
	ret = builtin_exit(argv, &app);
	cr_assert_eq(ret, 1);
	env_free(app.env_list);
}

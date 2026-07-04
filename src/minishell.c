/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   minishell.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kong <kong@student.42singapore.sg>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/02 21:00:17 by kong              #+#    #+#             */
/*   Updated: 2026/07/02 21:00:37 by kong             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

t_app	*init_app(char **envp)
{
	t_app	*app;

	app = malloc(sizeof(t_app));
	if (!app)
		return (printerr_syscall(ERR_MALLOC), NULL);
	app->env_list = env_init(envp);
	app->envp = env_to_array(app->env_list);
	app->exitcode = EX_OK;
	app->tokensll = NULL;
	app->ast = NULL;
	return (app);
}

void	process_prompt(t_app *app, char *str)
{
	app->tokensll = build_tokensll(app, str);
	if (validate_tokensll(app) == -1)
	{
		freetokensll(app->tokensll);
		app->tokensll = NULL;
		return ;
	}
	app->ast = parse_tokens(app->tokensll);
	freetokensll(app->tokensll);
	app->tokensll = NULL;
	if (!app->ast)
		return ;
	if (update_env_array(app) != 0)
		return (setexit(app, EX_ERR), ast_free(app->ast));
	if (expand_ast(app, app->ast) != 0)
		return (ast_free(app->ast));
	execute_ast(app, app->ast);
	if (update_env_array(app) != 0)
		setexit(app, EX_ERR);
	ast_free(app->ast);
	app->ast = NULL;
}

static char	*get_next_line_non_interactive(void)
{
	char	*line;
	size_t	len;
	ssize_t	read;

	line = NULL;
	len = 0;
	read = getline(&line, &len, stdin);
	if (read == -1)
	{
		free(line);
		return (NULL);
	}
	if (read > 0 && line[read - 1] == '\n')
		line[read - 1] = '\0';
	return (line);
}

static char	*read_prompt(void)
{
	char	*prompt;

	signals_at_prompt();
	g_signal = 0;
	if (isatty(STDIN_FILENO))
		prompt = readline("minishell$ ");
	else
		prompt = get_next_line_non_interactive();
	return (prompt);
}

static int	cleanup_app(t_app *app)
{
	int	exit_code;

	exit_code = app->exitcode;
	env_free(app->env_list);
	if (app->envp)
		freelst(app->envp);
	free(app);
	return (exit_code);
}

int	minishell(char **envp)
{
	t_app	*app;
	char	*prompt;

	app = init_app(envp);
	if (!app)
		hardexit();
	while (1)
	{
		prompt = read_prompt();
		if (!prompt)
		{
			if (isatty(STDIN_FILENO))
				ft_putstr_fd("exit\n", 1);
			break ;
		}
		if (g_signal == SIGINT)
			handle_sigint_in_main(app);
		if (*prompt)
			add_history(prompt);
		process_prompt(app, prompt);
		free(prompt);
	}
	return (cleanup_app(app));
}

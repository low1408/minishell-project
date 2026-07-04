/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   expand.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kong <kong@student.42singapore.sg>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/03 22:02:58 by kong              #+#    #+#             */
/*   Updated: 2026/07/03 22:02:59 by kong             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

static int	expand_wordlist(const char *input, char **envp, int last_status,
		t_wordlist *out)
{
	t_expand_ctx	ctx;
	int				res;

	if (!out || init_ctx(&ctx, input, envp, last_status) != 0)
		return (ERR_MALLOC);
	while (input && input[ctx.i])
	{
		res = expand_step(&ctx);
		if (res < 0)
			return (wl_free(&ctx.wl), sb_free(&ctx.sb), res);
	}
	if (ctx.word_in_progress && flush_word(&ctx.wl, &ctx.sb, 1) != 0)
		return (wl_free(&ctx.wl), sb_free(&ctx.sb), ERR_MALLOC);
	*out = ctx.wl;
	sb_free(&ctx.sb);
	return (0);
}

int	expand_word(const char *input, char **envp, int last_status,
		char ***out_words, size_t *out_count)
{
	t_wordlist	wl;

	if (!out_words || !out_count)
		return (ERR_MALLOC);
	*out_words = NULL;
	*out_count = 0;
	if (expand_wordlist(input, envp, last_status, &wl) != 0)
		return (ERR_MALLOC);
	*out_words = wl.items;
	*out_count = wl.count;
	return (0);
}

int	expand_argv(char **argv, char **envp, int last_status, char ***out_argv)
{
	t_argv_builder	ab;
	t_wordlist		wl;
	size_t			i;

	if (!out_argv || argv_builder_init(&ab) != 0)
		return (ERR_MALLOC);
	i = 0;
	while (argv && argv[i])
	{
		if (expand_wordlist(argv[i], envp, last_status, &wl) != 0)
			return (argv_builder_free(&ab), ERR_MALLOC);
		if (push_words_to_builder(&ab, wl.items, wl.count) != 0)
			return (argv_builder_free(&ab), ERR_MALLOC);
		i++;
	}
	*out_argv = ab.argv;
	return (0);
}

int	expand_redirs(t_redir *redir, char **envp, int last_status)
{
	t_wordlist	wl;

	while (redir)
	{
		if (redir->type == REDIR_HEREDOC)
		{
			redir = redir->next;
			continue ;
		}
		if (expand_wordlist(redir->target, envp, last_status, &wl) != 0)
			return (ERR_MALLOC);
		if (wl.count != 1 || wl.items[0][0] == '\0')
		{
			freelst(wl.items);
			return (errmsg(NULL, NULL, "ambiguous redirect"), ERR_CMDNEXEC);
		}
		free(redir->target);
		redir->target = wl.items[0];
		free(wl.items);
		redir = redir->next;
	}
	return (0);
}

static int	expand_cmd_node(t_app *app, t_cmd_node *cmd)
{
	char		**expanded;
	int			res;
	char		**envp;
	int			last_status;

	last_status = app->exitcode;
	if (process_assignments(app, cmd, last_status) != 0)
		return (ERR_MALLOC);
	envp = app->envp;
	if (expand_argv(cmd->argv, envp, last_status, &expanded) != 0)
		return (ERR_MALLOC);
	freelst(cmd->argv);
	cmd->argv = expanded;
	if (!cmd->argv[0])
	{
		freelst(cmd->argv);
		cmd->argv = NULL;
	}
	res = expand_redirs(cmd->redirs, envp, last_status);
	if (res != 0)
		setexit(app, EX_ERR);
	return (res);
}

int	expand_ast(t_app *app, t_ast_node *node)
{
	int	res;

	if (!node)
		return (0);
	if (node->type == NODE_CMD)
		return (expand_cmd_node(app, &node->content.cmd));
	if (node->type == NODE_BINOP)
	{
		res = expand_ast(app, node->content.binop.left);
		if (res != 0)
			return (res);
		return (expand_ast(app, node->content.binop.right));
	}
	return (0);
}

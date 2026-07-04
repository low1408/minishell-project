/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ast.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kong <kong@student.42singapore.sg>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/02 21:06:26 by kong              #+#    #+#             */
/*   Updated: 2026/07/02 21:06:28 by kong             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

static void	free_redirs(t_redir *redir)
{
	t_redir	*next;

	while (redir)
	{
		next = redir->next;
		if (redir->fd != -1)
			close(redir->fd);
		free(redir->target);
		free(redir);
		redir = next;
	}
}

t_ast_node	*ast_new_cmd(char **argv, t_redir *redirs, t_span span)
{
	t_ast_node	*node;

	node = malloc(sizeof(t_ast_node));
	if (!node)
		return (printerr_syscall(ERR_MALLOC), NULL);
	node->type = NODE_CMD;
	node->span = span;
	node->content.cmd.argv = argv;
	node->content.cmd.envp = NULL;
	node->content.cmd.redirs = redirs;
	return (node);
}

t_ast_node	*ast_new_binop(t_binop_type op, t_ast_node *left,
			t_ast_node *right, t_span span)
{
	t_ast_node	*node;

	node = malloc(sizeof(t_ast_node));
	if (!node)
		return (printerr_syscall(ERR_MALLOC), NULL);
	node->type = NODE_BINOP;
	node->span = span;
	node->content.binop.op = op;
	node->content.binop.left = left;
	node->content.binop.right = right;
	return (node);
}

void	ast_free(t_ast_node *node)
{
	if (!node)
		return ;
	if (node->type == NODE_CMD)
	{
		freelst(node->content.cmd.argv);
		freelst(node->content.cmd.envp);
		free_redirs(node->content.cmd.redirs);
	}
	else if (node->type == NODE_BINOP)
	{
		ast_free(node->content.binop.left);
		ast_free(node->content.binop.right);
	}
	free(node);
}

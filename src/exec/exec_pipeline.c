/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   exec_pipeline.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kong <kong@student.42singapore.sg>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/04 18:31:06 by kong              #+#    #+#             */
/*   Updated: 2026/07/04 18:31:07 by kong             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

static void	_flatten_cmds(t_ast_node *ast, t_cmd_node **cmds, int *i)
{
	if (!ast)
		return ;
	if (ast->type == NODE_BINOP)
	{
		_flatten_cmds(ast->content.binop.left, cmds, i);
		_flatten_cmds(ast->content.binop.right, cmds, i);
	}
	else
	{
		cmds[*i] = &(ast->content.cmd);
		(*i)++;
	}
	cmds[*i] = NULL;
}

t_cmd_node	**setup_cmds(t_app *app, t_ast_node *ast, t_pipeops *pipeops)
{
	t_cmd_node	**cmds;
	int			i;

	pipeops->n_cmd = count_cmds(ast);
	if (pipeops->n_cmd > 256)
		return (setexit(app, EX_ERR),
			ft_putstr_fd("minishell: pipeline too long\n", 2), NULL);
	cmds = malloc(sizeof(t_cmd_node *) * (pipeops->n_cmd + 1));
	if (!cmds)
		return (setexit(app, EX_ERR), perror(APP), NULL);
	i = 0;
	_flatten_cmds(ast, cmds, &i);
	return (cmds);
}

int	setup_pipe(t_app *app, t_pipeops *pipeops, int iter_i)
{
	int	n_pipes;

	n_pipes = pipeops->n_cmd - 1;
	pipeops->pipebuf[0] = -1;
	pipeops->pipebuf[1] = -1;
	pipeops->fdout = -1;
	if (n_pipes > 0 && iter_i < n_pipes)
	{
		if (pipe(pipeops->pipebuf) == -1)
			return (setexit(app, EX_ERR), perror(APP), -1);
		pipeops->fdout = pipeops->pipebuf[1];
	}
	return (0);
}

// after fork, called inside child
int	do_childproc(t_app *app, t_cmd_node *cmdnode, int fdin, int fdout)
{
	signals_default();
	if (fdin != -1)
	{
		dup2(fdin, STDIN_FILENO);
		close(fdin);
	}
	if (fdout != -1)
	{
		dup2(fdout, STDOUT_FILENO);
		close(fdout);
	}
	if (open_redirs(app, cmdnode) == -1)
		return (close_redirsfd(cmdnode), -1);
	if (do_dup2redirs(app, cmdnode) == -1)
		return (close_redirsfd(cmdnode), -1);
	close_redirsfd(cmdnode);
	if (do_exec(app, cmdnode) == -1)
		return (-1);
	return (0);
}

int	start_pipeline(t_app *app, t_pipeops *pipeops, t_cmd_node **cmds)
{
	int	i;
	int	exitcode;

	i = 0;
	while (i < pipeops->n_cmd)
	{
		if (setup_pipe(app, pipeops, i) == -1)
			return (free_pipeops(pipeops, i - 1), -1);
		pipeops->pids[i] = fork();
		if (pipeops->pids[i] == -1)
			return (free_pipeops(pipeops, i - 1),
				setexit(app, EX_ERR), perror(APP), -1);
		if (pipeops->pids[i] == 0)
		{
			if (pipeops->pipebuf[0] != -1)
				close(pipeops->pipebuf[0]);
			do_childproc(app, cmds[i], pipeops->prevfdin, pipeops->fdout);
			exitcode = app->exitcode;
			return (free(cmds), free(pipeops), exit(exitcode), -1);
		}
		update_pipeops(pipeops, i);
		i++;
	}
	return (0);
}

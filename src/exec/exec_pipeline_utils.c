/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   exec_pipeline_utils.c                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kong <kong@student.42singapore.sg>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/04 18:30:43 by kong              #+#    #+#             */
/*   Updated: 2026/07/04 18:30:45 by kong             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

int	count_cmds(t_ast_node *root)
{
	int	count;

	count = 0;
	while (root)
	{
		count++;
		if (root->type == NODE_BINOP)
			root = root->content.binop.left;
		else
			break ;
	}
	return (count);
}

t_pipeops	*init_pipeops(void)
{
	t_pipeops	*pipeops;

	pipeops = malloc(sizeof(t_pipeops));
	if (!pipeops)
		return (NULL);
	pipeops->n_cmd = 0;
	pipeops->prevfdin = -1;
	pipeops->fdout = -1;
	pipeops->pipebuf[0] = -1;
	pipeops->pipebuf[1] = -1;
	ft_memset(pipeops->pids, -1, sizeof(pipeops->pids));
	return (pipeops);
}

void	update_pipeops(t_pipeops *pipeops, int iter_i)
{
	if (pipeops->prevfdin != -1)
	{
		close(pipeops->prevfdin);
		pipeops->prevfdin = -1;
	}
	if (pipeops->pipebuf[1] != -1)
	{
		close(pipeops->pipebuf[1]);
		pipeops->pipebuf[1] = -1;
	}
	if (iter_i < pipeops->n_cmd - 1)
		pipeops->prevfdin = pipeops->pipebuf[0];
	else if (pipeops->pipebuf[0] != -1)
	{
		close(pipeops->pipebuf[0]);
		pipeops->pipebuf[0] = -1;
	}
}

void	free_pipeops(t_pipeops *pipeops, int last_pid_id)
{
	if (pipeops->prevfdin != -1)
		close(pipeops->prevfdin);
	if (pipeops->pipebuf[0] != -1)
		close(pipeops->pipebuf[0]);
	if (pipeops->pipebuf[1] != -1)
		close(pipeops->pipebuf[1]);
	wait_allpids(pipeops->pids, last_pid_id);
	free(pipeops);
}

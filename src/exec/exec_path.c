/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   exec_path.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lkai-yua <lkai-yua@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/04 15:06:15 by lkai-yua          #+#    #+#             */
/*   Updated: 2026/07/04 15:06:55 by lkai-yua         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

char	*buildgoodpath(t_app *app, char *envp, char *cmd)
{
	char	*temp;
	char	*path;

	temp = ft_strjoin(envp, "/");
	if (!temp)
		return (setexit(app, EX_ERR), perror(APP), NULL);
	path = ft_strjoin(temp, cmd);
	free(temp);
	if (!path)
		return (setexit(app, EX_ERR), perror(APP), NULL);
	if (access(path, X_OK) != 0)
		return (setexecerrno(app), free(path), NULL);
	return (path);
}

static char	*cmdwithpath(t_app *app, char *cmd)
{
	char	*goodpath;

	goodpath = NULL;
	if (access(cmd, X_OK) != 0)
		return (setexecerrno(app), ft_perror(cmd), NULL);
	goodpath = ft_strndup(cmd, ft_strlen(cmd));
	if (!goodpath)
		return (setexit(app, EX_ERR), perror(APP), NULL);
	return (goodpath);
}

static char	*matchcmdpath(t_app *app, char **pathlst, char *cmd)
{
	int		i;
	char	*goodpath;

	goodpath = NULL;
	i = 0;
	while (pathlst[i])
	{
		if (!pathlst[i][0])
			goodpath = buildgoodpath(app, ".", cmd);
		else
			goodpath = buildgoodpath(app, pathlst[i], cmd);
		if (goodpath || app->exitcode == EX_CMD_NEXEC)
			break ;
		i++;
	}
	if (!goodpath)
	{
		if (app->exitcode == EX_CMD_NOTFOUND)
			return (printerr_cmdnfound(cmd), NULL);
		return (ft_perror(cmd), NULL);
	}
	return (goodpath);
}

char	*resolvecmdpath(t_app *app, char **argv)
{
	char	*cmd;
	char	**pathlst;
	char	*goodpath;

	goodpath = NULL;
	cmd = argv[0];
	if (ft_strhaschr(cmd, '/'))
	{
		goodpath = cmdwithpath(app, cmd);
		if (!goodpath)
			return (NULL);
		return (goodpath);
	}
	pathlst = getrawpathlst(app, "PATH=");
	if (!pathlst)
		return (setexit(app, EX_CMD_NOTFOUND), printerr_cmdnfound(cmd), NULL);
	goodpath = matchcmdpath(app, pathlst, cmd);
	if (!goodpath)
		return (freelst(pathlst), NULL);
	freelst(pathlst);
	setexit(app, EX_OK);
	return (goodpath);
}

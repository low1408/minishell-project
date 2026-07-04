/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   exec_path_utils.c                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kong <kong@student.42singapore.sg>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/04 17:56:19 by kong              #+#    #+#             */
/*   Updated: 2026/07/04 17:56:20 by kong             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

int	countbydelim(char *str, char delim)
{
	int	count;

	if (!str)
		return (0);
	count = 1;
	while (*str)
	{
		while (*str && *str != delim)
			str++;
		if (*str == delim)
		{
			str++;
			count++;
		}
	}
	return (count);
}

static int	_splithelper(char **target, char *str, char delim)
{
	int	lenword;
	int	n_delim;

	lenword = 0;
	n_delim = 0;
	while (str[lenword] && str[lenword] != delim)
		lenword++;
	if (str[lenword] == delim)
		n_delim += 1;
	*target = ft_strndup(str, lenword);
	if (!*target)
		return (-1);
	return (lenword + n_delim);
}

char	**ft_splitbydelim(t_app *app, char *str, char delim)
{
	char	**lst;
	int		size;
	int		i;
	int		next_size;

	size = countbydelim(str, delim);
	if (size == 0)
		return (NULL);
	lst = ft_calloclst(size);
	if (!lst)
		return (setexit(app, EX_ERR), perror(APP), NULL);
	i = 0;
	while (i < size)
	{
		next_size = _splithelper(&(lst[i]), str, delim);
		if (next_size == -1)
			return (setexit(app, EX_ERR), perror(APP), freelst(lst), NULL);
		i++;
		str += next_size;
	}
	return (lst);
}

char	**getrawpathlst(t_app *app, char *match)
{
	int		i;
	char	**rawpath;
	char	**envp;

	envp = app->envp;
	if (!envp)
		return (NULL);
	i = 0;
	rawpath = NULL;
	while (envp[i])
	{
		if (ft_strncmp(envp[i], match, ft_strlen(match)) == 0)
		{
			rawpath = ft_splitbydelim(app, envp[i] + ft_strlen(match), ':');
			if (!rawpath)
				return (NULL);
			break ;
		}
		i++;
	}
	return (rawpath);
}

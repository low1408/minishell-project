/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   builtin_env.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kong <kong@student.42singapore.sg>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/03 12:09:14 by kong              #+#    #+#             */
/*   Updated: 2026/07/03 12:09:15 by kong             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

/**
 * @brief env builtin — print all environment variables with values.
 *
 * Variables with NULL values (export-only) are excluded.
 */
int	builtin_env(t_app *app)
{
	char	**envp;
	t_env	*cur;
	int		i;

	envp = app->envp;
	if (envp)
	{
		i = 0;
		while (envp[i])
		{
			ft_putstr_fd(envp[i], 1);
			write(1, "\n", 1);
			i++;
		}
		return (EX_OK);
	}
	cur = app->env_list;
	while (cur)
	{
		if (cur->value)
		{
			ft_putstr_fd(cur->key, 1);
			write(1, "=", 1);
			ft_putstr_fd(cur->value, 1);
			write(1, "\n", 1);
		}
		cur = cur->next;
	}
	return (EX_OK);
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   builtin_exit.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kong <kong@student.42singapore.sg>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/03 12:18:06 by kong              #+#    #+#             */
/*   Updated: 2026/07/03 12:18:07 by kong             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

/**
 * @brief Count the number of arguments in argv.
 */
static int	argv_count(char **argv)
{
	int	i;

	i = 0;
	while (argv[i])
		i++;
	return (i);
}

/**
 * @brief exit builtin — exit the shell.
 *
 * Rules:
 *   - "exit" alone → exit with last status
 *   - "exit 1 2" → "too many arguments", do not exit, status 1
 *   - "exit abc 2" → "numeric argument required", exit with 2
 *   - "exit abc" → "numeric argument required", exit with 2
 *   - "exit 42" → exit with status 42 (mod 256)
 */
int	builtin_exit(char **argv, t_app *app)
{
	long long	val;
	int			argc;
	int			exitcode;

	ft_putstr_fd("exit\n", 2);
	argc = argv_count(argv);
	if (argc == 1)
	{
		exitcode = app->exitcode;
		env_free(app->env_list);
		exit(exitcode);
	}
	if (!is_numeric(argv[1], &val))
	{
		errmsg("exit", argv[1], "numeric argument required");
		env_free(app->env_list);
		exit(EX_SYNTAX);
	}
	if (argc > 2)
		return (errmsg("exit", NULL, "too many arguments"), EX_ERR);
	env_free(app->env_list);
	exit((unsigned char)val);
	return (EX_OK);
}

/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   expand_assignment.c                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kong <kong@student.42singapore.sg>         +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/03 19:30:09 by kong              #+#    #+#             */
/*   Updated: 2026/07/03 19:31:10 by kong             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "minishell.h"

static int	is_valid_assignment(const char *arg, char **name, char **value)
{
	int	i;
	int	len;

	i = 0;
	if (!arg || (!is_name_start(arg[0])))
		return (0);
	while (arg[i] && arg[i] != '=')
	{
		if (arg[i] != '_' && (arg[i] < 'A' || arg[i] > 'Z')
			&& (arg[i] < 'a' || arg[i] > 'z') && (arg[i] < '0' || arg[i] > '9'))
			return (0);
		i++;
	}
	if (arg[i] != '=')
		return (0);
	*name = ft_strndup((char *)arg, i);
	if (!*name)
		return (0);
	len = ft_strlen(arg + i + 1);
	if (len > 0 && arg[i + 1 + len - 1] == ';')
		*value = ft_strndup((char *)arg + i + 1, len - 1);
	else
		*value = ft_strdup(arg + i + 1);
	return (*value != NULL || (free(*name), 0));
}

static char	*expand_assignment_value(char *raw, char **envp, int last_status)
{
	char		*val;
	char		**words;
	size_t		count;

	words = NULL;
	count = 0;
	if (expand_word(raw, envp, last_status, &words, &count) == 0)
	{
		if (count > 0 && words[0])
			val = ft_strdup(words[0]);
		else
			val = ft_strdup("");
		freelst(words);
	}
	else
		val = ft_strdup("");
	free(raw);
	return (val);
}

static int	apply_assignment(t_env **env, char **envp, char *name, char *raw,
		int last_status)
{
	char	*val;

	val = expand_assignment_value(raw, envp, last_status);
	if (!val)
		return (free(name), ERR_MALLOC);
	if (env_set(env, name, val) != 0)
		return (free(name), free(val), ERR_MALLOC);
	free(name);
	free(val);
	return (0);
}

static int	count_assignments(char **argv)
{
	char	*name;
	char	*raw_val;
	int		count;

	count = 0;
	while (argv && argv[count])
	{
		if (!is_valid_assignment(argv[count], &name, &raw_val))
			break ;
		free(name);
		free(raw_val);
		count++;
	}
	return (count);
}

static void	remove_assignment_prefixes(t_cmd_node *cmd, int count)
{
	int	i;

	i = 0;
	while (i < count)
	{
		free(cmd->argv[i]);
		i++;
	}
	i = 0;
	while (cmd->argv[i + count])
	{
		cmd->argv[i] = cmd->argv[i + count];
		i++;
	}
	cmd->argv[i] = NULL;
}

static int	apply_persistent_assignments(t_app *app, t_cmd_node *cmd,
		int count, int last_status)
{
	char	*name;
	char	*raw_val;
	int		i;

	i = 0;
	while (i < count)
	{
		if (!is_valid_assignment(cmd->argv[i], &name, &raw_val)
			|| apply_assignment(&app->env_list, app->envp, name, raw_val,
				last_status) != 0)
			return (ERR_MALLOC);
		i++;
	}
	remove_assignment_prefixes(cmd, count);
	return (update_env_array(app));
}

static int	apply_temporary_assignments(t_app *app, t_cmd_node *cmd, int count,
		int last_status)
{
	t_env	*tmp_env;
	char	*name;
	char	*raw_val;
	int		i;

	tmp_env = env_init(app->envp);
	i = 0;
	while (i < count)
	{
		if (!is_valid_assignment(cmd->argv[i], &name, &raw_val)
			|| apply_assignment(&tmp_env, app->envp, name, raw_val,
				last_status) != 0)
			return (env_free(tmp_env), ERR_MALLOC);
		i++;
	}
	cmd->envp = env_to_array(tmp_env);
	env_free(tmp_env);
	if (!cmd->envp)
		return (ERR_MALLOC);
	remove_assignment_prefixes(cmd, count);
	return (0);
}

int	process_assignments(t_app *app, t_cmd_node *cmd, int last_status)
{
	int	count;

	count = count_assignments(cmd->argv);
	if (count == 0)
		return (0);
	if (!cmd->argv[count])
		return (apply_persistent_assignments(app, cmd, count, last_status));
	return (apply_temporary_assignments(app, cmd, count, last_status));
}

#include "minishell.h"

/**
 * @brief Create a new environment node.
 * @param key Variable name (duplicated).
 * @param value Variable value (duplicated, may be NULL).
 * @return New node, or NULL on malloc failure.
 */
t_env	*env_new_node(const char *key, const char *value)
{
	t_env	*node;

	if (!key || !*key)
		return (NULL);
	node = malloc(sizeof(t_env));
	if (!node)
		return (perror(APP), NULL);
	node->key = ft_strdup(key);
	if (!node->key)
		return (free(node), printerr_syscall(ERR_MALLOC), NULL);
	if (value)
	{
		node->value = ft_strdup(value);
		if (!node->value)
			return (free(node->key), free(node), printerr_syscall(ERR_MALLOC), NULL);
	}
	else
		node->value = NULL;
	node->next = NULL;
	return (node);
}

/**
 * @brief Build the environment linked list from the envp array.
 * @param envp NULL-terminated array of "KEY=VALUE" strings.
 * @return Head of the new list, or NULL on failure.
 */
t_env	*env_init(char **envp)
{
	t_env	*head;
	t_env	*tail;
	t_env	*node;
	char	*eq;
	char	*key;
	size_t	i;

	head = NULL;
	tail = NULL;
	i = 0;
	while (envp && envp[i])
	{
		eq = ft_strchr(envp[i], '=');
		if (!eq)
		{
			i++;
			continue ;
		}
		key = ft_strndup(envp[i], (int)(eq - envp[i]));
		if (!key)
			return (env_free(head), NULL);
		node = env_new_node(key, eq + 1);
		free(key);
		if (!node)
			return (env_free(head), NULL);
		if (!head)
			head = node;
		else
			tail->next = node;
		tail = node;
		i++;
	}
	return (head);
}

/**
 * @brief Free the entire environment list.
 * @param list Head of the list.
 */
void	env_free(t_env *list)
{
	t_env	*next;

	while (list)
	{
		next = list->next;
		free(list->key);
		free(list->value);
		free(list);
		list = next;
	}
}

/**
 * @brief Find a variable node by key.
 * @param list Head of the environment list.
 * @param key Variable name to find.
 * @return The matching node, or NULL if not found.
 */
t_env	*env_find(t_env *list, const char *key)
{
	if (!key)
		return (NULL);
	while (list)
	{
		if (ft_strcmp(list->key, key) == 0)
			return (list);
		list = list->next;
	}
	return (NULL);
}

/**
 * @brief Look up a variable by key.
 * @param list Head of the environment list.
 * @param key Variable name to find.
 * @return The value string (may be NULL for export-only), or NULL if not found.
 */
char	*env_get(t_env *list, const char *key)
{
	t_env	*node;

	node = env_find(list, key);
	if (!node)
		return (NULL);
	return (node->value);
}

/**
 * @brief Set or create an environment variable.
 * @param list Pointer to head pointer (may insert at head).
 * @param key Variable name.
 * @param value Variable value (NULL to mark export-only).
 * @return 0 on success, ERR_MALLOC on allocation failure, ERR_INVAL on bad args.
 */
int	env_set(t_env **list, const char *key, const char *value)
{
	t_env	*cur;
	char	*new_val;

	if (!list)
		return (ERR_INVAL);
	if (!key || !*key)
		return (0);
	cur = *list;
	while (cur)
	{
		if (ft_strcmp(cur->key, key) == 0)
		{
			if (value)
			{
				new_val = ft_strdup(value);
				if (!new_val)
					return (ERR_MALLOC);
				free(cur->value);
				cur->value = new_val;
			}
			return (0);
		}
		cur = cur->next;
	}
	cur = env_new_node(key, value);
	if (!cur)
		return (ERR_MALLOC);
	cur->next = *list;
	*list = cur;
	return (0);
}

/**
 * @brief Remove a variable by key.
 * @param list Pointer to head pointer.
 * @param key Variable name to remove.
 * @return 0 always (silent success even if not found).
 */
int	env_unset(t_env **list, const char *key)
{
	t_env	*cur;
	t_env	*prev;

	if (!list)
		return (ERR_INVAL);
	if (!key || !*key)
		return (0);
	prev = NULL;
	cur = *list;
	while (cur)
	{
		if (ft_strcmp(cur->key, key) == 0)
		{
			if (prev)
				prev->next = cur->next;
			else
				*list = cur->next;
			free(cur->key);
			free(cur->value);
			free(cur);
			return (0);
		}
		prev = cur;
		cur = cur->next;
	}
	return (0);
}

/**
 * @brief Count nodes in the environment list.
 */
static size_t	env_count(t_env *list)
{
	size_t	n;

	n = 0;
	while (list)
	{
		if (list->value)
			n++;
		list = list->next;
	}
	return (n);
}

/**
 * @brief Convert the environment list to a NULL-terminated char** array.
 *
 * Only variables with non-NULL values are included (matching `env` behavior).
 * Caller must free with freelst().
 */
char	**env_to_array(t_env *list)
{
	char	**arr;
	size_t	i;
	char	*tmp;

	arr = ft_calloclst(env_count(list));
	if (!arr)
		return (NULL);
	i = 0;
	while (list)
	{
		if (list->value)
		{
			tmp = ft_strjoin(list->key, "=");
			if (!tmp)
				return (freelst(arr), NULL);
			arr[i] = ft_strjoin(tmp, list->value);
			free(tmp);
			if (!arr[i])
				return (freelst(arr), NULL);
			i++;
		}
		list = list->next;
	}
	arr[i] = NULL;
	return (arr);
}

int	update_env_array(t_app *app)
{
	char	**new_envp;

	new_envp = env_to_array(app->env_list);
	if (!new_envp)
		return (printerr_syscall(ERR_MALLOC), ERR_MALLOC);
	if (app->envp)
		freelst(app->envp);
	app->envp = new_envp;
	return (EX_OK);
}

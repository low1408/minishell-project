#ifndef ENVP_H
# define ENVP_H

typedef struct s_env
{
	char			*key;
	char			*value;
	struct s_env	*next;
}	t_env;

/* === env.c === */
t_env	*env_init(char **envp);
void	env_free(t_env *list);
t_env	*env_new_node(const char *key, const char *value);
t_env	*env_find(t_env *list, const char *key);
char	*env_get(t_env *list, const char *key);
int		env_set(t_env **list, const char *key, const char *value);
int		env_unset(t_env **list, const char *key);
char	**env_to_array(t_env *list);
void	env_free_array(char **arr);

#endif

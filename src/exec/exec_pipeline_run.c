
#include "minishell.h"

void	get_lastcmdstatus(t_app *app, pid_t *pids, int id_lastpid)
{
	int	status;

	status = 0;
	// when waitpid is blocking, a signal can interrupt and cause it to return -1
	while (waitpid(pids[id_lastpid], &status, 0) == -1)
	{
		if (errno != EINTR)
			break ;
	}
	if (WIFEXITED(status))
		app->exitcode = WEXITSTATUS(status);
	else if (WIFSIGNALED(status))
	{
		if (WTERMSIG(status) == SIGQUIT)
			ft_putstr_fd("Quit (core dumped)\n", 2);
		else if (WTERMSIG(status) == SIGINT)
			write(2, "\n", 1);
		app->exitcode = EX_SIG_BASE + WTERMSIG(status);
	}
}

void	wait_allpids(pid_t *pids, int pid_id)
{
	int	i;

	i = 0;
	while (i <= pid_id)
	{
		while (waitpid(pids[i], NULL, 0) == -1)
		{
			if (errno != EINTR)
				break ;
		}
		i++;
	}
}
int	do_exec(t_app *app, t_cmd_node *cmdnode)
{
	char	*cmdpath;
	char	**saved_envp;
	int		status;

	if (!cmdnode->argv)
		return (exit(EX_OK), -1);
	saved_envp = app->envp;
	if (cmdnode->envp)
		app->envp = cmdnode->envp;
	if (is_builtin(cmdnode->argv[0]))
	{
		status = exec_builtin(cmdnode->argv, app);
		return (setexit(app, status), exit(app->exitcode), -1);
	}
	cmdpath = resolvecmdpath(app, cmdnode->argv);
	if (!cmdpath)
		return (app->envp = saved_envp, -1);
	if (execve(cmdpath, cmdnode->argv, app->envp) == -1)
	{
		app->envp = saved_envp;
		free(cmdpath);
		setexecerrno(app);
		if (!ft_strhaschr(cmdnode->argv[0], '/') &&
			app->exitcode == EX_CMD_NOTFOUND)
			return (printerr_cmdnfound(cmdnode->argv[0]), -1);
		return (ft_perror(cmdnode->argv[0]), -1);
	}
	return (0);
}

/*
* only info that gets returned from child to parent:
exit status    → via waitpid
signals        → kernel delivers them
written files  → kernel persists them
pipe data      → kernel buffers it

* a distinction to make:
if command is passed as `cat` without path, it returns:
- 126: (errno) permission denied
- 127: command not found

* However, when passing '/cat' directly to execve (syscall), it returns:
- 126: (errno) permission denied
- 127: (errno) No such file or directory
*/

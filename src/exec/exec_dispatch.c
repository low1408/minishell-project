#include "minishell.h"

int	exec_builtinproc(t_app *app, t_cmd_node *cmdnode)
{
	int		temp_in;
	int		temp_out;
	int		status;
	char	**saved_envp;

	// builtin runs in parent — needed for cd, export, exit, unset
	temp_in = -1;
	temp_out = -1;
	saved_envp = app->envp;
	if (cmdnode->redirs)
	{
		temp_in = dup(STDIN_FILENO);
		temp_out = dup(STDOUT_FILENO);
		if (temp_in == -1 || temp_out == -1)
			return (restore_fd(temp_in, temp_out), -1);
		if (open_redirs(app, cmdnode) == -1)
			return (restore_fd(temp_in, temp_out), close_redirsfd(cmdnode), -1);
		if (do_dup2redirs(app, cmdnode) == -1)
			return (restore_fd(temp_in, temp_out), close_redirsfd(cmdnode), -1);
		close_redirsfd(cmdnode);
	}
	if (cmdnode->envp)
		app->envp = cmdnode->envp;
	status = exec_builtin(cmdnode->argv, app);
	app->envp = saved_envp;
	restore_fd(temp_in, temp_out);
	setexit(app, status);
	return (status);
}

int	exec_allcmds(t_app *app, t_ast_node *ast)
{
	t_pipeops	*pipeops;
	t_cmd_node	**cmds;

	pipeops = init_pipeops();
	if (!pipeops)
		return (setexit(app, EX_ERR), perror(APP), -1);
	cmds = setup_cmds(app, ast, pipeops);
	if (!cmds)
		return (free(pipeops), -1);
	signals_ignore();
	if (start_pipeline(app, pipeops, cmds) == -1)
		return (free(cmds), -1);
	get_lastcmdstatus(app, pipeops->pids, pipeops->n_cmd - 1);
	free(cmds);
	free_pipeops(pipeops, pipeops->n_cmd - 2);
	return (0);
}

int	execute_ast(t_app *app, t_ast_node *ast)
{
	if (!ast)
		return (0);
	if (collect_heredocs(app, ast) == -1)
		return (-1);
	if (ast->type == NODE_CMD)
	{
		if (!ast->content.cmd.argv || !ast->content.cmd.argv[0])
		{	// no cmd but has redirects: bash creates/truncates the file
			if (open_redirs(app, &ast->content.cmd) == -1)
				return (close_redirsfd(&ast->content.cmd), -1);
			close_redirsfd(&ast->content.cmd);
			return (0);
		}
		else if (is_builtin(ast->content.cmd.argv[0]))
		{
			if (exec_builtinproc(app, &ast->content.cmd) == -1)
				return (-1);
			return (0);
		}
	}
	if (exec_allcmds(app, ast) == -1)
		return (-1);
	return (0);
}

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <errno.h>

typedef struct pipeline
{
	int start;
	int end;
} t_pipeline;

typedef struct command
{
	int		start;
	int		end;
	int		in_fd;
	int		out_fd;
	pid_t	pid;
	int		exit_status;
} t_command;

size_t	ft_strlen(char *str)
{
	int i = 0;

	while (str[i])
		i++;
	return (i);
}

bool	ft_error(char *error, char *arg)
{
	char *prefix = "error: ";

	write(STDERR_FILENO, prefix, ft_strlen(prefix));
	write(STDERR_FILENO, error, ft_strlen(error));
	write(STDERR_FILENO, " ", 1);
	write(STDERR_FILENO, arg, ft_strlen(arg));
	write(STDERR_FILENO, "\n", 1);
	return (false);
}

t_pipeline get_next_pipeline(int ac, char **av, int i)
{
	t_pipeline pipeline;

	pipeline.start = i;
	while (i < ac)
	{
		if (strcmp(av[i], ";") == 0)
			break ;
		i++;
	}
	pipeline.end = i;
	return (pipeline);
}

t_command	get_next_command(char **av, int start, int end)
{
	t_command cmd = {0};
	int i;

	i = start;
	cmd.start = start;
	cmd.in_fd = STDIN_FILENO;
	cmd.out_fd = STDOUT_FILENO;
	while (i < end)
	{
		if (strcmp(av[i], "|") == 0)
			break;
		i++;
	}
	cmd.end = i;
	return (cmd);
}

bool	dup2_and_close(int pipe_fd, int std_fd)
{
	if (pipe_fd != std_fd)
	{
		if (dup2(pipe_fd, std_fd) == -1)
		{
			printf("file descriptor: %d %d\n", pipe_fd, std_fd);
			return ft_error("fatal", strerror(errno));
		}
		if (close(pipe_fd) == -1)
			return ft_error("fatal", "");
	}
	return (true);
}

void	child_process(t_command *cmd, char **av, char **env)
{
	if (!dup2_and_close(cmd->in_fd, STDIN_FILENO))
		exit(1);
	if (!dup2_and_close(cmd->out_fd, STDOUT_FILENO))
		exit(1);
	av[cmd->end] = NULL;
	if (execve(av[cmd->start], av + cmd->start, env) != 0)
		exit(ft_error("cannot execute", av[cmd->start]));
}

#define RD 0
#define WR 1

bool	fork_and_execute(t_command *cmd, char **av, char **env, bool last)
{
	static int old_fd = STDIN_FILENO;
	int pipe_fds[2];

	if (!last)
	{
		if(pipe(pipe_fds) == -1)
			return ft_error("fatal", "");
		cmd->out_fd = pipe_fds[WR];
	}
	cmd->pid = fork();
	if (cmd->pid == -1)
		return ft_error("fatal", "");
	if (cmd->pid == 0)
	{
		if (!last && close(pipe_fds[RD]) == -1)
			exit(ft_error("fatal", ""));
		cmd->in_fd = old_fd;
		child_process(cmd, av, env);
	}
	else
	{
		if (old_fd != STDIN_FILENO && close(old_fd) == -1)
			return ft_error("fatal", "");
		if (!last)
		{
			if (close(pipe_fds[WR]) == -1)
				return ft_error("fatal", "");
			old_fd = pipe_fds[RD];
		}
		else
			old_fd = STDIN_FILENO;
	}
	return (true);
}

int	builtin_cd(t_pipeline pipeline, char **av)
{
	if (pipeline.end - pipeline.start != 2)
		return ft_error("cd: bad arguments", "");
	if(chdir(av[pipeline.start + 1]) == -1)
		return ft_error("cd: cannot change directory to", av[pipeline.start + 1]);
	return (0);
}

int	run_pipeline(t_pipeline pipeline, char **av, char **env)
{
	int	i;
	int	nb_cmd = 0;
	bool last;
	t_command cmd;

	i = pipeline.start;
	if (strcmp(av[i], "cd") == 0)
		return (builtin_cd(pipeline, av));
	while (i < pipeline.end)
	{
		cmd = get_next_command(av, i, pipeline.end);
		nb_cmd++;
		last = (cmd.end == pipeline.end);
		if(!fork_and_execute(&cmd, av, env, last))
			exit(3);
		i = cmd.end + 1;
	}
	waitpid(cmd.pid, &cmd.exit_status, 0);
	while (nb_cmd - 1 > 0)
	{
		waitpid(-1, NULL, 0);
		nb_cmd--;
	}
	return (WEXITSTATUS(cmd.exit_status));
}

int main(int ac, char **av, char **env)
{
	t_pipeline	pipeline = {0};
	int			i = 1;
	int			ret = 0;

	while (i < ac)
	{
		pipeline = get_next_pipeline(ac, av, i);
		ret = run_pipeline(pipeline, av, env);
		i = pipeline.end + 1;
	}
	return (ret);
}

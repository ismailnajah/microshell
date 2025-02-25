#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>

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

void	print(char **av, int start, int end)
{
	int i = start;
	printf("DEBUG :{ ");
	while (i < end)
	{
		printf("%s ", av[i]);
		i++;
	}
	printf("}\n");
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
		dup2(pipe_fd, std_fd);
		close(pipe_fd);
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
		exit(2);
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
			return (false);
		cmd->out_fd = pipe_fds[WR];
	}
	cmd->pid = fork();
	if (cmd->pid == -1)
		return (false);
	if (cmd->pid == 0)
	{
		cmd->in_fd = old_fd;
		child_process(cmd, av, env);
	}
	else
	{
		if (old_fd != STDIN_FILENO)
			close(old_fd);
		if (!last)
		{
			close(pipe_fds[WR]);
			old_fd = pipe_fds[RD];
		}
		else
			old_fd = STDIN_FILENO;
	}
	return (true);
}

int	run_pipeline(t_pipeline pipeline, char **av, char **env)
{
	int	i;
	bool last;
	t_command cmd;

	i = pipeline.start;
	while (i < pipeline.end)
	{
		cmd = get_next_command(av, i, pipeline.end);
		last = (cmd.end == pipeline.end);
		fork_and_execute(&cmd, av, env, last);
		i = cmd.end + 1;
	}
	waitpid(cmd.pid, &cmd.exit_status, 0);
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
		//print_pipeline(pipeline, av);
		i = pipeline.end + 1;
	}
	return (ret);
}

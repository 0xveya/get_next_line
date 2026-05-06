/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line_bonus.c                             :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: sfurst <sfurst@student.42vienna.com>      #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:21 by sfurst           #+#    #+#              */
/*   Updated: 2026/05/06 22:28:04 by sfurst          ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "get_next_line_bonus.h"

int	has_newline(t_gnl *dat)
{
	ssize_t	i;

	i = 0;
	while (i < dat->len)
	{
		if (dat->data[i] == '\n')
			return (1);
		i++;
	}
	return (0);
}

int	read_until_line(int fd, t_gnl *dat)
{
	static char	buf[BUFFER_SIZE];
	ssize_t		bytes;

	while (!has_newline(dat))
	{
		bytes = read(fd, buf, BUFFER_SIZE);
		if (bytes < 0)
			return (0);
		if (bytes == 0)
			break ;
		if (!append_data(dat, buf, bytes))
			return (0);
	}
	return (1);
}

char	*extract_line(t_gnl *dat)
{
	char	*line;
	ssize_t	i;
	ssize_t	line_len;

	i = 0;
	while (i < dat->len && dat->data[i] != '\n')
		i++;
	if (i < dat->len && dat->data[i] == '\n')
		i++;
	line_len = i;
	line = malloc(line_len + 1);
	if (!line)
		return (NULL);
	return (ft_memcpy(line, dat->data, line_len), line[line_len] = '\0', line);
}

void	consume_line(t_gnl *dat)
{
	ssize_t	i;
	ssize_t	remaining;

	i = 0;
	while (i < dat->len && dat->data[i] != '\n')
		i++;
	if (i < dat->len && dat->data[i] == '\n')
		i++;
	remaining = dat->len - i;
	if (remaining > 0)
		ft_memmove(dat->data, dat->data + i, remaining);
	dat->len = remaining;
	if (dat->data)
		dat->data[dat->len] = '\0';
	if (dat->len == 0)
		free_stuff(dat);
}

char	*get_next_line(int fd)
{
	static t_gnl	dat[MAX_FD];
	char			*line;

	if (fd < 0 || fd >= MAX_FD || BUFFER_SIZE <= 0)
		return (NULL);
	if (!read_until_line(fd, &dat[fd]))
		return (free_stuff(&dat[fd]), NULL);
	if (dat[fd].len == 0)
		return (free_stuff(&dat[fd]), NULL);
	line = extract_line(&dat[fd]);
	if (!line)
		return (free_stuff(&dat[fd]), NULL);
	return (consume_line(&dat[fd]), line);
}

/*
#include <fcntl.h>
#include <stdio.h>

int	main(int argc, char **argv)
{
	int		fd1;
	int		fd2;
	char	*line1;
	char	*line2;

	if (argc != 3)
		return (printf("usage: %s <file1> <file2>\n", argv[0]), 1);
	fd1 = open(argv[1], O_RDONLY);
	fd2 = open(argv[2], O_RDONLY);
	if (fd1 < 0 || fd2 < 0)
		return (perror("open"), 1);
	line1 = get_next_line(fd1);
	line2 = get_next_line(fd2);
	while (line1 || line2)
	{
		if (line1)
		{
			printf("[fd1] %s", line1);
			free(line1);
			line1 = get_next_line(fd1);
		}
		if (line2)
		{
			printf("[fd2] %s", line2);
			free(line2);
			line2 = get_next_line(fd2);
		}
	}
	close(fd1);
	close(fd2);
	return (0);
}
*/

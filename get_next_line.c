/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line.c                                   :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: sfurst <sfurst@student.42vienna.com>      #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:21 by sfurst           #+#    #+#              */
/*   Updated: 2026/05/04 18:31:23 by sfurst          ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "get_next_line.h"

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
	char	*buf;
	ssize_t	bytes;

	buf = malloc(BUFFER_SIZE);
	if (!buf)
		return (0);
	while (!has_newline(dat))
	{
		bytes = read(fd, buf, BUFFER_SIZE);
		if (bytes < 0)
			return (free(buf), 0);
		if (bytes == 0)
			break ;
		if (!append_data(dat, buf, bytes))
			return (free(buf), 0);
	}
	return (free(buf), 1);
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
	static t_gnl	dat;
	char			*line;

	if (fd < 0 || BUFFER_SIZE <= 0)
		return (NULL);
	if (!read_until_line(fd, &dat))
		return (free_stuff(&dat), NULL);
	if (dat.len == 0)
		return (free_stuff(&dat), NULL);
	line = extract_line(&dat);
	if (!line)
		return (free_stuff(&dat), NULL);
	return (consume_line(&dat), line);
}

/*
#include <fcntl.h>
#include <stdio.h>

int	main(int argc, char **argv)
{
	int		fd;
	char	*line;

	if (argc != 2)
		return (printf("usage: %s <file>\n", argv[0]), 1);
	fd = open(argv[1], O_RDONLY);
	if (fd < 0)
		return (perror("open"), 1);
	line = get_next_line(fd);
	while (line)
	{
		printf("%s", line);
		free(line);
		line = get_next_line(fd);
	}
	close(fd);
	return (0);
}
*/

/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line_bonus.c                             :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: flaltens <flaltens@student.42vienna.com>  #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:21 by flaltens         #+#    #+#              */
/*   Updated: 2026/08/15 21:28:28 by flaltens        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "get_next_line_bonus.h"

static int	refill(int fd, t_gnl *gnl)
{
	if (!gnl->read_buf)
	{
		gnl->read_buf = malloc(BUFFER_SIZE);
		if (!gnl->read_buf)
			return (gnl->read_len = -1, 0);
	}
	gnl->pos = 0;
	gnl->read_len = read(fd, gnl->read_buf, BUFFER_SIZE);
	return (gnl->read_len > 0);
}

static ssize_t	chunk_len(t_gnl *gnl)
{
	ssize_t	len;

	len = 0;
	while (gnl->pos + len < gnl->read_len)
	{
		len++;
		if (gnl->read_buf[gnl->pos + len - 1] == '\n')
			break ;
	}
	return (len);
}

static char	*finish_line(t_gnl *gnl)
{
	char	*line;

	line = gnl->line;
	gnl->line = NULL;
	gnl->line_len = 0;
	gnl->line_cap = 0;
	return (line);
}

char	*get_next_line(int fd)
{
	static t_gnl	gnl[MAX_FD];
	t_gnl			*reader;
	ssize_t			len;

	if (fd < 0 || fd >= MAX_FD || BUFFER_SIZE <= 0)
		return (NULL);
	reader = &gnl[fd];
	while (reader->pos < reader->read_len || refill(fd, reader))
	{
		len = chunk_len(reader);
		if (!append_chunk(reader, reader->read_buf + reader->pos, len))
			return (clear_gnl(reader), NULL);
		reader->pos += len;
		if (reader->line[reader->line_len - 1] == '\n')
			return (finish_line(reader));
	}
	if (reader->read_len < 0 || reader->line_len == 0)
		return (clear_gnl(reader), NULL);
	return (finish_line(reader));
}

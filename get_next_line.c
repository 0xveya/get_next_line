/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line.c                                   :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: flaltens <flaltens@student.42vienna.com>  #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:21 by flaltens         #+#    #+#              */
/*   Updated: 2026/08/15 21:28:28 by flaltens        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "get_next_line.h"

static int	refill(int fd, t_gnl *gnl)
{
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
	static t_gnl	gnl;
	ssize_t			len;

	if (fd < 0 || BUFFER_SIZE <= 0)
		return (NULL);
	while (gnl.pos < gnl.read_len || refill(fd, &gnl))
	{
		len = chunk_len(&gnl);
		if (!append_chunk(&gnl, gnl.read_buf + gnl.pos, len))
			return (clear_gnl(&gnl), NULL);
		gnl.pos += len;
		if (gnl.line[gnl.line_len - 1] == '\n')
			return (finish_line(&gnl));
	}
	if (gnl.read_len < 0 || gnl.line_len == 0)
		return (clear_gnl(&gnl), NULL);
	return (finish_line(&gnl));
}

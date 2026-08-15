/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line_bonus.c                             :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: sfurst <sfurst@student.42vienna.com>      #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:21 by sfurst           #+#    #+#              */
/*   Updated: 2026/08/15 22:20:29 by sfurst          ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "get_next_line_bonus.h"

static ssize_t	chunk_len(t_gnl *gnl) __attribute__((target("avx2")));

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
	__m256i	nl;

	nl = _mm256_set1_epi8(GNL_DELIMITER);
	gnl->scan_i = 0;
	gnl->scan_n = gnl->read_len - gnl->pos;
	while (gnl->scan_n - gnl->scan_i >= 32)
	{
		gnl->scan_v = _mm256_loadu_si256((const __m256i *)(gnl->read_buf
					+ gnl->pos + gnl->scan_i));
		gnl->scan_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(gnl->scan_v,
					nl));
		if (gnl->scan_mask)
			return (gnl->scan_i + __builtin_ctz(gnl->scan_mask) + 1);
		gnl->scan_i += 32;
	}
	while (gnl->scan_i < gnl->scan_n)
	{
		if (gnl->read_buf[gnl->pos + gnl->scan_i++] == GNL_DELIMITER)
			return (gnl->scan_i);
	}
	return (gnl->scan_n);
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
		if (reader->read_buf[reader->pos - 1] == GNL_DELIMITER)
			return (finish_line(reader));
	}
	if (reader->read_len < 0 || reader->line_len == 0)
		return (clear_gnl(reader), NULL);
	return (finish_line(reader));
}

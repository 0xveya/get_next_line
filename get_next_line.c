/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line.c                                   :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: sfurst <sfurst@student.42vienna.com>      #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:21 by sfurst           #+#    #+#              */
/*   Updated: 2026/08/17 21:55:35 by sfurst          ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "get_next_line.h"

static ssize_t	chunk_len(t_gnl *gnl) __attribute__((target("avx2"), hot));

static int		refill(int fd, t_gnl *gnl) __attribute__((hot));

static char		*finish_line(t_gnl *gnl);

static int	refill(int fd, t_gnl *gnl)
{
	gnl->pos = 0;
	gnl->read_len = read(fd, gnl->read_buf, BUFFER_SIZE);
	return (__builtin_expect(gnl->read_len > 0, 1));
}

static ssize_t	chunk_len(t_gnl *gnl)
{
	__m256i	nl;

	nl = _mm256_set1_epi8(GNL_DELIMITER);
	gnl->scan_i = 0;
	gnl->scan_n = gnl->read_len - gnl->pos;
	while (__builtin_expect(gnl->scan_n - gnl->scan_i >= 32, 1))
	{
		gnl->scan_v = _mm256_loadu_si256((const __m256i *)(gnl->read_buf
					+ gnl->pos + gnl->scan_i));
		gnl->scan_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(gnl->scan_v,
					nl));
		if (__builtin_expect(gnl->scan_mask != 0, 0))
			return (gnl->scan_i + __builtin_ctz(gnl->scan_mask) + 1);
		gnl->scan_i += 32;
	}
	while (__builtin_expect(gnl->scan_i < gnl->scan_n, 0))
	{
		if (__builtin_expect(gnl->read_buf[gnl->pos
					+ gnl->scan_i++] == GNL_DELIMITER, 0))
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
	static t_gnl	gnl;
	ssize_t			len;

	while (__builtin_expect(gnl.pos < gnl.read_len || refill(fd, &gnl), 1))
	{
		len = chunk_len(&gnl);
		if (__builtin_expect(!append_chunk(&gnl, gnl.read_buf + gnl.pos, len),
				0))
			return (clear_gnl(&gnl), NULL);
		gnl.pos += len;
		if (__builtin_expect(gnl.read_buf[gnl.pos - 1] == GNL_DELIMITER, 0))
			return (finish_line(&gnl));
	}
	if (__builtin_expect(gnl.read_len < 0 || gnl.line_len == 0, 0))
		return (clear_gnl(&gnl), NULL);
	return (finish_line(&gnl));
}

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

static ssize_t	chunk_len(t_gnl *gnl) __attribute__((target("avx2"), hot));

static int		refill(int fd, t_gnl *gnl) __attribute__((hot));

static int	refill(int fd, t_gnl *gnl)
{
	long	ret;

	__asm__("syscall" : "=a"(ret) : "a"(0L), "D"((long)fd),
		"S"(gnl->read_buf), "d"((size_t)BUFFER_SIZE) : "rcx", "r11", "memory");
	gnl->pos = 0;
	gnl->read_len = ret;
	return (__builtin_expect(ret > 0, 1));
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

static inline char	*finish_line(t_gnl *gnl)
{
	char	*line;

	line = gnl->line;
	gnl->line = NULL;
	gnl->line_len = 0;
	gnl->line_cap = 0;
	return (line);
}

static int	init_reader(t_gnl *reader)
{
	if (reader->read_buf)
		return (1);
	reader->read_buf = malloc(BUFFER_SIZE);
	return (reader->read_buf != NULL);
}

char	*get_next_line(int fd)
{
	static t_gnl	gnl[MAX_FD];
	t_gnl			*reader;
	ssize_t			len;

	if (fd < 0 || fd >= MAX_FD)
		return (NULL);
	reader = &gnl[fd];
	if (!init_reader(reader))
		return (NULL);
	while (__builtin_expect(reader->pos < reader->read_len
			|| refill(fd, reader), 1))
	{
		len = chunk_len(reader);
		if (__builtin_expect(!append_chunk(reader,
					reader->read_buf + reader->pos, len), 0))
			return (clear_gnl(reader), NULL);
		reader->pos += len;
		if (__builtin_expect(reader->read_buf[reader->pos - 1]
				== GNL_DELIMITER, 0))
			return (finish_line(reader));
	}
	if (__builtin_expect(reader->read_len < 0 || reader->line_len == 0, 0))
		return (clear_gnl(reader), NULL);
	return (finish_line(reader));
}

/*
#include <fcntl.h>
#include <stdio.h>

int	main(int argc, char **argv)
{
	char	*line;
	int		fd;

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

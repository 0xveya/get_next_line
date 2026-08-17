/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line_utils_bonus.c                       :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: sfurst <sfurst@student.42vienna.com>      #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:16 by sfurst           #+#    #+#              */
/*   Updated: 2026/08/15 22:20:30 by sfurst          ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "get_next_line_bonus.h"
#include <limits.h>

static void	copy_bytes(char *dst, const char *src,
				ssize_t len) __attribute__((target("avx2"), hot));

static void	copy_bytes(char *dst, const char *src, ssize_t len)
{
	while (len >= 128)
	{
		_mm256_storeu_si256((__m256i *)dst,
			_mm256_loadu_si256((const __m256i *)src));
		_mm256_storeu_si256((__m256i *)(dst + 32),
			_mm256_loadu_si256((const __m256i *)(src + 32)));
		_mm256_storeu_si256((__m256i *)(dst + 64),
			_mm256_loadu_si256((const __m256i *)(src + 64)));
		_mm256_storeu_si256((__m256i *)(dst + 96),
			_mm256_loadu_si256((const __m256i *)(src + 96)));
		dst += 128;
		src += 128;
		len -= 128;
	}
	while (len >= 32)
	{
		_mm256_storeu_si256((__m256i *)dst,
			_mm256_loadu_si256((const __m256i *)src));
		dst += 32;
		src += 32;
		len -= 32;
	}
	while (len-- > 0)
		*dst++ = *src++;
}

static int	grow_line(t_gnl *gnl, ssize_t needed)
{
	char	*new;
	ssize_t	capacity;

	if (__builtin_expect(needed <= gnl->line_cap, 1))
		return (1);
	capacity = gnl->line_cap;
	if (__builtin_expect(capacity == 0, 0))
		capacity = BUFFER_SIZE + 1;
	if (__builtin_expect(capacity < 64, 0))
		capacity = 64;
	while (__builtin_expect(capacity < needed, 0))
	{
		if (__builtin_expect(capacity > SSIZE_MAX / 2, 0))
			capacity = needed;
		else
			capacity *= 2;
	}
	new = malloc(capacity);
	if (__builtin_expect(new == NULL, 0))
		return (0);
	copy_bytes(new, gnl->line, gnl->line_len);
	free(gnl->line);
	gnl->line = new;
	gnl->line_cap = capacity;
	return (1);
}

int	append_chunk(t_gnl *gnl, const char *chunk, ssize_t len)
{
	if (__builtin_expect(len > SSIZE_MAX - gnl->line_len - 1, 0))
		return (0);
	if (__builtin_expect(!grow_line(gnl, gnl->line_len + len + 1), 0))
		return (0);
	copy_bytes(gnl->line + gnl->line_len, chunk, len);
	gnl->line_len += len;
	gnl->line[gnl->line_len] = '\0';
	return (1);
}

void	clear_gnl(t_gnl *gnl)
{
	free(gnl->read_buf);
	free(gnl->line);
	gnl->read_buf = NULL;
	gnl->line = NULL;
	gnl->pos = 0;
	gnl->read_len = 0;
	gnl->line_len = 0;
	gnl->line_cap = 0;
}

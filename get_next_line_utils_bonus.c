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
				ssize_t len) __attribute__((target("avx2")));

static void	copy_bytes(char *dst, const char *src, ssize_t len)
{
	while (len >= 32)
	{
		_mm256_storeu_si256((__m256i *)(dst),
			_mm256_loadu_si256((const __m256i *)(src)));
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

	if (needed <= gnl->line_cap)
		return (1);
	capacity = gnl->line_cap;
	if (capacity == 0)
		capacity = BUFFER_SIZE + 1;
	if (capacity < 64)
		capacity = 64;
	while (capacity < needed)
	{
		if (capacity > SSIZE_MAX / 2)
			capacity = needed;
		else
			capacity *= 2;
	}
	new = malloc(capacity);
	if (!new)
		return (0);
	copy_bytes(new, gnl->line, gnl->line_len);
	free(gnl->line);
	gnl->line = new;
	gnl->line_cap = capacity;
	return (1);
}

int	append_chunk(t_gnl *gnl, const char *chunk, ssize_t len)
{
	if (len > SSIZE_MAX - gnl->line_len - 1)
		return (0);
	if (!grow_line(gnl, gnl->line_len + len + 1))
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

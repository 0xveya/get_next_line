/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line_utils.c                             :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: sfurst <sfurst@student.42vienna.com>      #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:16 by sfurst           #+#    #+#              */
/*   Updated: 2026/05/06 22:26:36 by sfurst          ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#include "get_next_line.h"

void	*ft_memmove(void *dst, const void *src, size_t len)
{
	unsigned char	*dest_char;
	unsigned char	*src_char;

	dest_char = (unsigned char *)dst;
	src_char = (unsigned char *)src;
	if (!dst && !src)
		return (dst);
	if (src < dst)
		while (len--)
			dest_char[len] = src_char[len];
	else
		while (len--)
			*dest_char++ = *src_char++;
	return (dst);
}

void	*ft_memcpy(void *dst, const void *src, size_t n)
{
	unsigned char	*dstc;
	unsigned char	*srcc;

	dstc = (unsigned char *)dst;
	srcc = (unsigned char *)src;
	while (n--)
		*dstc++ = *srcc++;
	return (dst);
}

size_t	ft_strlen(const char *s)
{
	size_t	i;

	i = 0;
	while ((s[i]) && (i++, 1))
		;
	return (i);
}

void	free_stuff(t_gnl *dat)
{
	if (dat->data)
		free(dat->data);
	dat->data = NULL;
	dat->cap = 0;
	dat->len = 0;
}

int	append_data(t_gnl *dat, char *buf, ssize_t bytes)
{
	char	*new;
	ssize_t	new_cap;

	if (dat->len + bytes + 1 > dat->cap)
	{
		new_cap = dat->cap;
		if (new_cap == 0)
			new_cap = bytes + 1;
		while (new_cap < dat->len + bytes + 1)
			new_cap *= 2;
		new = malloc(new_cap);
		if (!new)
			return (0);
		if (dat->data)
			ft_memcpy(new, dat->data, dat->len);
		free(dat->data);
		dat->data = new;
		dat->cap = new_cap;
	}
	ft_memcpy(dat->data + dat->len, buf, bytes);
	return (dat->len += bytes, dat->data[dat->len] = '\0', 1);
}

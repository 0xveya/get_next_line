/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line_bonus.c                             :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: sfurst <sfurst@student.42vienna.com>      #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:21 by sfurst           #+#    #+#              */
/*   Updated: 2026/05/04 19:01:16 by sfurst          ###   ########.fr        */
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

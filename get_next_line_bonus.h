/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line.h                                   :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: sfurst <sfurst@student.42vienna.com>      #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:25 by sfurst           #+#    #+#              */
/*   Updated: 2026/05/04 19:00:31 by sfurst          ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#ifndef GET_NEXT_LINE_BONUS_H
# define GET_NEXT_LINE_BONUS_H

# include <limits.h>
# include <stdlib.h>
# include <unistd.h>

# ifndef BUFFER_SIZE
#  define BUFFER_SIZE 1
# endif

# ifndef MAX_FD
#  define MAX_FD 1024
# endif

typedef struct s_gnl
{
	char	*data;
	ssize_t	cap;
	ssize_t	len;
}			t_gnl;

int			append_data(t_gnl *dat, char *buf, ssize_t bytes);
void		free_stuff(t_gnl *dat);
void		*ft_memcpy(void *dst, const void *src, size_t n);
void		*ft_memmove(void *dst, const void *src, size_t len);
size_t		ft_strlen(const char *s);
char		*get_next_line(int fd);

#endif

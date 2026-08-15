/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line.h                                   :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: sfurst <sfurst@student.42vienna.com>      #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:25 by sfurst           #+#    #+#              */
/*   Updated: 2026/08/15 22:20:28 by sfurst          ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#ifndef GET_NEXT_LINE_H
# define GET_NEXT_LINE_H

# include <stdlib.h>
# include <unistd.h>
# include <immintrin.h>

# ifndef BUFFER_SIZE
#  define BUFFER_SIZE 1
# endif
# ifndef GNL_DELIMITER
#  define GNL_DELIMITER '\n'
# endif

typedef struct s_gnl
{
	char			read_buf[BUFFER_SIZE];
	ssize_t			pos;
	ssize_t			read_len;
	char			*line;
	ssize_t			line_len;
	ssize_t			line_cap;
	ssize_t			scan_i;
	ssize_t			scan_n;
	unsigned int	scan_mask;
	__m256i			scan_v;
}					t_gnl;

int					append_chunk(t_gnl *gnl, const char *chunk, ssize_t len);
void				clear_gnl(t_gnl *gnl);
char				*get_next_line(int fd);

#endif

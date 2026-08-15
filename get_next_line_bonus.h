/* ************************************************************************** */
/*                                                                            */
/*                                                       :::      ::::::::    */
/*   get_next_line_bonus.h                             :+:      :+:    :+:    */
/*                                                   +:+ +:+         +:+      */
/*   By: flaltens <flaltens@student.42vienna.com>  #+#  +:+       +#+         */
/*                                               +#+#+#+#+#+   +#+            */
/*   Created: 2026/05/04 17:24:25 by flaltens         #+#    #+#              */
/*   Updated: 2026/08/15 21:28:29 by flaltens        ###   ########.fr        */
/*                                                                            */
/* ************************************************************************** */

#ifndef GET_NEXT_LINE_BONUS_H
# define GET_NEXT_LINE_BONUS_H

# include <stdlib.h>
# include <unistd.h>
# include <immintrin.h>

# ifndef BUFFER_SIZE
#  define BUFFER_SIZE 1
# endif
# ifndef MAX_FD
#  define MAX_FD 1024
# endif

typedef struct s_gnl
{
	char			*read_buf;
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

int			append_chunk(t_gnl *gnl, const char *chunk, ssize_t len);
void		clear_gnl(t_gnl *gnl);
char		*get_next_line(int fd);

#endif

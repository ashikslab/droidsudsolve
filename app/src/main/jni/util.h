/* 
 * libexact v1.0 --- a software library for solving combinatorial 
 *                   exact covering problems
 * 
 * Copyright (C) 2008 Petteri Kaski <petteri.kaski@cs.helsinki.fi>
 *                    Olli Pottonen <olli.pottonen@tkk.fi>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
 * MA  02110-1301, USA.
 * 
 */

/************************************ Internal utility functions and macros. */

#ifndef EXACT_UTIL_READ
#define EXACT_UTIL_READ

#include <stdlib.h>
#include <stdarg.h>

void *           exact_mem_alloc      (size_t size);
void             exact_mem_free       (void *p);

#define     EXACT_ALLOC(s)   exact_mem_alloc(s)
#define     EXACT_FREE(p)    exact_mem_free(p)

struct      exact_chunk_struct;
typedef     struct exact_chunk_struct   exact_chunk_t;

exact_chunk_t *  exact_chunk_alloc    (size_t entry, int entries);
void             exact_chunk_free     (exact_chunk_t *c);
void *           exact_chunk_get      (exact_chunk_t *c);

struct      exact_map_struct;
typedef     struct exact_map_struct     exact_map_t;
typedef     int exact_map_cmp_t         (const void *, const void *);  

exact_map_t *    exact_map_alloc      (size_t key, exact_map_cmp_t *f);
void             exact_map_free       (exact_map_t *m);
int              exact_map_find       (exact_map_t *m, void *k);
void *           exact_map_value      (exact_map_t *m);
void             exact_map_associate  (exact_map_t *m, void *v);

void             exact_error          (const char *fn, 
                                       int line, 
                                       const char *func,
                                       const char *format, ...);

void             exact_internal_error (const char *fn, 
                                       int line, 
                                       const char *func,
                                       const char *format, ...);

#define EXACT_ERROR(...) exact_error(__FILE__,__LINE__,__func__,__VA_ARGS__);
#define EXACT_INTERNAL_ERROR(...) \
                exact_internal_error(__FILE__,__LINE__,__func__,__VA_ARGS__);

#endif

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

/******************************************************** Library interface. */

#ifndef EXACT_READ
#define EXACT_READ

struct         exact_struct;
typedef        struct exact_struct  exact_t;
typedef int    exact_level_t        (void *, int, const int *);  
typedef int    exact_filter_t       (void *, int, const int *, int);

/* Initializing and releasing an instance. */

exact_t *      exact_alloc          (void);
void           exact_free           (exact_t *e);

/* Declaring an instance. */

void           exact_declare_row    (exact_t *e, int i, int b);
void           exact_declare_col    (exact_t *e, int j, int u);
void           exact_declare_entry  (exact_t *e, int i, int j);
int            exact_can_declare    (exact_t *e);

/* Iterating through the solutions. */

const int *    exact_solve          (exact_t *e, int *n);
void           exact_reset_solve    (exact_t *e);

/* Examining an instance. */

int            exact_is_row         (exact_t *e, int i);
int            exact_is_col         (exact_t *e, int j);
int            exact_is_entry       (exact_t *e, int i, int j);
int            exact_num_rows       (exact_t *e);
int            exact_num_cols       (exact_t *e);
int            exact_get_rows       (exact_t *e, int *i);
int            exact_get_cols       (exact_t *e, int *j);

/* Forcing a partial solution. */

void           exact_push           (exact_t *e, int j);
void           exact_pop            (exact_t *e);
int            exact_pushable       (exact_t *e, int j);
int            exact_can_push       (exact_t *e);
int            exact_num_push       (exact_t *e);
int            exact_get_push       (exact_t *e, int *j);

/* Controlling the search. */

void           exact_level          (exact_t *e, exact_level_t *l, void *p);
void           exact_filter         (exact_t *e, exact_filter_t *f, void *p);

#endif    

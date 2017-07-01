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

/*************************************************** Library implementation. */

#include <stdio.h>
#include <stdlib.h>
#include "exact.h"
#include "util.h"

/****************************************************** Internal structures. */

struct exact_rchead_struct;

typedef struct exact_lrudrc_struct 
{
    struct exact_lrudrc_struct *left;
    struct exact_lrudrc_struct *right;
    struct exact_lrudrc_struct *up;
    struct exact_lrudrc_struct *down;
    struct exact_rchead_struct *row;
    struct exact_rchead_struct *col;
} exact_lrudrc_t;

typedef struct exact_rchead_struct
{
    exact_lrudrc_t          links;
    int                     id;
    int                     count;
    int                     b;
} exact_rchead_t;

struct exact_struct
{
    /* Root element of the dancing link data structure. */
    exact_rchead_t          root;

    /* Memory management for the dancing link data structure. */
    exact_chunk_t *         lrudrc_chunk;
    exact_chunk_t *         rchead_chunk;

    /* Key--value maps for elements of the data structure. */
    exact_map_t *           rowid_to_rowhead;
    exact_map_t *           colid_to_colhead;
    exact_map_t *           entry_to_lrudrc;

    /* Counts etc. */
    int                     num_cols;
    int                     num_rows;
    int                     sum_b;  /* Sum of the entries b_i in the vector b.
                                     * Used as an upper bound for
                                     * the size of any solution. */
    int                     simple; /* Nonzero if b_i == 1 for all i, 
                                     * or equivalently, every row must
                                     * be covered exactly once. */

    /* Modes of operation. */
    int                     can_declare;
    int                     iteration_in_progress;
    int                     lock;

    /* The search stack. */
    int                     level;
    int *                   soln_stack;
    int                     soln_stack_capacity;
    int                     num_push;
    exact_lrudrc_t **       col_stack;
    int *                   filter_pos;
    exact_lrudrc_t **       filter_stack;
    int                     filter_stack_capacity;

    /* Cache for a row with the minimum branching factor. */
    exact_rchead_t *        cache;
    int                     cache_branching_factor;

    /* Search control functions and their user parameters. */
    exact_level_t *         level_f;
    void *                  level_p;
    exact_filter_t *        filter_f;
    void *                  filter_p;
};

/************************************************ Link/unlink/relink macros. */

#define lrreset(entry) \
{ (entry)->left        = entry; \
  (entry)->right       = entry; }

/* Unused macro
#define lrlinkright(to, entry) \
{ (entry)->right       = (to)->right; \
  (entry)->left        = to; \
  (to)->right->left    = entry; \
  (to)->right          = entry; }
*/

#define lrlinkleft(to, entry) \
{ (entry)->left        = (to)->left; \
  (entry)->right       = to; \
  (to)->left->right    = entry; \
  (to)->left           = entry; }

#define lrunlink(entry) \
{ (entry)->left->right = (entry)->right; \
  (entry)->right->left = (entry)->left; }

#define lrrelink(entry) \
{ (entry)->left->right = entry; \
  (entry)->right->left = entry; }

#define udreset(entry) \
{ (entry)->up          = entry; \
  (entry)->down        = entry; }

/* Unused macro
#define udlinkdown(to, entry) \
{ (entry)->down        = (to)->down; \
  (entry)->up          = to; \
  (to)->down->up       = entry; \
  (to)->down           = entry; }
*/

#define udlinkup(to, entry) \
{ (entry)->up          = (to)->up; \
  (entry)->down        = to; \
  (to)->up->down       = entry; \
  (to)->up             = entry; }

#define udunlink(entry) \
{ (entry)->up->down    = (entry)->down; \
  (entry)->down->up    = (entry)->up; }

#define udrelink(entry) \
{  (entry)->up->down   = entry; \
   (entry)->down->up   = entry; }

/******************** Structures & comparison functions for key--value maps. */

static int int_cmp(const void *a, const void *b)
{
    const int *aa = (const int *) a;
    const int *bb = (const int *) b;
    return (*aa > *bb) - (*aa < *bb);
}

typedef struct exact_rcid_struct
{
    int   row;
    int   col;
} exact_rcid_t;

static int rcid_cmp(const void *a, const void *b)
{
    const exact_rcid_t *aa = (const exact_rcid_t *) a;
    const exact_rcid_t *bb = (const exact_rcid_t *) b;
    return 2*((aa->row > bb->row) - (aa->row < bb->row)) +
              (aa->col > bb->col) - (aa->col < bb->col);
}

/************************************* Initialization and release functions. */

static void enlarge_soln_stack(exact_t *e, int size)
{
    EXACT_FREE(e->soln_stack);
    EXACT_FREE(e->col_stack);
    EXACT_FREE(e->filter_pos);

    e->soln_stack          = EXACT_ALLOC(sizeof(int)*size);
    e->col_stack           = EXACT_ALLOC(sizeof(exact_lrudrc_t *)*size);
    e->filter_pos          = EXACT_ALLOC(sizeof(int)*(size+1));
    e->soln_stack_capacity = size;
}

static void enlarge_filter_stack(exact_t *e, int size)
{
   EXACT_FREE(e->filter_stack);
   e->filter_stack          = EXACT_ALLOC(sizeof(exact_lrudrc_t *)*size);
   e->filter_stack_capacity = size;
}

#define SOLN_STACK_START_SIZE 128
#define FILTER_STACK_START_SIZE 128
#define EXACT_CHUNK_SIZE 256

static void exact_init(exact_t *e)
{
    e->num_cols              = 0;
    e->num_rows              = 0;
    e->num_push              = 0;
    e->sum_b                 = 0;
    e->simple                = 1;
    e->soln_stack_capacity   = SOLN_STACK_START_SIZE;
    e->soln_stack            = EXACT_ALLOC(sizeof(int)*e->soln_stack_capacity);
    e->col_stack             = EXACT_ALLOC(sizeof(exact_lrudrc_t *)*
                                           e->soln_stack_capacity);
    e->filter_pos            = EXACT_ALLOC(sizeof(int)*
                                           (e->soln_stack_capacity+1));
    e->filter_stack_capacity = FILTER_STACK_START_SIZE;
    e->filter_stack          = EXACT_ALLOC(sizeof(exact_lrudrc_t *)*
                                           e->filter_stack_capacity);
    e->lrudrc_chunk          = exact_chunk_alloc(sizeof(exact_lrudrc_t), 
                                                 EXACT_CHUNK_SIZE);
    e->rchead_chunk          = exact_chunk_alloc(sizeof(exact_rchead_t), 
                                                 EXACT_CHUNK_SIZE);
    e->rowid_to_rowhead      = exact_map_alloc(sizeof(int), &int_cmp);
    e->colid_to_colhead      = exact_map_alloc(sizeof(int), &int_cmp);  
    e->entry_to_lrudrc       = exact_map_alloc(sizeof(exact_rcid_t), 
                                               &rcid_cmp);
    e->can_declare           = 1;
    e->iteration_in_progress = 0;
    e->lock                  = 0;

    e->level_f               = NULL;
    e->level_p               = NULL;
    e->filter_f              = NULL;
    e->filter_p              = NULL;

    exact_lrudrc_t *l        = &e->root.links;
    l->row                   = &e->root;
    l->col                   = &e->root;
    lrreset(l);
    udreset(l);
}

static void exact_release(exact_t *e)
{
    exact_map_free(e->rowid_to_rowhead);
    exact_map_free(e->colid_to_colhead);
    exact_map_free(e->entry_to_lrudrc);
    exact_chunk_free(e->lrudrc_chunk);
    exact_chunk_free(e->rchead_chunk);
    EXACT_FREE(e->filter_stack);
    EXACT_FREE(e->filter_pos);
    EXACT_FREE(e->col_stack);
    EXACT_FREE(e->soln_stack);
}

exact_t *exact_alloc(void)
{
    exact_t *e = (exact_t *) EXACT_ALLOC(sizeof(exact_t));
    exact_init(e);
    return e;
}

void exact_free(exact_t *e)
{
    exact_release(e);
    EXACT_FREE(e);
}

/*********************************************************** Number of rows. */

int exact_num_rows(exact_t *e)
{
    return e->num_rows;
}

/******************************************************** Number of columns. */

int exact_num_cols(exact_t *e)
{
    return e->num_cols;
}

/*********************************** Check whether declarations are allowed. */

int exact_can_declare(exact_t *e)
{
    return e->can_declare;
}

/************************************************************ Declare a row. */

void exact_declare_row(exact_t *e, int i, int b)
{
    if(!e->can_declare)
        EXACT_ERROR("not in DECLARE mode, operation not permitted");
    if(exact_map_find(e->rowid_to_rowhead, &i))
        EXACT_ERROR("row already exists (i = %d)", i);
    if(b <= 0)
        EXACT_ERROR("parameter b must be positive (b = %d)", b);

    exact_rchead_t *rh = exact_chunk_get(e->rchead_chunk);
    rh->id             = i;
    rh->count          = 0;
    rh->b              = b;
    exact_map_associate(e->rowid_to_rowhead, rh);

    exact_lrudrc_t *r  = &rh->links;
    r->row             = rh;
    r->col             = &e->root;
    lrreset(r);

    exact_lrudrc_t *rl = &e->root.links;
    udlinkup(rl, r);

    e->num_rows++;
    e->sum_b  = e->sum_b + b;
    e->simple = e->simple && (b == 1);
    if(e->sum_b > e->soln_stack_capacity)
        enlarge_soln_stack(e, 2*e->sum_b);
}

/*********************************************** Check whether a row exists. */

int exact_is_row(exact_t *e, int i)
{
    return exact_map_find(e->rowid_to_rowhead, &i);
}

/********************************************************* Declare a column. */

void exact_declare_col(exact_t *e, int j, int u)
{
    if(!e->can_declare)
        EXACT_ERROR("not in DECLARE mode, operation not permitted");
    if(exact_map_find(e->colid_to_colhead, &j))
        EXACT_ERROR("column already exists (j = %d)", j);
    if(u <= 0)
        EXACT_ERROR("parameter u must be positive (u = %d)", u);

    exact_rchead_t *ch = exact_chunk_get(e->rchead_chunk);
    ch->id             = j;
    ch->count          = 0;
    ch->b              = u;
    exact_map_associate(e->colid_to_colhead, ch);

    exact_lrudrc_t *c  = &ch->links;
    c->row             = &e->root;
    c->col             = ch;
    udreset(c);

    exact_lrudrc_t *rl = &e->root.links;
    lrlinkleft(rl, c);

    e->num_cols++;
    if(e->num_cols > e->filter_stack_capacity)
        enlarge_filter_stack(e, 2*e->num_cols);
}

/******************************************** Check whether a column exists. */

int exact_is_col(exact_t *e, int j)
{
    return exact_map_find(e->colid_to_colhead, &j);
}

/****************************** Declare a 1-entry to the coefficient matrix. */

void exact_declare_entry(exact_t *e, int i, int j)
{
    if(!e->can_declare)
        EXACT_ERROR("not in DECLARE mode, operation not permitted");

    if(!exact_map_find(e->rowid_to_rowhead, &i))
        EXACT_ERROR("nonexistent row (i = %d)", i);
    exact_rchead_t *rh = exact_map_value(e->rowid_to_rowhead);

    if(!exact_map_find(e->colid_to_colhead, &j))
        EXACT_ERROR("nonexistent column (j = %d)", j);
    exact_rchead_t *ch = exact_map_value(e->colid_to_colhead);

    exact_rcid_t rcid = { i, j };
    if(exact_map_find(e->entry_to_lrudrc, &rcid))
        EXACT_ERROR("entry already exists (i = %d, j = %d)", i, j);

    rh->count++;
    ch->count++;
    exact_lrudrc_t *r = &rh->links;
    exact_lrudrc_t *c = &ch->links;
    exact_lrudrc_t *t = exact_chunk_get(e->lrudrc_chunk);
    t->row            = rh;
    t->col            = ch;
    lrlinkleft(r, t);
    udlinkup(c, t);

    exact_map_associate(e->entry_to_lrudrc, t);
}

/********************************************* Return the value of an entry. */

int exact_is_entry(exact_t *e, int i, int j)
{
    if(!exact_map_find(e->rowid_to_rowhead, &i))
        EXACT_ERROR("nonexistent row (i = %d)", i);
    if(!exact_map_find(e->colid_to_colhead, &j))
        EXACT_ERROR("nonexistent column (j = %d)", j);
    int k[2] = { i, j };
    if(exact_map_find(e->entry_to_lrudrc, k))
        return 1;
    return 0;
}

/*********************************** Output identifiers of unsatisfied rows. */

int exact_get_rows(exact_t *e, int *id)
{
  int n = 0;
  exact_lrudrc_t *r;
    for(r = e->root.links.down; 
        r != &e->root.links; 
        r = r->down) 
        id[n++] = r->row->id;
    return n;
}

/********************************** Output identifiers of available columns. */

int exact_get_cols(exact_t *e, int *id)
{
    int n = 0;
    exact_lrudrc_t *c;
    for(c = e->root.links.right; 
        c != &e->root.links; 
        c = c->right) 
        id[n++] = c->col->id;
    return n;
}

/*********** Subroutines for manipulating the "dancing link" data structure. */

/* 

        |       |             |       |             |       |
        |       |             |       |             |       |   
        |       |             |       |             |       |   
       up       |            up       |            up       |         
-------###########right------###########right------###########right--  COLUMN
       ##  ROOT ##           ##  col  ##           ##  col  ##          LIST
---left###########-------left###########-------left###########-------  
        |     down            |     down            |     down
        |       |             |       |             |       |
        |       |             |       |             |       |
        |       |             |       |             |       |
       up       |             |       |            up       |
-------###########right-------+-------+------------###########right--
       ##  row  ##            |       |            ## entry ##
---left###########------------+-------+--------left###########-------
        |     down            |       |             |     down
        |       |             |       |             |       |
        |       |             |       |             |       |
       up       |            up       |             |       |
-------###########right------###########right-------+-------+--------
       ##  row  ##           ## entry ##            |       |  
---left###########-------left###########------------+-------+--------
        |     down            |     down            |       |
        |       |             |       |             |       |

         ROW LIST

*/

/* Left--right unlink routine for a vertical interval. */

static int lrunlink_interval(exact_t *e, 
                             exact_lrudrc_t *a, 
                             const exact_lrudrc_t *b,
                             int skip)
{
    /* Start from a, go down until (but not including) b. */
  exact_lrudrc_t *d;
    for(d = a; d != b; d = d->down) {
        if(--d->row->count == 0) {
            /* The row of d is not covered, and 
             * has no intersecting columns available for covering it.
             * We can backtrack immediately. */
            skip = 1;
        }
        /* Update minimum branching row cache. */
        if(d->row->count < e->cache_branching_factor) {
            e->cache_branching_factor = d->row->count;
            e->cache                  = d->row;
        }
        lrunlink(d);
    }                                      
    return skip;
}


/* Left--right relink routine for a vertical interval. */

static void lrrelink_interval(exact_lrudrc_t *a, 
                              const exact_lrudrc_t *b)
{
    /* Start from a, go up until (but not including) b. */
  exact_lrudrc_t *d;
    for(d = a; d != b; d = d->up) {
        lrrelink(d);
        d->row->count++;
    }
}


/* Updates the data structure to reflect the fact that
 * the column of c was just pushed to the solution stack. */

static int push_col_update(exact_t *e, exact_lrudrc_t *c, int use_cache) 
{
    /* Initialize minimum branching row cache. */
    e->cache_branching_factor = use_cache ? (c->row->count + 1) : 0;
    e->cache                  = NULL;

    /* Initialize control variables. */
    int skip     = 0;                  /* Force backtrack upon return? */
    int unlink_c = (--c->col->b == 0); /* Unlink the column of c if 
                                        * the column bound is met. */

    /* Check whether any rows intersecting the column of c 
     * got covered as a result of the push. */

    /* Ensure that c is a column header. */
    c = &c->col->links;

    /* Traverse down the list at the column of c, 
     * starting from the entry below the column header. 
     * All entries except the column header are traversed. */
    exact_lrudrc_t *d;
    for(d = c->down; d != c ; d = d->down) {
        if(--d->row->b == 0) {
            /* The row of d just got covered. 
             * Proceed to unlink all columns intersecting the row of d. */

            /* In particular, remember to unlink the column of c
             * once this loop is done. */
            unlink_c = 1; 

            /* Unlink the row header of the row of d from the row list. */
            exact_lrudrc_t *rhd = &d->row->links;
            udunlink(rhd);
            /* Reminder: 
             * Do not decrease d->row->count in this loop, but do so
             * in the following loop (which will be executed since 
             * unlink_c == 1); otherwise the calls to lrunlink_interval 
             * in this loop can incorrectly force a backtrack. */

            /* Traverse right the list at the row of d.
             * All entries except the entry d are traversed, 
             * including the row header (rhd). */
            exact_lrudrc_t *dr;
            for(dr = d->right; dr != d; dr = dr->right) {
                if(dr != rhd) { /* Ignore the row header. */
                    /* Unlink the column of dr. */
                    exact_lrudrc_t *a = dr->down;
                    exact_lrudrc_t *b = &dr->col->links;
                    skip = lrunlink_interval(e, a, b, skip);
                    lrunlink(b); /* Unlink the column header. */
                    a = b->down;
                    b = dr;
                    skip = lrunlink_interval(e, a, b, skip);
                }                                                       
            }                                                           
        }                                                               
    }
    if(unlink_c) {                                                             
        /* Unlink the column of c. 
         * The entry c must be a column header. */
        lrunlink(c); /* Unlink header. */
        exact_lrudrc_t *d;
        for(d = c->down; d != c; d = d->down) {
            /* Caching is used only if all rows have b == 1. In that case,
             * the row d considered here is covered and should not be cached
             * even id d->row->count < e->cache_branching_factor.
             */ 
            d->row->count--;
            /* Reminder: 
             * Cannot backtrack here on d->row->count == 0 alone
             * because the row of d may be covered
             * (that is, d->row->b == 0).
             */
            lrunlink(d);                                                
        }
    }
    return skip;
}


/* Rewinds the data structure to reflect the fact that
 * the column of c was just popped off the solution stack. */

static void pop_col_update(exact_lrudrc_t *c)
{

    /* Ensure that c is a column header. */
    c = &c->col->links; 

    /* Relink the column of c if it was unlinked. */
    if(c->right->left != c) {
        /* Relink the column of c. 
         * The entry c must be a column header. */
      exact_lrudrc_t *d;
        for(d = c->up; d != c; d = d->up) {
            lrrelink(d);
            d->row->count++;
        }
        lrrelink(c); /* Relink header. */
    }

    /* Check whether any rows intersecting the column of c are uncovered
     * as a result of the pop. */

    /* Traverse up the list at the column of c, 
     * starting from the entry above the column header. 
     * All entries except the column header are traversed. */
    exact_lrudrc_t *d;
    for(d = c->up ; d != c ; d = d->up) {
        if(d->row->b == 0) {
            /* The row of d just got uncovered. 
             * Proceed to relink all columns intersecting the row of d. */

            /* Traverse left the list at the row of d.
             * All entries except the entry d are traversed, 
             * including the row header (rhd). */
            exact_lrudrc_t *rhd = &d->row->links;
            exact_lrudrc_t *dr;
            for(dr = d->left; dr != d; dr = dr->left) {
                if(dr != rhd) { /* Ignore row header. */
                    /* Relink the column of dr. */
                    exact_lrudrc_t *a = dr->up;
                    exact_lrudrc_t *b = &dr->col->links;
                    lrrelink_interval(a, b);
                    lrrelink(b); /* Relink the column header. */
                    a = b->up;
                    b = dr;
                    lrrelink_interval(a, b);
                }
            }
            /* Relink the row header of the row of d to the row list. */
            udrelink(rhd);
        }
        d->row->b++;
    }
    c->col->b++;
}
                                                                            
static int filter(exact_t *e, int lvl) 
{
    /* Evaluate filter function on candidate columns and
       unlink accordingly. */
    int skip = 0;
    int i    = e->filter_pos[lvl-1];

    /* Traverse right the column list.
     * All entries except the root are traversed. */
    exact_lrudrc_t *c;
    for(c = e->root.links.right;
        c != &e->root.links;
        c = c->right) {
        if(!e->filter_f(e->filter_p,
                        lvl,
                        e->soln_stack,
                        c->col->id)) {
            /* Unlink the column of c. */
            exact_lrudrc_t *a = c->down;
            exact_lrudrc_t *b = c;
            skip = lrunlink_interval(e, a, b, skip);
            lrunlink(c); /* Unlink the column header. */
            /* Save data for unfiltering. */
            e->filter_stack[i++] = c;
        }
    }
    /* Record the position for unfiltering. */
    e->filter_pos[lvl] = i;

    return skip;
}

static void unfilter(exact_t *e, int lvl) 
{
    /* Relink any columns unlinked by the filter function. */
    int i = e->filter_pos[lvl];
    while(i > e->filter_pos[lvl-1]) {
        exact_lrudrc_t *c = e->filter_stack[--i];
        /* Relink the column of c. */
        lrrelink(c); /* Relink the column header. */
        exact_lrudrc_t *a = c->up;
        exact_lrudrc_t *b = c;
        lrrelink_interval(a, b);
    }
}

/********************** Check whether forcing a partial solution is allowed. */

int exact_can_push(exact_t *e)
{
    if(e->iteration_in_progress)
        return 0;
    return 1;
}

/************************************************************ Push a column. */

void exact_push(exact_t *e, int j)
{
    if(e->iteration_in_progress)
        EXACT_ERROR("operation not permitted in ITERATE mode");

    /* Find the column. */
    if(!exact_map_find(e->colid_to_colhead, &j))
        EXACT_ERROR("nonexistent column (j = %d)", j);
    exact_rchead_t *ch = exact_map_value(e->colid_to_colhead);
    exact_lrudrc_t *c  = &ch->links;

    /* Check that column has not been unlinked. */
    if(c->left->right != c)
        EXACT_ERROR("cannot push an unlinked column (j = %d)", j);

    /* Check that the column is not empty. */
    if(ch->count == 0)
        EXACT_ERROR("pushing a column of zeroes (j = %d)", j);

    e->soln_stack[e->num_push++] = j;
    push_col_update(e, c, 0);
    e->can_declare = 0;
}

/************************************************************* Pop a column. */

void exact_pop(exact_t *e)
{
    if(e->iteration_in_progress)
        EXACT_ERROR("operation not permitted in ITERATE mode");
    if(e->num_push == 0)
        EXACT_ERROR("nothing to pop");
    int j = e->soln_stack[--e->num_push];

    /* Find the column. */
    exact_map_find(e->colid_to_colhead, &j);
    exact_rchead_t *ch = exact_map_value(e->colid_to_colhead);
    exact_lrudrc_t *c  = &ch->links;

    pop_col_update(c);
    if(e->num_push == 0)
        e->can_declare = 1;
}

/************************************************* Check validity of a push. */

int exact_pushable(exact_t *e, int j)
{
    if(e->iteration_in_progress)
        EXACT_ERROR("operation not permitted in ITERATE mode");

    /* Find the column. */
    if(!exact_map_find(e->colid_to_colhead, &j))
        EXACT_ERROR("nonexistent column (j = %d)", j);
    exact_rchead_t *ch = exact_map_value(e->colid_to_colhead);
    exact_lrudrc_t *c  = &ch->links;

    /* Check that column has not been unlinked. */
    if(c->left->right != c)
        return 0;

    /* Check that the column is not empty. */
    if(ch->count == 0)
        return 0;
    return 1;
}

/************************************************* Number of pushed columns. */

int exact_num_push(exact_t *e)
{
    return e->num_push;
}

/************************************* Output identifiers of pushed columns. */

int exact_get_push(exact_t *e, int *id)
{
    int n = e->num_push;
    int i;
    for(i = 0; i < n; i++) 
        id[i] = e->soln_stack[i];
    return n;
}

/******************************************************* Set level function. */

void exact_level(exact_t *e, exact_level_t *l, void *p)
{
    if(e->iteration_in_progress)
        EXACT_ERROR("operation not permitted in ITERATE mode");
    e->level_f = l;
    e->level_p = p;
}

/****************************************************** Set filter function. */

void exact_filter(exact_t *e, exact_filter_t *f, void *p)
{
    if(e->iteration_in_progress)
        EXACT_ERROR("operation not permitted in ITERATE mode");
    e->filter_f = f;
    e->filter_p = p;
}

/******************** Data structure consistency check (for test code only). */

void exact_check(exact_t *e);

void exact_check(exact_t *e)
{
    exact_lrudrc_t *d = &e->root.links;
    do {
        int s = 0;
        exact_lrudrc_t *r = d;
        do {
            if(r->left->right != r ||
               r->right->left != r)
                EXACT_INTERNAL_ERROR("invalid left/right link");
            if(r->row != d->row)
                EXACT_INTERNAL_ERROR("invalid row link");
            s++;
            int t = 0;
            exact_lrudrc_t *dr = r;
            do {
                if(dr->down->up != dr ||
                   dr->up->down != dr)
                    EXACT_INTERNAL_ERROR("invalid up/down link");
                if(dr->col != r->col)
                    EXACT_INTERNAL_ERROR("invalid column link");
                if(dr->row == &e->root)
                    t++;
                dr = dr->down;
            } while(dr != r);
            if(t != 1)
                EXACT_INTERNAL_ERROR("missing/repeated entry in column list");
            r = r->right;
        } while(r != d);
        if(d->row != &e->root && s != d->row->count + 1) {
            EXACT_INTERNAL_ERROR("invalid row count");
        }
        d = d->down;
    } while(d != &e->root.links);
}

/************************************************** Reset solution iterator. */

void exact_reset_solve(exact_t *e)
{    
    if(e->lock)
        EXACT_ERROR("re-entry while call on instance in progress");
    if(e->iteration_in_progress) {
        int lvl = e->level;
        while(lvl > e->num_push) {
            if(e->filter_f != NULL) 
                unfilter(e, lvl);
            lvl--;
            exact_lrudrc_t *c = e->col_stack[lvl];
            pop_col_update(c);
        }
        e->iteration_in_progress = 0;
        if(e->num_push == 0)
            e->can_declare = 1;     
    }
}

/******************************************************** Solution iterator. */

const int *exact_solve(exact_t *e, int *size)
{
    int lvl;

    if(e->lock)
        EXACT_ERROR("re-entry while call on instance in progress");
    e->lock = 1;

    if(e->iteration_in_progress) {
        lvl = e->level;
        goto lvl_down;
    }
    e->can_declare           = 0;
    e->iteration_in_progress = 1;
    lvl                      = e->num_push;
    e->filter_pos[lvl]       = 0;
    e->cache                 = NULL;
lvl_up:
    /* Call level function. */
    if(e->level_f != NULL)
        if(!e->level_f(e->level_p, lvl, e->soln_stack))
            goto lvl_down; 

    /* Report if solution found. */
    if(e->root.links.down == &e->root.links) {
        /* Solution found. Return to caller for reporting. */
        e->level = lvl;
        *size = lvl;
        e->lock = 0;
        return e->soln_stack;
        /* Will backtrack upon next entry to exact_solve. */
    }

    {
    exact_lrudrc_t *r, *c;  
    if(lvl <= e->num_push ||
       e->col_stack[lvl-1]->row->b == 0) {
        /* Select a new row to cover. */

        /* Selection heuristic: 
         * Select a row that leads to the minimum branching factor
         * for the search. Put otherwise, select a row with the
         * minimum number of columns currently intersecting it.
         */

        /* See if we have such a row cached.
         * Note: must check that the cached row has not been unlinked
         *       after it has been cached; an unlinked row has
         *       e->cache->b == 0. 
         */
        if(e->cache != NULL && e->cache->b != 0) {
            /* Yes. Use the cached row. */
            r = &e->cache->links;
        } else {
            /* No. Find a row with the minimum branching factor. */
            int record_count = e->num_cols + 1;
            r = NULL;
            exact_lrudrc_t *t;
            for(t = e->root.links.down; 
                t != &e->root.links; 
                t = t->down) {

                int count = t->row->count;
                if(count < record_count) {
                    r = t;
                    record_count = count;
                }
            }
        }

        /* Start with the first column at this row. */
        c = r->right;
    } else {
        /* The row selected at a preceding level is not yet covered, 
         * proceed to cover it further. */

        /* Start with the first available column that is
         * not to the left of the column pushed at the previous level. 
         * Note: any columns to the left will be considered as the search
         *       branches at the preceding level(s).
         */
        c = e->col_stack[lvl-1];
        while(c->right->left != c) {
            /* This is an unlinked column, 
             * proceed to the next column on the right. */
            c = c->right; /* Left--right links in an unlinked column 
                           * can still be traversed. This however
                           * may lead to traversing repeated 
                           * unlinked columns. */
            if(c == &c->row->links) {
                /* Row header encountered. 
                 * This row has no available columns, but
                 * the row is not yet covered; can backtrack. */
                goto lvl_down;
            }
        }
    }

    /* Branch the search: 
     * Push one column at a time until the row header is encountered. */
    for(; c != &c->row->links; c = c->right) {  
        /* Push the column of c and save backtrack data. */
        e->soln_stack[lvl] = c->col->id;
        e->col_stack[lvl] = c;
        /* Update data structure. */
        if(push_col_update(e, c, e->simple))
            goto lvl_skip; /* Update signals that we can backtrack. */
        lvl++;
        if(e->filter_f != NULL)
            if(filter(e, lvl))
                goto lvl_down;  /* Filtering signals that we can backtrack. */
        goto lvl_up; 
        /* Simulated recursive invocation here. */
    lvl_down:
        /* Backtrack and rewind data structure. */
        if(lvl == e->num_push) 
            goto finished;
        if(e->filter_f != NULL) 
            unfilter(e, lvl);
        lvl--;
        c = e->col_stack[lvl];
    lvl_skip:
        pop_col_update(c);
    }
    goto lvl_down;
    }
 finished:
    e->iteration_in_progress = 0;
    if(e->num_push == 0)
        e->can_declare = 1;
    e->lock = 0;
    return NULL;
}

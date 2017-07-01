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

/*********************************************** Internal utility functions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

int exact_mem_alloc_balance = 0;

void *exact_mem_alloc(size_t size)
{
    exact_mem_alloc_balance++;    
    void *p = malloc(size);
    if(p == NULL)
        EXACT_ERROR("malloc fails");
    return p;
}

void exact_mem_free(void *p)
{
    exact_mem_alloc_balance--;
    free(p);
}

struct exact_chunk_struct 
{
    int num_entries;
    int num_alloc;
    size_t entry_size;
    void *data;
    struct exact_chunk_struct *next;
};

exact_chunk_t *exact_chunk_alloc(size_t entry, int entries)
{
    exact_chunk_t *c = (exact_chunk_t *) EXACT_ALLOC(sizeof(exact_chunk_t));
    c->num_entries = entries;
    c->num_alloc = 0;
    c->entry_size = entry;
    c->data = EXACT_ALLOC(c->entry_size * c->num_entries);
    c->next = NULL;
    return c;
}

void exact_chunk_free(exact_chunk_t *c)
{
    while(c != NULL) {
        exact_chunk_t *t = c->next;
        EXACT_FREE(c->data);
        EXACT_FREE(c);
        c = t;
    }
}

void *exact_chunk_get(exact_chunk_t *c) 
{
    if(c->num_alloc >= c->num_entries) {
        exact_chunk_t *n = 
            (exact_chunk_t *) EXACT_ALLOC(sizeof(exact_chunk_t));
        n->num_entries = c->num_entries;
        n->num_alloc = c->num_alloc;
        n->entry_size = c->entry_size;
        n->data = c->data;
        n->next = c->next;
        c->next = n;
        c->num_entries = 2*c->num_entries;
        c->data = EXACT_ALLOC(c->entry_size * c->num_entries);
        c->num_alloc = 0;
    }
    return ((char *) c->data) + c->entry_size*c->num_alloc++;
}

void exact_error(const char *fn, int line, const char *func, 
                 const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, 
            "ERROR [detected by libexact interface; file = %s, line = %d]\n"
            "%s: ",
            fn,
            line,
            func);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();    
}

void exact_internal_error(const char *fn, int line, const char *func, 
                          const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, 
            "ERROR [libexact internal error; file = %s, line = %d]\n"
            "%s: ",
            fn,
            line,
            func);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();    
}

/******************** Map from keys to values -- implemented as an AVL tree. */

struct avl_node_struct;

typedef struct avl_node_struct 
{
    void *                     key;
    void *                     value;    
    struct avl_node_struct *   left;
    struct avl_node_struct *   right;
    int                        height;    
} avl_node_t;

struct exact_map_struct
{
    avl_node_t *               root;
    exact_chunk_t *            node_chunk;
    exact_chunk_t *            key_chunk;
    size_t                     key_size;
    exact_map_cmp_t *          cmp_f;

    void *                     active_key;
    void **                    active_value_p;
    int                        path_length;
    int                        path_capacity;
    avl_node_t ***             path;
};

#define AVL_CHUNK_SIZE 128
#define PATH_CAPACITY_START 32

static void enlarge_path(exact_map_t *m, int capacity)
{
    EXACT_FREE(m->path);
    m->path_capacity = capacity;
    m->path          = EXACT_ALLOC(sizeof(avl_node_t **)*m->path_capacity);
}

static void exact_map_init(exact_map_t *m, size_t key, exact_map_cmp_t *f)
{
    m->key_size        = key;
    m->root            = NULL;
    m->active_value_p  = NULL;
    m->node_chunk      = exact_chunk_alloc(sizeof(avl_node_t), AVL_CHUNK_SIZE);
    m->key_chunk       = exact_chunk_alloc(m->key_size, AVL_CHUNK_SIZE);
    m->path_capacity   = PATH_CAPACITY_START;
    m->path            = EXACT_ALLOC(sizeof(avl_node_t **)*m->path_capacity);
    m->path_length     = 0;
    m->active_key      = EXACT_ALLOC(m->key_size);
    m->cmp_f           = f;
}

static void exact_map_release(exact_map_t *m)
{
    EXACT_FREE(m->active_key);
    EXACT_FREE(m->path);    
    exact_chunk_free(m->key_chunk);
    exact_chunk_free(m->node_chunk);    
}

exact_map_t *exact_map_alloc(size_t key, exact_map_cmp_t *f)
{
    exact_map_t *m = (exact_map_t *) EXACT_ALLOC(sizeof(exact_map_t));
    exact_map_init(m, key, f);
    return m;
}

void exact_map_free(exact_map_t *m)
{
    exact_map_release(m);
    EXACT_FREE(m);
}

static int height(avl_node_t *n)
{
    if(n == NULL)
        return -1;
    return n->height;
}

int exact_map_find(exact_map_t *m, void *key)
{
    memcpy(m->active_key, key, m->key_size);
    avl_node_t *n = m->root;
    int t = height(n)+2;
    if(t > m->path_capacity)
        enlarge_path(m, 2*t);
    m->path_length = 1;
    m->path[0] = &m->root;
    while(n != NULL) {
        t = m->cmp_f(n->key, m->active_key);
        if(t == 0) {
            m->active_value_p = &n->value;
            return 1;
        }
        if(t > 0) {
            m->path[m->path_length++] = &n->left;
            n = n->left;
        } else {
            m->path[m->path_length++] = &n->right;
            n = n->right;
        }
    }
    m->active_value_p = NULL;
    return 0;
}

void *exact_map_value(exact_map_t *m)
{
    void **av = m->active_value_p;
    if(av == NULL)
        EXACT_INTERNAL_ERROR("no value available");
    return *av;
}

static int max(int a, int b)
{
    return a > b ? a : b;
}

/*  AVL outer rotation:
 *
 *            d                              b
 *           / \       <--- left ---        / \
 *          b   E                          A   d
 *         / \          --- right -->         / \
 *        A   C                              C   E
 *
 */

static avl_node_t *rotate_outer_right(avl_node_t *d)
{
    avl_node_t *b = d->left;
    d->left       = b->right;
    b->right      = d;

    d->height = max(height(d->left), height(d->right)) + 1;
    b->height = max(height(b->left), height(b->right)) + 1;

    return b;
}

static avl_node_t *rotate_outer_left(avl_node_t *b)
{
    avl_node_t *d = b->right;
    b->right      = d->left;
    d->left       = b;

    b->height = max(height(b->left), height(b->right)) + 1;
    d->height = max(height(d->left), height(d->right)) + 1;

    return d;
}

/*  AVL inner rotation right:
 *
 *        f                         f                          d
 *       / \                       / \                        / \
 *      b   G   --- b left -->    d   G    -- f right -->    /   \ 
 *     / \                       / \                        b     f
 *    A   d                     b   E                      / \   / \
 *       / \                   / \                        A   C E   G
 *      C   E                 A   C
 *
 */

static avl_node_t *rotate_inner_right(avl_node_t *f)
{
    f->left = rotate_outer_left(f->left);
    return rotate_outer_right(f);
}

/*  AVL inner rotation left:
 *
 *     b                           b                           d
 *    / \                         / \                         / \
 *   A   f     -- f right -->    A   d     --- b left -->    /   \ 
 *      / \                         / \                     b     f
 *     d   G                       C   f                   / \   / \
 *    / \                             / \                 A   C E   G
 *   C   E                           E   G
 *
 */

static avl_node_t *rotate_inner_left(avl_node_t *b)
{
    b->right = rotate_outer_right(b->right);
    return rotate_outer_left(b);
}

void exact_map_associate(exact_map_t *m, void *v)
{
    void **av = m->active_value_p;
    if(av != NULL) {
        *av = v;
        return;
    }
    if(m->path_length == 0)
        EXACT_INTERNAL_ERROR("no query issued");

    avl_node_t *n     = exact_chunk_get(m->node_chunk);
    n->left           = NULL;
    n->right          = NULL;
    n->height         = 0;
    n->key            = exact_chunk_get(m->key_chunk);
    n->value          = v;
    memcpy(n->key, m->active_key, m->key_size);

    m->active_value_p = &n->value;
    avl_node_t **p    = m->path[--m->path_length];
    *p                = n;

    while(m->path_length > 0) {
        p              = m->path[--m->path_length];
        avl_node_t *nn = n;
        n              = *p;
        /* A child of n was updated to the value nn. */
        if(n->left == nn) {
            /* Left child of n was updated. */
            if(height(n->left) - height(n->right) == 2) {
                /* Left subtree is too high after update, must rebalance. */
                if(height(n->left->left) + 1 == n->left->height) {
                    /* The cause is the left--left subtree. */
                    n = rotate_outer_right(n);
                } else {
                    /* The cause is the left--right subtree. */
                    n = rotate_inner_right(n);
                }
            }
        } else {
            /* Right child of n was updated. */
            if(height(n->right) - height(n->left) == 2) {
                /* Right child is too high after update, must rebalance. */
                if(height(n->right->right) + 1 == n->right->height) {
                    /* The cause is the right--right subtree. */
                    n = rotate_outer_left(n);
                } else {
                    /* The cause is the right--left subtree. */
                    n = rotate_inner_left(n);
                }
            }
        }
        n->height = max(height(n->left), 
                        height(n->right)) + 1;
        *p = n;
    }
}

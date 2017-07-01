/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <string.h>
#include <jni.h>
#include "exact.h"

/* This is a trivial JNI example where we use a native method
 * to return a new VM String. See the corresponding Java source
 * file located at:
 *
 *   apps/samples/hello-jni/project/src/com/example/hellojni/HelloJni.java
 */

#define UNDEF -1
static void solve_sudoku(int *a)
{
    exact_t *e = exact_alloc();

    /* Row--column */
    int r, v, c, s;
    for(r = 0; r < 9; r++)
        for(c = 0; c < 9; c++)
            exact_declare_row(e, 9*r+c, 1);
    /* Row--value */
    for(r = 0; r < 9; r++)
        for(v = 0; v < 9; v++)
            exact_declare_row(e, 81+9*r+v, 1);
    /* Column--value */
    for(c = 0; c < 9; c++)
        for(v = 0; v < 9; v++)
            exact_declare_row(e, 162+9*c+v, 1);
    /* Subsquare--value */
    for(s = 0; s < 9; s++)
        for(v = 0; v < 9; v++)
            exact_declare_row(e, 243+9*s+v, 1); 
    /* Row--column--value */
    for(r = 0; r < 9; r++) {
        for(c = 0; c < 9; c++) {
            s = 3*(r/3)+c/3;
            for(v = 0; v < 9; v++) {
                exact_declare_col(e, 9*9*r+9*c+v, 1);

                /* Row--column */
                exact_declare_entry(e, 9*r+c, 9*9*r+9*c+v); 

                /* Row--value */
                exact_declare_entry(e, 81+9*r+v, 9*9*r+9*c+v); 

                /* Column--value */
                exact_declare_entry(e, 162+9*c+v, 9*9*r+9*c+v); 

                /* Subsquare--value */
                exact_declare_entry(e, 243+9*s+v, 9*9*r+9*c+v); 
            }
        }
    }

    /* Push the columns corresponding to the given partial solution. */
    for(r = 0; r < 9; r++) {
        for(c = 0; c < 9; c++) {
            if(a[9*r+c] != UNDEF) {
                int j = 9*9*r+9*c+a[9*r+c];
                if(!exact_pushable(e, j)) {
                    /* The partial solution is conflicting. */
                  a[0]=100;
                    goto solve_done;
                }
                exact_push(e, j);
            }
        }
    }

    /* List all complete solutions. */
    int n, i;
    const int *b;
    while((b = exact_solve(e, &n)) != NULL) {
        /* Put the solution into matrix form. */
        for(i = 0; i < n; i++) {
            int r = b[i]/81;
            int c = b[i]/9; c = c%9;
            int v = b[i]%9;
            a[9*r+c] = v;
        }
        return; /*added by ashik- since i need only one soltion */
        /* Rewind the matrix. */
        /* for(int i = exact_num_push(e); i < n; i++) { */
        /*     int r = b[i]/81; */
        /*     int c = b[i]/9; c = c%9; */
        /*     a[9*r+c] = UNDEF; */
        /* } */
    }   
solve_done:
    exact_free(e);
}

jintArray
Java_com_oldestmonk_droidsudsolve_MainActivity_exactcaller( JNIEnv* env,
                                                            jobject thiz,
                                                            jintArray jproblem )
{
  jintArray result;
  int size = 81;
  jint cproblem[81];
  (*env)->GetIntArrayRegion(env, jproblem, 0, size, cproblem);
  result = (*env)->NewIntArray(env, size);
  if (result == NULL) {
    return NULL; /* out of memory error thrown */
  }

  int a[81];
  int i;
  for(i=0; i<81; i++) {
    a[i] = cproblem[i]-1;
  }
  solve_sudoku(a);
  for(i=0; i<81; i++) {
    cproblem[i] = a[i]+1;
  }

  // fill a temp structure to use to populate the java int array
  jint fill[81];
  for (i = 0; i < size; i++) {
    fill[i] = cproblem[i]; // put whatever logic you want.
  }
  // move from the temp structure to the java structure
  (*env)->SetIntArrayRegion(env, result, 0, size, fill);
  return result;
}

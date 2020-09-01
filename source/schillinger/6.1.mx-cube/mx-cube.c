/*
 * This file is part of the Schillinger Max Package, found at
 * https://github.com/rybenmensch/schillinger. Copyright (c) 2020 Manolo Müller.
 *
 * The Schillinger Max Package is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ext.h"
#include "ext_obex.h"

typedef struct _schillinger {
    long p_len;
    long sq_len;
    long c_len;
    long s_len;
    long s2_len;
    t_atom_long *polynom;
    t_atom_long *square;
    t_atom_long *cube;
    t_atom_long *sync;
    t_atom_long *sync2;
} t_schillinger;

typedef struct _mx_cube {
    t_object p_ob;
    t_schillinger t;
    void *square_out;
    void *cube_out;
    void *sync_out;
    void *sync2_out;
} t_mx_cube;

void *mx_cube_new(t_symbol *s, long argc, t_atom *argv);
void mx_cube_free(t_mx_cube *x);
void mx_cube_pat(t_mx_cube *x, t_symbol *s, long argc, t_atom *argv);
void mx_cube_assist(t_mx_cube *x, void *b, long m, long a, char *s);
void mx_cube_bang(t_mx_cube *x);

void print(t_mx_cube *x);

t_class *mx_cube_class;

void ext_main(void *r)
{
    t_class *c;

    c = class_new("mx-cube", (method)mx_cube_new, (method)mx_cube_free,
                  sizeof(t_mx_cube), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_cube_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mx_cube_bang, "bang", 0);
    class_addmethod(c, (method)mx_cube_pat, "pat", A_GIMME, 0);

    class_register(CLASS_BOX, c);
    mx_cube_class = c;
}

void *mx_cube_new(t_symbol *s, long argc, t_atom *argv)
{
    // allocate class
    t_mx_cube *x = (t_mx_cube *)object_alloc(mx_cube_class);

    x->sync2_out = outlet_new((t_object *)x, NULL);
    x->sync_out = outlet_new((t_object *)x, NULL);
    x->cube_out = outlet_new((t_object *)x, NULL);

    x->t.polynom = NULL;
    x->t.square = NULL;
    x->t.cube = NULL;
    x->t.sync = NULL;
    x->t.sync2 = NULL;
    return (x);
}

void mx_cube_free(t_mx_cube *x)
{
    if (x->t.polynom) sysmem_freeptr(x->t.polynom);
    if (x->t.square) sysmem_freeptr(x->t.square);
    if (x->t.cube) sysmem_freeptr(x->t.cube);
    if (x->t.sync) sysmem_freeptr(x->t.sync);
    if (x->t.sync2) sysmem_freeptr(x->t.sync2);
}

void mx_cube_assist(t_mx_cube *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                sprintf(s, "(pat) Pattern to be cubed");
                break;
        }
    } else {
        switch (a) {
            case 0:
                sprintf(s, "Cubed resultant pattern");
                break;
            case 1:
                sprintf(s, "Square synced to cube pattern");
                break;
            case 2:
                sprintf(s, "Input pattern synced to cube pattern");
                break;
        }
    }
}

void mx_cube_bang(t_mx_cube *x)
{
    if (!x->t.cube) {
        post("No pattern received yet.");
        return;
    }

    print(x);
}

void mx_cube_pat(t_mx_cube *x, t_symbol *s, long argc, t_atom *argv)
{
    if (!argc)  // no arguments, do nothing and exit
        return;

    x->t.p_len = argc;
    x->t.sq_len = pow(argc, 2);
    x->t.c_len = pow(argc, 3);
    x->t.s_len = pow(argc, 2);
    x->t.s2_len = argc;

    if (x->t.cube) sysmem_freeptr(x->t.cube);
    x->t.cube =
        (t_atom_long *)sysmem_newptrclear(x->t.c_len * sizeof(t_atom_long));

    if (x->t.polynom) sysmem_freeptr(x->t.polynom);
    x->t.polynom =
        (t_atom_long *)sysmem_newptrclear(x->t.p_len * sizeof(t_atom_long));

    if (x->t.square) sysmem_freeptr(x->t.square);
    x->t.square =
        (t_atom_long *)sysmem_newptrclear(x->t.sq_len * sizeof(t_atom_long));

    if (x->t.sync) sysmem_freeptr(x->t.sync);
    x->t.sync =
        (t_atom_long *)sysmem_newptrclear(x->t.s_len * sizeof(t_atom_long));

    if (x->t.sync2) sysmem_freeptr(x->t.sync2);
    x->t.sync2 =
        (t_atom_long *)sysmem_newptrclear(x->t.s2_len * sizeof(t_atom_long));

    long arg_sum = 0;
    for (int i = 0; i < x->t.p_len; i++) {
        x->t.polynom[i] = atom_getlong(argv + i);
        arg_sum += x->t.polynom[i];
    }

    int resultcount = 0;
    long arg_square = arg_sum * arg_sum;

    for (int i = 0; i < x->t.p_len; i++) {
        for (int j = 0; j < x->t.p_len; j++) {
            x->t.square[resultcount++] = x->t.polynom[i] * x->t.polynom[j];
        }
        x->t.sync2[i] = arg_square * x->t.polynom[i];
    }

    for (int i = 0; i < x->t.sq_len; i++) {
        x->t.sync[i] = arg_sum * x->t.square[i];
    }

    resultcount = 0;

    for (int i = 0; i < x->t.p_len; i++) {
        for (int j = 0; j < x->t.sq_len; j++) {
            x->t.cube[resultcount++] = x->t.polynom[i] * x->t.square[j];
        }
    }

    print(x);
}

void print(t_mx_cube *x)
{
    t_atom square[x->t.sq_len];
    t_atom cube[x->t.c_len];
    t_atom sync[x->t.sq_len];
    t_atom sync2[x->t.p_len];

    for (int i = 0; i < x->t.c_len; i++) {
        atom_setlong(cube + i, x->t.cube[i]);
    }

    for (int i = 0; i < x->t.sq_len; i++) {
        atom_setlong(square + i, x->t.square[i]);
        atom_setlong(sync + i, x->t.sync[i]);
    }

    for (int i = 0; i < x->t.s2_len; i++) {
        atom_setlong(sync2 + i, x->t.sync2[i]);
    }

    outlet_anything(x->cube_out, gensym("pat"), x->t.c_len, cube);
    outlet_anything(x->sync_out, gensym("pat"), x->t.s_len, sync);
    outlet_anything(x->sync2_out, gensym("pat"), x->t.s2_len, sync2);
}

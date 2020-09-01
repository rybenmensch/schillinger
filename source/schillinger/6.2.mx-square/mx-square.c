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
    long steps;
    t_atom_long *polynom;  // pattern from pat msg (user supplied)
    t_atom_long *result;
    t_atom_long *sync;
    long p_len;
    long arg_sum;
} t_schillinger;

typedef struct _mx_square {
    t_object p_ob;
    t_schillinger t;
    void *pat_out;
    void *sync_out;
} t_mx_square;

void *mx_square_new(t_symbol *s, long argc, t_atom *argv);
void mx_square_free(t_mx_square *x);
void mx_square_pat(t_mx_square *x, t_symbol *s, long argc, t_atom *argv);
void mx_square_assist(t_mx_square *x, void *b, long m, long a, char *s);
void mx_square_bang(t_mx_square *x);

void print(t_mx_square *x);

t_class *mx_square_class;

void ext_main(void *r)
{
    t_class *c;

    c = class_new("mx-square", (method)mx_square_new, (method)mx_square_free,
                  sizeof(t_mx_square), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_square_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mx_square_bang, "bang", 0);
    class_addmethod(c, (method)mx_square_pat, "pat", A_GIMME, 0);

    class_register(CLASS_BOX, c);
    mx_square_class = c;
}

void *mx_square_new(t_symbol *s, long argc, t_atom *argv)
{
    // allocate class
    t_mx_square *x = (t_mx_square *)object_alloc(mx_square_class);

    x->sync_out = outlet_new((t_object *)x, NULL);
    x->pat_out = outlet_new((t_object *)x, NULL);

    x->t.steps = 1;  // init to one, lest we get divide by zero error later on
    x->t.arg_sum = 0;
    x->t.polynom = NULL;
    x->t.sync = NULL;
    x->t.result = NULL;
    return (x);
}

void mx_square_free(t_mx_square *x)
{
    if (x->t.polynom) sysmem_freeptr(x->t.polynom);

    if (x->t.sync) sysmem_freeptr(x->t.sync);
    if (x->t.result) sysmem_freeptr(x->t.result);
}

void mx_square_assist(t_mx_square *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                sprintf(s, "(pat) Pattern to be squared");
                break;
        }
    } else {
        switch (a) {
            case 0:
                sprintf(s, "Squared resultant pattern");
                break;
            case 1:
                sprintf(s, "Sync pattern");
                break;
        }
    }
}

void mx_square_bang(t_mx_square *x)
{
    if (!x->t.result) {
        post("No pattern received yet.");
        return;
    }

    print(x);
}

void mx_square_pat(t_mx_square *x, t_symbol *s, long argc, t_atom *argv)
{
    if (!argc)  // no arguments, do nothing and exit
        return;

    if (x->t.polynom) sysmem_freeptr(x->t.polynom);
    x->t.polynom =
        (t_atom_long *)sysmem_newptrclear(argc * sizeof(t_atom_long));

    x->t.arg_sum = 0;
    for (int i = 0; i < argc; i++) {
        x->t.polynom[i] = atom_getlong(argv + i);
        x->t.arg_sum += x->t.polynom[i];
    }

    long power = 2;
    long a_pow = pow(argc, power);
    x->t.steps = a_pow;
    x->t.p_len = argc;

    if (x->t.result) sysmem_freeptr(x->t.result);
    x->t.result =
        (t_atom_long *)sysmem_newptrclear(x->t.steps * sizeof(t_atom_long));

    if (x->t.sync) sysmem_freeptr(x->t.sync);
    x->t.sync = (t_atom_long *)sysmem_newptrclear(argc * sizeof(t_atom_long));

    int resultcount = 0;
    for (int i = 0; i < argc; i++) {
        for (int j = 0; j < argc; j++) {
            x->t.result[resultcount++] = x->t.polynom[i] * x->t.polynom[j];
        }
        x->t.sync[i] = x->t.arg_sum * x->t.polynom[i];
    }

    // PRINTING
    print(x);
}

void print(t_mx_square *x)
{
    t_atom result[x->t.steps];
    t_atom sync[x->t.p_len];

    for (int i = 0; i < x->t.steps; i++) {
        atom_setlong(result + i, x->t.result[i]);
    }
    for (int i = 0; i < x->t.p_len; i++) {
        atom_setlong(sync + i, x->t.sync[i]);
    }

    outlet_anything(x->pat_out, gensym("pat"), x->t.steps, result);
    outlet_anything(x->sync_out, gensym("pat"), x->t.p_len, sync);
}

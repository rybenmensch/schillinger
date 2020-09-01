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
#include <stdarg.h>

typedef struct _schillinger {
    long a;
    long b;
    long b_amt;
    t_ptr r_pat;
    t_ptr a_pat;
    t_ptr *b_pat;
    long steps;
    long steps_b;
} t_schillinger;

typedef struct _mxp2_nsg {
    t_object p_ob;
    t_schillinger t;
    void *step_out;
    void *r_out;
    void *a_out;
    void *b_out;
    char *out_names[4];
} t_mxp2_nsg;

void *mxp2_nsg_new(t_symbol *s, long argc, t_atom *argv);
void mxp2_nsg_free(t_mxp2_nsg *x);
void mxp2_nsg_gen(t_mxp2_nsg *x, long a, long b);
void mxp2_nsg_assist(t_mxp2_nsg *x, void *b, long m, long a, char *s);
void mxp2_nsg_bang(t_mxp2_nsg *x);
void outlet_s(void *outlet, char *selector, int argc, char *msg, ...);
void mx_outlet(t_mxp2_nsg *x, char *pre, int a, int b, int c);

t_class *mxp2_nsg_class;

void ext_main(void *r)
{
    t_class *c;

    c = class_new("mxp2", (method)mxp2_nsg_new, (method)mxp2_nsg_free,
                  sizeof(t_mxp2_nsg), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mxp2_nsg_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mxp2_nsg_bang, "bang", 0);
    class_addmethod(c, (method)mxp2_nsg_gen, "gen", A_LONG, A_LONG, 0);

    class_register(CLASS_BOX, c);
    mxp2_nsg_class = c;
}

void *mxp2_nsg_new(t_symbol *s, long argc, t_atom *argv)
{
    // allocate class
    t_mxp2_nsg *x = (t_mxp2_nsg *)object_alloc(mxp2_nsg_class);

    x->step_out = outlet_new((t_object *)x, NULL);
    x->b_out = outlet_new((t_object *)x, NULL);
    x->a_out = outlet_new((t_object *)x, NULL);
    x->r_out = outlet_new((t_object *)x, NULL);

    x->out_names[0] = "r";
    x->out_names[1] = "a";
    x->out_names[2] = "b";
    x->out_names[3] = "stp";

    x->t.a = 0;
    x->t.b = 0;
    x->t.steps = 1;  // init to one, lest we get divide by zero error later on
    x->t.b_amt = 2;

    x->t.r_pat = sysmem_newptrclear(x->t.steps * sizeof(t_ptr));
    x->t.a_pat = sysmem_newptrclear(x->t.steps * sizeof(t_ptr));
    x->t.b_pat = (t_ptr *)sysmem_newptrclear(x->t.b_amt * sizeof(t_ptr *));

    for (int i = 0; i < x->t.b_amt; i++) {
        x->t.b_pat[i] = sysmem_newptrclear(x->t.steps * sizeof(t_ptr));
    }

    if (argc == 2) {
        x->t.a = atom_getlong(argv);
        x->t.b = atom_getlong(argv + 1);
    }

    return (x);
}

void mxp2_nsg_free(t_mxp2_nsg *x)
{
    sysmem_freeptr(x->t.r_pat);
    sysmem_freeptr(x->t.a_pat);

    for (int i = 0; i < x->t.b_amt; i++) {
        sysmem_freeptr(x->t.b_pat[i]);
    }
    sysmem_freeptr(x->t.b_pat);
}

void mxp2_nsg_assist(t_mxp2_nsg *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                sprintf(s, "(gen) Generator pair");
                break;
        }
    } else {
        switch (a) {
            case 0:
                sprintf(s, "(patbin) r");
                break;
            case 1:
                sprintf(s, "(patbin) a");
                break;
            case 2:
                sprintf(s, "(patbin) b0-bn");
                break;
            case 3:
                sprintf(s, "To step visualiser");
                break;
        }
    }
}

void mxp2_nsg_bang(t_mxp2_nsg *x)
{
    if (x->t.a && x->t.b) {
        mxp2_nsg_gen(x, x->t.a, x->t.b);
    } else {
        post("No generator pair received yet!");
    }
}

void mxp2_nsg_gen(t_mxp2_nsg *x, long a, long b)
{
    // b may not be larger than a
    a = CLAMP(a, 1, 9);
    b = CLAMP(b, 1, a);

    x->t.a = a;
    x->t.b = b;

    x->t.steps = a * a;
    x->t.steps_b = a * b;

    long old_b_amt = x->t.b_amt;
    x->t.b_amt = a - b + 1;
    long newsize = (long)x->t.steps * sizeof(t_ptr);

    sysmem_freeptr(x->t.r_pat);
    x->t.r_pat = sysmem_newptrclear(newsize);

    sysmem_freeptr(x->t.a_pat);
    x->t.a_pat = sysmem_newptrclear(newsize);

    for (int i = 0; i < old_b_amt; i++) {
        sysmem_freeptr(x->t.b_pat[i]);
    }
    sysmem_freeptr(x->t.b_pat);
    x->t.b_pat = (t_ptr *)sysmem_newptrclear(x->t.b_amt * sizeof(t_ptr *));

    for (int i = 0; i < x->t.b_amt; i++) {
        x->t.b_pat[i] = sysmem_newptrclear(x->t.steps * sizeof(t_ptr));
    }

    for (int i = 0; i < 4; i++) {
        outlet_s(x->step_out, x->out_names[i], 1, "clear");
        outlet_s(x->step_out, x->out_names[i], 2, "rows", 1);
        outlet_s(x->step_out, x->out_names[i], 2, "columns", (int)x->t.steps);
    }
    outlet_s(x->step_out, "b", 2, "rows", x->t.b_amt);

    outlet_int(x->step_out, 1);
    outlet_int(x->step_out, x->t.steps);

    for (int i = 0; i < x->t.steps; i += a) {
        // a
        x->t.a_pat[i] = 1;
        mx_outlet(x, "a", i, 0, 1);

        // r
        x->t.r_pat[i] = 1;
        mx_outlet(x, "r", i, 0, 1);
    }

    for (int i = 0; i < x->t.steps_b; i += b) {
        for (int j = 0; j < x->t.b_amt; j++) {
            // b
            x->t.b_pat[j][i + (j * (int)a)] = 1;
            mx_outlet(x, "b", i + (j * (int)a), j, 1);
            // r
            x->t.r_pat[i + (j * (int)a)] = 1;
            mx_outlet(x, "r", i + (j * (int)a), 0, 1);
        }
    }

    // print out the patterns
    t_atom atom_r_pat[x->t.steps];
    t_atom atom_a_pat[x->t.steps];
    t_atom atom_b_pat[x->t.b_amt][x->t.steps];

    for (int i = 0; i < x->t.steps; i++) {
        atom_setlong(atom_r_pat + i, x->t.r_pat[i]);
        atom_setlong(atom_a_pat + i, x->t.a_pat[i]);
    }
    outlet_anything(x->r_out, gensym("patbin"), x->t.steps, atom_r_pat);
    outlet_anything(x->a_out, gensym("patbin"), x->t.steps, atom_a_pat);

    char buffer[3];
    for (int i = 0; i < x->t.b_amt; i++) {
        snprintf(buffer, 3, "b%d", i);
        atom_setsym(atom_b_pat[i], gensym("patbin"));
        for (int j = 1; j < x->t.steps + 1; j++) {
            atom_setlong(atom_b_pat[i] + j, x->t.b_pat[i][j - 1]);
        }
        outlet_anything(x->b_out, gensym(buffer), x->t.steps + 1,
                        atom_b_pat[i]);
    }
}

void mx_outlet(t_mxp2_nsg *x, char *pre, int a, int b, int c)
{
    t_atom argv[3];
    atom_setlong(argv, a);
    atom_setlong(argv + 1, b);
    atom_setlong(argv + 2, c);
    outlet_anything(x->step_out, gensym(pre), 3, argv);
}

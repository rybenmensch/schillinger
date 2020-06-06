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

#define R1 0
#define A1 1
#define B1 2
#define C1 3
#define R2 4
#define A2 5
#define B2 6
#define C2 7
#define CD 8
#define CP 9
#define STP 10

typedef struct _schillinger {
    long a;
    long b;
    long c;
    t_ptr *pat_list;
    long steps;
} t_schillinger;

typedef struct _mx_3g_nsg {
    t_object p_ob;
    t_schillinger t;
    void *step_out;
    void *outlet_list[8];
    char *out_names[9];
} t_mx_3g_nsg;

void *mx_3g_nsg_new(t_symbol *s, long argc, t_atom *argv);
void mx_3g_nsg_free(t_mx_3g_nsg *x);
void mx_3g_nsg_gen(t_mx_3g_nsg *x, long a, long b, long c);
void mx_3g_nsg_assist(t_mx_3g_nsg *x, void *b, long m, long a, char *s);
void mx_3g_nsg_bang(t_mx_3g_nsg *x);
void outlet_s(t_mx_3g_nsg *x, char *selector, int argc, char *msg, ...);
void mx_outlet(t_mx_3g_nsg *x, char *pre, int a, int b, int c);

t_class *mx_3g_nsg_class;

void ext_main(void *r)
{
    t_class *c;

    c = class_new("mx-3g", (method)mx_3g_nsg_new, (method)mx_3g_nsg_free,
                  sizeof(t_mx_3g_nsg), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_3g_nsg_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mx_3g_nsg_bang, "bang", 0);
    class_addmethod(c, (method)mx_3g_nsg_gen, "gen", A_LONG, A_LONG, A_LONG, 0);

    class_register(CLASS_BOX, c);
    mx_3g_nsg_class = c;
}

void *mx_3g_nsg_new(t_symbol *s, long argc, t_atom *argv)
{
    // allocate class
    t_mx_3g_nsg *x = (t_mx_3g_nsg *)object_alloc(mx_3g_nsg_class);

    x->step_out = outlet_new((t_object *)x, NULL);
    for (int i = 0; i < 8; i++) {
        x->outlet_list[7 - i] = outlet_new((t_object *)x, NULL);
    }

    x->out_names[0] = "r1";
    x->out_names[1] = "a1";
    x->out_names[2] = "b1";
    x->out_names[3] = "c1";
    x->out_names[4] = "r2";
    x->out_names[5] = "a2";
    x->out_names[6] = "b2";
    x->out_names[7] = "c2";
    x->out_names[8] = "stp";

    t_schillinger *p_s = &x->t;
    p_s->a = 0;
    p_s->b = 0;
    p_s->c = 0;
    p_s->steps = 1;  // init to one, lest we get divide by zero error later on

    p_s->pat_list = (t_ptr *)sysmem_newptrclear(8 * sizeof(t_ptr));
    for (int i = 0; i < 8; i++) {
        p_s->pat_list[i] = sysmem_newptr(p_s->steps * sizeof(t_ptr));
    }

    if (argc == 3) {
        p_s->a = atom_getlong(argv);
        p_s->b = atom_getlong(argv + 1);
        p_s->c = atom_getlong(argv + 2);
    }

    return (x);
}

void mx_3g_nsg_free(t_mx_3g_nsg *x)
{
    t_schillinger *p_s = &x->t;

    if (p_s->pat_list) {
        for (int i = 0; i < 8; i++) {
            sysmem_freeptr(p_s->pat_list[i]);
        }
        sysmem_freeptr(p_s->pat_list);
    }
}

void mx_3g_nsg_assist(t_mx_3g_nsg *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                sprintf(s, "(gen) Generator trio");
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
                sprintf(s, "(patbin) b");
                break;
            case 3:
                sprintf(s, "(patbin) c");
                break;
            case 4:
                sprintf(s, "(patbin) r'");
                break;
            case 5:
                sprintf(s, "(patbin) a'");
                break;
            case 6:
                sprintf(s, "(patbin) b'");
                break;
            case 7:
                sprintf(s, "(patbin) c'");
                break;
            case 8:
                sprintf(s, "To step visualiser");
                break;
        }
    }
}

void mx_3g_nsg_bang(t_mx_3g_nsg *x)
{
    t_schillinger *p_s = &(x->t);

    if (p_s->a && p_s->b && p_s->c) {
        mx_3g_nsg_gen(x, p_s->a, p_s->b, p_s->c);
    } else {
        post("No generator trio received yet!");
    }
}

void mx_3g_nsg_gen(t_mx_3g_nsg *x, long a, long b, long c)
{
    t_schillinger *p_s = &(x->t);
    p_s->steps = a * b * c;

    p_s->a = a;
    p_s->b = b;
    p_s->c = c;
    long newsize = (long)p_s->steps * sizeof(int);

    for (int i = 0; i < 8; i++) {
        sysmem_freeptr(p_s->pat_list[i]);
        p_s->pat_list[i] = sysmem_newptrclear(newsize);
    }

    for (int i = 0; i < 9; i++) {
        outlet_s(x, x->out_names[i], 1, "clear");
        outlet_s(x, x->out_names[i], 2, "rows", 1);
        outlet_s(x, x->out_names[i], 2, "columns", (int)p_s->steps);
    }

    outlet_int(x->step_out, 1);
    outlet_int(x->step_out, p_s->steps);

    for (int i = 0; i < p_s->steps; i += a) {
        // a
        p_s->pat_list[A1][i] = 1;
        mx_outlet(x, "a1", i, 0, 1);

        // r
        p_s->pat_list[R1][i] = 1;
        mx_outlet(x, "r1", i, 0, 1);
    }

    for (int i = 0; i < p_s->steps; i += b) {
        p_s->pat_list[B1][i] = 1;
        mx_outlet(x, "b1", i, 0, 1);

        p_s->pat_list[R1][i] = 1;
        mx_outlet(x, "r1", i, 0, 1);
    }

    for (int i = 0; i < p_s->steps; i += c) {
        p_s->pat_list[C1][i] = 1;
        mx_outlet(x, "c1", i, 0, 1);

        p_s->pat_list[R1][i] = 1;
        mx_outlet(x, "r1", i, 0, 1);
    }

    // COUNTERTHEME

    for (int i = 0; i < p_s->steps; i += (b * c)) {
        // a
        p_s->pat_list[A2][i] = 1;
        mx_outlet(x, "a2", i, 0, 1);

        // r
        p_s->pat_list[R2][i] = 1;
        mx_outlet(x, "r2", i, 0, 1);
    }

    for (int i = 0; i < p_s->steps; i += (a * c)) {
        p_s->pat_list[B2][i] = 1;
        mx_outlet(x, "b2", i, 0, 1);

        p_s->pat_list[R2][i] = 1;
        mx_outlet(x, "r2", i, 0, 1);
    }

    for (int i = 0; i < p_s->steps; i += (a * b)) {
        p_s->pat_list[C2][i] = 1;
        mx_outlet(x, "c2", i, 0, 1);

        p_s->pat_list[R2][i] = 1;
        mx_outlet(x, "r2", i, 0, 1);
    }

    // print out the patterns

    t_atom atom_pats[8][p_s->steps];

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < p_s->steps; j++) {
            atom_setlong(atom_pats[i] + j, p_s->pat_list[i][j]);
        }
        outlet_anything(x->outlet_list[i], gensym("patbin"), p_s->steps,
                        atom_pats[i]);
    }
}

void mx_outlet(t_mx_3g_nsg *x, char *pre, int a, int b, int c)
{
    t_atom argv[3];
    atom_setlong(argv, a);
    atom_setlong(argv + 1, b);
    atom_setlong(argv + 2, c);
    outlet_anything(x->step_out, gensym(pre), 3, argv);
}

void outlet_s(t_mx_3g_nsg *x, char *selector, int argc, char *msg, ...)
{
    // the first symbol message counts as argc as well
    t_atom argv[argc];
    int temp;
    va_list ap;
    va_start(ap, msg);
    atom_setsym(argv, gensym(msg));

    for (int i = 1; i < argc; i++) {
        temp = va_arg(ap, int);
        atom_setlong(argv + i, temp);
    }

    outlet_anything(x->step_out, gensym(selector), argc, argv);
    va_end(ap);
}

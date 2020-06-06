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
#include "z_dsp.h"
#include <stdarg.h>

// signal outlets (r pat, a pat, b pat, r' pat, a' pat, b' patcd, cp, stepnr)
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

typedef struct _mx_3g {
    t_pxobject p_ob;
    int counter;
    int step_prev;
    t_schillinger t;
    void *msg_out;
    char *out_names[9];
} t_mx_3g;

void *mx_3g_new(t_symbol *s, long argc, t_atom *argv);
void mx_3g_free(t_mx_3g *x);
void mx_3g_gen(t_mx_3g *x, long a, long b, long c);
void mx_3g_assist(t_mx_3g *x, void *b, long m, long a, char *s);
void mx_3g_bang(t_mx_3g *x);
void mx_3g_perform64(t_mx_3g *x, t_object *dsp64, double **ins, long numins,
                     double **outs, long numouts, long sampleframes, long flags,
                     void *userparam);
void mx_3g_dsp64(t_mx_3g *x, t_object *dsp64, short *count, double samplerate,
                 long maxvectorsize, long flags);
void outlet_s(t_mx_3g *x, char *selector, int argc, char *msg, ...);
void mx_outlet(t_mx_3g *x, char *pre, int a, int b, int c);

t_class *mx_3g_class;  // global pointer to the object class - so max can
                       // reference the object

void ext_main(void *r)
{
    t_class *c;

    c = class_new("mx-3g~", (method)mx_3g_new, (method)mx_3g_free,
                  sizeof(t_mx_3g), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_3g_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)mx_3g_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mx_3g_bang, "bang", 0);
    class_addmethod(c, (method)mx_3g_gen, "gen", A_LONG, A_LONG, A_LONG, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    mx_3g_class = c;
}

void *mx_3g_new(t_symbol *s, long argc, t_atom *argv)
{
    // allocate class
    t_mx_3g *x = (t_mx_3g *)object_alloc(mx_3g_class);

    // create one message outlet
    x->msg_out = outlet_new((t_object *)x, NULL);

    // set up DSP, create signal inlets (3)
    dsp_setup((t_pxobject *)x, 3);
    // signal outlets (r pat, a pat, b pat, c pat, r2 pat, a2 pat, b2 pat,
    // c2pat, cd, cp, stepnr)
    int i;
    for (i = 0; i < 11; i++) {
        outlet_new((t_object *)x, "signal");
    }

    x->counter = 0;
    x->step_prev = 0;

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
    for (i = 0; i < 8; i++) {
        p_s->pat_list[i] = sysmem_newptrclear(p_s->steps * sizeof(t_ptr));
    }

    if (argc == 3) {
        p_s->a = atom_getlong(argv);
        p_s->b = atom_getlong(argv + 1);
        p_s->c = atom_getlong(argv + 2);
        mx_3g_bang(x);
    }

    return (x);
}

void mx_3g_free(t_mx_3g *x)
{
    t_schillinger *p_s = &x->t;
    dsp_free((t_pxobject *)x);

    if (p_s->pat_list) {
        for (int i = 0; i < 8; i++) {
            sysmem_freeptr(p_s->pat_list[i]);
        }
        sysmem_freeptr(p_s->pat_list);
    }
}

void mx_3g_assist(t_mx_3g *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                sprintf(s,
                        "(signal) Click to advance one step\n(gen) Generator "
                        "trio\n(bang) Output to step visualiser");
                break;
            case 1:
                sprintf(s, "(signal) Click to reset sequencer");
                break;
            case 2:
                sprintf(s, "(signal) Step number to jump to");
                break;
        }
    } else {
        switch (a) {
            case 0:
                sprintf(s, "Resultant");
                break;
            case 1:
                sprintf(s, "Generator a");
                break;
            case 2:
                sprintf(s, "Generator b");
                break;
            case 3:
                sprintf(s, "Generator c");
                break;
            case 4:
                sprintf(s, "Resultant'");
                break;
            case 5:
                sprintf(s, "Generator a'");
                break;
            case 6:
                sprintf(s, "Generator b'");
                break;
            case 7:
                sprintf(s, "Generator c'");
                break;
            case 8:
                sprintf(s, "Common denominator");
                break;
            case 9:
                sprintf(s, "Common product");
                break;
            case 10:
                sprintf(s, "Step number");
                break;
            case 11:
                sprintf(s, "To sequencer");
                break;
        }
    }
}

void mx_3g_bang(t_mx_3g *x)
{
    t_schillinger *p_s = &(x->t);

    if (p_s->a && p_s->b && p_s->c) {
        mx_3g_gen(x, p_s->a, p_s->b, p_s->c);
    } else {
        post("No generator trio received yet!");
    }
}

void mx_3g_gen(t_mx_3g *x, long a, long b, long c)
{
    t_schillinger *p_s = &(x->t);
    p_s->steps = a * b * c;

    int i;

    p_s->a = a;
    p_s->b = b;
    p_s->c = c;

    long newsize = (long)p_s->steps * sizeof(t_ptr);

    for (i = 0; i < 8; i++) {
        sysmem_freeptr(p_s->pat_list[i]);
        p_s->pat_list[i] = sysmem_newptrclear(newsize);
    }

    for (i = 0; i < 9; i++) {
        outlet_s(x, x->out_names[i], 1, "clear");
        outlet_s(x, x->out_names[i], 2, "rows", 1);
        outlet_s(x, x->out_names[i], 2, "columns", (int)p_s->steps);
    }

    outlet_int(x->msg_out, 1);
    outlet_int(x->msg_out, p_s->steps);

    for (i = 0; i < p_s->steps; i += a) {
        // a
        p_s->pat_list[A1][i] = 1;
        mx_outlet(x, "a1", i, 0, 1);

        // r
        p_s->pat_list[R1][i] = 1;
        mx_outlet(x, "r1", i, 0, 1);
    }

    for (i = 0; i < p_s->steps; i += b) {
        p_s->pat_list[B1][i] = 1;
        mx_outlet(x, "b1", i, 0, 1);

        p_s->pat_list[R1][i] = 1;
        mx_outlet(x, "r1", i, 0, 1);
    }

    for (i = 0; i < p_s->steps; i += c) {
        p_s->pat_list[C1][i] = 1;
        mx_outlet(x, "c1", i, 0, 1);

        p_s->pat_list[R1][i] = 1;
        mx_outlet(x, "r1", i, 0, 1);
    }

    // COUNTERTHEME

    for (i = 0; i < p_s->steps; i += (b * c)) {
        // a
        p_s->pat_list[A2][i] = 1;
        mx_outlet(x, "a2", i, 0, 1);

        // r
        p_s->pat_list[R2][i] = 1;
        mx_outlet(x, "r2", i, 0, 1);
    }

    for (i = 0; i < p_s->steps; i += (a * c)) {
        p_s->pat_list[B2][i] = 1;
        mx_outlet(x, "b2", i, 0, 1);

        p_s->pat_list[R2][i] = 1;
        mx_outlet(x, "r2", i, 0, 1);
    }

    for (i = 0; i < p_s->steps; i += (a * b)) {
        p_s->pat_list[C2][i] = 1;
        mx_outlet(x, "c2", i, 0, 1);

        p_s->pat_list[R2][i] = 1;
        mx_outlet(x, "r2", i, 0, 1);
    }
}

void mx_3g_perform64(t_mx_3g *x, t_object *dsp64, double **ins, long numins,
                     double **outs, long numouts, long sampleframes, long flags,
                     void *userparam)
{
    t_double *in1_p = ins[0];
    t_double *in2_p = ins[1];
    t_double *in3_p = ins[2];
    t_double *r1_out = outs[R1];
    t_double *a1_out = outs[A1];
    t_double *b1_out = outs[B1];
    t_double *c1_out = outs[C1];
    t_double *r2_out = outs[R2];
    t_double *a2_out = outs[A2];
    t_double *b2_out = outs[B2];
    t_double *c2_out = outs[C2];
    t_double *cd_out = outs[CD];
    t_double *cp_out = outs[CP];
    t_double *stp_out = outs[STP];
    long n = sampleframes;
    t_double in1, in2, in3;

    t_schillinger *p_s = &x->t;

    while (n--) {
        in1 = *in1_p++;
        in2 = *in2_p++;
        in3 = *in3_p++;

        // detect click, increase on click
        if (in1 > 0.) {
            x->counter++;
        }
        x->counter %= p_s->steps;

        // detect click, reset counter on click
        if (in2 > 0.) {
            x->counter = 0;
        }

        // if new in3 is different than previous step, reset the counter to in3
        // (only on ONE frame!)
        if (x->step_prev != in3 && in3 != 0) {
            x->counter = ((int)(in3 - 1)) % p_s->steps;
        }

        x->step_prev = in3;

        *r1_out++ = in1 * (int)p_s->pat_list[R1][x->counter];
        *a1_out++ = in1 * (int)p_s->pat_list[A1][x->counter];
        *b1_out++ = in1 * (int)p_s->pat_list[B1][x->counter];
        *c1_out++ = in1 * (int)p_s->pat_list[C1][x->counter];
        *r2_out++ = in1 * (int)p_s->pat_list[R2][x->counter];
        *a2_out++ = in1 * (int)p_s->pat_list[A2][x->counter];
        *b2_out++ = in1 * (int)p_s->pat_list[B2][x->counter];
        *c2_out++ = in1 * (int)p_s->pat_list[C2][x->counter];

        *cd_out++ = in1;

        // cp_out is 1 on one click, when x->counter is 0 and v is one
        *cp_out++ = (!x->counter) && in1;
        *stp_out++ = x->counter;
    }
    return;
}

void mx_3g_dsp64(t_mx_3g *x, t_object *dsp64, short *count, double samplerate,
                 long maxvectorsize, long flags)
{
    object_method(dsp64, gensym("dsp_add64"), x, mx_3g_perform64, 0, NULL);
}

void mx_outlet(t_mx_3g *x, char *pre, int a, int b, int c)
{
    t_atom argv[3];
    atom_setlong(argv, a);
    atom_setlong(argv + 1, b);
    atom_setlong(argv + 2, c);
    outlet_anything(x->msg_out, gensym(pre), 3, argv);
}

void outlet_s(t_mx_3g *x, char *selector, int argc, char *msg, ...)
{
    // the first symbol message counts as argc as well
    t_atom argv[argc];
    int i, temp;
    va_list ap;
    va_start(ap, msg);
    atom_setsym(argv, gensym(msg));

    for (i = 1; i < argc; i++) {
        temp = va_arg(ap, int);
        atom_setlong(argv + i, temp);
    }

    outlet_anything(x->msg_out, gensym(selector), argc, argv);
    va_end(ap);
}

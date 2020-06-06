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

// signal outlets (r pat, a pat, b pat, cd, cp, stepnr)
#define R_OUT 0
#define A_OUT 1
#define B_OUT 2
#define CD_OUT 3
#define CP_OUT 4
#define STP_OUT 5

typedef struct _schillinger {
    long a;
    long b;
    t_ptr a_pat;
    t_ptr b_pat;
    t_ptr r_pat;
    long steps;
} t_schillinger;

typedef struct _mxp1 {
    t_pxobject p_ob;
    int counter;
    int step_prev;
    t_schillinger t;
    void *msg_out;
    char *out_names[4];
} t_mxp1;

void *mxp1_new(t_symbol *s, long argc, t_atom *argv);
void mxp1_free(t_mxp1 *x);
void mxp1_gen(t_mxp1 *x, long a, long b);
void mxp1_assist(t_mxp1 *x, void *b, long m, long a, char *s);
void mxp1_bang(t_mxp1 *x);
void mxp1_perform64(t_mxp1 *x, t_object *dsp64, double **ins, long numins,
                    double **outs, long numouts, long sampleframes, long flags,
                    void *userparam);
void mxp1_dsp64(t_mxp1 *x, t_object *dsp64, short *count, double samplerate,
                long maxvectorsize, long flags);
void outlet_s(t_mxp1 *x, char *selector, int argc, char *msg, ...);
void mx_outlet(t_mxp1 *x, char *pre, int a, int b, int c);

t_class *mxp1_class;  // global pointer to the object class - so max can
                      // reference the object

void ext_main(void *r)
{
    t_class *c;

    c = class_new("mxp1~", (method)mxp1_new, (method)mxp1_free, sizeof(t_mxp1),
                  NULL, A_GIMME, 0);
    class_addmethod(c, (method)mxp1_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)mxp1_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mxp1_bang, "bang", 0);
    class_addmethod(c, (method)mxp1_gen, "gen", A_LONG, A_LONG, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    mxp1_class = c;
}

void *mxp1_new(t_symbol *s, long argc, t_atom *argv)
{
    // allocate class
    t_mxp1 *x = (t_mxp1 *)object_alloc(mxp1_class);

    // create one message outlet
    x->msg_out = outlet_new((t_object *)x, NULL);

    // set up DSP, create signal inlets (3)
    dsp_setup((t_pxobject *)x, 3);
    // signal outlets (r pat, a pat, b pat, cd, cp, stepnr
    int i;
    for (i = 0; i < 6; i++) {
        outlet_new((t_object *)x, "signal");
    }

    x->counter = 0;
    x->step_prev = 0;

    x->out_names[0] = "r";
    x->out_names[1] = "a";
    x->out_names[2] = "b";
    x->out_names[3] = "stp";

    t_schillinger *p_s = &x->t;
    p_s->a = 0;
    p_s->b = 0;
    p_s->steps = 1;  // init to one, lest we get divide by zero error later on

    p_s->a_pat = sysmem_newptrclear(p_s->steps * sizeof(int));
    p_s->b_pat = sysmem_newptrclear(p_s->steps * sizeof(int));
    p_s->r_pat = sysmem_newptrclear(p_s->steps * sizeof(int));

    if (argc == 2) {
        p_s->a = atom_getlong(argv);
        p_s->b = atom_getlong(argv + 1);
        mxp1_bang(x);
    }

    return (x);
}

void mxp1_free(t_mxp1 *x)
{
    t_schillinger *p_s = &x->t;
    dsp_free((t_pxobject *)x);

    sysmem_freeptr(p_s->r_pat);
    sysmem_freeptr(p_s->a_pat);
    sysmem_freeptr(p_s->b_pat);
}

void mxp1_assist(t_mxp1 *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                sprintf(s,
                        "(signal) Click to advance one step\n(gen) Generator "
                        "pair\n(bang) Output to step visualiser");
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
                sprintf(s, "Common denominator");
                break;
            case 4:
                sprintf(s, "Common product");
                break;
            case 5:
                sprintf(s, "Step number");
                break;
            case 6:
                sprintf(s, "To step visualiser");
                break;
        }
    }
}

void mxp1_bang(t_mxp1 *x)
{
    t_schillinger *p_s = &(x->t);

    if (p_s->a && p_s->b) {
        mxp1_gen(x, p_s->a, p_s->b);
    } else {
        post("No generator pair received yet!");
    }
}

void mxp1_gen(t_mxp1 *x, long a, long b)
{
    t_schillinger *p_s = &(x->t);
    a = (a == 0) ? 1 : a;
    b = (b == 0) ? 1 : b;
    p_s->steps = a * b;

    int i;

    p_s->a = a;
    p_s->b = b;
    long newsize = (long)p_s->steps * sizeof(int);
    t_ptr temp1, temp2, temp3;
    temp1 = sysmem_resizeptrclear(p_s->r_pat, newsize);
    temp2 = sysmem_resizeptrclear(p_s->a_pat, newsize);
    temp3 = sysmem_resizeptrclear(p_s->b_pat, newsize);
    p_s->r_pat = temp1;
    p_s->a_pat = temp2;
    p_s->b_pat = temp3;

    for (i = 0; i < newsize; i++) {
        p_s->r_pat[i] = 0;
        p_s->a_pat[i] = 0;
        p_s->b_pat[i] = 0;
    }

    for (i = 0; i < 4; i++) {
        outlet_s(x, x->out_names[i], 1, "clear");
        outlet_s(x, x->out_names[i], 2, "rows", 1);
        outlet_s(x, x->out_names[i], 2, "columns", (int)p_s->steps);
    }

    outlet_int(x->msg_out, 1);
    outlet_int(x->msg_out, p_s->steps);

    for (i = 0; i < p_s->steps; i += a) {
        // a
        p_s->a_pat[i] = 1;
        mx_outlet(x, "a", i, 0, 1);

        // r
        p_s->r_pat[i] = 1;
        mx_outlet(x, "r", i, 0, 1);
    }

    for (i = 0; i < p_s->steps; i += b) {
        p_s->b_pat[i] = 1;
        mx_outlet(x, "b", i, 0, 1);

        p_s->r_pat[i] = 1;
        mx_outlet(x, "r", i, 0, 1);
    }
}

void mxp1_perform64(t_mxp1 *x, t_object *dsp64, double **ins, long numins,
                    double **outs, long numouts, long sampleframes, long flags,
                    void *userparam)
{
    t_double *in1_p = ins[0];
    t_double *in2_p = ins[1];
    t_double *in3_p = ins[2];
    t_double *r_out = outs[R_OUT];
    t_double *a_out = outs[A_OUT];
    t_double *b_out = outs[B_OUT];
    t_double *cd_out = outs[CD_OUT];
    t_double *cp_out = outs[CP_OUT];
    t_double *stp_out = outs[STP_OUT];
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

        *r_out++ = in1 * (int)p_s->r_pat[x->counter];
        *a_out++ = in1 * (int)p_s->a_pat[x->counter];
        *b_out++ = in1 * (int)p_s->b_pat[x->counter];

        *cd_out++ = in1;

        // cp_out is 1 on one click, when x->counter is 0 and v is one
        *cp_out++ = (!x->counter) && in1;
        *stp_out++ = x->counter;
    }
    return;
}

void mxp1_dsp64(t_mxp1 *x, t_object *dsp64, short *count, double samplerate,
                long maxvectorsize, long flags)
{
    object_method(dsp64, gensym("dsp_add64"), x, mxp1_perform64, 0, NULL);
}

void mx_outlet(t_mxp1 *x, char *pre, int a, int b, int c)
{
    t_atom argv[3];
    atom_setlong(argv, a);
    atom_setlong(argv + 1, b);
    atom_setlong(argv + 2, c);
    outlet_anything(x->msg_out, gensym(pre), 3, argv);
}

void outlet_s(t_mxp1 *x, char *selector, int argc, char *msg, ...)
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

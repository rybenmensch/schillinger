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

/* periodicities 2
 * for this we need MC outputs
 */

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
    long b_amt;
    t_double **b_outs;
    t_ptr a_pat;
    t_ptr *b_pat;
    t_ptr r_pat;
    long steps;
    long steps_b;
} t_schillinger;

typedef struct _mxp2 {
    t_pxobject p_ob;
    int counter;
    int step_prev;
    t_schillinger t;
    void *msg_out;
    char *out_names[4];
    long b_offset;
} t_mxp2;

void *mxp2_new(t_symbol *s, long argc, t_atom *argv);
void mxp2_free(t_mxp2 *x);
void mxp2_gen(t_mxp2 *x, long a, long b);
void mxp2_assist(t_mxp2 *x, void *b, long m, long a, char *s);
void mxp2_bang(t_mxp2 *x);
long mxp2_multichanneloutputs(t_mxp2 *x, long index);
void mxp2_perform64(t_mxp2 *x, t_object *dsp64, double **ins, long numins,
                    double **outs, long numouts, long sampleframes, long flags,
                    void *userparam);
void mxp2_dsp64(t_mxp2 *x, t_object *dsp64, short *count, double samplerate,
                long maxvectorsize, long flags);
void outlet_s(void *outlet, char *selector, int argc, char *msg, ...);
void mx_outlet(t_mxp2 *x, char *pre, int a, int b, int c);

t_class *mxp2_class;

void ext_main(void *r)
{
    t_class *c;

    c = class_new("mxp2~", (method)mxp2_new, (method)mxp2_free, sizeof(t_mxp2),
                  NULL, A_GIMME, 0);
    class_addmethod(c, (method)mxp2_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)mxp2_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mxp2_bang, "bang", 0);
    class_addmethod(c, (method)mxp2_gen, "gen", A_LONG, A_LONG, 0);
    class_addmethod(c, (method)mxp2_multichanneloutputs, "multichanneloutputs",
                    A_CANT, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    mxp2_class = c;
}

void *mxp2_new(t_symbol *s, long argc, t_atom *argv)
{
    // allocate class
    t_mxp2 *x = (t_mxp2 *)object_alloc(mxp2_class);

    // create one message outlet
    x->msg_out = outlet_new((t_object *)x, NULL);

    // set up DSP, create signal inlets (3)
    dsp_setup((t_pxobject *)x, 3);
    // x->p_ob.z_misc |= Z_NO_INPLACE;

    // signal outlets cd, cp, stepnr
    for (int i = 0; i < 3; i++) {
        outlet_new((t_object *)x, "signal");
    }

    // mc outlet b
    outlet_new((t_object *)x, "multichannelsignal");
    // r pat, a pat
    for (int i = 0; i < 2; i++) {
        outlet_new((t_object *)x, "signal");
    }

    x->counter = 0;
    x->step_prev = 0;
    x->b_offset = 10;

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
    x->t.b_pat = (t_ptr *)sysmem_newptrclear(x->t.b_amt * sizeof(t_ptr));
    x->t.b_outs =
        (t_double **)sysmem_newptrclear(x->b_offset * sizeof(t_double *));

    for (int i = 0; i < x->t.b_amt; i++) {
        x->t.b_pat[i] = sysmem_newptrclear(x->t.steps * sizeof(t_ptr));
        x->t.b_outs[i] = NULL;
    }

    if (argc == 2) {
        x->t.a = atom_getlong(argv);
        x->t.b = atom_getlong(argv + 1);
        mxp2_bang(x);
    }

    return (x);
}

void mxp2_free(t_mxp2 *x)
{
    dsp_free((t_pxobject *)x);

    sysmem_freeptr(x->t.r_pat);
    sysmem_freeptr(x->t.a_pat);

    for (int i = 0; i < x->t.b_amt; i++) {
        sysmem_freeptr(x->t.b_pat[i]);
    }
    sysmem_freeptr(x->t.b_pat);

    sysmem_freeptr(x->t.b_outs);
}

void mxp2_assist(t_mxp2 *x, void *b, long m, long a, char *s)
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
                sprintf(s, "Generators b0-bn");
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
                sprintf(s, "To sequencer");
                break;
        }
    }
}

void mxp2_bang(t_mxp2 *x)
{
    if (x->t.a && x->t.b) {
        mxp2_gen(x, x->t.a, x->t.b);
    } else {
        post("No generator pair received yet!");
    }
}

long mxp2_multichanneloutputs(t_mxp2 *x, long index)
{
    if (2 == index) {  // index where we want our mc sig
        // return x->t.b_amt;
        return x->b_offset;
    } else {
        return 1;
    }
}

void mxp2_gen(t_mxp2 *x, long a, long b)
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

    /******************************************************************/
    sysmem_freeptr(x->t.r_pat);
    x->t.r_pat = sysmem_newptrclear(newsize);

    sysmem_freeptr(x->t.a_pat);
    x->t.a_pat = sysmem_newptrclear(newsize);

    if (x->t.b_outs == NULL) {
        post("b_outs is NULL. abort\n", x->t.b_outs);
        return;
    }

    sysmem_freeptr(x->t.b_outs);
    x->t.b_outs =
        (t_double **)sysmem_newptrclear(x->b_offset * sizeof(t_double *));

    if (x->t.b_outs == NULL) {
        post("allocation failed!\n");
        return;
    }

    // free the old array, which has the size of old_b_amt
    for (int i = 0; i < old_b_amt; i++) {
        sysmem_freeptr(x->t.b_pat[i]);
    }
    // free the pointer itself
    sysmem_freeptr(x->t.b_pat);

    x->t.b_pat = (t_ptr *)sysmem_newptrclear(x->t.b_amt * sizeof(t_ptr *));
    for (int i = 0; i < x->t.b_amt; i++) {
        x->t.b_pat[i] = sysmem_newptrclear(x->t.steps * sizeof(t_ptr));
    }

    // *****************************************************************

    for (int i = 0; i < 4; i++) {
        outlet_s(x->msg_out, x->out_names[i], 1, "clear");
        outlet_s(x->msg_out, x->out_names[i], 2, "rows", 1);
        outlet_s(x->msg_out, x->out_names[i], 2, "columns", (int)x->t.steps);
    }
    outlet_s(x->msg_out, "b", 2, "rows", x->t.b_amt);

    outlet_int(x->msg_out, x->t.b_amt);
    outlet_int(x->msg_out, x->t.steps);

    // ****************************************************************

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
}

void mxp2_perform64(t_mxp2 *x, t_object *dsp64, double **ins, long numins,
                    double **outs, long numouts, long sampleframes, long flags,
                    void *userparam)
{
    long n = sampleframes;
    t_double in1, in2, in3;

    t_double *in1_p = ins[0];
    t_double *in2_p = ins[1];
    t_double *in3_p = ins[2];

    t_double *r_out = outs[0];
    t_double *a_out = outs[1];

    long ra_off = 2;
    long b_off = x->b_offset;
    long rab_off = b_off + ra_off;
    t_double **b_o = x->t.b_outs;
    int b = (int)x->t.b_amt;
    int *p_counter = &x->counter;
    long *p_steps = &x->t.steps;

    for (int i = 0; i < x->b_offset; i++) {
        if (i >= b) {
            set_zero64(outs[i + ra_off], n);
        } else {
            b_o[i] = outs[i + ra_off];
        }
    }

    t_double *cd_out = outs[rab_off];
    t_double *cp_out = outs[rab_off + 1];
    t_double *stp_out = outs[rab_off + 2];

    while (n--) {
        in1 = *in1_p++;
        in2 = *in2_p++;
        in3 = *in3_p++;

        // detect click, increase on click
        if (in1 > 0.) {
            (*p_counter)++;
        }

        // x->counter %=x->t.steps;
        *p_counter %= (*p_steps);

        // detect click, reset counter on click
        if (in2 > 0.) {
            // x->counter = 0;
            *p_counter = 0;
        }

        // if new in3 is different than previous step, reset the counter to in3
        // (only on ONE frame!)
        if (x->step_prev != in3 && in3 != 0) {
            x->counter = ((int)(in3 - 1)) % x->t.steps;
        }

        x->step_prev = in3;

        *r_out++ = in1 * (int)x->t.r_pat[x->counter];
        *a_out++ = in1 * (int)x->t.a_pat[x->counter];

        for (int i = 0; i < b; i++) {
            t_double temp = in1 * (int)x->t.b_pat[i][x->counter];
            *b_o[i]++ = CLAMP(temp, -1, 1);
        }

        *cd_out++ = in1;

        // cp_out is 1 on one click, when x->counter is 0 and v is one
        *cp_out++ = (!x->counter) && in1;
        *stp_out++ = x->counter;
    }
    return;
}

void mxp2_dsp64(t_mxp2 *x, t_object *dsp64, short *count, double samplerate,
                long maxvectorsize, long flags)
{
    object_method(dsp64, gensym("dsp_add64"), x, mxp2_perform64, 0, NULL);
}

void mx_outlet(t_mxp2 *x, char *pre, int a, int b, int c)
{
    t_atom argv[3];
    atom_setlong(argv, a);
    atom_setlong(argv + 1, b);
    atom_setlong(argv + 2, c);
    outlet_anything(x->msg_out, gensym(pre), 3, argv);
}

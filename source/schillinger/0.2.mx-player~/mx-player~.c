#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include <stdarg.h>

// signal outlets (pat, cd, cp, stepnr)
#define PAT_OUT 0
#define CD_OUT 1
#define CP_OUT 2
#define STP_OUT 3

typedef struct _schillinger {
    t_atom_long *pattern;
    t_atom_long *binpat;
    long steps;
    long bin_steps;
} t_schillinger;

typedef struct _mx_player {
    t_pxobject p_ob;
    int counter;
    int step_prev;
    t_schillinger t;
    void *msg_out;
    char *out_names[2];
} t_mx_player;

void *mx_player_new(t_symbol *s, long argc, t_atom *argv);
void mx_player_patbin(t_mx_player *x, t_symbol *s, long argc, t_atom *argv);
void mx_player_assist(t_mx_player *x, void *b, long m, long a, char *s);
void mx_player_bang(t_mx_player *x);
void mx_player_print(t_mx_player *x);
void mx_player_perform64(t_mx_player *x, t_object *dsp64, double **ins,
                         long numins, double **outs, long numouts,
                         long sampleframes, long flags, void *userparam);
void mx_player_dsp64(t_mx_player *x, t_object *dsp64, short *count,
                     double samplerate, long maxvectorsize, long flags);
void mx_player_free(t_mx_player *x);
void mx_player_pat(t_mx_player *x, t_symbol *s, long argc, t_atom *argv);
void mx_player_npat(t_mx_player *x, t_symbol *s, long argc, t_atom *argv);

void outlet_s(t_mx_player *x, char *selector, int argc, char *msg, ...);
void mx_outlet(t_mx_player *x, char *pre, int a, int b, int c);
long pattobin(long argc, t_atom_long **bin, t_atom_long **pat);
long bintopat(long argc, t_atom_long **pat, t_atom_long **bin);

t_class *mx_player_class;  // global pointer to the object class - so max can
                           // reference the object

void ext_main(void *r)
{
    t_class *c;

    c = class_new("mx-player~", (method)mx_player_new, (method)mx_player_free,
                  sizeof(t_mx_player), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_player_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)mx_player_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mx_player_bang, "bang", 0);
    class_addmethod(c, (method)mx_player_pat, "pat", A_GIMME, 0);
    class_addmethod(c, (method)mx_player_patbin, "patbin", A_GIMME, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    mx_player_class = c;
}

void *mx_player_new(t_symbol *s, long argc, t_atom *argv)
{
    // allocate class
    t_mx_player *x = (t_mx_player *)object_alloc(mx_player_class);

    // create one message outlet
    x->msg_out = outlet_new((t_object *)x, NULL);

    // set up DSP, create signal inlets (3)
    dsp_setup((t_pxobject *)x, 3);
    // signal outlets (pat, cd, cp, stepnr)
    for (int i = 0; i < 4; i++) {
        outlet_new((t_object *)x, "signal");
    }

    x->counter = 0;
    x->step_prev = 0;

    x->out_names[0] = "r";
    x->out_names[1] = "stp";

    x->t.steps = 1;  // init to one, lest we get divide by zero error later on
    x->t.bin_steps =
        1;  // init to one, lest we get divide by zero error later on

    x->t.pattern = NULL;
    x->t.binpat = NULL;
    return (x);
}

void mx_player_free(t_mx_player *x)
{
    dsp_free((t_pxobject *)x);

    if (x->t.pattern) {
        sysmem_freeptr(x->t.pattern);
    }

    if (x->t.binpat) {
        sysmem_freeptr(x->t.binpat);
    }
}

void mx_player_assist(t_mx_player *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                sprintf(s,
                        "(signal) Click to advance one step | (pat) Pattern");
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
                sprintf(s, "Pattern");
                break;
            case 1:
                sprintf(s, "Common denominator");
                break;
            case 2:
                sprintf(s, "Common product");
                break;
            case 3:
                sprintf(s, "Step number");
                break;
            case 4:
                sprintf(s, "To sequencer");
                break;
        }
    }
}

void mx_player_bang(t_mx_player *x)
{
    if (x->t.pattern || x->t.binpat) {
        mx_player_print(x);
    } else {
        post("No pattern received yet!");
    }
}

void mx_player_print(t_mx_player *x)
{
    for (int i = 0; i < 2; i++) {
        outlet_s(x, x->out_names[i], 1, "clear");
        outlet_s(x, x->out_names[i], 2, "rows", 1);
        outlet_s(x, x->out_names[i], 2, "columns", (int)x->t.bin_steps);
    }

    outlet_int(x->msg_out, 1);
    outlet_int(x->msg_out, x->t.bin_steps);

    for (int i = 0; i < x->t.bin_steps; i++) {
        mx_outlet(x, "r", i, 0, (int)x->t.binpat[i]);
    }
}

void mx_player_pat(t_mx_player *x, t_symbol *s, long argc, t_atom *argv)
{
    if (!argc) return;
    x->t.steps = argc;

    if (x->t.pattern) {
        sysmem_freeptr(x->t.pattern);
    }

    x->t.pattern =
        (t_atom_long *)sysmem_newptrclear(x->t.steps * sizeof(t_atom_long));

    for (int i = 0; i < argc; i++) {
        t_atom_long temp = atom_getlong(argv + i);
        x->t.pattern[i] = (temp == 0) ? 1 : temp;
    }

    x->t.bin_steps = pattobin(x->t.steps, &(x->t.binpat), &(x->t.pattern));
    mx_player_print(x);
}

void mx_player_patbin(t_mx_player *x, t_symbol *s, long argc, t_atom *argv)
{
    x->t.bin_steps = argc;

    if (x->t.binpat) {
        sysmem_freeptr(x->t.binpat);
    }

    x->t.binpat =
        (t_atom_long *)sysmem_newptrclear(x->t.bin_steps * sizeof(t_atom_long));

    for (int i = 0; i < argc; i++) {
        x->t.binpat[i] = atom_getlong(argv + i);
    }

    x->t.steps = bintopat(x->t.steps, &(x->t.pattern), &(x->t.binpat));
    mx_player_print(x);
}

void mx_player_npat(t_mx_player *x, t_symbol *s, long argc, t_atom *argv)
{
    if (argc < 3) {
        post("no");
        return;
    }
    post("argc: %d", argc);
}

long pattobin(long argc, t_atom_long **bin, t_atom_long **pat)
{
    // oh god pls no pointers to pointers
    // treating them as 2d-arrays works, so i dont need to know the exact
    // pointer arithmetic
    long pat_sum = 0;

    for (int i = 0; i < argc; i++) {
        pat_sum += pat[0][i];
    }

    if (bin) {
        sysmem_freeptr(bin);
    }

    *bin = (t_atom_long *)sysmem_newptrclear(pat_sum * sizeof(t_atom_long));
    int other = 0;
    for (int i = 0; i < argc; i++) {
        bin[0][other] = 1;
        other += pat[0][i];
    }

    return pat_sum;
}

long bintopat(long argc, t_atom_long **pat, t_atom_long **bin)
{
    long beatcount = 0;

    for (int i = 0; i < argc; i++) {
        beatcount += bin[0][i];
    }

    if (pat) {
        sysmem_freeptr(pat);
    }

    *pat = (t_atom_long *)sysmem_newptrclear(beatcount * sizeof(t_atom_long));

    t_atom_long lpat[beatcount + 1];
    int counter = 0;

    for (int i = 0; i < argc; i++) {
        if (bin[0][i] == 1) {
            lpat[counter++] = i;
        }
    }

    lpat[beatcount] = argc;

    for (int i = 0; i < beatcount; i++) {
        pat[0][i] = lpat[i + 1] - lpat[i];
    }

    return beatcount;
}

void mx_player_perform64(t_mx_player *x, t_object *dsp64, double **ins,
                         long numins, double **outs, long numouts,
                         long sampleframes, long flags, void *userparam)
{
    t_double *in1_p = ins[0];
    t_double *in2_p = ins[1];
    t_double *in3_p = ins[2];
    t_double *r_out = outs[PAT_OUT];
    t_double *cd_out = outs[CD_OUT];
    t_double *cp_out = outs[CP_OUT];
    t_double *stp_out = outs[STP_OUT];
    long n = sampleframes;
    t_double in1, in2, in3;

    if (!x->t.binpat) {
        set_zero64(r_out, sampleframes);
        set_zero64(cd_out, sampleframes);
        set_zero64(cp_out, sampleframes);
        set_zero64(stp_out, sampleframes);
        return;
    }

    while (n--) {
        in1 = *in1_p++;
        in2 = *in2_p++;
        in3 = *in3_p++;

        // detect click, increase on click
        if (in1 > 0.) {
            x->counter++;
        }
        x->counter %= x->t.bin_steps;

        // detect click, reset counter on click
        if (in2 > 0.) {
            x->counter = 0;
        }

        // if new in3 is different than previous step, reset the counter to in3
        // (only on ONE frame!)
        if (x->step_prev != in3 && in3 != 0) {
            x->counter = ((int)(in3 - 1)) % x->t.bin_steps;
        }

        x->step_prev = in3;
        if (x->t.binpat) {
            t_double temp = in1 * (int)x->t.binpat[x->counter];
            *r_out++ = CLAMP(temp, -1, 1);
        }
        *cd_out++ = in1;

        // cp_out is 1 on one click, when x->counter is 0 and v is one
        *cp_out++ = (!x->counter) && in1;
        *stp_out++ = x->counter;
    }
    return;
}

void mx_player_dsp64(t_mx_player *x, t_object *dsp64, short *count,
                     double samplerate, long maxvectorsize, long flags)
{
    object_method(dsp64, gensym("dsp_add64"), x, mx_player_perform64, 0, NULL);
}

void mx_outlet(t_mx_player *x, char *pre, int a, int b, int c)
{
    t_atom argv[3];
    atom_setlong(argv, a);
    atom_setlong(argv + 1, b);
    atom_setlong(argv + 2, c);
    outlet_anything(x->msg_out, gensym(pre), 3, argv);
}

void outlet_s(t_mx_player *x, char *selector, int argc, char *msg, ...)
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

    outlet_anything(x->msg_out, gensym(selector), argc, argv);
    va_end(ap);
}

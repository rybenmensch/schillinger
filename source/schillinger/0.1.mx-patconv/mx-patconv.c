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

typedef struct _mx_patconv {
    t_object p_ob;
    void *msg_out;
    long patbin_length;
    long pat_length;
    t_atom_long *patbin;
    t_atom_long *pat;
    int last_called;
} t_mx_patconv;

void *mx_patconv_new(t_symbol *s, long argc, t_atom *argv);
void mx_patconv_free(t_mx_patconv *x);
void mx_patconv_pat(t_mx_patconv *x, t_symbol *s, long argc, t_atom *argv);
void mx_patconv_patbin(t_mx_patconv *x, t_symbol *s, long argc, t_atom *argv);
void mx_patconv_assist(t_mx_patconv *x, void *b, long m, long a, char *s);
void mx_patconv_bang(t_mx_patconv *x);

t_class *mx_patconv_class;

void ext_main(void *r)
{
    t_class *c;

    c = class_new("mx-patconv", (method)mx_patconv_new, (method)mx_patconv_free,
                  sizeof(t_mx_patconv), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_patconv_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mx_patconv_bang, "bang", 0);
    class_addmethod(c, (method)mx_patconv_pat, "pat", A_GIMME, 0);
    class_addmethod(c, (method)mx_patconv_patbin, "patbin", A_GIMME, 0);

    class_register(CLASS_BOX, c);
    mx_patconv_class = c;
}

void *mx_patconv_new(t_symbol *s, long argc, t_atom *argv)
{
    // allocate class
    t_mx_patconv *x = (t_mx_patconv *)object_alloc(mx_patconv_class);

    x->msg_out = outlet_new((t_object *)x, NULL);  // msg outlet

    x->patbin = NULL;
    x->pat = NULL;

    return (x);
}

void mx_patconv_free(t_mx_patconv *x)
{
    if (x->patbin) sysmem_freeptr(x->patbin);

    if (x->pat) sysmem_freeptr(x->patbin);
}

void mx_patconv_assist(t_mx_patconv *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_INLET) {
        switch (a) {
            case 0:
                sprintf(s, "(pat|patbin) Pattern");
                break;
        }
    } else {
        switch (a) {
            case 0:
                sprintf(s, "Converted Pattern");
                break;
        }
    }
}

void mx_patconv_patbin(t_mx_patconv *x, t_symbol *s, long argc, t_atom *argv)
{
    if (atom_getlong(argv) == 0) {
        post("(patbin) may not start with 0.");
        return;
    }

    x->last_called = 0;
    t_atom_long patbin[argc];
    x->patbin_length = argc;

    if (x->patbin) sysmem_freeptr(x->patbin);
    x->patbin = (t_atom_long *)sysmem_newptrclear(x->patbin_length *
                                                  sizeof(t_atom_long));

    for (int i = 0; i < argc; i++) {
        patbin[i] = atom_getlong(argv + i);
        x->patbin[i] = patbin[i];
    }

    int beatcount = 0;

    for (int i = 0; i < argc; i++) {
        beatcount += patbin[i];
    }

    t_atom_long lpat[beatcount + 1];
    t_atom_long result[beatcount];
    x->pat_length = beatcount;

    if (x->pat) sysmem_freeptr(x->pat);
    x->pat =
        (t_atom_long *)sysmem_newptrclear(x->pat_length * sizeof(t_atom_long));

    int counter = 0, rest = 0;
    t_bool first_rest = FALSE;
    for (int i = 0; i < argc; i++) {
        if (patbin[i] == 1) {
            lpat[counter++] = i;
            first_rest = FALSE;
        }
        if (first_rest) {
            rest++;
        }
        if (i == 0 && patbin[i] == 0) {
            first_rest = TRUE;
            rest++;
        }
    }

    lpat[beatcount] = argc;
    for (int i = 0; i < beatcount; i++) {
        result[i] = lpat[i + 1] - lpat[i];
    }
    if (rest) {
        // rest functionality is disabled for now, because no other objects
        // supports it, and there is no time to implement proper functionality.
        // maybe for later release..
        /*
        char buffer[4];
        t_atom pat[beatcount+1];
        snprintf(buffer, 4, "r%d", rest);
        atom_setsym(pat, gensym(buffer));
        for(int i=1;i<beatcount+1;i++){
            atom_setlong(pat+i, result[i-1]);
        }
        outlet_anything(x->msg_out, gensym("pat"), beatcount+1, pat);
         */
    } else {
        t_atom pat[beatcount];
        for (int i = 0; i < beatcount; i++) {
            atom_setlong(pat + i, result[i]);
            x->pat[i] = result[i];
        }
        outlet_anything(x->msg_out, gensym("pat"), beatcount, pat);
    }
}

void mx_patconv_pat(t_mx_patconv *x, t_symbol *s, long argc, t_atom *argv)
{
    if (atom_getlong(argv) == 0) {
        post("(pat) may not start with 0.");
        return;
    }

    x->last_called = 1;
    t_atom_long pat[argc];
    x->pat_length = argc;

    if (x->pat) sysmem_freeptr(x->pat);
    x->pat =
        (t_atom_long *)sysmem_newptrclear(x->pat_length * sizeof(t_atom_long));

    long pat_sum = 0;

    for (int i = 0; i < argc; i++) {
        pat[i] = atom_getlong(argv + i);
        x->pat[i] = pat[i];
        pat_sum += pat[i];
    }

    t_atom_long patbin[pat_sum];
    x->patbin_length = pat_sum;

    if (x->patbin) sysmem_freeptr(x->patbin);
    x->patbin = (t_atom_long *)sysmem_newptrclear(x->patbin_length *
                                                  sizeof(t_atom_long));

    for (int i = 0; i < pat_sum; i++) {
        patbin[i] = 0;
    }

    int other = 0;
    for (int i = 0; i < argc; i++) {
        patbin[other] = 1;
        other += pat[i];
    }

    t_atom patbin_atom[pat_sum];

    for (int i = 0; i < pat_sum; i++) {
        atom_setlong(patbin_atom + i, patbin[i]);
        x->patbin[i] = patbin[i];
    }

    outlet_anything(x->msg_out, gensym("patbin"), pat_sum, patbin_atom);
}

void mx_patconv_bang(t_mx_patconv *x)
{
    if (!(x->patbin && x->pat)) {
        post("No pattern received yet!");
        return;
    }
    // last called == 0: output pat
    // last called == 1: output patbin

    if (x->last_called == 0) {
        t_atom arg[x->pat_length];

        for (int i = 0; i < x->pat_length; i++) {
            atom_setlong(arg + i, x->pat[i]);
        }

        outlet_anything(x->msg_out, gensym("pat"), x->pat_length, arg);
    } else {
        t_atom arg[x->patbin_length];

        for (int i = 0; i < x->patbin_length; i++) {
            atom_setlong(arg + i, x->patbin[i]);
        }

        outlet_anything(x->msg_out, gensym("patbin"), x->patbin_length, arg);
    }
}

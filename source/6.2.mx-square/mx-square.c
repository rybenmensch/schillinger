/*
 * This file is part of the Schillinger Max Package, found at https://github.com/rybenmensch/schillinger.
 * Copyright (c) 2020 Manolo Müller.
 *
 * The Schillinger Max Package is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
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

typedef struct _schillinger{
    long steps;
    t_ptr polynom; //pattern from pat msg (user supplied)
    long p_len;
    t_ptr result;
    t_ptr sync;
    long arg_sum;
} t_schillinger;

typedef struct _mx_square {
    t_object p_ob;
    t_schillinger t;
    void *pat_out;
    void *sync_out;
} t_mx_square;

void *mx_square_new(t_symbol *s,  long argc, t_atom *argv);
void mx_square_free(t_mx_square *x);
void mx_square_pat(t_mx_square *x,t_symbol *s, long argc, t_atom *argv);
void mx_square_assist(t_mx_square *x, void *b, long m, long a, char *s);
void mx_square_bang(t_mx_square *x);

void print(t_mx_square *x);

t_class *mx_square_class;

void ext_main(void *r){
    t_class *c;
    
    c = class_new("mx-square", (method)mx_square_new, (method)mx_square_free, sizeof(t_mx_square), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_square_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mx_square_bang, "bang", 0);
    class_addmethod(c, (method)mx_square_pat, "pat", A_GIMME, 0);
    
    class_register(CLASS_BOX, c);
    mx_square_class = c;
}

void *mx_square_new(t_symbol *s, long argc, t_atom *argv){
    //allocate class
    t_mx_square *x = (t_mx_square *)object_alloc(mx_square_class);
    
    x->sync_out = outlet_new((t_object *)x, NULL);
    x->pat_out = outlet_new((t_object *)x, NULL);
    
    t_schillinger *p_s = &x->t;
    p_s->steps = 1; //init to one, lest we get divide by zero error later on
    p_s->arg_sum = 0;
    p_s->polynom = NULL;
    p_s->sync = NULL;
    p_s->result = NULL;
    return (x);
}

void mx_square_free(t_mx_square *x){
    t_schillinger *p_s = &x->t;
    if(p_s->polynom)
        sysmem_freeptr(p_s->polynom);
    
    if(p_s->sync)
        sysmem_freeptr(p_s->sync);
    if(p_s->result)
        sysmem_freeptr(p_s->result);
}

void mx_square_assist(t_mx_square *x, void *b, long m, long a, char *s){
    if(m == ASSIST_INLET){
        switch(a){
            case 0:
                sprintf(s, "(pat) Pattern to be squared");
                break;
        }
    }else{
        switch(a){
            case 0:
                sprintf(s, "Squared resultant pattern");
                break;
            case 1:
                sprintf(s, "Sync pattern");
                break;
        }
    }
}

void mx_square_bang(t_mx_square *x){
    t_schillinger *p_s = &x->t;
    
    if(!p_s->result){
        post("No pattern received yet.");
        return;
    }
    
    print(x);
}

void mx_square_pat(t_mx_square *x,t_symbol *s, long argc, t_atom *argv){
    if(!argc)   //no arguments, do nothing and exit
        return;
    
    t_schillinger *p_s = &x->t;
    
    if(p_s->polynom)
        sysmem_freeptr(p_s->polynom);
    p_s->polynom = sysmem_newptrclear(argc * sizeof(t_ptr));
    
    p_s->arg_sum = 0;
    for(int i=0;i<argc;i++){
        p_s->polynom[i] = atom_getlong(argv+i);
        p_s->arg_sum += p_s->polynom[i];
    }

    long power = 2;
    long a_pow = pow(argc, power);
    p_s->steps = a_pow;
    p_s->p_len = argc;
    
    if(p_s->result)
        sysmem_freeptr(p_s->result);
    p_s->result = sysmem_newptrclear(p_s->steps);
    
    if(p_s->sync)
        sysmem_freeptr(p_s->sync);
    p_s->sync = sysmem_newptrclear(argc);
    
    int resultcount=0;
    for(int i=0;i<argc;i++){
        for(int j=0;j<argc;j++){
            p_s->result[resultcount++] = p_s->polynom[i] * p_s->polynom[j];
        }
        p_s->sync[i] = p_s->arg_sum * p_s->polynom[i];
    }
    
    //PRINTING
    print(x);

}

void print(t_mx_square *x){
    t_schillinger *p_s = &x->t;
    
    t_atom result[p_s->steps];
    t_atom sync[p_s->p_len];
    
    for(int i=0;i<p_s->steps;i++){
        atom_setlong(result+i, p_s->result[i]);
    }
    for(int i=0;i<p_s->p_len;i++){
        atom_setlong(sync+i, p_s->sync[i]);
    }
    
    outlet_anything(x->pat_out, gensym("pat"), p_s->steps, result);
    outlet_anything(x->sync_out, gensym("pat"), p_s->p_len, sync);
}

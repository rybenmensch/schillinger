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
#include <stdarg.h>

typedef struct _schillinger{
    long a;
    long b;
    t_ptr a_pat;
    t_ptr b_pat;
    t_ptr r_pat;
    long steps;
} t_schillinger;

typedef struct _mxp1_nsg {
    t_object p_ob;
    t_schillinger t;
    void *step_out;
    void *r_out;
    void *a_out;
    void *b_out;
    char* out_names[4];
} t_mxp1_nsg;

void *mxp1_nsg_new(t_symbol *s,  long argc, t_atom *argv);
void mxp1_nsg_free(t_mxp1_nsg *x);
void mxp1_nsg_gen(t_mxp1_nsg *x, long a, long b);
void mxp1_nsg_assist(t_mxp1_nsg *x, void *b, long m, long a, char *s);
void mxp1_nsg_bang(t_mxp1_nsg *x);
void outlet_s(t_mxp1_nsg *x, char *selector, int argc, char *msg, ...);
void mx_outlet(t_mxp1_nsg *x, char *pre, int a, int b, int c);

t_class *mxp1_nsg_class;

void ext_main(void *r){
    t_class *c;
    
    c = class_new("mxp1", (method)mxp1_nsg_new, (method)mxp1_nsg_free, sizeof(t_mxp1_nsg), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mxp1_nsg_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mxp1_nsg_bang, "bang", 0);
    class_addmethod(c, (method)mxp1_nsg_gen, "gen", A_LONG, A_LONG, 0);
    
    class_register(CLASS_BOX, c);
    mxp1_nsg_class = c;

}

void *mxp1_nsg_new(t_symbol *s, long argc, t_atom *argv){
    //allocate class
    t_mxp1_nsg *x = (t_mxp1_nsg *)object_alloc(mxp1_nsg_class);
    
    x->step_out = outlet_new((t_object *)x, NULL);
    x->b_out    = outlet_new((t_object *)x, NULL);
    x->a_out    = outlet_new((t_object *)x, NULL);
    x->r_out    = outlet_new((t_object *)x, NULL);

    x->out_names[0] = "r";
    x->out_names[1] = "a";
    x->out_names[2] = "b";
    x->out_names[3] = "stp";
    
    t_schillinger *p_s = &x->t;

    
    p_s->steps = 1; //init to one, lest we get divide by zero error later on
    p_s->a = 0;
    p_s->b = 0;
    p_s->a_pat = sysmem_newptrclear(p_s->steps * sizeof(int));
    p_s->b_pat = sysmem_newptrclear(p_s->steps * sizeof(int));
    p_s->r_pat = sysmem_newptrclear(p_s->steps * sizeof(int));
    
    if(argc == 2){
        p_s->a = atom_getlong(argv);
        p_s->b = atom_getlong(argv+1);
    }
    
    return (x);
}

void mxp1_nsg_free(t_mxp1_nsg *x){
    t_schillinger *p_s = &x->t;
    
    sysmem_freeptr(p_s->r_pat);
    sysmem_freeptr(p_s->a_pat);
    sysmem_freeptr(p_s->b_pat);
}

void mxp1_nsg_assist(t_mxp1_nsg *x, void *b, long m, long a, char *s){
    if(m == ASSIST_INLET){
        switch(a){
            case 0:
                sprintf(s, "(gen) Generator pair | (bang) Output patterns");
                break;
        }
    }else{
        switch(a){
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
                sprintf(s, "To step visualiser");
                break;
        }
    }
}

void mxp1_nsg_bang(t_mxp1_nsg *x){
    t_schillinger *p_s = &(x->t);
    
    if(p_s->a && p_s->b){
        mxp1_nsg_gen(x, p_s->a, p_s->b);
    }else{
        post("No generator pair received yet!");
    }
}

void mxp1_nsg_gen(t_mxp1_nsg *x, long a, long b){
    t_schillinger *p_s = &(x->t);
    p_s->steps = a*b;
    
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
    

    for(int i=0;i<newsize;i++){
        p_s->r_pat[i]=0;
        p_s->a_pat[i]=0;
        p_s->b_pat[i]=0;
    }
        
    for(int i=0;i<4;i++){
        outlet_s(x, x->out_names[i], 1, "clear");
        outlet_s(x, x->out_names[i], 2, "rows", 1);
        outlet_s(x, x->out_names[i], 2, "columns", (int)p_s->steps);
    }
    
    outlet_int(x->step_out, 1);
    outlet_int(x->step_out, p_s->steps);
    
    for(int i=0;i<p_s->steps;i+=a){
        //a
        p_s->a_pat[i] = 1;
        mx_outlet(x, "a", i, 0, 1);
        
        //r
        p_s->r_pat[i] = 1;
        mx_outlet(x, "r", i, 0, 1);
    }
    
    for(int i=0;i<p_s->steps;i+=b){
        p_s->b_pat[i] = 1;
        mx_outlet(x, "b", i, 0, 1);
        
        p_s->r_pat[i] = 1;
        mx_outlet(x, "r", i, 0, 1);
    }
    
    //print out the patterns
    
    t_atom atom_r_pat[p_s->steps];
    t_atom atom_a_pat[p_s->steps];
    t_atom atom_b_pat[p_s->steps];
    
    for(int i=0;i<p_s->steps;i++){
        atom_setlong(atom_r_pat+i, p_s->r_pat[i]);
        atom_setlong(atom_a_pat+i, p_s->a_pat[i]);
        atom_setlong(atom_b_pat+i, p_s->b_pat[i]);
    }
    
    outlet_anything(x->r_out, gensym("patbin"), p_s->steps, atom_r_pat);
    outlet_anything(x->a_out, gensym("patbin"), p_s->steps, atom_a_pat);
    outlet_anything(x->b_out, gensym("patbin"), p_s->steps, atom_b_pat);
}

void mx_outlet(t_mxp1_nsg *x, char *pre, int a, int b, int c){
    t_atom argv[3];
    atom_setlong(argv, a);
    atom_setlong(argv+1, b);
    atom_setlong(argv+2, c);
    outlet_anything(x->step_out, gensym(pre), 3, argv);
}

void outlet_s(t_mxp1_nsg *x, char *selector, int argc, char *msg, ...){
    //the first symbol message counts as argc as well
    t_atom argv[argc];
    int i, temp;
    va_list ap;
    va_start(ap, msg);
    atom_setsym(argv, gensym(msg));
    
    for(i=1;i<argc;i++){
        temp = va_arg(ap, int);
        atom_setlong(argv+i, temp);
    }
    
    outlet_anything(x->step_out, gensym(selector), argc, argv);
    va_end(ap);
}

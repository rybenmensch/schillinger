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
    long p_len;
    long sq_len;
    long c_len;
    long s_len;
    long s2_len;
    t_ptr polynom;
    t_ptr square;
    t_ptr cube;
    t_ptr sync;
    t_ptr sync2;
} t_schillinger;

typedef struct _mx_cube {
    t_object p_ob;
    t_schillinger t;
    void *square_out;
    void *cube_out;
    void *sync_out;
    void *sync2_out;
} t_mx_cube;

void *mx_cube_new(t_symbol *s,  long argc, t_atom *argv);
void mx_cube_free(t_mx_cube *x);
void mx_cube_pat(t_mx_cube *x,t_symbol *s, long argc, t_atom *argv);
void mx_cube_assist(t_mx_cube *x, void *b, long m, long a, char *s);
void mx_cube_bang(t_mx_cube *x);

void print(t_mx_cube *x);

t_class *mx_cube_class;

void ext_main(void *r){
    t_class *c;
    
    c = class_new("mx-cube", (method)mx_cube_new, (method)mx_cube_free, sizeof(t_mx_cube), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_cube_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mx_cube_bang, "bang", 0);
    class_addmethod(c, (method)mx_cube_pat, "pat", A_GIMME, 0);
    
    class_register(CLASS_BOX, c);
    mx_cube_class = c;
}

void *mx_cube_new(t_symbol *s, long argc, t_atom *argv){
    //allocate class
    t_mx_cube *x = (t_mx_cube *)object_alloc(mx_cube_class);
    
    x->sync2_out = outlet_new((t_object *)x, NULL);
    x->sync_out = outlet_new((t_object *)x, NULL);
    //x->square_out = outlet_new((t_object *)x, NULL);
    x->cube_out = outlet_new((t_object *)x, NULL);
    
    t_schillinger *p_s = &x->t;

    p_s->polynom = NULL;
    p_s->square = NULL;
    p_s->cube = NULL;
    p_s->sync = NULL;
    p_s->sync2 = NULL;
    return (x);
}

void mx_cube_free(t_mx_cube *x){
    t_schillinger *p_s = &x->t;
    if(p_s->polynom)
        sysmem_freeptr(p_s->polynom);
    if(p_s->square)
        sysmem_freeptr(p_s->square);
    if(p_s->cube)
        sysmem_freeptr(p_s->cube);
    if(p_s->sync)
        sysmem_freeptr(p_s->sync);
    if(p_s->sync2)
        sysmem_freeptr(p_s->sync2);
}

void mx_cube_assist(t_mx_cube *x, void *b, long m, long a, char *s){
    if(m == ASSIST_INLET){
        switch(a){
            case 0:
                sprintf(s, "(pat) Pattern to be cubed");
                break;
        }
    }else{
        switch(a){
            case 0:
                sprintf(s, "Cubed resultant pattern");
                break;
                /*
            case 1:
                sprintf(s, "Squared resultant pattern");
                break;
                 */
            case 1:
                sprintf(s, "Square synced to cube pattern");
                break;
            case 2:
                sprintf(s, "Input pattern synced to cube pattern");
                break;
        }
    }
}

void mx_cube_bang(t_mx_cube *x){
    t_schillinger *p_s = &x->t;
    
    if(!p_s->cube){
        post("No pattern received yet.");
        return;
    }
    
    print(x);
}

void mx_cube_pat(t_mx_cube *x,t_symbol *s, long argc, t_atom *argv){
    if(!argc)   //no arguments, do nothing and exit
        return;
    
    t_schillinger *p_s = &x->t;
    
    p_s->p_len = argc;
    p_s->sq_len = pow(argc, 2);
    p_s->c_len = pow(argc, 3);
    p_s->s_len = pow(argc, 2);
    p_s->s2_len = argc;
    
    if(p_s->cube)
        sysmem_freeptr(p_s->cube);
    p_s->cube = sysmem_newptrclear(p_s->c_len);
    
    if(p_s->polynom)
        sysmem_freeptr(p_s->polynom);
    p_s->polynom = sysmem_newptrclear(p_s->p_len);
    
    if(p_s->square)
        sysmem_freeptr(p_s->square);
    p_s->square = sysmem_newptrclear(p_s->sq_len);
    
    if(p_s->sync)
        sysmem_freeptr(p_s->sync);
    p_s->sync = sysmem_newptrclear(p_s->s_len);
    
    if(p_s->sync2)
        sysmem_freeptr(p_s->sync2);
    p_s->sync2 = sysmem_newptrclear(p_s->s2_len);
   
    long arg_sum = 0;
    for(int i=0;i<p_s->p_len;i++){
        p_s->polynom[i] = atom_getlong(argv+i);
        arg_sum += p_s->polynom[i];
    }
    
    int resultcount=0;
    long arg_square = arg_sum * arg_sum;
    for(int i=0;i<p_s->p_len;i++){
        for(int j=0;j<p_s->p_len;j++){
            p_s->square[resultcount++] = p_s->polynom[i] * p_s->polynom[j];
        }
        p_s->sync2[i] = arg_square * p_s->polynom[i];
    }

    for(int i=0;i<p_s->sq_len;i++){
        p_s->sync[i] = arg_sum * p_s->square[i];
    }
    
    resultcount = 0;
    
    for(int i=0;i<p_s->p_len;i++){
        for(int j=0;j<p_s->sq_len;j++){
            p_s->cube[resultcount++] = p_s->polynom[i] * p_s->square[j];
        }
    }
    
    print(x);
}

void print(t_mx_cube *x){
    t_schillinger *p_s = &x->t;
    
    t_atom square[p_s->sq_len];
    t_atom cube[p_s->c_len];
    t_atom sync[p_s->sq_len];
    t_atom sync2[p_s->p_len];
    
    for(int i=0;i<p_s->c_len;i++){
        atom_setlong(cube+i, p_s->cube[i]);
    }

    for(int i=0;i<p_s->sq_len;i++){
        atom_setlong(square+i, p_s->square[i]);
        atom_setlong(sync+i, p_s->sync[i]);
    }
    
    for(int i=0;i<p_s->s2_len;i++){
        atom_setlong(sync2+i, p_s->sync2[i]);
    }
    
    //outlet_anything(x->square_out, gensym("pat"), p_s->sq_len, square);
    outlet_anything(x->cube_out, gensym("pat"), p_s->c_len, cube);
    outlet_anything(x->sync_out, gensym("pat"), p_s->s_len, sync);
    outlet_anything(x->sync2_out, gensym("pat"), p_s->s2_len, sync2);
}

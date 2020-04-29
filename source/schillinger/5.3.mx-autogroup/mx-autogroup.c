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

typedef struct _mx_autogroup {
    t_object p_ob;
    void *group_out;
    void *list_out;
    void *umenu_out;
    t_atom_long *pattern;
    long pat_len;
    t_atom *output;
    t_atom_long group_size;
    t_atom_long grouper;
} t_mx_autogroup;

void *mx_autogroup_new(t_symbol *s,  long argc, t_atom *argv);
void mx_autogroup_free(t_mx_autogroup *x);
void mx_autogroup_assist(t_mx_autogroup *x, void *b, long m, long a, char *s);
void mx_autogroup_bang(t_mx_autogroup *x);
void mx_autogroup_pat(t_mx_autogroup *x, t_symbol *s, long argc, t_atom *argv);
void mx_autogroup_patbin(t_mx_autogroup *x, t_symbol *s, long argc, t_atom *argv);
void mx_autogroup_groupby(t_mx_autogroup *x, long grouper);

t_class *mx_autogroup_class;

void ext_main(void *r){
    t_class *c;
    
    c = class_new("mx-autogroup", (method)mx_autogroup_new, (method)mx_autogroup_free, sizeof(t_mx_autogroup), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_autogroup_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mx_autogroup_bang, "bang", 0);
    class_addmethod(c, (method)mx_autogroup_pat, "pat", A_GIMME, 0);
    class_addmethod(c, (method)mx_autogroup_patbin, "patbin", A_GIMME, 0);
    class_addmethod(c, (method)mx_autogroup_groupby, "groupby", A_LONG, 0);
    
    class_register(CLASS_BOX, c);
    mx_autogroup_class = c;
}

void *mx_autogroup_new(t_symbol *s, long argc, t_atom *argv){
    t_mx_autogroup *x = (t_mx_autogroup *)object_alloc(mx_autogroup_class);

    x->umenu_out = outlet_new((t_object *)x, NULL);
    x->list_out = outlet_new((t_object *)x, NULL);
    x->group_out = outlet_new((t_object *)x, NULL);
    
    x->group_size = 0;
    x->grouper = 0;
    x->pat_len = 1;
    x->pattern = NULL;
    x->output = NULL;
    
    return (x);
}

void mx_autogroup_free(t_mx_autogroup *x){
    if(x->pattern)
        sysmem_freeptr(x->pattern);
    
    if(x->output){
        sysmem_freeptr(x->output);
        
    }
}

void mx_autogroup_assist(t_mx_autogroup *x, void *b, long m, long a, char *s){
    if(m == ASSIST_INLET){
        switch(a){
            case 0:
                sprintf(s, "(pat) Pattern to be grouped");
                break;
        }
    }else{
        switch(a){
            case 0:
                sprintf(s, "(message) Grouped pattern");
                break;
            case 1:
                sprintf(s, "(list) Possible groupings in list form");
                break;
            case 2:
                sprintf(s, "(message) Connect to umenu to see possible groupings");
                break;
        }
    }
}

void mx_autogroup_pat(t_mx_autogroup *x, t_symbol *s, long argc, t_atom *argv){
    int sum = 0;
    x->pat_len = argc;
    
    if(x->pattern)
        sysmem_freeptr(x->pattern);
    
    x->pattern = (t_atom_long *)sysmem_newptrclear(x->pat_len * sizeof(t_atom_long));
    
    t_atom_long max = 0;
    
    for(int i=0;i<argc;i++){
        x->pattern[i] = atom_getlong(argv+i);
        if(x->pattern[i]>max){
            max = x->pattern[i];
        }
        sum += x->pattern[i];
    }
    t_atom listout[sum];
    int loc = 0;
    outlet_anything(x->umenu_out, gensym("clear"), 0, NIL);
    
    for(int i=2;i<sum;i++){
        if(sum % i == 0 && i >= max){
            atom_setlong(listout+loc, i);
            loc++;
            t_atom out[1];
            atom_setlong(out, i);
            outlet_anything(x->umenu_out, gensym("append"), 1, out);
        }
    }
    
    outlet_list(x->list_out, NULL, loc, listout);
}

void mx_autogroup_groupby(t_mx_autogroup *x, long grouper){
    x->grouper = grouper;
    if(!x->pattern){
        post("No pattern received yet.");
        return;
    }
    
    t_atom_long sum = 0, group_size = 0, group_amt = 0;
    if(x->output){
        sysmem_freeptr(x->output);
    }
    x->output = (t_atom *)sysmem_newptrclear(2 * x->pat_len * sizeof(t_atom));
    
    for(int i=0;i<x->pat_len;i++){
        if(sum == grouper || sum == 0){
            /*
            if(group_amt<10){
                snprintf(s, 3, "g%lld", group_amt++);
            }else{
                snprintf(s, 4, "g%lld", group_amt++);
            }
            atom_setsym(x->output+group_size++, gensym(s));
             */
            group_amt++;
            atom_setsym(x->output+group_size++, gensym("g"));
            sum = 0;
        }
        atom_setlong(x->output+group_size++, x->pattern[i]);
        sum += x->pattern[i];
    }
    x->group_size = group_size;
    outlet_anything(x->group_out, gensym("group"), group_size, x->output);
}

void mx_autogroup_bang(t_mx_autogroup *x){
    if(!x->pattern){
        post("No pattern received yet.\n");
        return;
    }else if(x->grouper==0){
        post("No grouping received yet.\n");
        return;
    }else{
        outlet_anything(x->group_out, gensym("group"), x->group_size, x->output);
    }
}

void mx_autogroup_patbin(t_mx_autogroup *x, t_symbol *s, long argc, t_atom *argv){
    if(atom_getlong(argv)==0){
        post("(patbin) may not start with 0.");
        return;
    }
    
    t_atom_long patbin[argc];
    t_atom_long patbin_length = argc;
    
    for(int i=0;i<argc;i++){
        patbin[i] = atom_getlong(argv+i);
    }
    
    int beatcount = 0;
    
    for(int i=0;i<argc;i++){
        beatcount += patbin[i];
    }
    
    t_atom_long lpat[beatcount+1];
    t_atom_long result[beatcount];
    x->pat_len = beatcount;
    
    if(x->pattern)
        sysmem_freeptr(x->pattern);
    x->pattern = (t_atom_long *)sysmem_newptrclear(x->pat_len * sizeof(t_atom_long));
    
    int counter=0, rest=0;
    t_bool first_rest = FALSE;
    for(int i=0;i<argc;i++){
        if(patbin[i]==1){
            lpat[counter++] = i;
            first_rest = FALSE;
        }
        if(first_rest){
            rest++;
        }
        if(i==0 && patbin[i]==0){
            first_rest = TRUE;
            rest++;
        }
    }
    
    lpat[beatcount] = argc;
    for(int i=0;i<beatcount;i++){
        result[i] = lpat[i+1]-lpat[i];
    }
    if(rest){
        //rest functionality is disabled for now, because no other objects supports it, and there is no time
        //to implement proper functionality. maybe for later release..
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
    }else{
        t_atom pat[beatcount];
        for(int i=0;i<beatcount;i++){
            atom_setlong(pat+i, result[i]);
        }
        mx_autogroup_pat(x, gensym(""), beatcount, pat);
        //outlet_anything(x->msg_out, gensym("pat"), beatcount, pat);
    }
}

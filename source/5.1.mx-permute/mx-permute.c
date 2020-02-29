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

typedef struct _mx_permute {
    t_object p_ob;
    void *pat_out;
    void *patbin_out;
    void *int_out;
    void *bang_out;
    t_atom_long *pattern;
    t_atom_long **permutations;
    long unq_perm;
    long pat_len;
    t_bool circ_mode;
} t_mx_permute;

void *mx_permute_new(t_symbol *s,  long argc, t_atom *argv);
void mx_permute_free(t_mx_permute *x);
void mx_permute_assist(t_mx_permute *x, void *b, long m, long a, char *s);
void mx_permute_bang(t_mx_permute *x);
void mx_permute_pat(t_mx_permute *x, t_symbol *s, long argc, t_atom *argv);
void mx_permute_patbin(t_mx_permute *x, t_symbol *s, long argc, t_atom *argv);
void mx_permute_recall(t_mx_permute *x, long a);
void mx_permute_recallbin(t_mx_permute *x, long a);
void mx_permute_circular(t_mx_permute *x, long shift);
void mx_permute_anticircular(t_mx_permute *x, long shift);

void circ_clw(t_mx_permute *x, t_atom_long output[], long shift);
void circ_aclw(t_mx_permute *x, t_atom_long array[], long shift);

long factorial(long n);
void swap(t_atom_long *dest, t_atom_long *src);
int shouldSwap(t_atom_long *arr, t_atom_long start, t_atom_long curr);
void findPermutations(t_atom_long *arr, t_atom_long index, t_atom_long n, t_atom_long **result, int *r_index);
void augment_entry(t_hashtab *t, long key);

t_class *mx_permute_class;

void ext_main(void *r){
    t_class *c;
    
    c = class_new("mx-permute", (method)mx_permute_new, (method)mx_permute_free, sizeof(t_mx_permute), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_permute_assist, "assist", A_CANT, 0);
    //class_addmethod(c, (method)mx_permute_bang, "bang", 0);
    class_addmethod(c, (method)mx_permute_pat, "pat", A_GIMME, 0);
    class_addmethod(c, (method)mx_permute_patbin, "patbin", A_GIMME, 0);
    class_addmethod(c, (method)mx_permute_recall, "recall", A_LONG, 0);
    class_addmethod(c, (method)mx_permute_circular, "circular", A_LONG, 0);
    class_addmethod(c, (method)mx_permute_anticircular, "anticircular", A_LONG, 0);
    
    CLASS_ATTR_LONG(c, "circ_mode", 0, t_mx_permute, circ_mode);
    CLASS_ATTR_ENUM(c, "circ_mode", 0, "forwards reverse");
    CLASS_ATTR_STYLE(c, "circ_mode", 0, "enumindex");
    CLASS_ATTR_FILTER_CLIP(c, "circ_mode", 0, 1);

    class_register(CLASS_BOX, c);
    mx_permute_class = c;
}

void *mx_permute_new(t_symbol *s, long argc, t_atom *argv){
    //allocate class
    t_mx_permute *x = (t_mx_permute *)object_alloc(mx_permute_class);
    x->bang_out = bangout((t_object *)x);
    x->int_out = outlet_new((t_object *)x, NULL);
    x->patbin_out = outlet_new((t_object *)x, NULL);
    x->pat_out = outlet_new((t_object *)x, NULL);
    x->pat_len = 1;
    x->pattern = NULL;
    x->permutations = NULL;
    x->circ_mode = 0;

    attr_args_process(x, argc, argv);
    return (x);
}

void mx_permute_free(t_mx_permute *x){
    if(x->permutations){
        for(int i=0;i<x->unq_perm;i++){
            sysmem_freeptr(x->permutations[i]);
        }
        sysmem_freeptr(x->permutations);
    }
    
    if(x->pattern){
        sysmem_freeptr(x->pattern);
    }
}

void mx_permute_assist(t_mx_permute *x, void *b, long m, long a, char *s){
    if(m == ASSIST_INLET){
        switch(a){
            case 0:
                sprintf(s, "Messages in");
                break;
        }
    }else{
        switch(a){
            case 0:
                sprintf(s, "(pat) Permuted pattern");
                break;
            case 1:
                sprintf(s, "(patbin) Permuted pattern");
                break;
            case 2:
                sprintf(s, "(int) Amount of permutations");
                break;
            case 3:
                sprintf(s, "(bang) when highest permutation is reached");
                break;
        }
    }
}

void mx_permute_bang(t_mx_permute *x){
    if(!x->pattern){
        post("No pattern received yet.\n");
        return;
    }
}

void mx_permute_pat(t_mx_permute *x, t_symbol *s, long argc, t_atom *argv){
    /*
    if(atom_getlong(argv) == 0){
        post("(pat) may not start with 0.");
        return;
    }
    */
     
    t_hashtab *tab = (t_hashtab *)hashtab_new(0);
    t_atom_long args[argc];
    x->pat_len = argc;
    long total_perm = factorial(argc);
    
    if(x->pattern){
        sysmem_freeptr(x->pattern);
    }
    x->pattern = (t_atom_long*)sysmem_newptr(x->pat_len * sizeof(t_atom_long));
    
    for(int i=0;i<argc;i++){
        long current = atom_getlong(argv+i);
        args[i] = current;
        x->pattern[i] = current;
        hashtab_storelong(tab, (t_symbol *)args[i], 0);
    }
    
    for(int i=0;i<argc;i++){
        long current = args[i];
        augment_entry(tab, current);
    }
    
    long unq_perm_div = 1;
    
    t_symbol    **keys = NULL;
    long        numKeys = hashtab_getsize(tab);
    t_atom_long vals[numKeys];
    hashtab_getkeys(tab, &numKeys, &keys);
    
    for(int i=0;i<numKeys;i++){
        hashtab_lookuplong(tab, keys[i], vals+i);
        unq_perm_div *= factorial(vals[i]);
    }
    
    if(x->permutations){
        for(int i=0;i<x->unq_perm;i++){
            sysmem_freeptr(x->permutations[i]);
        }
        
        sysmem_freeptr(x->permutations);
    }
    
    x->unq_perm = (long)total_perm/unq_perm_div;
    
    x->permutations = (t_atom_long **)sysmem_newptrclear(x->unq_perm*sizeof(t_atom_long *));
    
    for(int i=0;i<x->unq_perm;i++){
        x->permutations[i] = (t_atom_long *)sysmem_newptrclear(argc*sizeof(t_atom_long));
    }
    
    int r_index = 0;
    findPermutations(args, 0, argc, x->permutations, &r_index);
    
    mx_permute_recall(x, 0);
    outlet_int(x->int_out, x->unq_perm);
    object_free((t_object *)tab);
}

void mx_permute_patbin(t_mx_permute *x, t_symbol *s, long argc, t_atom *argv){
    if(atom_getlong(argv) == 0){
        post("(patbin) may not start with 0.");
        return;
    }
    
    t_atom_long binpat[argc];
    
    for(int i=0;i<argc;i++){
        binpat[i] = atom_getlong(argv+i);
    }
    
    int beatcount = 0;

    for(int i=0;i<argc;i++){
        beatcount += binpat[i];
    }
    
    t_atom_long lpat[beatcount+1];
    t_atom_long result[beatcount];
    int counter=0;
    
    for(int i=0;i<argc;i++){
        if(binpat[i]==1){
            lpat[counter++] = i;
        }
    }
    lpat[beatcount] = argc;

    t_atom pat[beatcount];
    
    for(int i=0;i<beatcount;i++){
        result[i] = lpat[i+1]-lpat[i];
        atom_setlong(pat+i, result[i]);
    }
    
    mx_permute_pat(x, NULL, beatcount, pat);
}

void circ_clw(t_mx_permute *x, t_atom_long output[], long shift){
    t_atom argv[x->pat_len];

    for(int j=0;j<shift;j++){
        t_atom_long temp = output[0];
        t_atom_long temparray[x->pat_len];
        
        for(int i=0;i<x->pat_len;i++){
            temparray[i] = output[i];
        }
        
        for(int i=0;i<x->pat_len-1;i++){
            output[i] = temparray[i+1];
        }
        
        output[x->pat_len-1] = temp;
    }
    
    for(int i=0;i<x->pat_len;i++){
        atom_setlong(argv+i, output[i]);
    }
    
    outlet_anything(x->pat_out, gensym("pat"), x->pat_len, argv);
}

void circ_aclw(t_mx_permute *x, t_atom_long output[], long shift){
    t_atom argv[x->pat_len];
    
    for(int j=0;j<shift;j++){
        t_atom_long temp = output[x->pat_len-1];
        long temparray[x->pat_len];
        
        for(int i=0;i<x->pat_len;i++){
            temparray[i] = output[i];
        }
        
        for(int i=0;i<x->pat_len-1;i++){
            output[i+1] = temparray[i];
        }
        
        output[0] = temp;
    }
    
    for(int i=0;i<x->pat_len;i++){
        atom_setlong(argv+i, output[i]);
    }
    
    outlet_anything(x->pat_out, gensym("pat"), x->pat_len, argv);
}


void mx_permute_circular(t_mx_permute *x, long shift){
    //begriffe klären, clockwise, anticlockwise, circular, anticircular?
    //circ_mode == 0: clockwise
    //circ_mode == 1: anticlockwise
    if(!x->pattern){
        post("No pattern received yet.\n");
        return;
    }
    
    shift = abs((int)shift);
    t_atom_long output[x->pat_len];
    
    memcpy(output, x->pattern, x->pat_len * sizeof(t_atom_long));
    switch(x->circ_mode){
        case 0:
            circ_clw(x, output, shift);
            break;
        case 1:
            circ_aclw(x, output, shift);
            break;
    }
}

void mx_permute_anticircular(t_mx_permute *x, long shift){
    //circ_mode == 0: forwards
    //circ_mode == 1: reverse
    if(!x->pattern){
        post("No pattern received yet.\n");
        return;
    }
    
    shift = abs((int)shift);
    t_atom_long output[x->pat_len];
    
    for(int i=1;i<x->pat_len;i++){
        output[i] = x->pattern[x->pat_len-i];
    }
    output[0] = x->pattern[0];
    
    switch(x->circ_mode){
        case 0:
            circ_clw(x, output, shift);
            break;
        case 1:
            circ_aclw(x, output, shift);
            break;
    }
}

void mx_permute_recall(t_mx_permute *x, long a){
    if(!x->permutations){
        post("No pattern received yet.");
        return;
    }
    a = CLAMP(a, 1, x->unq_perm);
    a--;
    if(a==x->unq_perm-1){
        outlet_bang(x->bang_out);
    }
    
    t_atom argv[x->pat_len];
    for(int i=0;i<x->pat_len;i++){
        atom_setlong(argv+i, x->permutations[a][i]);
    }
    outlet_anything(x->pat_out, gensym("pat"), x->pat_len, argv);

    mx_permute_recallbin(x, a);
}

void mx_permute_recallbin(t_mx_permute *x, long a){
    if(!x->permutations){
        post("No pattern received yet.");
        return;
    }
    
    a = CLAMP(a, 0, x->unq_perm-1);
    
    long pat_sum = 0;

    for(int i=0;i<x->pat_len;i++){
        pat_sum += x->permutations[a][i];
    }
    
    if(x->permutations[a][0] == 0 && x->pat_len==1){
        t_atom argv[1];
        atom_setlong(argv, 0);
        outlet_anything(x->patbin_out, gensym("patbin"), 1, argv);
        return;
    }
    
    t_ptr patbin = sysmem_newptrclear(pat_sum * sizeof(t_ptr));
    
    int other = 0;
    
    for(int i=0;i<x->pat_len;i++){
        patbin[other] = 1;
        other+=x->permutations[a][i];
    }
    
    t_atom argv[pat_sum];
    for(int i=0;i<pat_sum;i++){
        atom_setlong(argv+i, patbin[i]);
    }
    
    outlet_anything(x->patbin_out, gensym("patbin"), pat_sum, argv);
    sysmem_freeptr(patbin);
}

long factorial(long n){
    if(n==0){
        return 1;
    }else{
        return (n*factorial(n-1));
    }
}

void swap(t_atom_long *dest, t_atom_long *src){
    t_atom_long tmp = *src;
    *src = *dest;
    *dest = tmp;
}

int shouldSwap(t_atom_long *arr, t_atom_long start, t_atom_long curr){
    for(t_atom_long i=start;i<curr;i++){
        if(arr[i] == arr[curr]){
            return 0;
        }
    }
    return 1;
}

void findPermutations(t_atom_long *arr, t_atom_long index, t_atom_long n, t_atom_long **result, int *r_index){
    if(index>=n){
        memcpy(result[*r_index], arr, (size_t)n*sizeof(t_atom_long *));
        
        (*r_index)++;
        return;
    }
    
    for(t_atom_long i=index;i<n;i++){
        int check = shouldSwap(arr, index, i);
        if(check){
            swap(&(arr[index]), &(arr[i]));
            findPermutations(arr, index+1, n, result, r_index);
            swap(&(arr[index]), &(arr[i]));
        }
    }
}

void augment_entry(t_hashtab *t, long key){
    t_atom_long val;
    hashtab_lookuplong(t, (t_symbol *)key, &val);
    val++;
    hashtab_storelong(t, (t_symbol *)key, val);
}

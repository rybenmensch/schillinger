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

typedef struct _mx_permute_groups {
    t_object p_ob;
    void *pat_out;
    void *patbin_out;
    void *int_out;
    void *bang_out;
    t_atom_long group_amt;
    t_atom_long **groups;
    t_symbol **sym_groups;
    t_symbol ***permutations;
    t_atom_long unq_perm;
    t_bool circ_mode;
} t_mx_permute_groups;

void *mx_permute_groups_new(t_symbol *s, long argc, t_atom *argv);
void mx_permute_groups_free(t_mx_permute_groups *x);
void mx_permute_groups_assist(t_mx_permute_groups *x, void *b, long m, long a, char *s);
void mx_permute_groups_group(t_mx_permute_groups *x, t_symbol *s, long argc, t_atom *argv);
void mx_permute_groups_recall(t_mx_permute_groups *x, long a);
void mx_permute_groups_circular(t_mx_permute_groups *x, long shift);
void mx_permute_groups_anticircular(t_mx_permute_groups *x, long shift);

void circ_clw(t_mx_permute_groups *x, t_symbol **output, long shift);
void circ_aclw(t_mx_permute_groups *x, t_symbol **array, long shift);

void findPermutations(t_symbol **arr, t_atom_long index, t_atom_long n, t_symbol ***result, int *r_index);
int shouldSwap(t_symbol **arr, t_atom_long start, t_atom_long curr);
void swap(t_symbol **dest, t_symbol **src);
void augment_entry(t_hashtab *t, t_symbol *key);
long factorial(long n);

void print(t_mx_permute_groups *x, t_symbol **s);

t_class *mx_permute_groups_class;

void ext_main(void *r){
    t_class *c;
    
    c = class_new("mx-permute-groups", (method)mx_permute_groups_new, (method)mx_permute_groups_free, sizeof(t_mx_permute_groups), NULL, A_GIMME, 0);
    class_addmethod(c, (method)mx_permute_groups_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)mx_permute_groups_group, "group", A_GIMME, 0);
    class_addmethod(c, (method)mx_permute_groups_recall, "recall", A_LONG, 0);
    class_addmethod(c, (method)mx_permute_groups_circular, "circular", A_LONG, 0);
    class_addmethod(c, (method)mx_permute_groups_anticircular, "anticircular", A_LONG, 0);

    CLASS_ATTR_LONG(c, "circ_mode", 0, t_mx_permute_groups, circ_mode);
    CLASS_ATTR_ENUM(c, "circ_mode", 0, "forwards reverse");
    CLASS_ATTR_STYLE(c, "circ_mode", 0, "enumindex");
    CLASS_ATTR_FILTER_CLIP(c, "circ_mode", 0, 1);
    
    class_register(CLASS_BOX, c);
    mx_permute_groups_class = c;
}

void *mx_permute_groups_new(t_symbol *s, long argc, t_atom *argv){
    //allocate class
    t_mx_permute_groups *x = (t_mx_permute_groups *)object_alloc(mx_permute_groups_class);
    x->bang_out = bangout((t_object *)x);
    x->int_out = outlet_new((t_object *)x, NULL);
    x->patbin_out = outlet_new((t_object *)x, NULL);
    x->pat_out = outlet_new((t_object *)x, NULL);

    x->groups = NULL;
    x->sym_groups = NULL;
    
    x->circ_mode = 0;
    attr_args_process(x, argc, argv);
    
    return (x);
}
void mx_permute_groups_free(t_mx_permute_groups *x){
    if(x->groups){
        for(int i=0;i<x->group_amt;i++){
            sysmem_freeptr(x->groups[i]);
        }
        sysmem_freeptr(x->groups);
    }
    
    if(x->sym_groups){
        sysmem_freeptr(x->sym_groups);
    }
    
    if(x->permutations){
        for(int i=0;i<x->unq_perm;i++){
            sysmem_freeptr(x->permutations[i]);
        }
    }
    sysmem_freeptr(x->permutations);
}

void mx_permute_groups_assist(t_mx_permute_groups *x, void *b, long m, long a, char *s){
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
                sprintf(s, "(int) Amount of permutations");
                break;
            case 2:
                sprintf(s, "(bang) when highest permutation is reached");
                break;
        }
    }
}

void mx_permute_groups_circular(t_mx_permute_groups *x, long shift){
    shift--;
    //circ_mode == 0: clockwise
    //circ_mode == 1: anticlockwise
    if(!x->permutations){
        post("No pattern received yet.\n");
        return;
    }
    
    shift = abs((int)shift);
    t_symbol *output[x->group_amt];
    
    memcpy(output, x->sym_groups, x->group_amt * sizeof(t_symbol *));
    switch(x->circ_mode){
        case 0:
            circ_clw(x, output, shift);
            break;
        case 1:
            circ_aclw(x, output, shift);
            break;
    }
}

void mx_permute_groups_anticircular(t_mx_permute_groups *x, long shift){
    shift--;
     //circ_mode == 0: forwards
     //circ_mode == 1: reverse
     if(!x->permutations){
         post("No pattern received yet.\n");
         return;
     }
     
     shift = abs((int)shift);
     t_symbol *output[x->group_amt];
     
     for(int i=1;i<x->group_amt;i++){
         output[i] = x->sym_groups[x->group_amt-i];
     }
     output[0] = x->sym_groups[0];
     
     switch(x->circ_mode){
         case 0:
             circ_clw(x, output, shift);
             break;
         case 1:
             circ_aclw(x, output, shift);
             break;
     }
}

void circ_clw(t_mx_permute_groups *x, t_symbol **output, long shift){
    for(int j=0;j<shift;j++){
        t_symbol *temp = output[0];
        t_symbol *temparray[x->group_amt];
        
        for(int i=0;i<x->group_amt;i++){
            temparray[i] = output[i];
        }
        
        for(int i=0;i<x->group_amt-1;i++){
            output[i] = temparray[i+1];
        }
        
        output[x->group_amt-1] = temp;
    }
    
    print(x, output);
}

void circ_aclw(t_mx_permute_groups *x, t_symbol **output, long shift){
    for(int j=0;j<shift;j++){
        t_symbol *temp = output[x->group_amt-1];
        t_symbol *temparray[x->group_amt];
        
        for(int i=0;i<x->group_amt;i++){
            temparray[i] = output[i];
        }
        
        for(int i=0;i<x->group_amt-1;i++){
            output[i+1] = temparray[i];
        }
        
        output[0] = temp;
    }

    print(x, output);
}

void mx_permute_groups_group(t_mx_permute_groups *x, t_symbol *s, long argc, t_atom *argv){
    if(argc==1){
        post("Bad group pattern.");
        post("Usage: group G x y G z");
        return;
    }
    
    if(x->groups){
        for(int i=0;i<x->group_amt;i++){
            sysmem_freeptr(x->groups[i]);
        }
        sysmem_freeptr(x->groups);
    }
    
    //longs are  type 1, symbols are type 3
    //split by symbols to get groups
    int g_c = 0;    //group counter
    
    //get amount of groups (count symbols)
    for(int i=0;i<argc;i++){
        if(atom_gettype(argv+i) == 3){
            g_c++;
        }
    }
    x->group_amt = g_c;
    
    x->groups = (t_atom_long **)sysmem_newptrclear(x->group_amt * sizeof(t_atom_long *));
    int group_sizes[x->group_amt];
    g_c = 0;

    //get amt of elements per group and allocate memory accordingly
    int ing_c = 0;  //amt. of elements per group

    for(int i=1;i<=argc;i++){
        if(atom_gettype(argv+i)==3 || i==argc){
            x->groups[g_c] = (t_atom_long *)sysmem_newptrclear(ing_c * sizeof(t_atom_long));
            group_sizes[g_c] = ing_c;
            ing_c = 0;
            g_c++;
        }else{
            ing_c++;
        }
    }
    
    //fill the arrays with the actual values of the groups
    g_c = ing_c = 0;
    for(int i=1;i<argc;i++){
        if(atom_gettype(argv+i)==3){
            ing_c = 0;
            g_c++;
        }else{
            x->groups[g_c][ing_c++] = atom_getlong(argv+i);
        }
    }
    
    ////////////////////////
    //turn array of ints into array of symbols (turn into func?)
    
    if(x->sym_groups){
        sysmem_freeptr(x->sym_groups);
    }
    x->sym_groups = (t_symbol **)sysmem_newptr(x->group_amt * sizeof(t_symbol *));
    
    for(int i=0;i<x->group_amt;i++){
        char s[100];
        
        for(int j=0;j<group_sizes[i]*2-1;j++){
            if(j%2 == 0){
                sprintf(&s[j], "%lld", x->groups[i][j/2]);
            }else{
                sprintf(&s[j], " ");
            }
        }
        x->sym_groups[i] = gensym(s);
    }
    
    ////////////////////////
    //hash the symbols
    
    t_hashtab *tab = (t_hashtab *)hashtab_new(0);
    long total_perm = factorial(x->group_amt);
    
    for(int i=0;i<x->group_amt;i++){
        hashtab_storelong(tab, x->sym_groups[i], 0);
    }
    
    for(int i=0;i<x->group_amt;i++){
        t_symbol *current = x->sym_groups[i];
        augment_entry(tab, current);
    }
    
    ////////////////////////
    //calculate permutation amount
    
    long unq_perm_div = 1;
    
    t_symbol    **keys = NULL;
    long        numKeys = hashtab_getsize(tab);
    t_atom_long vals[numKeys];
    hashtab_getkeys(tab, &numKeys, &keys);
    
    for(int i=0;i<numKeys;i++){
        hashtab_lookuplong(tab, keys[i], vals+i);
        unq_perm_div *= factorial(vals[i]);
    }    
    
    x->unq_perm = (long)total_perm/unq_perm_div;

    //a pointer to (array of) arrays of t_symbol-pointers..
    x->permutations = (t_symbol ***)sysmem_newptrclear(x->unq_perm*sizeof(t_symbol **));
    
    for(int i=0;i<x->unq_perm;i++){
        //x->permutations[i] = (t_symbol **)sysmem_newptrclear(argc*sizeof(t_symbol *));
        x->permutations[i] = (t_symbol **)sysmem_newptrclear(x->group_amt*sizeof(t_symbol *));
    }

    int r_index = 0;
    
    findPermutations(x->sym_groups, 0, x->group_amt, x->permutations, &r_index);

    outlet_int(x->int_out, x->unq_perm);
    mx_permute_groups_recall(x, 0);
}

void mx_permute_groups_recall(t_mx_permute_groups *x, long a){
    if(!x->permutations){
        post("No pattern received yet.");
        return;
    }
    a = CLAMP(a, 1, x->unq_perm);
    a--;
    if(a==x->unq_perm-1){
        outlet_bang(x->bang_out);
    }
    
    print(x, x->permutations[a]);
}

void print(t_mx_permute_groups *x, t_symbol **s){
    //it's a mess..
    //needs a clean rewrite probably
    
    t_atom_long **long_array;
    long_array = (t_atom_long **)sysmem_newptrclear(x->group_amt * sizeof(t_atom_long *));
    
    //forgive the naming
    long array[500];
    long array_counter = 0;
    
    t_atom args[500]; //hope for no overflows :)
    int g_c = 0;
    long pat_sum = 0;
    
    for(int i=0;i<x->group_amt;i++){
        char *buff = s[i]->s_name;

        //count elements in each group
        int j=0;
        int c=0;
        while(buff[j] != '\0'){
            if(buff[j++] == ' '){
                ;
            }else{
                c++;
            }
        }
        long_array[i] = (t_atom_long *)sysmem_newptrclear(c * sizeof(t_atom_long));
        
        int k=0;
        for(j=0;j<c;j++){
            //this feels wrong, but don't have any other clue tbh
            //and it works, so..
            long_array[i][j] = atoi(&buff[k]);
            array[array_counter++] = long_array[i][j];
            pat_sum += long_array[i][j];
            atom_setlong((args+g_c), long_array[i][j]);
            k+=2;   //skip spaces
            g_c++;
        }
    }

    outlet_anything(x->pat_out, gensym("pat"), g_c, args);

    t_atom_long *patbin = (t_atom_long *)sysmem_newptrclear(pat_sum * sizeof(t_atom_long));
    
    int other = 0;
    for(int i=0;i<array_counter;i++){
        patbin[other] = 1;
        other+=array[i];
    }
    
    t_atom argv[pat_sum];
    for(int i=0;i<pat_sum;i++){
        atom_setlong(argv+i, patbin[i]);
    }
    
    outlet_anything(x->patbin_out, gensym("patbin"), pat_sum, argv);
    sysmem_freeptr(patbin);
    
    for(int i=0;i<x->group_amt;i++){
        sysmem_freeptr(long_array[i]);
    }
    sysmem_freeptr(long_array);
}

/************************************************/

long factorial(long n){
    if(n==0){
        return 1;
    }else{
        return (n*factorial(n-1));
    }
}

void swap(t_symbol **dest, t_symbol **src){
    t_symbol *tmp = *src;
    *src = *dest;
    *dest = tmp;
}

int shouldSwap(t_symbol **arr, t_atom_long start, t_atom_long curr){
    for(t_atom_long i=start;i<curr;i++){
        if(arr[i] == arr[curr]){
            return 0;
        }
    }
    return 1;
}

void findPermutations(t_symbol **arr, t_atom_long index, t_atom_long n, t_symbol ***result, int *r_index){
    if(index>=n){
        
        memcpy(result[*r_index], arr, (size_t)n*sizeof(t_symbol **));
        
        (*r_index)++;
        return;
    }
    
    for(t_atom_long i=index;i<n;i++){
        int check = shouldSwap(arr, index, i);
        if(check){
            swap(&arr[index], &arr[i]);
            findPermutations(arr, index+1, n, result, r_index);
            swap(&(arr[index]), &(arr[i]));
        }
    }
}

void augment_entry(t_hashtab *t, t_symbol *key){
    t_atom_long val;
    hashtab_lookuplong(t, key, &val);
    val++;
    hashtab_storelong(t, key, val);
}

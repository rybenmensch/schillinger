#include "ext.h"
#include <stdarg.h>

/**
    Output an arbitraty number of symbols to an outlet of choice, with a
    selector of choice.

    @param    outlet    Outlet the messages are output to
    @param    selector  Selector string
    @param    arc       Number of arbitrary arguments + 1 (msg)
    @param    msg       The string preceeding the arbitrty number of arguments
    @param    ...       Arbitrty number of arguments
*/
void outlet_s(void *outlet, char *selector, int argc, char *msg, ...)
{
    t_atom argv[argc];
    atom_setsym(argv, gensym(msg));

    va_list ap;
    va_start(ap, msg);
    for (int i = 1; i < argc; i++) {
        atom_setlong(argv + i, va_arg(ap, int));
    }
    va_end(ap);

    outlet_anything(outlet, gensym(selector), argc, argv);
}

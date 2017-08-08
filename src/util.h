#ifndef UTIL_H_
#define UTIL_H_

#include "types.h"

void eval_print (entry_p entry);
void pretty_print (entry_p entry);
void print_list(entry_p *entries);
void ror(entry_p *e);
entry_p local(entry_p e);
entry_p global(entry_p e);

#endif

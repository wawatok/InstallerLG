//------------------------------------------------------------------------------
// debug.h:
//
// TODO
//------------------------------------------------------------------------------
// Copyright (C) 2018-2020, Ola Söder. All rights reserved.
// Licensed under the AROS PUBLIC LICENSE (APL) Version 1.1
//------------------------------------------------------------------------------

#ifndef DEBUG_H_
#define DEBUG_H_

#include "types.h"

#if defined(AMIGA) && !defined(LG_TEST)
#include <clib/debug_protos.h>
#define DBG(...) KPrintF((CONST_STRPTR)__VA_ARGS__)
#else
#define DBG(...) fprintf(stderr, __VA_ARGS__)
#endif

entry_p n_astraw(entry_p contxt);
entry_p n_asbraw(entry_p contxt);
entry_p n_asbeval(entry_p contxt);
entry_p n_eval(entry_p contxt);
entry_p n_options(entry_p contxt);

#endif

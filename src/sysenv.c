#include <string.h>
#include "sysenv.h"
#include "alloc.h"
#include "util.h"
#include "native.h"
  
entry_p init(entry_p contxt)
{
    entry_p e = new_native
    (
        strdup("onerror"), __LINE__, m_procedure, 
        push
        (
            new_contxt(), 
            new_custom
            (
                strdup("@onerror"), __LINE__, NULL, 
                push
                (
                    new_contxt(),
                    new_native
                    (
                        strdup("select"), __LINE__, m_select,
                        push(push
                        (
                            new_contxt(),
                            new_number(1)
                        ),
                            push
                            (
                                new_contxt(), 
                                new_number(0)
                            )
                        ), 
                        DANGLE
                    ) 
                )
            )
        ),
        DANGLE
    );
    append(&contxt->children, e); 
    e->parent = contxt; 
    ror(contxt->children);
    e = new_native
    (
        strdup("set"), __LINE__, m_set, 
        push(push(push(push(push(push
        (
            new_contxt(), 
            new_symbol(strdup("@user-level"))),
            new_number(2)
        ),
            new_symbol(strdup("@pretend"))),
            new_number(0)
        ),
            new_symbol(strdup("@io-err"))),
            new_number(0)
        ),
        NUMBER
    );
    append(&contxt->children, e); 
    e->parent = contxt; 
    ror(contxt->children);
    e = new_native
    (
        strdup("set"), __LINE__, m_set,
        push(push(push(push(push(push(push(push(
        push(push(push(push(push(push(push(push(
        push(push(push(push(push(push(push(push(
        push(push(push(push(push(push(push(push(
        push(push(push(push(push(push(push(push
        (
            new_contxt(), 
            new_symbol(strdup("@abort-button"))),
            new_string(strdup("Abort"))
        ),
            new_symbol(strdup("@app-name"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@askoptions-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@askchoice-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@asknumber-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@askstring-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@askdisk-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@askfile-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@askdir-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@copylib-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@copyfiles-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@makedir-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@startup-help"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@default-dest"))),
            new_string(strdup("Work:"))
        ),
            new_symbol(strdup("@error-msg"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@execute-dir"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@icon"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@installer-version"))),
            new_string(strdup(""))
        ),
            new_symbol(strdup("@language"))),
            new_string(strdup("english"))
        ),
            new_symbol(strdup("@special-msg"))),
            new_string(strdup(""))
        ),
        STRING
    );
    append(&contxt->children, e); 
    e->parent = contxt; 
    ror(contxt->children);
    return contxt; 
/*
3369 `@each-name'
3370 `@each-type'
3371      used in @{"`foreach'" Link "cs_foreach"}.
*/
}


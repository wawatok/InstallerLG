#include <stdio.h>
#include "util.h"
#include "debug.h"
#include "alloc.h"

void eval_print (entry_p entry)
{
    if(!entry)
    {
        printf ("NULL\n");
        return; 
    }
    switch (entry->type)
    {
        case NUMBER:
            printf ("%d\n", entry->id);
            break;

        case STRING:
            printf ("%s\n", entry->name);
            break;

        case SYMBOL:
        case SYMREF:
        case NATIVE:
        case CUSTOM:
        case CUSREF:
        case CONTXT:
        case STATUS:
        case DANGLE:
            break;
    }
}

void pretty_print (entry_p entry)
{
    static int ind = 0; 
    char *tbs[] = { "\t", "\t\t", "\t\t\t", 
                    "\t\t\t\t", "\t\t\t\t\t" }; 
    if(!entry)
    {
        printf ("\tNULL\n\n");
        return; 
    }
    switch (entry->type)
    {
        case NUMBER:
            printf ("\tNUMBER\n");
            printf ("%sId:\t%d\n", tbs[ind], entry->id);
            break;

        case STRING:
            printf ("\tSTRING\n");
            printf ("%sName:\t%s\n", tbs[ind], entry->name);
            break;

        case SYMBOL:
            printf ("\tSYMBOL\n");
            printf ("%sName:\t%s\n", tbs[ind], entry->name);
            printf ("%sRef:", tbs[ind]);
            pretty_print (entry->reference);
            break;

        case SYMREF:
            printf ("\tSYMREF\n");
            printf ("%sName:\t%s\n", tbs[ind], entry->name);
            break;

        case NATIVE:
            printf ("\tNATIVE\n");
            printf ("%sName:\t%s\n", tbs[ind], entry->name);
            printf ("%sCall:\t%p\n", tbs[ind], entry->call);
            break;

        case CUSTOM:
            printf ("\tCUSTOM\n");
            printf ("%sName:\t%s\n", tbs[ind], entry->name);
            printf ("%sCall:\t%p\n", tbs[ind], entry->call);
            if(entry->children || 
               entry->symbols)
            {
                entry_p *e = entry->children;
                if(e && *e)
                {
                    while(*e && *e != SENTINEL)
                    {
                        printf ("%sChl:", tbs[ind]); 
                        ind++; 
                        pretty_print(*e); 
                        ind--; 
                        e++; 
                    }
                }
                e = entry->symbols;
                if(e && *e)
                {
                    while(*e && *e != SENTINEL)
                    {
                        printf ("%sSym:", tbs[ind]); 
                        ind++; 
                        pretty_print(*e); 
                        ind--; 
                        e++; 
                    }
                }
            }
            break;

        case CUSREF:
            printf ("\tCUSREF\n");
            printf ("%sName:\t%s\n", tbs[ind], entry->name);
            break;

        case CONTXT:
            printf ("\tCONTXT\n"); 
            if(entry->children || 
               entry->symbols)
            {
                entry_p *e = entry->children;
                if(e && *e)
                {
                    while(*e && *e != SENTINEL)
                    {
                        printf ("%sChl:", tbs[ind]); 
                        ind++; 
                        pretty_print(*e); 
                        ind--; 
                        e++; 
                    }
                }
                e = entry->symbols;
                if(e && *e)
                {
                    while(*e && *e != SENTINEL)
                    {
                        printf ("%sSym:", tbs[ind]); 
                        ind++; 
                        pretty_print(*e); 
                        ind--; 
                        e++; 
                    }
                }
            }
            break;

        case STATUS:
            printf ("\tSTATUS\n");
            printf ("%sName:\t%s\n", tbs[ind], entry->name);
            printf ("%sId:\t%d\n", tbs[ind], entry->id);
            break;

        case DANGLE:
            printf ("\tDANGLE\n");
            break;
    }
}

entry_p local(entry_p e)
{
    for(; e && 
        e->type != CONTXT && 
        e->type != CUSTOM 
        ; e = e->parent);
    return e; 
}

entry_p global(entry_p e)
{
    for(e = local(e); 
        local(e->parent); 
        e = local(e->parent)); 
    return e; 
}


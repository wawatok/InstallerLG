//----------------------------------------------------------------------------
// file.h: 
//
// File operations 
//
//----------------------------------------------------------------------------

#include "alloc.h"
#include "error.h"
#include "eval.h"
#include "file.h"
#include "gui.h"
#include "probe.h"
#include "resource.h"
#include "strop.h"
#include "util.h"

#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef AMIGA
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <workbench/workbench.h>
#endif

//----------------------------------------------------------------------------
// Helper functions
//----------------------------------------------------------------------------
typedef enum { NOFAIL, OKNODELETE, FORCE, ASKUSER } cmode_t; 
static int h_copyfile(entry_p contxt, const char *src, const char *dst, int info, int nogauge, cmode_t mode);
static int h_exists(const char *n);
static char *h_fileonly(int id, const char *s);
static pnode_p h_filetree(int id, const char *src, const char *dst, int files, entry_p choices, entry_p pattern);
static int h_makedir(entry_p contxt, const char *dst);
static int h_protect_get(entry_p contxt, const char *file, LONG *mask);
static int h_protect_set(entry_p contxt, const char *file, LONG mask);
static int h_readonly(const char *file);
static int h_confirm(entry_p contxt, const char *msg, const char *hlp);
static int h_confirm_obsolete(entry_p contxt, const char *msg, const char *nfo);

//----------------------------------------------------------------------------
// (copyfiles (prompt..) (help..) (source..) (dest..) (newname..) (choices..)
//     (all) (pattern..) (files) (infos) (confirm..) (safe) (optional
//     <option> <option> ...) (delopts <option> <option> ...) (nogauge))
//
//     copy files (and subdir's by default).  files option say NO
//     subdirectories
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_copyfiles(entry_p contxt)
{
    // We need atleast one argument
    if(c_sane(contxt, 1))
    {
        entry_p prompt     = get_opt(contxt, OPT_PROMPT),   // OK
                help       = get_opt(contxt, OPT_HELP),     // OK
                source     = get_opt(contxt, OPT_SOURCE),   // OK
                dest       = get_opt(contxt, OPT_DEST),     // OK
                newname    = get_opt(contxt, OPT_NEWNAME),  // OK
                choices    = get_opt(contxt, OPT_CHOICES),  // OK
                all        = get_opt(contxt, OPT_ALL),      // OK
                pattern    = get_opt(contxt, OPT_PATTERN),  // OK
                infos      = get_opt(contxt, OPT_INFOS),    // OK
                files      = get_opt(contxt, OPT_FILES),    // OK
                confirm    = get_opt(contxt, OPT_CONFIRM),  // OK
                safe       = get_opt(contxt, OPT_SAFE),     // OK
                optional   = get_opt(contxt, OPT_OPTIONAL), // OK
                delopts    = get_opt(contxt, OPT_DELOPTS),  // OK
                nogauge    = get_opt(contxt, OPT_NOGAUGE),  // OK
                fonts      = get_opt(contxt, OPT_FONTS),
                fail       = get_opt(delopts, OPT_FAIL) ?
                             NULL : get_opt(optional, OPT_FAIL),
                nofail     = get_opt(delopts, OPT_NOFAIL) ?
                             NULL : get_opt(optional, OPT_NOFAIL),
                oknodelete = get_opt(delopts, OPT_OKNODELETE) ?
                             NULL : get_opt(optional, OPT_OKNODELETE),
                force      = get_opt(delopts, OPT_FORCE) ?
                             NULL : get_opt(optional, OPT_FORCE),
                askuser    = get_opt(delopts, OPT_ASKUSER) ?
                             NULL : get_opt(optional, OPT_ASKUSER);

        DNUM = 0; 
        fonts = force = askuser; 

        // The (pattern) (choices) and (all) options
        // are mutually exclusive. 
        if((pattern && (choices || all)) ||
           (choices && (pattern || all)) ||
           (all && (choices || pattern)))
        {
            error(contxt->id, ERR_OPTION_MUTEX, 
                  "pattern/choices/all"); 
            RCUR; 
        }

        // The (fail) (nofail) and (oknodelete) options
        // are mutually exclusive. 
        if((fail && (nofail || oknodelete)) ||
           (nofail && (fail || oknodelete)) ||
           (oknodelete && (nofail || fail)))
        {
            error(contxt->id, ERR_OPTION_MUTEX, 
                  "fail/nofail/oknodelete"); 
            RCUR; 
        }

        if(source && dest) 
        {
            pnode_p tree; 
            const char *src = str(source), 
                       *dst = str(dest); 

            if(h_exists(src) == 2 &&
               !all && !choices && !pattern)
            {
                error(contxt->id, ERR_MISSING_OPTION, 
                      "all/choices/pattern"); 
                RCUR; 
            }

            if(get_numvar(contxt, "@pretend") && !safe)
            {
                RNUM(1); 
            }

            if(h_exists(dst) == 2 &&
               !h_confirm_obsolete(contxt, tr(S_ODIR), dst))
            {
                error(HALT); 
                h_log(contxt, tr(S_ACPY), src, dst); 
                RCUR; 
            }

            tree = h_filetree
            (
                contxt->id, src, dst, 
                files ? 1 : 0,
                choices, 
                pattern
            ); 

            if(tree)
            {
                int go = 0; 
                pnode_p cur = tree; 

                if(newname)
                {
                    // Replace file name if single file and 
                    // the 'newname' option is set
                    if(cur->next && cur->type == 2 &&
                       cur->next->type == 1 && !cur->next->next)
                    {
                        free(cur->next->copy); 
                        cur->next->copy = h_tackon(contxt->id, dst, str(newname)); 
                    }
                }

                // Find out if we need confirmation...
                if(confirm)
                {
                    // The default threshold is expert.
                    int level = get_numvar(contxt, "@user-level"),
                        th = 2;

                    // If the (confirm ...) option contains 
                    // something that can be translated into
                    // a new threshold value...
                    if(confirm->children && 
                       confirm->children[0] && 
                       confirm->children[0] != end())
                    {
                        // ...then do so.
                        th = num(confirm->children[0]);
                    }
                                    
                    // If we are below the threshold value,
                    // don't care about getting confirmation
                    // from the user.
                    if(level < th) 
                    {
                        confirm = NULL; 
                    }

                    // Make sure that we have the prompt and
                    // help texts that we need if 'confirm'
                    // is set. It's not strictly necessary 
                    // if 'confirm' is not set, but it's not
                    // valid code so lets fail anyway.
                    if(!prompt || !help)
                    {
                        cur = NULL; 
                        error(contxt->id, ERR_MISSING_OPTION, 
                              prompt ? "help" : "prompt"); 
                    }
                }

                if(cur)
                {
                    go = gui_copyfiles_start
                    (
                        prompt ? str(prompt) : NULL, 
                        confirm ? str(help) : NULL, 
                        cur, 
                        confirm != NULL
                    ); 
                }

                if(go == 1)
                {
                    DNUM = 1; 

                    for(; cur && DNUM; 
                        cur = cur->next)
                    {
                        LONG prm; 

                        // Copy file / create dir / skip if zero:ed
                        switch(cur->type)
                        {
                            case 0:  continue; 
                            case 1:  DNUM = h_copyfile
                                     (
                                        contxt, 
                                        cur->name, 
                                        cur->copy, 
                                        infos ? 1 : 0, 
                                        nogauge ? 1 : 0,
                                        nofail ? 1 : 0 
                                     ); 
                                     break; 
                            case 2:  DNUM = h_makedir(contxt, cur->copy); break; 
                            default: error(PANIC); DNUM = 0; 
                        }

                        // No point setting permissions on failure / user abort.
                        if(DNUM)
                        {
                            // Preserve permissions 
                            if(!h_protect_get(contxt, cur->name, &prm) ||
                               !h_protect_set(contxt, cur->copy, prm))
                            {
                                DNUM = 0; 
                            }
                        }
                    }

                    gui_copyfiles_end(); 
                }
                else
                {
                    if(go == -1)
                    {
                        error(HALT); 
                    }
                }

                cur = tree; 
                while(cur)
                {
                    tree = cur; 
                    free(cur->name); 
                    free(cur->copy); 
                    cur = cur->next; 
                    free(tree); 
                }
            }
        }
        else
        {
            error(contxt->id, ERR_MISSING_OPTION, 
                  !source ? "source" : "dest"); 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC);
    }

    // We don't know if we're successsful, 
    // at this point, return what we have. 
    RCUR; 
}

//----------------------------------------------------------------------------
// (copylib (prompt..) (help..) (source..) (dest..) (newname..) (infos)
//     (confirm) (safe) (optional <option> <option> ...) (delopts <option>
//     <option> ...) (nogauge))
//
//     install a library if newer version
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_copylib(entry_p contxt)
{
    // We need atleast one argument
    if(c_sane(contxt, 1))
    {   
        entry_p prompt     = get_opt(contxt, OPT_PROMPT),
                help       = get_opt(contxt, OPT_HELP),
                confirm    = get_opt(contxt, OPT_CONFIRM),
                safe       = get_opt(contxt, OPT_SAFE),
                source     = get_opt(contxt, OPT_SOURCE),
                dest       = get_opt(contxt, OPT_DEST),
                newname    = get_opt(contxt, OPT_NEWNAME),
                infos      = get_opt(contxt, OPT_INFOS),
                noposition = get_opt(contxt, OPT_NOPOSITION),
                nogauge    = get_opt(contxt, OPT_NOGAUGE),
                optional   = get_opt(contxt, OPT_OPTIONAL),
                delopts    = get_opt(contxt, OPT_DELOPTS);

        confirm = safe; 
        noposition = optional = delopts = nogauge; 

        DNUM = 0; 

        if(prompt && help &&
           source && dest) 
        {
            const char *s = str(source), 
                       *d = str(dest); 

            // Does the source file exist?
            if(h_exists(s) == 1)
            {
                int dt = h_exists(d);

                if(dt == 1)
                {
                    // Destination is a file, not a dir. 
                    error(contxt->id, ERR_NOT_A_DIR, d); 
                }
                else
                {
                    if(!dt)
                    {
                        // Directory doesn't exist, create
                        // it. One level deep only. 
                        if(!mkdir(d, 0777))
                        {
                            // Log the success. 
                            h_log(contxt, tr(S_CRTD), d); 
                            dt = 2; 
                        }
                        else
                        {
                            // Permission problems or the dir is
                            // more than 1 level deeper than the
                            // existing path. 
                            error(contxt->id, ERR_WRITE_DIR, d); 
                        }
                    }

                    if(dt)
                    {
                        // The full path to the destination file. 
                        char *f; 

                        // Are we renaming the file?
                        if(newname)
                        {
                            // Yes, append the new name to the path.
                            f = h_tackon(contxt->id, d, str(newname));
                        }
                        else
                        {
                            // No, append the file part of the (possibly)
                            // full source path to the destination path. 
                            char *o = h_fileonly(contxt->id, s);
                            f = h_tackon(contxt->id, d, o);
                            free(o); 
                        }

                        // If we're not out of memory and destination dir
                        // and / or destination file is non empty, f will
                        // be <> 0. 
                        if(f) 
                        {
                            // Get type info of f.
                            int ft = h_exists(f);
        
                            // Does it exist?
                            if(!ft)
                            {
                                // No such file, just copy source file to the
                                // destination dir. 
                                DNUM = h_copyfile
                                (
                                    contxt, s, f,       
                                    infos ? 1 : 0,
                                    1, // nogauge
                                    0
                                );
                            }
                            else
                            // It's a file.
                            if(ft == 1)
                            {
                                int vs = h_getversion(contxt, s),
                                    vf = h_getversion(contxt, f);
                                
                                // Did we succeed getting a version
                                // string from both files? 
                                if(vs && vf) 
                                {
                                    // Is the source newer than the
                                    // current file?  
                                    if(vs > vf) 
                                    {
                                        // Yes.
                                        DNUM = h_copyfile
                                        (
                                            contxt, s, f,       
                                            infos ? 1 : 0,
                                            1, // nogauge
                                            0 
                                        );
                                    }
                                    else
                                    {
                                        // No. 
                                        DNUM = 0; 
                                    }
                                }
                                else
                                {
                                    // Could not find version string.
                                    error(contxt->id, ERR_NO_VERSION, vs ? f : s); 
                                }
                            }
                            // It's a dir. 
                            else
                            {
                                // Dest file exists, but is a directory. 
                                error(contxt->id, ERR_NOT_A_FILE, f); 
                            }

                            // The h_tackon function allocates memory
                            // the we need to free.
                            free(f); 
                        }
                    }
                }
            }
            else
            {
                // No file, doesn't exist or is dir. 
                error(contxt->id, ERR_NOT_A_FILE, s); 
            }
        }
        else
        {
            error
            (
                contxt->id, ERR_MISSING_OPTION, 
                !source ? "source" : 
                !dest ? "dest" :
                !prompt ? "prompt" : "help"
            ); 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC);
    }

    RCUR; 
}

//----------------------------------------------------------------------------
// (delete file (help..) (prompt..) (confirm..) 
//     (infos) (optional <option> <option> ...) (all)
//     (delopts <option> <option> ...) (safe))
//
//     delete file
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_delete(entry_p contxt)
{
    // We need atleast one argument
    if(c_sane(contxt, 1))
    {
        const char *file = str(CARG(1));

        // HOW DO WE TREAT DIRS? And (all) + pattern match?
        if(h_exists(file))
        {
            BOOL delete = TRUE; 
            entry_p help     = get_opt(CARG(2), OPT_HELP),
                    prompt   = get_opt(CARG(2), OPT_PROMPT), 
                    confirm  = get_opt(CARG(2), OPT_CONFIRM), 
                    infos    = get_opt(CARG(2), OPT_INFOS), 
                    optional = get_opt(CARG(2), OPT_OPTIONAL), 
                    all      = get_opt(CARG(2), OPT_ALL), 
                    delopts  = get_opt(CARG(2), OPT_DELOPTS), 
                    safe     = get_opt(CARG(2), OPT_SAFE),
                    force    = get_opt(delopts, OPT_FORCE) ? 
                               NULL : get_opt(optional, OPT_FORCE),
                    askuser  = get_opt(delopts, OPT_ASKUSER) ? 
                               NULL : get_opt(optional, OPT_ASKUSER);

            all = help = prompt; 

            // We should no delete the file if we're in
            // pretend mode unless it's (safe)
            if(get_numvar(contxt, "@pretend") && !safe)
            {
                delete = FALSE; 
            }
            else
            // When using (force), we should always delete. 
            if(!force)
            {
                // If the file is readonly, this is an error, 
                // unless we ask and get permission from the
                // user to force the deletion.
                if(h_readonly(file))
                {
                    // Ask the user for confirmation when 
                    // (askuser) is set. 
                    if(!(askuser && 
                         h_confirm_obsolete(contxt, tr(S_DWRT), file)))
                    {
                        // This will turn into an error if the
                        // user says no. 
                        error(contxt->id, ERR_DELETE_FILE, file); 
                        RNUM(0); 
                    }
                }
                else
                // If the file is writable, but we ask the
                // user for permission anyway, and we get a
                // no, then we should not delete. It's not 
                // an error though.
                if(confirm && 
                   !h_confirm_obsolete(contxt, tr(S_DNRM), file))
                {
                    delete = FALSE; 
                }
            }

            // Did we go through the hoops above? 
            if(delete)
            {
                // If yes, this must succeed, otherwise we
                // will abort with an error. 
                if(!remove(file))
                {
                    // The file has been deleted. 
                    h_log(contxt, tr(S_DLTD), file); 

                    // Shall we delete the info file as well? 
                    if(infos)
                    {
                        // Info file = file + .info. 
                        char info[PATH_MAX]; 
                        snprintf(info, PATH_MAX, "%s.info", file); 

                        // Delete the info file, if there is any. 
                        if(!remove(info))
                        {
                            // The info file has been deleted. 
                            h_log(contxt, tr(S_DLTD), info); 
                        }
                    }
                }
                else
                {
                    error(contxt->id, ERR_DELETE_FILE, file); 
                    RNUM(0); 
                }
            }
        }
        else
        {
            // We have nothing to do. 
            h_log(contxt, tr(S_NSFL), file); 
        }

        // We succeeded, please note that non-existing files 
        // can take us here as well. 
        RNUM(1); 
    }  
    else
    {
        // The parser is broken
        error(PANIC); 
        RCUR; 
    }
}

//----------------------------------------------------------------------------
// (exists <filename> (noreq))
//     0 if no, 1 if file, 2 if dir
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_exists(entry_p contxt)
{
    if(c_sane(contxt, 1))
    {
        // Non-existance. 
        int r = 0; 

        // Supress volume requester?
        if(get_opt(contxt, OPT_NOREQ))
        {
            #ifdef AMIGA
            struct Process *p = (struct Process *) 
                FindTask(NULL);

            // Save the current window ptr. 
            APTR w = p->pr_WindowPtr;

            // Disable auto request. 
            p->pr_WindowPtr = (APTR) -1L; 
            #endif

            // Get type (file / dir / 0)
            r = h_exists(str(CARG(1))); 

            #ifdef AMIGA
            // Restore auto request. 
            p->pr_WindowPtr = w; 
            #endif
        }
        else
        {
            // Get type (file / dir / 0)
            r = h_exists(str(CARG(1))); 
        }

        RNUM(r); 
    }
    else
    {
        // The parser is broken
        error(PANIC);
        RCUR; 
    }
}

//----------------------------------------------------------------------------
// (fileonly <path>)
//     return file part of path (see pathonly)
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_fileonly(entry_p contxt)
{
    if(c_sane(contxt, 1))
    {
        char *r = h_fileonly(contxt->id, str(CARG(1))); 
        if(r)
        {
            RSTR(r); 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC); 
    }

    REST; 
}

//----------------------------------------------------------------------------
// (foreach <dir> <pattern> <statements>)
//     do for entries in directory
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_foreach(entry_p contxt)
{
    if(contxt || c_sane(contxt, 3))
    {
        // Pattern matching is not done
        // on non-Amiga systems.
        #ifdef AMIGA
        const char *pt = str(CARG(2)); 
        #endif

        // Open dir and assume failure. 
        const char *dn = str(CARG(1)); 
        DIR *dir = opendir(dn);  
        pnode_p top = NULL;
        int err = 1; 

        // Permission to read? 
        if(dir) 
        {
            static char cwd[PATH_MAX];
            struct dirent *ent = readdir(dir); 

            // Save the current working directory. 
            if(getcwd(cwd, PATH_MAX) == cwd)
            {
                // Enter the directory <drawer name>
                if(!chdir(dn))
                {
                    // Allocate memory for the start node. 
                    pnode_p cur; 
                    top = calloc(1, sizeof(struct pnode_t)); 

                    #ifdef AMIGA
                    // This file info block will be used for all files
                    // and subdirs in the directory. 
                    struct FileInfoBlock *fib = (struct FileInfoBlock *) 
                           AllocDosObject(DOS_FIB, NULL); 

                    // Use cur as a gatekeeper. If set to NULL, nothing
                    // will take place except clean ups. 
                    cur = fib ? top : NULL; 
                    #else
                    // No pattern matching = no fib.
                    cur = top; 
                    #endif

                    // The dir might be empty but that's not an 
                    // error. 
                    err = 0; 

                    // Iterate over all entries in dir.
                    while(ent && cur && !err)
                    {
                        // Name of file / dir. 
                        const char *fn = ent->d_name; 

                        #ifndef AMIGA 
                        // Filter out the magic on non-Amigas.
                        if(strcmp(fn, ".") &&
                           strcmp(fn, ".."))
                        #endif
                        {
                            #ifdef AMIGA
                            // The dir is not empty, we should be able
                            // to go all the way here. Assume failure.
                            err = 1; 

                            // Lock and get the information we need
                            // from the current entry
                            BPTR lock = (BPTR) Lock(fn, ACCESS_READ);

                            if(lock)
                            {
                                if(Examine(lock, fib))
                                {
                                    // Please note that if we fail to 
                                    // lock and examine cur->name will
                                    // be NULL.
                                    cur->type = fib->fib_DirEntryType;
                                    cur->name = strdup(fn); 

                                    // We're probably good. PANIC:s will 
                                    // be caught further down. 
                                    err = 0; 
                                }

                                UnLock(lock); 
                            }
                            #else
                            cur->type = h_exists(fn);
                            cur->name = strdup(fn); 
                            #endif

                            // An empty name indicates a PANIC only if
                            // we didn't have any locking problems. 
                            if(!cur->name)
                            {
                                // Setting cur to NULL will generate a
                                // PANIC further down. 
                                if(!err)
                                {
                                    cur = NULL;
                                }

                                // Always bail out. 
                                break;
                            }
                        }

                        // Get next entry. 
                        ent = readdir(dir); 

                        // We need to check for cur->name here, or else the
                        // the filtering of '.' and '..' would not work, we 
                        // would get entries without names. 
                        if(ent && cur->name)
                        {
                            cur->next = calloc(1, sizeof(struct pnode_t));
                            cur = cur->next;
                        }
                    }

                    #ifdef AMIGA
                    // If dir is empty, fib will be NULL. 
                    if(fib)
                    {
                        FreeDosObject(DOS_FIB, fib); 
                    }
                    #endif

                    if(!cur)
                    {
                        // Out of memory.
                        error(PANIC); 
                        err = 1;
                    }

                    // Go back the directory where
                    // we started. 
                    chdir(cwd); 
                }
            }

            // Done. 
            closedir(dir); 
        }
        
        if(err)
        {
            // We could end up here for a number
            // of reasons. More details perhaps?
            error(contxt->id, ERR_READ_DIR, dn); 
        }

        // Iterate over the entire list of files / dirs and
        // free entries one by one. If the pattern matches 
        // the current entry, set variables and execute what
        // we have in argument 3. 
        while(top)
        {
            int skip = err; 
            pnode_p old = top; 

            // 'Export' name and type info if we have a name. 
            if(top->name)
            {
                #ifdef AMIGA
                if(!err)
                {
                    static char buf[BUFSIZ]; 

                    // Parse pattern. 
                    switch(ParsePattern(pt, buf, sizeof(buf)))
                    {
                        // If we have any wildcards, try to match. 
                        case 1: 
                            skip = MatchPattern(buf, top->name) ? 0 : 1;
                            break;

                        // If no wildcards, do a simple string comparsion.
                        case 0:
                            skip = strcmp(top->name, pt); 
                            break; 

                        // We probably had a buffer overflow. 
                        default:
                            error(contxt->id, ERR_OVERFLOW, pt); 
                            err = 1; 
                    }
                }
                #endif

                // We always export, for memory management reasons. 
                set_numvar(contxt, "@each-type", (int) top->type); 
                set_strvar(contxt, "@each-name", top->name); 

                if(!skip)
                {
                    // Execute the code contained in
                    // the third argument. 
                    invoke(CARG(3)); 
                }
            }

            // Free current entry and proceed with the next. 
            top = top->next; 
            free(old); 
        }
        
        RNUM
        (
            err ? 0 : 1
        ); 
    }
    else
    {
        // The parser is broken
        error(PANIC); 
        RNUM(0); 
    }
}

//----------------------------------------------------------------------------
// (makeassign <assign> [<path>] (safe)) ; note: <assign> doesn't need `:''
//     create an assignment
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_makeassign(entry_p contxt)
{
    if(c_sane(contxt, 1))
    {
        entry_p safe = get_opt(contxt, OPT_SAFE);

        // Is this a safe operation or are we not 
        // running in pretend mode? 
        if(safe || !get_numvar(contxt, "@pretend"))
        {
            // The name of the assign. 
            char *asn = str(CARG(1)); 

            // Assume failure..
            DNUM = 0; 

            // Are we going to create an assign?
            if(CARG(2) && CARG(2) != end() &&
               CARG(2)->type != OPTION)
            { 
                // The destination. 
                char *dst = str(CARG(2)); 

                #ifdef AMIGA
                BPTR lock = (BPTR) Lock(dst, ACCESS_READ);
                if(lock)
                {
                    // Create the assign. After this,
                    // the lock will be owned by the 
                    // system, do not unlock or use.
                    DNUM = AssignLock(asn, lock) ? 1 : 0; 
                }
                #else
                DNUM = 1;
                #endif
                
                // Log the outcome. 
                h_log
                (
                    contxt, 
                    DNUM ? tr(S_ACRT) : tr(S_ACRE),
                    asn, 
                    dst
                ); 
            }
            else
            {
                #ifdef AMIGA
                // Remove assign.
                DNUM = AssignLock(str(CARG(1)), NULL) ? 1 : 0; 
                #else
                DNUM = 2;
                #endif

                // Log the outcome. 
                h_log
                (
                    contxt, 
                    DNUM ? tr(S_ADEL) : tr(S_ADLE),
                    asn
                ); 
            }

            if(!DNUM)
            {
                // Could not create / rm assign / get lock. 
                error(contxt->id, ERR_ASSIGN, str(CARG(1))); 
            }
        }
        else
        {
            // Pretend. 
            DNUM = 1; 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC); 
    }

    // Return what we have. Could
    // be a failure or a success.
    RCUR; 
}

//----------------------------------------------------------------------------
// (makedir <name> (prompt..) (help..) (infos) (confirm..) (safe))
//     make a directory
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_makedir(entry_p contxt)
{
    // We need atleast one argument, the name
    // of the directory to be created.
    if(c_sane(contxt, 1))
    {
        entry_p opt = contxt->children[1]; 
        if(opt)
        {
            // handle options
        }
        RNUM
        (
            h_makedir(contxt, str(CARG(1))); 
        ); 
    }
    else
    {
        // The parser is broken
        error(PANIC); 
        RNUM(0);
    }
}

//----------------------------------------------------------------------------
// (protect <file> [<string of flags to change>] [<dec. mask>] <parameters>)
//     get/set file protection flags
//
// 8 7 6 5 4 3 2 1  <- Bit number
// h s p a r w e d  <- corresponding protection flag
// ^ ^ ^ ^ ^ ^ ^ ^
// | | | | | | | |
// | | | | | | | +- | 
// | | | | | | +--- | 0 = flag set
// | | | | | +----- | 1 = flag clear
// | | | | +------- | 
// | | | |
// | | | |
// | | | +--------- | 
// | | +----------- | 0 = flag clear
// | +------------- | 1 = flag set
// +--------------- | 
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_protect(entry_p contxt)
{
    // We need atleast two arguments
    if(c_sane(contxt, 2))
    {
        LONG r = 0, mask = num(CARG(2)); 
        const char *file = str(CARG(1)); 
        entry_p override = get_opt(CARG(3), OPT_OVERRIDE),
                    safe = get_opt(CARG(3), OPT_SAFE); 

        safe = NULL; 

        if(mask)
        {
            r = mask; 
        }
        else
        {
            int m = 0; 
            const char *flags = str(CARG(2)); 
            size_t n = strlen(flags); 
            if(override)
            {
                r = num(override); 
            }
            else if(!h_protect_get(contxt, file, &r))
            {
                // Helper will set proper error
                RNUM(0); 
            }
            r ^= 0x0f; /* invert 1-4 */
            for(size_t i = 0; i < n; i++)
            {
                int b = 0; 
                switch(flags[i])
                {
                    case '+': m = 1; break; 
                    case '-': m = 2; break; 
                    case 'h': b = 1 << 7; break; 
                    case 's': b = 1 << 6; break; 
                    case 'p': b = 1 << 5; break; 
                    case 'a': b = 1 << 4; break; 
                    case 'r': b = 1 << 3; break; 
                    case 'w': b = 1 << 2; break; 
                    case 'e': b = 1 << 1; break; 
                    case 'd': b = 1 << 0; break; 
                }
                switch(m)
                {
                    case 0: r = b; m = 1; break; 
                    case 1: r |= b; break; 
                    case 2: r &= ~b; break; 
                }
            }
            r ^= 0x0f; /* invert 1-4 */
        }
        if(!override)
        {
            if(!h_protect_set(contxt, file, r))
            {
                // Helper will set proper error
                RNUM(0); 
            }
        }
        else
        {
            r = mask ? mask : r; 
        }
        RNUM(r); 
    }
    else
    {
        // The parser is broken
        error(PANIC); 
        RCUR; 
    }
}

//----------------------------------------------------------------------------
// (startup (prompt..) (command..))
//     add a command to the boot scripts (startup-sequence, user-startup)
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_startup(entry_p contxt)
{
    // Expect failure. 
    DNUM = 0; 

    // We need atleast two arguments
    if(c_sane(contxt, 2))
    {
        const char *app = str(CARG(1)); 
        entry_p command  = get_opt(CARG(2), OPT_COMMAND),
                help     = get_opt(CARG(2), OPT_HELP),
                prompt   = get_opt(CARG(2), OPT_PROMPT), 
                confirm  = get_opt(CARG(2), OPT_CONFIRM), 
                override = get_opt(CARG(2), OPT_OVERRIDE);

        help = prompt = confirm; 

        // We need a command to add. 
        if(!command) 
        {
            error(contxt->id, ERR_MISSING_OPTION, "command"); 
            RCUR; 
        }

        // And somewhere to put it.  
        if(!strlen(app))
        {
            error(contxt->id, ERR_INVALID_APP, "NULL"); 
            RCUR; 
        }

        if(c_sane(command, 1))
        {
            const char *cmd = str(command->children[0]), 
                       /* Variable instead of override? */
                       *fln = override ? str(override) : "s:user-startup";
            const size_t len = strlen(";BEGIN ") + strlen(app), 
                         ins = strlen(cmd) + 2;
            char *pre = calloc(len + 1, 1), 
                 *pst = calloc(len + 1, 1), 
                 *buf = NULL;

            if(pre && pst)
            {
                // We don't need to write yet. 
                FILE *fp = fopen(fln, "r"); 
                if(fp)
                {
                    // Seek to the end so that we
                    // can use ftell below to get
                    // the size of the file. 
                    if(!fseek(fp, 0L, SEEK_END))
                    {
                        // Worst case: empty file + 3 NL +
                        // terminating 0 + BEGIN and END 
                        // markers + command.
                        size_t osz = (size_t) ftell(fp), 
                               nsz = osz + 2 * len + ins;

                        // Allocate enough memory so that
                        // we can keep the old file + any
                        // changes that we need to do in 
                        // memory at the same time. 
                        buf = calloc(nsz, 1);
                        if(buf)
                        {
                            // Read the whole file in one go. 
                            if(!fseek(fp, 0L, SEEK_SET) && 
                               fread(buf, 1, osz, fp) == osz) 
                            {
                                // Prepare the header and footer. 
                                snprintf(pre, len + 1, ";BEGIN %s", app);
                                snprintf(pst, len + 1, ";END %s", app);

                                // Do we already have an entry in
                                // the current file? 
                                char *beg = strstr(buf, pre), 
                                     *end = strstr(buf, pst); 

                                // Replace the current entry by inserting
                                // the new one at the same location. 
                                if(beg && end)
                                {
                                    // Move the tail of the buffer to the right place. 
                                    memmove(beg + len + ins, end, (buf + osz) - end + 1); 

                                    // Insert the command string. 
                                    memcpy(beg + len + 1, cmd, ins - 2); 

                                    // Add surrounding line feeds so that the 
                                    // command won't end up being a comment. 
                                    beg[len + ins - 1] = beg[len] = '\n';
                                }
                                // No existing entry. Append the new entry. 
                                else
                                {
                                    // Header. 
                                    memcpy(buf + osz, pre, len); 

                                    // Command. 
                                    memcpy(buf + osz + len + 1, cmd, ins - 2); 

                                    // Footer. 
                                    memcpy(buf + osz + len + ins, pst, len); 

                                    // Add surrounding line feeds so that the 
                                    // command won't end up being a comment. 
                                    buf[osz + len + ins - 1] = buf[osz + len] = '\n';

                                    // Add a newline at the end. 
                                    buf[osz + len + ins + strlen(pst)] = '\n'; 
                                }
                            }
                            else
                            {
                                // Read problem, error will be set
                                // further down if buf == NULL.
                                free(buf); 
                                buf = NULL; 
                            }
                        }
                        else
                        {
                            // Out of memory
                            error(PANIC); 
                        }
                    }
                    // Reading done. 
                    fclose(fp); 
                }
            }
            else
            {
                // Out of memory
                error(PANIC); 
            }

            // We have no use for the header
            // and footer anymore. 
            free(pre); 
            free(pst); 

            // If we have a buffer everything wen't fine
            // above. Go ahead and write buffer to file. 
            if(buf)
            {
                // Use a temporary file to make sure that we 
                // don't mess up the current file if disk space
                // becomes a problem, the system crashes, the 
                // power is lost and so on and so forth. 
                size_t tln = strlen(fln) + sizeof(".XXXXXX\0");
                char *tmp = calloc(tln, 1); 

                if(tmp)
                {
                    // Use mktemp, I don't know if we can use 
                    // anything better without risking that the
                    // temp file ends up outside of S:, T: for 
                    // example. 
                    snprintf(tmp, tln, "%s.XXXXXX", fln);

                    // Temporary workaround to make this build
                    // on MorphOS. Where's mktemp?
                    #ifndef __MORPHOS__
                    tmp = mktemp(tmp); 
                    #endif

                    FILE *fp = fopen(tmp, "w"); 

                    if(fp)  
                    {
                        // Write everything to the temporary 
                        // file at once. 
                        if(fputs(buf, fp) != EOF)
                        {
                            // Close file and release buffer. 
                            fclose(fp); 
                            free(buf); 
                            buf = NULL; 

                            // Open the target file just to 
                            // make sure that we have write 
                            // permissions. 
                            fp = fopen(fln, "a"); 
                            if(fp)
                            {
                                // Close it immediately, we're not
                                // going to write directly to it. 
                                fclose(fp); 
                                
                                // Do a less un-atomic write to
                                // the real file by renaming the
                                // temporary file.
                                if(!rename(tmp, fln))
                                {
                                    // We're done. 
                                    free(tmp); 
                                    tmp = NULL; 
                                    DNUM = 1; 
                                }
                            }
                            else
                            {
                                // We aren't allowed to write data
                                // to the target file so we need to 
                                // clean up. We don't want old temp 
                                // files laying around.
                                if(remove(tmp))
                                {
                                    // This is highly unlikely, but why not? 
                                    error(contxt->id, ERR_WRITE_FILE, tmp); 
                                }
                            }
                        }
                        else
                        {
                            // Failure. Could not write to disk.
                            // The old file is still intact. 
                            fclose(fp); 
                        }
                    }

                    // If we haven't released tmp by now, we failed
                    // when attempting one of the write operations 
                    // above. 
                    if(tmp)
                    {
                        free(tmp); 
                        error(contxt->id, ERR_WRITE_FILE, fln); 
                    }
                }
                else
                {
                    // Out of memory
                    error(PANIC); 
                }

                // We no longer need the buffer holding
                // the new file contents. 
                free(buf); 
            }
            else
            {
                // No buffer == read problem, or OOM but then
                // we already have a PANIC. 
                error(contxt->id, ERR_READ_FILE, fln); 
            }
        }
        else
        {
            // Broken parser. 
            error(PANIC); 
        }
    }
    else
    {
        // Broken parser. 
        error(PANIC); 
    }

    // Return whatever we
    // have at this point. 
    RCUR; 
}

//----------------------------------------------------------------------------
// (textfile (prompt..) (help..) (dest..) (append) (include..) (confirm..) (safe))
//      create text file from other text files and strings
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//
//----------------------------------------------------------------------------
entry_p m_textfile(entry_p contxt)
{
    // We need atleast one argument
    if(c_sane(contxt, 1))
    {
        entry_p prompt   = get_opt(contxt, OPT_PROMPT),
                help     = get_opt(contxt, OPT_HELP),
                dest     = get_opt(contxt, OPT_DEST),
                append   = get_opt(contxt, OPT_APPEND),
                include  = get_opt(contxt, OPT_INCLUDE),
                confirm  = get_opt(contxt, OPT_CONFIRM),
                safe     = get_opt(contxt, OPT_SAFE); 

        DNUM = 0; 

        if(dest)
        {
            // Find out if we need confirmation...
            if(confirm)
            {
                // The default threshold is expert.
                int level = get_numvar(contxt, "@user-level"),
                    th = 2;

                // If the (confirm ...) option contains 
                // something that can be translated into
                // a new threshold value...
                if(confirm->children && 
                   confirm->children[0] && 
                   confirm->children[0] != end())
                {
                    // ...then do so.
                    th = num(confirm->children[0]);
                }
                                
                // If we are below the threshold value,
                // don't care about getting confirmation
                // from the user.
                if(level < th) 
                {
                    confirm = NULL; 
                }

                // Make sure that we have the prompt and
                // help texts that we need if 'confirm'
                // is set. It's not strictly necessary 
                // if 'confirm' is not set, but it's not
                // valid code so lets fail anyway.
                if(!prompt || !help)
                {
                    error(contxt->id, ERR_MISSING_OPTION, 
                          prompt ? "help" : "prompt"); 
                    RCUR; 
                }
            }


            // If we need confirmation and the user skips
            // or aborts, return. On abort, the HALT will
            // be set by h_confirm. 
            if(confirm && 
               !h_confirm(contxt, str(prompt), str(help)))
            {
                RCUR; 
            }

            // Is this a safe operation or are we not 
            // running in pretend mode? 
            if(safe || !get_numvar(contxt, "@pretend"))
            {
                // Overwrite existing file. 
                const char *fn = str(dest); 
                FILE *fp = fopen(fn, "w"); 

                if(fp)
                {
                    // Assume success. 
                    DNUM = 1; 

                    // Include before append. 
                    if(include)
                    {
                        // File to copy. 
                        const char *fi = str(include); 
                        FILE *fs = fopen(fi, "r"); 

                        if(fs)
                        {
                            static char buf[BUFSIZ]; 
                            size_t n = fread(buf, 1, BUFSIZ, fs);

                            // Log operation.
                            h_log(contxt, tr(S_INCL), fi, fn); 

                            // Copy the whole file. 
                            while(n)
                            {
                                if(fwrite(buf, 1, n, fp) != n)
                                {
                                    error(contxt->id, ERR_WRITE_FILE, fn); 
                                    DNUM = 0; 
                                    break; 
                                }

                                // More data? 
                                n = fread(buf, 1, BUFSIZ, fs);
                            }

                            fclose(fs); 
                        }
                    }

                    // Don't append if the include failed. 
                    if(append && DNUM)
                    {
                        // String to append. 
                        const char *ap = str(append); 

                        // Log operation.
                        h_log(contxt, tr(S_APND), ap, fn); 

                        if(fputs(ap, fp) == EOF)
                        {
                            error(contxt->id, ERR_WRITE_FILE, fn); 
                            DNUM = 0; 
                        }
                    }

                    fclose(fp); 

                    // We must append and / or include,
                    // or else this doesn't make sense.
                    if(!append && !include)
                    {
                        error(contxt->id, ERR_NOTHING_TO_DO, contxt->name); 
                        DNUM = 0; 
                    }
                }
                else
                {
                    error(contxt->id, ERR_WRITE_FILE, fn); 
                }
            }
            else
            {
                // A non safe operation in pretend
                // mode always succeeds. 
                DNUM = 1; 
            }
        }
        else
        {
            error(contxt->id, ERR_MISSING_OPTION, "dest"); 
        }
    }
    else
    {
        error(PANIC); 
    }
            
    // Success / failure. 
    RCUR; 
}

//----------------------------------------------------------------------------
// (tooltype (prompt..) (help..) (dest..) (settooltype..) (setstack..)
//     (setdefaulttool..) (noposition) (confirm..) (safe))
//
//     modify an icon
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_tooltype(entry_p contxt)
{
    // No arguments needed. 
    if(c_sane(contxt, 0))
    {
        entry_p help            = get_opt(contxt, OPT_HELP),
                prompt          = get_opt(contxt, OPT_PROMPT),
                dest            = get_opt(contxt, OPT_DEST),
                settooltype     = get_opt(contxt, OPT_SETTOOLTYPE),
                setdefaulttool  = get_opt(contxt, OPT_SETDEFAULTTOOL),
                setposition     = get_opt(contxt, OPT_SETPOSITION),
                swapcolors      = get_opt(contxt, OPT_SWAPCOLORS),
                confirm         = get_opt(contxt, OPT_CONFIRM),
                safe            = get_opt(contxt, OPT_SAFE);

        help = prompt = settooltype = setposition = 
        swapcolors = confirm = safe; 
        DNUM = 0; 

        // We need something to work with. 
        if(dest) 
        {
            // Something is 'dest'.info 
            const char *file = str(dest);

            #ifdef AMIGA
            // Get icon information. 
            struct DiskObject *obj = (struct DiskObject *) 
                GetDiskObject(file);

            if(obj)
            {
                // Do we want to change the default tool of 
                // this 'project'?
                if(setdefaulttool)
                {
                    // We need to save the current value. 
                    const char *new = str(setdefaulttool),
                               *old = obj->do_DefaultTool;

                    // Should we perhaps check the length
                    // of this string? Or is a length of
                    // zero a reasonable way of removing
                    // a tool? 
                    obj->do_DefaultTool = (char *) new; 

                    // Nope:
/*
 `(settooltype <tooltype> <value>)'
     Used to modify a tooltype to a certain value.  If the tooltype
     does not exist it will be created; if the `<value>' parameter is
     omitted, the tooltype will be deleted.  A tooltype without a value
     may be added in the following manner:

          (settooltype <tooltype-string> "")

     Remember that `(tooltype <tooltype-string>)' deletes the tooltype
     given.
*/
                    // Save the changes to the .info file. 
                    if(PutDiskObject(file, obj))
                    {
                        // Success.
                        DNUM = 1; 
                    }
                    else
                    {
                        // We failed for some unknown reason.
                        error(contxt->id, ERR_WRITE_FILE, file); 
                    }

                    // Restore the old value, otherwise memory
                    // will be lost / trashed. Refer to the 
                    // icon.library documentation. 
                    obj->do_DefaultTool = (char *) old; 
                }

                // Free the DiskObject after restoring it to
                // the state it was in before our changes. 
                FreeDiskObject(obj);
            }
            else
            {
                // More information? IoErr() is nice.  
                error(contxt->id, ERR_READ_FILE, file); 
            }
            #else
            setdefaulttool = NULL; 
            file = NULL; 
            DNUM = 1; 
            #endif
        }
        else
        {
            error(contxt->id, ERR_MISSING_OPTION, "dest"); 
        }
    }
    else
    {
        // The parser is broken
        error(PANIC); 
    }

    // We don't know if we're successsful, 
    // at this point, return what we have. 
    RCUR; 
}

//----------------------------------------------------------------------------
// (transcript <string1> <string2>)
//     puts concatenated strings in log file
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_transcript(entry_p contxt)
{
    // We need atleast one argument, one or
    // more strings to put into the log file.
    if(c_sane(contxt, 1))
    {
        size_t len = 0; 
        for(entry_p *e = contxt->children; 
            *e && *e != end(); e++)
        {
            if((*e)->type != OPTION)
            {
                len += strlen(str(*e)); 
            }
        }
        char *buf = calloc(len + 2, 1);
        if(buf)
        {
            for(entry_p *e = contxt->children; 
                *e && *e != end(); e++)
            {
                if((*e)->type != OPTION)
                {
                    strcat(buf, str(*e)); 
                }
            }

            DNUM = h_log(contxt, "%s\n", buf) ? 1 : 0; 
            free(buf); 
            RCUR; 
        }
    }

    // The parser isn't necessarily broken 
    // if we end up here. We could alse be
    // out of memory.
    error(PANIC); 
    RCUR; 
}

//----------------------------------------------------------------------------
// (rename <old> <new> (help..) (prompt..) (confirm..) (safe))
//     rename files
//
// Refer to Installer.guide 1.19 (29.4.96) 1995-96 by ESCOM AG
//----------------------------------------------------------------------------
entry_p m_rename(entry_p contxt)
{
    // We need atleast two arguments
    if(c_sane(contxt, 2))
    {
        entry_p help    = get_opt(CARG(3), OPT_HELP),
                prompt  = get_opt(CARG(3), OPT_PROMPT), 
                confirm = get_opt(CARG(3), OPT_CONFIRM), 
                disk    = get_opt(CARG(3), OPT_DISK), 
                safe    = get_opt(CARG(3), OPT_SAFE);

        const char *fr  = str(CARG(1)), 
                   *to  = str(CARG(2));

        // Do we need confirmation?
        if(confirm)
        {
            // The default threshold is expert.
            int level = get_numvar(contxt, "@user-level"); 
            int th = 2;

            // If the (confirm ...) option contains 
            // something that can be translated into
            // a new threshold value...
            if(confirm->children && 
               confirm->children[0] && 
               confirm->children[0] != end())
            {
                // ...then do so.
                th = num(confirm->children[0]);
            }
                            
            // If we are below the threshold value,
            // don't care about getting confirmation
            // from the user.
            if(level < th) 
            {
                confirm = NULL; 
            }

            // Make sure that we have the prompt and
            // help texts that we need if 'confirm'
            // is set. It's not strictly necessary 
            // if 'confirm' is not set, but it's not
            // valid code so lets fail anyway. OS3.9
            // isn't this strict though. Relax?
            if(!prompt || !help)
            {
                error(contxt->id, ERR_MISSING_OPTION, 
                      prompt ? "help" : "prompt"); 
                RNUM(0); 
            }
        }

        // If we need confirmation and the user skips
        // or aborts, return. On abort, the HALT will
        // be set by h_confirm. 
        if(confirm && 
           !h_confirm(contxt, str(prompt), str(help)))
        {
            RNUM(0); 
        }

        // Is this a safe operation or are we not 
        // running in pretend mode? 
        if(safe || !get_numvar(contxt, "@pretend"))
        {
            // Are we going to rename a file/dir?
            if(!disk)
            {
                // Fail if the target exists. 
                if(!h_exists(to) && !rename(fr, to))
                {
                    // Success. 
                    h_log(contxt, tr(S_FRND), fr, to); 
                    RNUM(-1); 
                }
                else
                {
                    error(contxt->id, ERR_RENAME_FILE, fr); 
                    RNUM(0); 
                }
            }
            // No, we're going to relabel a volume. 
            else
            {
                #ifdef AMIGA
                // Rename volume.
                if(!Relabel(fr, to))
                {
                    // Failure.
                    RNUM(0);
                }
                #endif

                // Success. 
                h_log(contxt, tr(S_FRND), fr, to); 
                RNUM(-1); 
            }
        }
        else
        {
            // Non safe operations always 
            // succeed in pretend mode.
            RNUM(-1); 
        }
    }  
    else
    {
        // The parser is broken
        error(PANIC); 
        RCUR; 
    }
}

//----------------------------------------------------------------------------
// copyfiles / copylib helper function
//----------------------------------------------------------------------------
static int h_copyfile(entry_p contxt, 
                      const char *src, 
                      const char *dst,
                      int info, 
                      int nogauge, 
                      cmode_t mode)
{
    if(contxt && src && dst)
    { 
        if(gui_copyfiles_setcur(src, nogauge))
        { 
            static char buf[BUFSIZ]; 
            FILE *fs = fopen(src, "r"); 

            if(fs)
            {
                FILE *fd = fopen(dst, "w"); 
                if(fd)
                {
                    size_t n = fread(buf, 1, BUFSIZ, fs);

                    // Read and write until there is nothing more
                    // to read.
                    while(n)
                    {
                        if(fwrite(buf, 1, n, fd) != n)
                        {
                            error(contxt->id, ERR_WRITE_FILE, dst); 
                            break; 
                        }
                        n = fread(buf, 1, BUFSIZ, fs);
                    }

                    // Close input and output files.
                    fclose(fs); 
                    fclose(fd); 
                    
                    // The number of bytes read and not written 
                    // should be zero.
                    if(!n)
                    {
                        // Write to the log file (if logging is
                        // enabled). 
                        h_log(contxt, tr(S_CPYD), src, dst); 

                        // Are we going to copy the icon as well?
                        if(info)
                        {
                            // The source icon. 
                            static char is[PATH_MAX]; 
                            snprintf(is, sizeof(is), "%s.info", src); 

                            // Only if it exists, it's not an error
                            // if it's missing.
                            if(h_exists(is) == 1)
                            {
                                // The destination icon. 
                                static char id[PATH_MAX]; 
                                snprintf(id, sizeof(id), "%s.info", dst); 

                                // Recur without info set. 
                                return h_copyfile
                                (
                                    contxt, 
                                    is, 
                                    id, 
                                    0, 
                                    nogauge,
                                    mode
                                ); 
                            }
                        }

                        // We succeeded.
                        return 1; 
                    }
                }
                else
                {
                    fclose(fs); 

                    if(mode & (1 << NOFAIL))
                    {
                        // Ignore failure. 
                        h_log(contxt, tr(S_NCPY), src, dst); 
                        return 1;
                    }
                    else
                    {
                        error(contxt->id, ERR_WRITE_FILE, dst); 
                    }
                }
            }
            else
            {
                if(mode & (1 << NOFAIL))
                {
                    // Ignore failure. 
                    h_log(contxt, tr(S_NCPY), src, dst); 
                    return 1;
                }
                else
                {
                    error(contxt->id, ERR_READ_FILE, src); 
                }
            }
        }
        else
        {
            error(HALT); 
            h_log(contxt, tr(S_ACPY), src, dst); 
        }
    }

    // Failure or user abort.
    return 0; 
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
static int h_exists(const char *n)
{
    // NULL is a valid argument but
    // this 'file' doesn't exist.
    if(n)
    {
        #ifdef AMIGA
        int r = 0; 
        struct FileInfoBlock *fib = (struct FileInfoBlock *) 
               AllocDosObject(DOS_FIB, NULL); 
        if(fib)
        {
            BPTR lock = (BPTR) Lock(n, ACCESS_READ);
            if(lock)
            {
                if(Examine(lock, fib))
                {
                    /*
                    ST_ROOT          1
                    ST_USERDIR       2

                    ST_SOFTLINK      3 
                    NOTE: this Shows up as a directory 
                          unless checked for explicitly

                    ST_LINKDIR       4
                    ST_FILE         -3
                    ST_LINKFILE     -4
                    */
                    if(fib->fib_DirEntryType < 0)
                    {
                        // A file. 
                        r = 1;  
                    }
                    else if(fib->fib_DirEntryType > 0)
                    {
                        // A directory. 
                        r = 2; 
                    }
                    // Return values according to the 
                    // CBM installer documentation.
                }
                UnLock(lock); 
            }
            FreeDosObject(DOS_FIB, fib); 
        }
        return r; 
        #else
        // This implementation doesn't work on MorphOS.
        // I have no clue why, it works on AROS.  
        struct stat fs; 
        if(!stat(n, &fs))
        {
            // A file ...
            if(S_ISREG(fs.st_mode))
            {
                // Value according to the CBM 
                // installer documentation.
                return 1; 
            }
            // ... or a directory ...
            if(S_ISDIR(fs.st_mode))
            {
                // Ibid.
                return 2; 
            }
        }
        #endif
    }

    // Ibid.
    return 0; 
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
static char *h_fileonly(int id, 
                        const char *s)
{
    if(s)
    {
        size_t l = strlen(s), 
               i = l - 1;
        if(l && 
           s[i] != '/' &&
           s[i] != ':' )
        {
            char *r; 
            while(i &&
                  s[i - 1] != '/' && 
                  s[i - 1] != ':' ) i--;
            r = calloc(l - i + 1, 1); 
            if(r)
            {
                memcpy(r, s + i, l - i); 
                return r; 
            }
            error(PANIC); 
        }
        else
        {
            error(id, ERR_NOT_A_FILE, s); 
        }   
    }
    return NULL; 
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
static pnode_p h_filetree(int id, 
                          const char *src, 
                          const char *dst, 
                          int files, 
                          entry_p choices, 
                          entry_p pattern)
{
    char *n_src = NULL,
         *n_dst = NULL; 

    if(src && dst)
    {
        int type = h_exists(src); 

        if(type == 2)
        {
            DIR *dir = opendir(src);  

            if(dir) 
            {
                // If the (choices) option is set we need to
                // verify that all choices exist (as files).
                if(choices)
                {
                    // Unless the parser is broken, 
                    // we will have >= one child. 
                    entry_p *e = choices->children;

                    // Iterate over all filenames.
                    while(*e && *e != end())
                    {
                        // Build path. 
                        n_src = h_tackon(id, src, str(*e)); 

                        if(n_src)
                        {
                            // If it doesn't exist, free everything and
                            // bail out. 
                            if(!h_exists(n_src))
                            {
                                error(id, ERR_NO_SUCH_FILE_OR_DIR, n_src); 
                                free(n_src); 
                                closedir(dir); 
                                return NULL; 
                            }
                            else
                            {
                                // Next filename.
                                free(n_src); 
                                e++; 
                            }
                        }
                    }
                }

                pnode_p node = calloc(1, sizeof(struct pnode_t)), 
                        head = node; 

                if(node)
                {
                    struct dirent *entry = readdir(dir); 

                    node->name = strdup(src); 
                    node->copy = strdup(dst); 
                    node->type = 2; 

                    while(entry)
                    {
                        n_src = h_tackon(id, src, entry->d_name), 
                        n_dst = h_tackon(id, dst, entry->d_name); 

                        if(n_src && n_dst)
                        {
                            if(choices)
                            {
                                // Unless the parser is broken, 
                                // we will have >= one child. 
                                entry_p *e = choices->children;

                                // Iterate over all filenames.
                                while(*e && *e != end())
                                {
                                    // Stop when we have a match.
                                    if(!strcmp(str(*e), entry->d_name))
                                    {
                                        break; 
                                    }

                                    // Next filename.
                                    e++; 
                                }

                                // Get proper type if we have a match.
                                if(*e && *e != end())
                                {
                                    type = h_exists(n_src); 
                                }
                                // Otherwise clear type, this will make
                                // m_copyfiles skip it.
                                else
                                {
                                    type = 0; 
                                }
                            }
                            else 
                            if(pattern)
                            {
                                #ifdef AMIGA
                                // Use a static buffer, Installer.guide
                                // restricts pattern length to 64. It 
                                // seems like MatchPattern can use a lot
                                // of stack if we use long patterns, so 
                                // let's not get rid of this limitation. 
                                static char pat[BUFSIZ]; 
                                LONG w = ParsePattern(str(pattern), pat, sizeof(pat)); 

                                // Can we parse the pattern?
                                if(w >= 0)
                                {
                                    // Use pattern matching if we have one or more
                                    // wildcards, otherwise use plain strcmp().
                                    if((w && MatchPattern(pat, entry->d_name)) ||
                                       (w && !strcmp(pat, entry->d_name)))
                                    {
                                        // Match, get proper type.
                                        type = h_exists(n_src); 
                                    }
                                    else
                                    {
                                        // Not a match, skip this.
                                        type = 0; 
                                    }
                                }
                                else
                                {
                                    // We probably had a buffer overflow.
                                    // No more pattern matching today.
                                    error(id, ERR_OVERFLOW, str(pattern)); 
                                    pattern = NULL; 
                                }
                                #else
                                type = h_exists(n_src); 
                                #endif
                            }
                            else
                            {
                                // File or directory? 
                                type = h_exists(n_src); 
                            }

                            // If we have a directory, recur.
                            if(type == 2)
                            {
                                // Unless the (files) option is set. 
                                if(!files)
                                {
                                    #ifndef AMIGA 
                                    if(strcmp(entry->d_name, ".") &&
                                       strcmp(entry->d_name, ".."))
                                    #endif
                                    {
                                        // Get tree of subdirectory.
                                        node->next = h_filetree
                                        (
                                            id, 
                                            n_src, 
                                            n_dst, 
                                            files,
                                            choices,
                                            pattern
                                        ); 

                                        // Fast forward to the end of
                                        // the list.
                                        while(node->next)
                                        {
                                            node = node->next; 
                                        }
                                    }
                                }

                                // We don't need to store the names
                                // of directories, release them.
                                free(n_src); 
                                free(n_dst); 
                            }
                            else
                            {
                                node->next = calloc(1, sizeof(struct pnode_t)); 
                                if(node->next)
                                {
                                    node->next->type = type; 
                                    node->next->name = n_src; 
                                    node->next->copy = n_dst; 
                                    node = node->next; 
                                }
                                else
                                {
                                    error(PANIC); 
                                    break; 
                                }
                            }
                        }
                        else
                        {
                            break; 
                        }
                        entry = readdir(dir); 
                    }
                }
                closedir(dir); 
                return head; 
            }
            else
            {
                error(id, ERR_READ_DIR, src); 
                return NULL; 
            }
        }
        else
        if(type == 1)
        {
            pnode_p file = calloc(1, sizeof(struct pnode_t)),
                    head = calloc(1, sizeof(struct pnode_t)); 
            if(head && file)
            {
                n_src = strdup(src); 
                n_dst = strdup(dst); 
                if(n_src && n_dst)  
                {
                    char *tac; 
                    head->type = 2; 
                    head->next = file; 
                    head->name = n_src; 
                    head->copy = n_dst; 
                    tac = h_fileonly(id, src);
                    n_dst = h_tackon(id, dst, tac); 
                    n_src = strdup(src);
                    free(tac); 
                    if(n_src && n_dst)  
                    {
                        file->type = 1; 
                        file->name = n_src; 
                        file->copy = n_dst; 
                        return head; 
                    }
                    else
                    {
                        free(head->name); 
                        free(head->copy); 
                    }
                }
                free(head); 
                free(file); 
            }
        }
        else
        {
            error(id, ERR_NO_SUCH_FILE_OR_DIR, src); 
            return NULL; 
        }
    }
    error(PANIC); 
    free(n_src); 
    free(n_dst); 
    return NULL; 
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
int h_log(entry_p contxt, const char *fmt, ...)
{
    // Use whatever format and arguments we get
    va_list ap; 

    if(contxt && fmt)
    {
        // Only if logging is enabled... 
        if(get_numvar(contxt, "@log"))
        {
            // Use the log file set in init(..) or by the user.
            const char *lf = get_strvar(contxt, "@log-file");

            // Don't care about existing files, just append.
            FILE *fp = fopen(lf, "a"); 
            int n = -1; 

            if(fp)
            {
                n = fprintf(fp, "[%d:%s] ", contxt->id, contxt->name);  

                if(n > 0)
                {
                    va_start(ap, fmt); 
                    n = vfprintf(fp, fmt, ap); 
                    va_end(ap); 
                }

                fclose(fp); 
            }

            // Could we open the file AND write all data to it? 
            if(n < 0)
            {
                error(contxt->id, ERR_WRITE_FILE, lf); 
                n = 0; 
            }

            return n; 
        }

        // We suceeded doing
        // nothing.
        return 1; 
    }

    // We failed.
    return 0; 
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
static int h_makedir(entry_p contxt, const char *dst)
{
    if(contxt && dst)
    {
        char *dir; 

        if(h_exists(dst) == 2)
        {
            // Nothing to do.
            h_log(contxt, tr(S_EDIR), dst); 
            return 1; 
        }

        dir = strdup(dst); 
        if(dir)
        {
            int d = 1, 
                l = (int) strlen(dir);
            for(int i = 0; i < l; i++)
            {
                if(dir[i] == '/')
                {
                    d++;  
                }
            }
            while(d--)
            {
                for(int i = l; i >= 0; i--)
                {
                    if(dir[i] == '/' ||
                       dir[i] == '\0')
                    {
                        char c = dir[i]; 
                        dir[i] = '\0'; 
                        if(mkdir(dir, 0777) == 0)
                        {
                            if(i == l)
                            {
                                free(dir);
                                h_log(contxt, tr(S_CRTD), dst); 
                                return 1; 
                            }
                            dir[i] = c; 
                            break; 
                        }
                        else
                        {
                            dir[i] = c; 
                        }
                    }
                }
            }
            free(dir);
            error(contxt->id, ERR_WRITE_DIR, dst); 
        }
        else
        {
            error(PANIC); 
        }
    }
    return 0; 
}

//----------------------------------------------------------------------------
// m_protect get helper
//----------------------------------------------------------------------------
static int h_protect_get(entry_p contxt, 
                         const char *file,
                         LONG *mask)
{
    if(contxt && mask && file)
    {
#ifdef AMIGA
        struct FileInfoBlock *fib = (struct FileInfoBlock *) AllocDosObject(DOS_FIB, NULL); 

        if(fib)
        {
            BOOL done = FALSE; 
            BPTR lock = (BPTR) Lock(file, ACCESS_READ);

            if(lock)
            {
                if(Examine(lock, fib))
                {
                    *mask = fib->fib_Protection; 
                    done = TRUE; 
                }

                UnLock(lock); 
            }

            FreeDosObject(DOS_FIB, fib); 

            if(!done)
            {
                error(contxt->id, ERR_GET_PERM, file); 
            }

            h_log(contxt, tr(S_GMSK), file, *mask); 
            return done; 
        }
#else
        return TRUE; 
#endif
    }

    error(PANIC); 
    return FALSE; 
}

//----------------------------------------------------------------------------
// m_protect set helper
//----------------------------------------------------------------------------
static int h_protect_set(entry_p contxt, 
                         const char *file, 
                         LONG mask)
{
    if(contxt && file)
    {
#ifdef AMIGA
        if(!SetProtection(file, mask))
        {
            error(contxt->id, ERR_SET_PERM, file); 
            return FALSE; 
        }
#endif
        h_log(contxt, tr(S_PTCT), file, mask); 
        return TRUE; 
    }
    else
    {
        error(PANIC); 
        return FALSE; 
    }
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
static int h_readonly(const char *file)
{
    if(file)
    {
        FILE *fp = fopen(file, "r+"); 
        if(fp)
        {
            fclose(fp); 
            return FALSE; 
        }
        return TRUE; 
    }
    else
    {
        error(PANIC); 
        return FALSE; 
    }
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
static int h_confirm_obsolete(entry_p contxt, 
                     const char *msg, 
                     const char *nfo) 
{
    if(contxt)
    {
        // Setting @yes can be used to mimic
        // a user saying yes to all prompts. 
        if(get_numvar(contxt, "@yes"))
        {
            return TRUE; 
        }
        else
        {
            // Format message and show dialog.
            static char buf[BUFSIZ]; 
            snprintf(buf, sizeof(buf), "%s\n\n%s", nfo, msg); 
            return gui_confirm(buf);
        }
    }
    else
    {
        error(PANIC); 
        return FALSE; 
    }
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
static int h_confirm(entry_p contxt, 
                     const char *msg, 
                     const char *hlp) 
{
    if(contxt)
    {
        // By setting @yes, @skip or @abort
        // user behaviour can be simulated.
        int yes = get_numvar(contxt, "@yes"),
            skip = get_numvar(contxt, "@skip"),
            abort = get_numvar(contxt, "@abort"),
            ret; 

        // If we have any overrides, translate
        // them to the corresponding gui return 
        // value. 
        if(yes || skip || abort)
        {
            ret = abort ? -1 : yes; 
        }
        // No overrides, get confirmation from 
        // the user. 
        else
        {
            ret = gui_run(msg, hlp);
        }

        // On abort, set HALT state. The return
        // value is that same as skip.
        if(ret < 0)
        {
            error(HALT); 
            ret = 0; 
        }

        // True or false.
        return ret; 
    }
    else
    {
        // Broken parser. 
        error(PANIC); 
        return 0; 
    }
}

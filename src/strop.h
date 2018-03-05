//----------------------------------------------------------------------------
// strop.h: 
//
// String operations
//
//----------------------------------------------------------------------------

#ifndef STROP_H_
#define STROP_H_

#include "types.h"

entry_p m_cat(entry_p contxt);
entry_p m_expandpath(entry_p contxt);
entry_p m_fmt(entry_p contxt);
entry_p m_pathonly(entry_p contxt);
entry_p m_patmatch(entry_p contxt);
entry_p m_strlen(entry_p contxt);
entry_p m_substr(entry_p contxt);
entry_p m_tackon(entry_p contxt);

//----------------------------------------------------------------------------
// Helper functions
//----------------------------------------------------------------------------
char *h_tackon(int id, const char *p, const char *f);

#endif

//----------------------------------------------------------------------------
// probe.h: 
//
// Environment information retrieval
//
//----------------------------------------------------------------------------

#ifndef PROBE_H_
#define PROBE_H_

#include "types.h"

entry_p m_database(entry_p contxt);
entry_p m_earlier(entry_p contxt);
entry_p m_getassign(entry_p contxt);
entry_p m_getdevice(entry_p contxt);
entry_p m_getdiskspace(entry_p contxt);
entry_p m_getenv(entry_p contxt);
entry_p m_getsize(entry_p contxt);
entry_p m_getsum(entry_p contxt);
entry_p m_getversion(entry_p contxt);
entry_p m_iconinfo(entry_p contxt);

int h_getversion(entry_p contxt, const char *file);

#endif

#ifndef DEFS_H
#define DEFS_H

enum RESULT {AC = 0, PE, WA, CE, RE, ME, TE, OLE, SLE, SW};
enum LANGUAGE {LANG_C = 0, LANG_CPP, LANG_JAVASCRIPT, LANG_PYTHON, LANG_GO, LANG_TEXT, LANG_PYPY3, LANG_BINARY, LANG_CUSTOM};
enum SPJ_MODE {SPJ_NO = 0, SPJ_COMPARE, SPJ_INTERACTIVE, SPJ_REPORT};
enum TRACE_ACTION {ALLOW = 0, SKIP = 1, DENY = 2, KILL = 3};

#endif
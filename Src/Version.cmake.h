#pragma once
#define set(verStr) namespace LockdownVersion { extern int Major, Minor, Revision; struct Parser { Parser(const char*);  }; static Parser parser(#verStr); }

set("LOCKDOWN_VERSION" "1.0.4")

#undef set

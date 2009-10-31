#ifndef _LIBDOUBS_GLOBALS_H
#define _LIBDOUBS_GLOBALS_H

#include "libdoubs_config.h"
#include "ScreenMgr.h"

class LIBDOUBS_API globals
{
public:

public:
	static void init();
	static ScreenMgr* getScreenMgr();
	static void uninit();

private:
	static ScreenMgr* screenMgr;
};


#endif /* _LIBDOUBS_GLOBALS_H */
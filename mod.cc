/* -*- Mode: C; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t; -*- */

#include <dlfcn.h>
#include <stdio.h>

static void
open_lib(const char* lib)
{
	void* handle;

	fprintf(stderr, "dlopen('%s') ...", lib);
	handle = dlopen(lib, RTLD_LAZY);
	fprintf(stderr, " -> %p (error? %s)\n", handle, dlerror());
}

int
main(int argc, char** argv)
{
	if (argc > 1) {
		open_lib("/home/cjones/rr/workbench/libctor.so");
		return 0;
	}

	open_lib("/home/cjones/rr/ff-prof/memory/mozalloc/libmozalloc.so");
	open_lib("/home/cjones/rr/ff-prof/db/sqlite3/src/libmozsqlite3.so");
	open_lib("/home/cjones/rr/ff-prof/security/nss/lib/nss/libnss3.so");
	open_lib("/home/cjones/rr/ff-prof/security/nss/lib/ssl/libssl3.so");
	open_lib("/home/cjones/rr/ff-prof/toolkit/library/libxul.so");

//	open_lib("/home/cjones/rr/ff-prof/toolkit/system/gnome/libmozgnome.so");
	open_lib("/home/cjones/rr/ff-prof/toolkit/system/dbus/libdbusservice.so");
//	open_lib("/home/cjones/rr/ff-prof/browser/components/build/libbrowsercomps.so");

	return 0;
}

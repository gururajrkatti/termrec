noinst_LTLIBRARIES = libcompat.la libutils.la libttyutils.la
libcompat_la_SOURCES = sys/asprintf.c sys/threads.h sys/compat.h
libutils_la_SOURCES = sys/utils.h sys/error.h sys/debuglog.c
libttyutils_la_SOURCES = sys/ttysize.h

libutils_la_LIBADD = libcompat.la

if WIN32
libcompat_la_SOURCES += sys/win32/threads.h sys/win32/compat.c
libutils_la_SOURCES += sys/win32/winutils.c sys/win32/winutils.h
libttyutils_la_SOURCES += sys/win32/conssize.c
else
libcompat_la_SOURCES += sys/unix/threads.h
libutils_la_SOURCES += sys/unix/utils.c
libttyutils_la_SOURCES += sys/unix/ttysize.c
endif

if WIN32NET
libcompat_la_SOURCES += sys/win32/net.c sys/win32/net.h
endif

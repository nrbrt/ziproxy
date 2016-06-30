dnl
dnl Autoconf macros from the GNU autoconf macro archive
dnl
dnl http://www.gnu.org/software/ac-archive/
dnl


dnl @synopsis ETR_SOCKET_NSL
dnl
dnl This macro figures out what libraries are required on this platform
dnl to link sockets programs.  It's usually -lsocket and/or -lnsl or
dnl neither.  We test for all three combinations.
dnl
dnl @version $Id: acinclude.m4,v 1.4 2004/07/11 22:17:46 jurov Exp $
dnl @author Warren Young <warren@etr-usa.com>
dnl
AC_DEFUN([ETR_SOCKET_NSL],
[
AC_CACHE_CHECK(for libraries containing socket functions,
ac_cv_socket_libs, [
        oCFLAGS=$CFLAGS

        AC_TRY_LINK([
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
                ],
                [
                        struct in_addr add;
                        int sd = socket(AF_INET, SOCK_STREAM, 0);
                        inet_ntoa(add);
                ],
                ac_cv_socket_libs=-lc, ac_cv_socket_libs=no)

        if test x"$ac_cv_socket_libs" = "xno"
        then
                CFLAGS="$oCFLAGS -lsocket"
                AC_TRY_LINK([
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
                        ],
                        [
                                struct in_addr add;
                                int sd = socket(AF_INET, SOCK_STREAM, 0);
                                inet_ntoa(add);
                        ],
                        ac_cv_socket_libs=-lsocket, ac_cv_socket_libs=no)
        fi

        if test x"$ac_cv_socket_libs" = "xno"
        then
                CFLAGS="$oCFLAGS -lsocket -lnsl"
                AC_TRY_LINK([
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
                        ],
                        [
                                struct in_addr add;
                                int sd = socket(AF_INET, SOCK_STREAM, 0);
                                inet_ntoa(add);
                        ],
                        ac_cv_socket_libs="-lsocket -lnsl", ac_cv_socket_libs=no)
        fi

        CFLAGS=$oCFLAGS
])

        if test x"$ac_cv_socket_libs" = "xno"
        then
                AC_MSG_ERROR([Cannot find socket libraries])
        elif test x"$ac_cv_socket_libs" = "x-lc"
        then
                ETR_SOCKET_LIBS=""
        else
                ETR_SOCKET_LIBS="$ac_cv_socket_libs"
        fi

        AC_SUBST(ETR_SOCKET_LIBS)
]) dnl ETR_SOCKET_NSL

dnl Checks for libraries, their placement can be given with parameter.
dnl First library-config script is checked for. If not found, default options
dnl are used.
dnl Usage: USE_LIB(libname, [extra_help, [ACTION-IF-GIVEN,
dnl         [ ACTION-IF-NOT-GIVEN]]])
AC_DEFUN([USE_LIB],
[AC_ARG_WITH($1,
[  --with-$1[=DIR]	Set the location where lib$1 is installed. $2 ],[
	if test "$withval" != "" && test -x "$withval/bin/$1-config" ; then
		cfgloc="$withval/bin/$1-config"
	elif which $1-config > /dev/null 2>&1 ; then
		cfgloc=`which $1-config`
	else
		AC_MSG_NOTICE([$1-config not found.])
	fi
	if test "$cfgloc" != "" ; then
	    if cfl=`$cfgloc --cflags` ; then
		CFLAGS="$CFLAGS $cfl"
	    else
		AC_MSG_WARN([$withval/$1-config --cflags failed.])
		unset cfgloc
	    fi
	    unset cfl
	    if ldfl=`$cfgloc --libs` ; then
		LDFLAGS="$LDFLAGS $ldfl"
	    else
		AC_MSG_WARN([$withval/$1-config --libs failed.])
		unset cfgloc
	    fi
	    unset ldfl
	fi
	if test "$withval" != "" && test "$cfgloc" == "" ; then
	
	    if test -d $withval/include; then
		    CFLAGS="$CFLAGS -I$withval/include"
	    else
		    AC_MSG_WARN([Directory $withval/include not found.])
	    fi
	    if test -d $withval/lib; then 
		    LDFLAGS="$LDFLAGS -L$withval/lib"   
	    else
		    AC_MSG_WARN([Directory $withval/lib not found.])
	    fi
	fi
   unset cfgloc
   $3],
   [$4])]
) dnl USE_LIB

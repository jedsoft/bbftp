# CCIN2P3_CHECK_RFIO
# ------------------
# $1 is action if found
# $2 is action if not found
# $3 is rfio root dir
AC_DEFUN([CCIN2P3_CHECK_RFIO],[
OLDLDFLAGS="$LDFLAGS"
OLDCPPFLAGS="$CPPFLAGS"
OLDLIBS=$LIBS
if test $# -gt 2 ; then
    rfio_root_dir=$3
else
    rfio_root_dir="/usr/local"
fi
# LDFLAGS="$LDFLAGS -L$rfio_root_dir/lib"
LIBS="$LIBS $rfio_root_dir/lib/libshift.a"
CPPFLAGS="$CPPFLAGS -I$rfio_root_dir/include -I$rfio_root_dir/include/shift"
AC_SEARCH_LIBS([getservbyname_r],[socket xnet])dnl
AC_SEARCH_LIBS([gethostbyaddr_r],[nsl])dnl
AC_SEARCH_LIBS([sched_get_priority_max],[rt posix4])dnl
AC_CHECK_FILE([$rfio_root_dir/include/shift/rfio_api.h],[result=yes],[result=no])
if test $result = yes ; then
    AC_SEARCH_LIBS([pthread_create],[pthread pthreads dce])dnl
    AC_SEARCH_LIBS([pthread_kill],[pthread pthreads])dnl
    AC_CHECK_FUNC([rfio_open64],[AC_DEFINE(WITH_RFIO64)])
else
    AC_CHECK_FUNC([rfio_open],[AC_DEFINE(WITH_RFIO)])
fi
if test "$ac_cv_func_rfio_open64" = "no" || test "$ac_cv_func_rfio_open" = "no" ; then
    LDFLAGS="$OLDLDFLAGS"
    CPPFLAGS="$OLDCPPFLAGS"
    LIBS="$OLDLIBS"
    ccin2p3_cv_rfio=no
else
    AC_CHECK_HEADERS([shift.h],,[ AC_MSG_ERROR(shift.h not found in $CPPFLAGS)])
#    LIBS="$LIBS $rfio_root_dir/lib/libshift.a"
    LDFLAGS="$OLDLDFLAGS"
    dnl in RFIO 32 bits, rfio_HsmIf_FindPhysicalPath means CASTOR is supported
    if test "$ac_cv_func_rfio_open" = "yes" ; then
        AC_CHECK_FUNC([rfio_HsmIf_FindPhysicalPath],[result=yes],[result=no])
        if test $result = yes ; then
            dnl add xxx/shift include dir for stage_api.h
            dnl CPPFLAGS="$CPPFLAGS -I$rfio_root_dir/include/shift"
            AC_CHECK_HEADER([stage_api.h],[result=yes],[AC_MSG_ERROR(shift/stage_api.h not found in $CPPFLAGS)])
            AC_DEFINE(CASTOR)
        fi
    else
        dnl in RFIO 64 bits, rfio_setcos means CASTOR is not supported
        AC_CHECK_FUNC([rfio_setcos],[result=yes],[result=no])
        if test $result = no ; then
            dnl add xxx/shift include dir for stage_api.h
            unset ac_cv_func_rfio_setcos
            AC_ARG_WITH([hpss],[  
  --with-hpss             Enable HPSS interface: automatic search (DEFAULT) in /usr/local
  --with-hpss=DIR         Enable HPSS interface: search libhpss in DIR/lib
  --without-hpss          Disable HPSS interface],
              [resulthpss=yes],[resulthpss=no])
            if test $resulthpss != no ; then
              if test $with_hpss = yes ; then
                hpss_root_dir="/usr/local"
              else
                hpss_root_dir=$with_hpss
              fi
              OLDLIBS="$LIBS"
              LIBS="$LIBS $hpss_root_dir/lib/libhpss.a"
              AC_CHECK_FUNC([rfio_setcos],[result=yes],[result=no])
              if test $result = no ; then
                LIBS="$OLDLIBS"
              fi
            fi
        fi
        if test $result = no ; then
            dnl CPPFLAGS="$CPPFLAGS -I$rfio_root_dir/include/shift"
            AC_CHECK_HEADER([stage_api.h],[result=yes],[AC_MSG_ERROR(shift/stage_api.h not found in $CPPFLAGS)])
            AC_CHECK_FUNCS([stageclr_Path])
            AC_DEFINE(CASTOR)
        fi
    fi
    ccin2p3_cv_rfio=yes
fi
AS_IF([test $ccin2p3_cv_rfio = yes],[$1],[$2])[]dnl
])

AC_INIT([Recurse], 0.0.0, [outshined@riseup.net], [], [])

AC_PREREQ([2.68])

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_SRCDIR([code/main.cpp])

AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.14 subdir-objects -Wall -Werror])

AM_PROG_AR

LT_INIT

AC_LANG([C++])

#-------------------------------------------------------------------------------

AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_MKDIR_P

#-------------------------------------------------------------------------------

AS_IF([test "x$GCC" = "xyes"],[
    AX_CHECK_COMPILE_FLAG([-std=c++14], [
        CXXFLAGS="$CXXFLAGS -std=c++14"], [
        AC_MSG_ERROR([no C++14 support])])
])

#-------------------------------------------------------------------------------

PKG_CHECK_MODULES([NEBULA_FOUNDATION], [nebula-foundation >= 0.0.0])

#-------------------------------------------------------------------------------

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

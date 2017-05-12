#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.50])
AC_INIT([Trailer Trash], [1.9], [])
AC_CONFIG_SRCDIR([include/FullScreen.h])'
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([xml2], [xmlSAXUserParseFile])
AC_CHECK_LIB([ncurses], [printw])
AC_CHECK_LIB([pthread], [sem_wait])

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([getcwd mkdir rmdir])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT


#
# Balboa configure.ac -- autoreconf input
#

# Initialization

AC_INIT([libdlog], [0.9d], [joncook@nmsu.edu])

# put autoconf tools in config directory

AC_CONFIG_AUX_DIR([config])

# helper macros in the config dir
# AC_CONFIG_MACRO_DIR([macros])

# init automake

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# check we are in the right spot

AC_CONFIG_SRCDIR([src/publicapi.c])


# Make not building shared libs the default
#AM_DISABLE_SHARED

# Check for programs

AC_PROG_CC
#AC_PROG_CXX
AC_PROG_LIBTOOL
#AC_PROG_YACC
#AC_PROG_LEX

AC_CONFIG_MACRO_DIR([config/m4])

#AC_CHECK_PROGS(tclProg,[tclsh],[none])
#if test "x$tclProg" = xnone ; then
#  AC_MSG_ERROR([Error: The Tcl interpreter tclsh must be available])
#fi

# Check for libraries
# Check for header files
# Check for typedefs, structs, other compiler oddities
# Check for library functions

CFLAGS='-g -Wall'
CXXFLAGS='-g -Wall'

# Output

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
   Makefile
   src/Makefile
   test/Makefile
])
AC_OUTPUT

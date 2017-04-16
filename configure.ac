#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([vkbootstrap], [0.1], [egor.artemov@gmail.com])
AC_CONFIG_SRCDIR([src/main_x11.c])
AC_CONFIG_HEADERS([src/config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE([enable])
# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([XCB], [xcb >= 1.12])
AC_SEARCH_LIBS([vkGetInstanceProcAddr], [vulkan])
# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

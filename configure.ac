dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(focaes, 0.1)


AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()

AC_PROG_CC
AC_PROG_CXX

AC_CHECK_FUNCS([memset mkdir socket strcasecmp])
AC_CHECK_HEADERS([arpa/inet.h limits.h netinet/in.h stdint.h string.h unistd.h])
AC_CHECK_HEADER_STDBOOL

AC_TYPE_INT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT

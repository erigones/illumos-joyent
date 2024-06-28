/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1985-2011 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                 Eclipse Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*          http://www.eclipse.org/org/documents/epl-v10.html           *
*         (with md5 checksum b35adb5213ca9657e911e9befb180842)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
#if __sgi && _hdr_locale_attr

/*
 * irix 6.5 introduced __libc_attr referenced by
 * ctype and locale macros; this hack lets
 * 6.5 a.outs run on irix < 6.5
 *
 * NOTE: this hack also freezes the US locale
 */

#include <locale_attr.h>

static __ctype_t	_ast_ctype_tbl =
{
 {
 0x00000000, 0x00000020, 0x00000020, 0x00000020,
 0x00000020, 0x00000020, 0x00000020, 0x00000020,
 0x00000020, 0x00000020, 0x80000028, 0x00000028,
 0x00000028, 0x00000028, 0x00000028, 0x00000020,
 0x00000020, 0x00000020, 0x00000020, 0x00000020,
 0x00000020, 0x00000020, 0x00000020, 0x00000020,
 0x00000020, 0x00000020, 0x00000020, 0x00000020,
 0x00000020, 0x00000020, 0x00000020, 0x00000020,
 0x00000020, 0x80008008, 0x00000010, 0x00000010,
 0x00000010, 0x00000010, 0x00000010, 0x00000010,
 0x00000010, 0x00000010, 0x00000010, 0x00000010,
 0x00000010, 0x00000010, 0x00000010, 0x00000010,
 0x00000010, 0x00000084, 0x00000084, 0x00000084,
 0x00000084, 0x00000084, 0x00000084, 0x00000084,
 0x00000084, 0x00000084, 0x00000084, 0x00000010,
 0x00000010, 0x00000010, 0x00000010, 0x00000010,
 0x00000010, 0x00000010, 0x00000081, 0x00000081,
 0x00000081, 0x00000081, 0x00000081, 0x00000081,
 0x00000001, 0x00000001, 0x00000001, 0x00000001,
 0x00000001, 0x00000001, 0x00000001, 0x00000001,
 0x00000001, 0x00000001, 0x00000001, 0x00000001,
 0x00000001, 0x00000001, 0x00000001, 0x00000001,
 0x00000001, 0x00000001, 0x00000001, 0x00000001,
 0x00000010, 0x00000010, 0x00000010, 0x00000010,
 0x00000010, 0x00000010, 0x00000082, 0x00000082,
 0x00000082, 0x00000082, 0x00000082, 0x00000082,
 0x00000002, 0x00000002, 0x00000002, 0x00000002,
 0x00000002, 0x00000002, 0x00000002, 0x00000002,
 0x00000002, 0x00000002, 0x00000002, 0x00000002,
 0x00000002, 0x00000002, 0x00000002, 0x00000002,
 0x00000002, 0x00000002, 0x00000002, 0x00000002,
 0x00000010, 0x00000010, 0x00000010, 0x00000010,
 0x00000020, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000, 0x00000000, 0x00000000, 0x00000000,
 0x00000000,
 },
 {
    -1,     0,     1,     2,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,
    15,    16,    17,    18,    19,    20,    21,    22,
    23,    24,    25,    26,    27,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,
    39,    40,    41,    42,    43,    44,    45,    46,
    47,    48,    49,    50,    51,    52,    53,    54,
    55,    56,    57,    58,    59,    60,    61,    62,
    63,    64,    97,    98,    99,   100,   101,   102,
   103,   104,   105,   106,   107,   108,   109,   110,
   111,   112,   113,   114,   115,   116,   117,   118,
   119,   120,   121,   122,    91,    92,    93,    94,
    95,    96,    97,    98,    99,   100,   101,   102,
   103,   104,   105,   106,   107,   108,   109,   110,
   111,   112,   113,   114,   115,   116,   117,   118,
   119,   120,   121,   122,   123,   124,   125,   126,
   127,   128,   129,   130,   131,   132,   133,   134,
   135,   136,   137,   138,   139,   140,   141,   142,
   143,   144,   145,   146,   147,   148,   149,   150,
   151,   152,   153,   154,   155,   156,   157,   158,
   159,   160,   161,   162,   163,   164,   165,   166,
   167,   168,   169,   170,   171,   172,   173,   174,
   175,   176,   177,   178,   179,   180,   181,   182,
   183,   184,   185,   186,   187,   188,   189,   190,
   191,   192,   193,   194,   195,   196,   197,   198,
   199,   200,   201,   202,   203,   204,   205,   206,
   207,   208,   209,   210,   211,   212,   213,   214,
   215,   216,   217,   218,   219,   220,   221,   222,
   223,   224,   225,   226,   227,   228,   229,   230,
   231,   232,   233,   234,   235,   236,   237,   238,
   239,   240,   241,   242,   243,   244,   245,   246,
   247,   248,   249,   250,   251,   252,   253,   254,
   255,
 },
 {
   -1,     0,     1,     2,     3,     4,     5,     6, 
    7,     8,     9,    10,    11,    12,    13,    14, 
   15,    16,    17,    18,    19,    20,    21,    22, 
   23,    24,    25,    26,    27,    28,    29,    30, 
   31,    32,    33,    34,    35,    36,    37,    38, 
   39,    40,    41,    42,    43,    44,    45,    46, 
   47,    48,    49,    50,    51,    52,    53,    54, 
   55,    56,    57,    58,    59,    60,    61,    62, 
   63,    64,    65,    66,    67,    68,    69,    70, 
   71,    72,    73,    74,    75,    76,    77,    78, 
   79,    80,    81,    82,    83,    84,    85,    86, 
   87,    88,    89,    90,    91,    92,    93,    94, 
   95,    96,    65,    66,    67,    68,    69,    70, 
   71,    72,    73,    74,    75,    76,    77,    78, 
   79,    80,    81,    82,    83,    84,    85,    86, 
   87,    88,    89,    90,   123,   124,   125,   126, 
  127,   128,   129,   130,   131,   132,   133,   134, 
  135,   136,   137,   138,   139,   140,   141,   142, 
  143,   144,   145,   146,   147,   148,   149,   150, 
  151,   152,   153,   154,   155,   156,   157,   158, 
  159,   160,   161,   162,   163,   164,   165,   166, 
  167,   168,   169,   170,   171,   172,   173,   174, 
  175,   176,   177,   178,   179,   180,   181,   182, 
  183,   184,   185,   186,   187,   188,   189,   190, 
  191,   192,   193,   194,   195,   196,   197,   198, 
  199,   200,   201,   202,   203,   204,   205,   206, 
  207,   208,   209,   210,   211,   212,   213,   214, 
  215,   216,   217,   218,   219,   220,   221,   222, 
  223,   224,   225,   226,   227,   228,   229,   230, 
  231,   232,   233,   234,   235,   236,   237,   238, 
  239,   240,   241,   242,   243,   244,   245,   246, 
  247,   248,   249,   250,   251,   252,   253,   254, 
  255, 
 },
 {
 000, 000, 000, 000, 000, 000, 000,
 },
};

extern __attr_t ___libc_attr =
{
 &_ast_ctype_tbl,
 { 0 },
 { 0 },
 { 1 },
};

#pragma weak __libc_attr = ___libc_attr

#endif

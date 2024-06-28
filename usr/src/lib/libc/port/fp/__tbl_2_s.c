/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include "lint.h"
#include <sys/types.h>

/* table of 176 multiples of 2**1 */
const unsigned short __tbl_2_small_digits [] = { 1,
/* 2**1 = */
2 /* e   0 */,
/* 2**2 = */
4 /* e   0 */,
/* 2**3 = */
8 /* e   0 */,
/* 2**4 = */
16 /* e   0 */,
/* 2**5 = */
32 /* e   0 */,
/* 2**6 = */
64 /* e   0 */,
/* 2**7 = */
128 /* e   0 */,
/* 2**8 = */
256 /* e   0 */,
/* 2**9 = */
512 /* e   0 */,
/* 2**10 = */
1024 /* e   0 */,
/* 2**11 = */
2048 /* e   0 */,
/* 2**12 = */
4096 /* e   0 */,
/* 2**13 = */
8192 /* e   0 */,
/* 2**14 = */
6384 /* e   0 */,    1 /* e   4 */,
/* 2**15 = */
2768 /* e   0 */,    3 /* e   4 */,
/* 2**16 = */
5536 /* e   0 */,    6 /* e   4 */,
/* 2**17 = */
1072 /* e   0 */,   13 /* e   4 */,
/* 2**18 = */
2144 /* e   0 */,   26 /* e   4 */,
/* 2**19 = */
4288 /* e   0 */,   52 /* e   4 */,
/* 2**20 = */
8576 /* e   0 */,  104 /* e   4 */,
/* 2**21 = */
7152 /* e   0 */,  209 /* e   4 */,
/* 2**22 = */
4304 /* e   0 */,  419 /* e   4 */,
/* 2**23 = */
8608 /* e   0 */,  838 /* e   4 */,
/* 2**24 = */
7216 /* e   0 */, 1677 /* e   4 */,
/* 2**25 = */
4432 /* e   0 */, 3355 /* e   4 */,
/* 2**26 = */
8864 /* e   0 */, 6710 /* e   4 */,
/* 2**27 = */
7728 /* e   0 */, 3421 /* e   4 */,    1 /* e   8 */,
/* 2**28 = */
5456 /* e   0 */, 6843 /* e   4 */,    2 /* e   8 */,
/* 2**29 = */
912 /* e   0 */, 3687 /* e   4 */,    5 /* e   8 */,
/* 2**30 = */
1824 /* e   0 */, 7374 /* e   4 */,   10 /* e   8 */,
/* 2**31 = */
3648 /* e   0 */, 4748 /* e   4 */,   21 /* e   8 */,
/* 2**32 = */
7296 /* e   0 */, 9496 /* e   4 */,   42 /* e   8 */,
/* 2**33 = */
4592 /* e   0 */, 8993 /* e   4 */,   85 /* e   8 */,
/* 2**34 = */
9184 /* e   0 */, 7986 /* e   4 */,  171 /* e   8 */,
/* 2**35 = */
8368 /* e   0 */, 5973 /* e   4 */,  343 /* e   8 */,
/* 2**36 = */
6736 /* e   0 */, 1947 /* e   4 */,  687 /* e   8 */,
/* 2**37 = */
3472 /* e   0 */, 3895 /* e   4 */, 1374 /* e   8 */,
/* 2**38 = */
6944 /* e   0 */, 7790 /* e   4 */, 2748 /* e   8 */,
/* 2**39 = */
3888 /* e   0 */, 5581 /* e   4 */, 5497 /* e   8 */,
/* 2**40 = */
7776 /* e   0 */, 1162 /* e   4 */,  995 /* e   8 */,    1 /* e  12 */,

/* 2**41 = */
5552 /* e   0 */, 2325 /* e   4 */, 1990 /* e   8 */,    2 /* e  12 */,

/* 2**42 = */
1104 /* e   0 */, 4651 /* e   4 */, 3980 /* e   8 */,    4 /* e  12 */,

/* 2**43 = */
2208 /* e   0 */, 9302 /* e   4 */, 7960 /* e   8 */,    8 /* e  12 */,

/* 2**44 = */
4416 /* e   0 */, 8604 /* e   4 */, 5921 /* e   8 */,   17 /* e  12 */,

/* 2**45 = */
8832 /* e   0 */, 7208 /* e   4 */, 1843 /* e   8 */,   35 /* e  12 */,

/* 2**46 = */
7664 /* e   0 */, 4417 /* e   4 */, 3687 /* e   8 */,   70 /* e  12 */,

/* 2**47 = */
5328 /* e   0 */, 8835 /* e   4 */, 7374 /* e   8 */,  140 /* e  12 */,

/* 2**48 = */
656 /* e   0 */, 7671 /* e   4 */, 4749 /* e   8 */,  281 /* e  12 */,

/* 2**49 = */
1312 /* e   0 */, 5342 /* e   4 */, 9499 /* e   8 */,  562 /* e  12 */,

/* 2**50 = */
2624 /* e   0 */,  684 /* e   4 */, 8999 /* e   8 */, 1125 /* e  12 */,

/* 2**51 = */
5248 /* e   0 */, 1368 /* e   4 */, 7998 /* e   8 */, 2251 /* e  12 */,

/* 2**52 = */
496 /* e   0 */, 2737 /* e   4 */, 5996 /* e   8 */, 4503 /* e  12 */,

/* 2**53 = */
992 /* e   0 */, 5474 /* e   4 */, 1992 /* e   8 */, 9007 /* e  12 */,

/* 2**54 = */
1984 /* e   0 */,  948 /* e   4 */, 3985 /* e   8 */, 8014 /* e  12 */,
1 /* e  16 */,
/* 2**55 = */
3968 /* e   0 */, 1896 /* e   4 */, 7970 /* e   8 */, 6028 /* e  12 */,
3 /* e  16 */,
/* 2**56 = */
7936 /* e   0 */, 3792 /* e   4 */, 5940 /* e   8 */, 2057 /* e  12 */,
7 /* e  16 */,
/* 2**57 = */
5872 /* e   0 */, 7585 /* e   4 */, 1880 /* e   8 */, 4115 /* e  12 */,
14 /* e  16 */,
/* 2**58 = */
1744 /* e   0 */, 5171 /* e   4 */, 3761 /* e   8 */, 8230 /* e  12 */,
28 /* e  16 */,
/* 2**59 = */
3488 /* e   0 */,  342 /* e   4 */, 7523 /* e   8 */, 6460 /* e  12 */,
57 /* e  16 */,
/* 2**60 = */
6976 /* e   0 */,  684 /* e   4 */, 5046 /* e   8 */, 2921 /* e  12 */,
115 /* e  16 */,
/* 2**61 = */
3952 /* e   0 */, 1369 /* e   4 */,   92 /* e   8 */, 5843 /* e  12 */,
230 /* e  16 */,
/* 2**62 = */
7904 /* e   0 */, 2738 /* e   4 */,  184 /* e   8 */, 1686 /* e  12 */,
461 /* e  16 */,
/* 2**63 = */
5808 /* e   0 */, 5477 /* e   4 */,  368 /* e   8 */, 3372 /* e  12 */,
922 /* e  16 */,
/* 2**64 = */
1616 /* e   0 */,  955 /* e   4 */,  737 /* e   8 */, 6744 /* e  12 */,
1844 /* e  16 */,
/* 2**65 = */
3232 /* e   0 */, 1910 /* e   4 */, 1474 /* e   8 */, 3488 /* e  12 */,
3689 /* e  16 */,
/* 2**66 = */
6464 /* e   0 */, 3820 /* e   4 */, 2948 /* e   8 */, 6976 /* e  12 */,
7378 /* e  16 */,
/* 2**67 = */
2928 /* e   0 */, 7641 /* e   4 */, 5896 /* e   8 */, 3952 /* e  12 */,
4757 /* e  16 */,    1 /* e  20 */,
/* 2**68 = */
5856 /* e   0 */, 5282 /* e   4 */, 1793 /* e   8 */, 7905 /* e  12 */,
9514 /* e  16 */,    2 /* e  20 */,
/* 2**69 = */
1712 /* e   0 */,  565 /* e   4 */, 3587 /* e   8 */, 5810 /* e  12 */,
9029 /* e  16 */,    5 /* e  20 */,
/* 2**70 = */
3424 /* e   0 */, 1130 /* e   4 */, 7174 /* e   8 */, 1620 /* e  12 */,
8059 /* e  16 */,   11 /* e  20 */,
/* 2**71 = */
6848 /* e   0 */, 2260 /* e   4 */, 4348 /* e   8 */, 3241 /* e  12 */,
6118 /* e  16 */,   23 /* e  20 */,
/* 2**72 = */
3696 /* e   0 */, 4521 /* e   4 */, 8696 /* e   8 */, 6482 /* e  12 */,
2236 /* e  16 */,   47 /* e  20 */,
/* 2**73 = */
7392 /* e   0 */, 9042 /* e   4 */, 7392 /* e   8 */, 2965 /* e  12 */,
4473 /* e  16 */,   94 /* e  20 */,
/* 2**74 = */
4784 /* e   0 */, 8085 /* e   4 */, 4785 /* e   8 */, 5931 /* e  12 */,
8946 /* e  16 */,  188 /* e  20 */,
/* 2**75 = */
9568 /* e   0 */, 6170 /* e   4 */, 9571 /* e   8 */, 1862 /* e  12 */,
7893 /* e  16 */,  377 /* e  20 */,
/* 2**76 = */
9136 /* e   0 */, 2341 /* e   4 */, 9143 /* e   8 */, 3725 /* e  12 */,
5786 /* e  16 */,  755 /* e  20 */,
/* 2**77 = */
8272 /* e   0 */, 4683 /* e   4 */, 8286 /* e   8 */, 7451 /* e  12 */,
1572 /* e  16 */, 1511 /* e  20 */,
/* 2**78 = */
6544 /* e   0 */, 9367 /* e   4 */, 6572 /* e   8 */, 4903 /* e  12 */,
3145 /* e  16 */, 3022 /* e  20 */,
/* 2**79 = */
3088 /* e   0 */, 8735 /* e   4 */, 3145 /* e   8 */, 9807 /* e  12 */,
6290 /* e  16 */, 6044 /* e  20 */,
/* 2**80 = */
6176 /* e   0 */, 7470 /* e   4 */, 6291 /* e   8 */, 9614 /* e  12 */,
2581 /* e  16 */, 2089 /* e  20 */,    1 /* e  24 */,
/* 2**81 = */
2352 /* e   0 */, 4941 /* e   4 */, 2583 /* e   8 */, 9229 /* e  12 */,
5163 /* e  16 */, 4178 /* e  20 */,    2 /* e  24 */,
/* 2**82 = */
4704 /* e   0 */, 9882 /* e   4 */, 5166 /* e   8 */, 8458 /* e  12 */,
327 /* e  16 */, 8357 /* e  20 */,    4 /* e  24 */,
/* 2**83 = */
9408 /* e   0 */, 9764 /* e   4 */,  333 /* e   8 */, 6917 /* e  12 */,
655 /* e  16 */, 6714 /* e  20 */,    9 /* e  24 */,
/* 2**84 = */
8816 /* e   0 */, 9529 /* e   4 */,  667 /* e   8 */, 3834 /* e  12 */,
1311 /* e  16 */, 3428 /* e  20 */,   19 /* e  24 */,
/* 2**85 = */
7632 /* e   0 */, 9059 /* e   4 */, 1335 /* e   8 */, 7668 /* e  12 */,
2622 /* e  16 */, 6856 /* e  20 */,   38 /* e  24 */,
/* 2**86 = */
5264 /* e   0 */, 8119 /* e   4 */, 2671 /* e   8 */, 5336 /* e  12 */,
5245 /* e  16 */, 3712 /* e  20 */,   77 /* e  24 */,
/* 2**87 = */
528 /* e   0 */, 6239 /* e   4 */, 5343 /* e   8 */,  672 /* e  12 */,
491 /* e  16 */, 7425 /* e  20 */,  154 /* e  24 */,
/* 2**88 = */
1056 /* e   0 */, 2478 /* e   4 */,  687 /* e   8 */, 1345 /* e  12 */,
982 /* e  16 */, 4850 /* e  20 */,  309 /* e  24 */,
/* 2**89 = */
2112 /* e   0 */, 4956 /* e   4 */, 1374 /* e   8 */, 2690 /* e  12 */,
1964 /* e  16 */, 9700 /* e  20 */,  618 /* e  24 */,
/* 2**90 = */
4224 /* e   0 */, 9912 /* e   4 */, 2748 /* e   8 */, 5380 /* e  12 */,
3928 /* e  16 */, 9400 /* e  20 */, 1237 /* e  24 */,
/* 2**91 = */
8448 /* e   0 */, 9824 /* e   4 */, 5497 /* e   8 */,  760 /* e  12 */,
7857 /* e  16 */, 8800 /* e  20 */, 2475 /* e  24 */,
/* 2**92 = */
6896 /* e   0 */, 9649 /* e   4 */,  995 /* e   8 */, 1521 /* e  12 */,
5714 /* e  16 */, 7601 /* e  20 */, 4951 /* e  24 */,
/* 2**93 = */
3792 /* e   0 */, 9299 /* e   4 */, 1991 /* e   8 */, 3042 /* e  12 */,
1428 /* e  16 */, 5203 /* e  20 */, 9903 /* e  24 */,
/* 2**94 = */
7584 /* e   0 */, 8598 /* e   4 */, 3983 /* e   8 */, 6084 /* e  12 */,
2856 /* e  16 */,  406 /* e  20 */, 9807 /* e  24 */,    1 /* e  28 */,

/* 2**95 = */
5168 /* e   0 */, 7197 /* e   4 */, 7967 /* e   8 */, 2168 /* e  12 */,
5713 /* e  16 */,  812 /* e  20 */, 9614 /* e  24 */,    3 /* e  28 */,

/* 2**96 = */
336 /* e   0 */, 4395 /* e   4 */, 5935 /* e   8 */, 4337 /* e  12 */,
1426 /* e  16 */, 1625 /* e  20 */, 9228 /* e  24 */,    7 /* e  28 */,

/* 2**97 = */
672 /* e   0 */, 8790 /* e   4 */, 1870 /* e   8 */, 8675 /* e  12 */,
2852 /* e  16 */, 3250 /* e  20 */, 8456 /* e  24 */,   15 /* e  28 */,

/* 2**98 = */
1344 /* e   0 */, 7580 /* e   4 */, 3741 /* e   8 */, 7350 /* e  12 */,
5705 /* e  16 */, 6500 /* e  20 */, 6912 /* e  24 */,   31 /* e  28 */,

/* 2**99 = */
2688 /* e   0 */, 5160 /* e   4 */, 7483 /* e   8 */, 4700 /* e  12 */,
1411 /* e  16 */, 3001 /* e  20 */, 3825 /* e  24 */,   63 /* e  28 */,

/* 2**100 = */
5376 /* e   0 */,  320 /* e   4 */, 4967 /* e   8 */, 9401 /* e  12 */,
2822 /* e  16 */, 6002 /* e  20 */, 7650 /* e  24 */,  126 /* e  28 */,

/* 2**101 = */
752 /* e   0 */,  641 /* e   4 */, 9934 /* e   8 */, 8802 /* e  12 */,
5645 /* e  16 */, 2004 /* e  20 */, 5301 /* e  24 */,  253 /* e  28 */,

/* 2**102 = */
1504 /* e   0 */, 1282 /* e   4 */, 9868 /* e   8 */, 7605 /* e  12 */,
1291 /* e  16 */, 4009 /* e  20 */,  602 /* e  24 */,  507 /* e  28 */,

/* 2**103 = */
3008 /* e   0 */, 2564 /* e   4 */, 9736 /* e   8 */, 5211 /* e  12 */,
2583 /* e  16 */, 8018 /* e  20 */, 1204 /* e  24 */, 1014 /* e  28 */,

/* 2**104 = */
6016 /* e   0 */, 5128 /* e   4 */, 9472 /* e   8 */,  423 /* e  12 */,
5167 /* e  16 */, 6036 /* e  20 */, 2409 /* e  24 */, 2028 /* e  28 */,

/* 2**105 = */
2032 /* e   0 */,  257 /* e   4 */, 8945 /* e   8 */,  847 /* e  12 */,
334 /* e  16 */, 2073 /* e  20 */, 4819 /* e  24 */, 4056 /* e  28 */,

/* 2**106 = */
4064 /* e   0 */,  514 /* e   4 */, 7890 /* e   8 */, 1695 /* e  12 */,
668 /* e  16 */, 4146 /* e  20 */, 9638 /* e  24 */, 8112 /* e  28 */,

/* 2**107 = */
8128 /* e   0 */, 1028 /* e   4 */, 5780 /* e   8 */, 3391 /* e  12 */,
1336 /* e  16 */, 8292 /* e  20 */, 9276 /* e  24 */, 6225 /* e  28 */,
1 /* e  32 */,
/* 2**108 = */
6256 /* e   0 */, 2057 /* e   4 */, 1560 /* e   8 */, 6783 /* e  12 */,
2672 /* e  16 */, 6584 /* e  20 */, 8553 /* e  24 */, 2451 /* e  28 */,
3 /* e  32 */,
/* 2**109 = */
2512 /* e   0 */, 4115 /* e   4 */, 3120 /* e   8 */, 3566 /* e  12 */,
5345 /* e  16 */, 3168 /* e  20 */, 7107 /* e  24 */, 4903 /* e  28 */,
6 /* e  32 */,
/* 2**110 = */
5024 /* e   0 */, 8230 /* e   4 */, 6240 /* e   8 */, 7132 /* e  12 */,
690 /* e  16 */, 6337 /* e  20 */, 4214 /* e  24 */, 9807 /* e  28 */,
12 /* e  32 */,
/* 2**111 = */
48 /* e   0 */, 6461 /* e   4 */, 2481 /* e   8 */, 4265 /* e  12 */,
1381 /* e  16 */, 2674 /* e  20 */, 8429 /* e  24 */, 9614 /* e  28 */,
25 /* e  32 */,
/* 2**112 = */
96 /* e   0 */, 2922 /* e   4 */, 4963 /* e   8 */, 8530 /* e  12 */,
2762 /* e  16 */, 5348 /* e  20 */, 6858 /* e  24 */, 9229 /* e  28 */,
51 /* e  32 */,
/* 2**113 = */
192 /* e   0 */, 5844 /* e   4 */, 9926 /* e   8 */, 7060 /* e  12 */,
5525 /* e  16 */,  696 /* e  20 */, 3717 /* e  24 */, 8459 /* e  28 */,
103 /* e  32 */,
/* 2**114 = */
384 /* e   0 */, 1688 /* e   4 */, 9853 /* e   8 */, 4121 /* e  12 */,
1051 /* e  16 */, 1393 /* e  20 */, 7434 /* e  24 */, 6918 /* e  28 */,
207 /* e  32 */,
/* 2**115 = */
768 /* e   0 */, 3376 /* e   4 */, 9706 /* e   8 */, 8243 /* e  12 */,
2102 /* e  16 */, 2786 /* e  20 */, 4868 /* e  24 */, 3837 /* e  28 */,
415 /* e  32 */,
/* 2**116 = */
1536 /* e   0 */, 6752 /* e   4 */, 9412 /* e   8 */, 6487 /* e  12 */,
4205 /* e  16 */, 5572 /* e  20 */, 9736 /* e  24 */, 7674 /* e  28 */,
830 /* e  32 */,
/* 2**117 = */
3072 /* e   0 */, 3504 /* e   4 */, 8825 /* e   8 */, 2975 /* e  12 */,
8411 /* e  16 */, 1144 /* e  20 */, 9473 /* e  24 */, 5349 /* e  28 */,
1661 /* e  32 */,
/* 2**118 = */
6144 /* e   0 */, 7008 /* e   4 */, 7650 /* e   8 */, 5951 /* e  12 */,
6822 /* e  16 */, 2289 /* e  20 */, 8946 /* e  24 */,  699 /* e  28 */,
3323 /* e  32 */,
/* 2**119 = */
2288 /* e   0 */, 4017 /* e   4 */, 5301 /* e   8 */, 1903 /* e  12 */,
3645 /* e  16 */, 4579 /* e  20 */, 7892 /* e  24 */, 1399 /* e  28 */,
6646 /* e  32 */,
/* 2**120 = */
4576 /* e   0 */, 8034 /* e   4 */,  602 /* e   8 */, 3807 /* e  12 */,
7290 /* e  16 */, 9158 /* e  20 */, 5784 /* e  24 */, 2799 /* e  28 */,
3292 /* e  32 */,    1 /* e  36 */,
/* 2**121 = */
9152 /* e   0 */, 6068 /* e   4 */, 1205 /* e   8 */, 7614 /* e  12 */,
4580 /* e  16 */, 8317 /* e  20 */, 1569 /* e  24 */, 5599 /* e  28 */,
6584 /* e  32 */,    2 /* e  36 */,
/* 2**122 = */
8304 /* e   0 */, 2137 /* e   4 */, 2411 /* e   8 */, 5228 /* e  12 */,
9161 /* e  16 */, 6634 /* e  20 */, 3139 /* e  24 */, 1198 /* e  28 */,
3169 /* e  32 */,    5 /* e  36 */,
/* 2**123 = */
6608 /* e   0 */, 4275 /* e   4 */, 4822 /* e   8 */,  456 /* e  12 */,
8323 /* e  16 */, 3269 /* e  20 */, 6279 /* e  24 */, 2396 /* e  28 */,
6338 /* e  32 */,   10 /* e  36 */,
/* 2**124 = */
3216 /* e   0 */, 8551 /* e   4 */, 9644 /* e   8 */,  912 /* e  12 */,
6646 /* e  16 */, 6539 /* e  20 */, 2558 /* e  24 */, 4793 /* e  28 */,
2676 /* e  32 */,   21 /* e  36 */,
/* 2**125 = */
6432 /* e   0 */, 7102 /* e   4 */, 9289 /* e   8 */, 1825 /* e  12 */,
3292 /* e  16 */, 3079 /* e  20 */, 5117 /* e  24 */, 9586 /* e  28 */,
5352 /* e  32 */,   42 /* e  36 */,
/* 2**126 = */
2864 /* e   0 */, 4205 /* e   4 */, 8579 /* e   8 */, 3651 /* e  12 */,
6584 /* e  16 */, 6158 /* e  20 */,  234 /* e  24 */, 9173 /* e  28 */,
705 /* e  32 */,   85 /* e  36 */,
/* 2**127 = */
5728 /* e   0 */, 8410 /* e   4 */, 7158 /* e   8 */, 7303 /* e  12 */,
3168 /* e  16 */, 2317 /* e  20 */,  469 /* e  24 */, 8346 /* e  28 */,
1411 /* e  32 */,  170 /* e  36 */,
/* 2**128 = */
1456 /* e   0 */, 6821 /* e   4 */, 4317 /* e   8 */, 4607 /* e  12 */,
6337 /* e  16 */, 4634 /* e  20 */,  938 /* e  24 */, 6692 /* e  28 */,
2823 /* e  32 */,  340 /* e  36 */,
/* 2**129 = */
2912 /* e   0 */, 3642 /* e   4 */, 8635 /* e   8 */, 9214 /* e  12 */,
2674 /* e  16 */, 9269 /* e  20 */, 1876 /* e  24 */, 3384 /* e  28 */,
5647 /* e  32 */,  680 /* e  36 */,
/* 2**130 = */
5824 /* e   0 */, 7284 /* e   4 */, 7270 /* e   8 */, 8429 /* e  12 */,
5349 /* e  16 */, 8538 /* e  20 */, 3753 /* e  24 */, 6768 /* e  28 */,
1294 /* e  32 */, 1361 /* e  36 */,
/* 2**131 = */
1648 /* e   0 */, 4569 /* e   4 */, 4541 /* e   8 */, 6859 /* e  12 */,
699 /* e  16 */, 7077 /* e  20 */, 7507 /* e  24 */, 3536 /* e  28 */,
2589 /* e  32 */, 2722 /* e  36 */,
/* 2**132 = */
3296 /* e   0 */, 9138 /* e   4 */, 9082 /* e   8 */, 3718 /* e  12 */,
1399 /* e  16 */, 4154 /* e  20 */, 5015 /* e  24 */, 7073 /* e  28 */,
5178 /* e  32 */, 5444 /* e  36 */,
/* 2**133 = */
6592 /* e   0 */, 8276 /* e   4 */, 8165 /* e   8 */, 7437 /* e  12 */,
2798 /* e  16 */, 8308 /* e  20 */,   30 /* e  24 */, 4147 /* e  28 */,
357 /* e  32 */,  889 /* e  36 */,    1 /* e  40 */,
/* 2**134 = */
3184 /* e   0 */, 6553 /* e   4 */, 6331 /* e   8 */, 4875 /* e  12 */,
5597 /* e  16 */, 6616 /* e  20 */,   61 /* e  24 */, 8294 /* e  28 */,
714 /* e  32 */, 1778 /* e  36 */,    2 /* e  40 */,
/* 2**135 = */
6368 /* e   0 */, 3106 /* e   4 */, 2663 /* e   8 */, 9751 /* e  12 */,
1194 /* e  16 */, 3233 /* e  20 */,  123 /* e  24 */, 6588 /* e  28 */,
1429 /* e  32 */, 3556 /* e  36 */,    4 /* e  40 */,
/* 2**136 = */
2736 /* e   0 */, 6213 /* e   4 */, 5326 /* e   8 */, 9502 /* e  12 */,
2389 /* e  16 */, 6466 /* e  20 */,  246 /* e  24 */, 3176 /* e  28 */,
2859 /* e  32 */, 7112 /* e  36 */,    8 /* e  40 */,
/* 2**137 = */
5472 /* e   0 */, 2426 /* e   4 */,  653 /* e   8 */, 9005 /* e  12 */,
4779 /* e  16 */, 2932 /* e  20 */,  493 /* e  24 */, 6352 /* e  28 */,
5718 /* e  32 */, 4224 /* e  36 */,   17 /* e  40 */,
/* 2**138 = */
944 /* e   0 */, 4853 /* e   4 */, 1306 /* e   8 */, 8010 /* e  12 */,
9559 /* e  16 */, 5864 /* e  20 */,  986 /* e  24 */, 2704 /* e  28 */,
1437 /* e  32 */, 8449 /* e  36 */,   34 /* e  40 */,
/* 2**139 = */
1888 /* e   0 */, 9706 /* e   4 */, 2612 /* e   8 */, 6020 /* e  12 */,
9119 /* e  16 */, 1729 /* e  20 */, 1973 /* e  24 */, 5408 /* e  28 */,
2874 /* e  32 */, 6898 /* e  36 */,   69 /* e  40 */,
/* 2**140 = */
3776 /* e   0 */, 9412 /* e   4 */, 5225 /* e   8 */, 2040 /* e  12 */,
8239 /* e  16 */, 3459 /* e  20 */, 3946 /* e  24 */,  816 /* e  28 */,
5749 /* e  32 */, 3796 /* e  36 */,  139 /* e  40 */,
/* 2**141 = */
7552 /* e   0 */, 8824 /* e   4 */,  451 /* e   8 */, 4081 /* e  12 */,
6478 /* e  16 */, 6919 /* e  20 */, 7892 /* e  24 */, 1632 /* e  28 */,
1498 /* e  32 */, 7593 /* e  36 */,  278 /* e  40 */,
/* 2**142 = */
5104 /* e   0 */, 7649 /* e   4 */,  903 /* e   8 */, 8162 /* e  12 */,
2956 /* e  16 */, 3839 /* e  20 */, 5785 /* e  24 */, 3265 /* e  28 */,
2996 /* e  32 */, 5186 /* e  36 */,  557 /* e  40 */,
/* 2**143 = */
208 /* e   0 */, 5299 /* e   4 */, 1807 /* e   8 */, 6324 /* e  12 */,
5913 /* e  16 */, 7678 /* e  20 */, 1570 /* e  24 */, 6531 /* e  28 */,
5992 /* e  32 */,  372 /* e  36 */, 1115 /* e  40 */,
/* 2**144 = */
416 /* e   0 */,  598 /* e   4 */, 3615 /* e   8 */, 2648 /* e  12 */,
1827 /* e  16 */, 5357 /* e  20 */, 3141 /* e  24 */, 3062 /* e  28 */,
1985 /* e  32 */,  745 /* e  36 */, 2230 /* e  40 */,
/* 2**145 = */
832 /* e   0 */, 1196 /* e   4 */, 7230 /* e   8 */, 5296 /* e  12 */,
3654 /* e  16 */,  714 /* e  20 */, 6283 /* e  24 */, 6124 /* e  28 */,
3970 /* e  32 */, 1490 /* e  36 */, 4460 /* e  40 */,
/* 2**146 = */
1664 /* e   0 */, 2392 /* e   4 */, 4460 /* e   8 */,  593 /* e  12 */,
7309 /* e  16 */, 1428 /* e  20 */, 2566 /* e  24 */, 2249 /* e  28 */,
7941 /* e  32 */, 2980 /* e  36 */, 8920 /* e  40 */,
/* 2**147 = */
3328 /* e   0 */, 4784 /* e   4 */, 8920 /* e   8 */, 1186 /* e  12 */,
4618 /* e  16 */, 2857 /* e  20 */, 5132 /* e  24 */, 4498 /* e  28 */,
5882 /* e  32 */, 5961 /* e  36 */, 7840 /* e  40 */,    1 /* e  44 */,

/* 2**148 = */
6656 /* e   0 */, 9568 /* e   4 */, 7840 /* e   8 */, 2373 /* e  12 */,
9236 /* e  16 */, 5714 /* e  20 */,  264 /* e  24 */, 8997 /* e  28 */,
1764 /* e  32 */, 1923 /* e  36 */, 5681 /* e  40 */,    3 /* e  44 */,

/* 2**149 = */
3312 /* e   0 */, 9137 /* e   4 */, 5681 /* e   8 */, 4747 /* e  12 */,
8472 /* e  16 */, 1429 /* e  20 */,  529 /* e  24 */, 7994 /* e  28 */,
3529 /* e  32 */, 3846 /* e  36 */, 1362 /* e  40 */,    7 /* e  44 */,

/* 2**150 = */
6624 /* e   0 */, 8274 /* e   4 */, 1363 /* e   8 */, 9495 /* e  12 */,
6944 /* e  16 */, 2859 /* e  20 */, 1058 /* e  24 */, 5988 /* e  28 */,
7059 /* e  32 */, 7692 /* e  36 */, 2724 /* e  40 */,   14 /* e  44 */,

/* 2**151 = */
3248 /* e   0 */, 6549 /* e   4 */, 2727 /* e   8 */, 8990 /* e  12 */,
3889 /* e  16 */, 5719 /* e  20 */, 2116 /* e  24 */, 1976 /* e  28 */,
4119 /* e  32 */, 5385 /* e  36 */, 5449 /* e  40 */,   28 /* e  44 */,

/* 2**152 = */
6496 /* e   0 */, 3098 /* e   4 */, 5455 /* e   8 */, 7980 /* e  12 */,
7779 /* e  16 */, 1438 /* e  20 */, 4233 /* e  24 */, 3952 /* e  28 */,
8238 /* e  32 */,  770 /* e  36 */,  899 /* e  40 */,   57 /* e  44 */,

/* 2**153 = */
2992 /* e   0 */, 6197 /* e   4 */,  910 /* e   8 */, 5961 /* e  12 */,
5559 /* e  16 */, 2877 /* e  20 */, 8466 /* e  24 */, 7904 /* e  28 */,
6476 /* e  32 */, 1541 /* e  36 */, 1798 /* e  40 */,  114 /* e  44 */,

/* 2**154 = */
5984 /* e   0 */, 2394 /* e   4 */, 1821 /* e   8 */, 1922 /* e  12 */,
1119 /* e  16 */, 5755 /* e  20 */, 6932 /* e  24 */, 5809 /* e  28 */,
2953 /* e  32 */, 3083 /* e  36 */, 3596 /* e  40 */,  228 /* e  44 */,

/* 2**155 = */
1968 /* e   0 */, 4789 /* e   4 */, 3642 /* e   8 */, 3844 /* e  12 */,
2238 /* e  16 */, 1510 /* e  20 */, 3865 /* e  24 */, 1619 /* e  28 */,
5907 /* e  32 */, 6166 /* e  36 */, 7192 /* e  40 */,  456 /* e  44 */,

/* 2**156 = */
3936 /* e   0 */, 9578 /* e   4 */, 7284 /* e   8 */, 7688 /* e  12 */,
4476 /* e  16 */, 3020 /* e  20 */, 7730 /* e  24 */, 3238 /* e  28 */,
1814 /* e  32 */, 2333 /* e  36 */, 4385 /* e  40 */,  913 /* e  44 */,

/* 2**157 = */
7872 /* e   0 */, 9156 /* e   4 */, 4569 /* e   8 */, 5377 /* e  12 */,
8953 /* e  16 */, 6040 /* e  20 */, 5460 /* e  24 */, 6477 /* e  28 */,
3628 /* e  32 */, 4666 /* e  36 */, 8770 /* e  40 */, 1826 /* e  44 */,

/* 2**158 = */
5744 /* e   0 */, 8313 /* e   4 */, 9139 /* e   8 */,  754 /* e  12 */,
7907 /* e  16 */, 2081 /* e  20 */,  921 /* e  24 */, 2955 /* e  28 */,
7257 /* e  32 */, 9332 /* e  36 */, 7540 /* e  40 */, 3653 /* e  44 */,

/* 2**159 = */
1488 /* e   0 */, 6627 /* e   4 */, 8279 /* e   8 */, 1509 /* e  12 */,
5814 /* e  16 */, 4163 /* e  20 */, 1842 /* e  24 */, 5910 /* e  28 */,
4514 /* e  32 */, 8665 /* e  36 */, 5081 /* e  40 */, 7307 /* e  44 */,

/* 2**160 = */
2976 /* e   0 */, 3254 /* e   4 */, 6559 /* e   8 */, 3019 /* e  12 */,
1628 /* e  16 */, 8327 /* e  20 */, 3684 /* e  24 */, 1820 /* e  28 */,
9029 /* e  32 */, 7330 /* e  36 */,  163 /* e  40 */, 4615 /* e  44 */,
1 /* e  48 */,
/* 2**161 = */
5952 /* e   0 */, 6508 /* e   4 */, 3118 /* e   8 */, 6039 /* e  12 */,
3256 /* e  16 */, 6654 /* e  20 */, 7369 /* e  24 */, 3640 /* e  28 */,
8058 /* e  32 */, 4661 /* e  36 */,  327 /* e  40 */, 9230 /* e  44 */,
2 /* e  48 */,
/* 2**162 = */
1904 /* e   0 */, 3017 /* e   4 */, 6237 /* e   8 */, 2078 /* e  12 */,
6513 /* e  16 */, 3308 /* e  20 */, 4739 /* e  24 */, 7281 /* e  28 */,
6116 /* e  32 */, 9323 /* e  36 */,  654 /* e  40 */, 8460 /* e  44 */,
5 /* e  48 */,
/* 2**163 = */
3808 /* e   0 */, 6034 /* e   4 */, 2474 /* e   8 */, 4157 /* e  12 */,
3026 /* e  16 */, 6617 /* e  20 */, 9478 /* e  24 */, 4562 /* e  28 */,
2233 /* e  32 */, 8647 /* e  36 */, 1309 /* e  40 */, 6920 /* e  44 */,
11 /* e  48 */,
/* 2**164 = */
7616 /* e   0 */, 2068 /* e   4 */, 4949 /* e   8 */, 8314 /* e  12 */,
6052 /* e  16 */, 3234 /* e  20 */, 8957 /* e  24 */, 9125 /* e  28 */,
4466 /* e  32 */, 7294 /* e  36 */, 2619 /* e  40 */, 3840 /* e  44 */,
23 /* e  48 */,
/* 2**165 = */
5232 /* e   0 */, 4137 /* e   4 */, 9898 /* e   8 */, 6628 /* e  12 */,
2105 /* e  16 */, 6469 /* e  20 */, 7914 /* e  24 */, 8251 /* e  28 */,
8933 /* e  32 */, 4588 /* e  36 */, 5239 /* e  40 */, 7680 /* e  44 */,
46 /* e  48 */,
/* 2**166 = */
464 /* e   0 */, 8275 /* e   4 */, 9796 /* e   8 */, 3257 /* e  12 */,
4211 /* e  16 */, 2938 /* e  20 */, 5829 /* e  24 */, 6503 /* e  28 */,
7867 /* e  32 */, 9177 /* e  36 */,  478 /* e  40 */, 5361 /* e  44 */,
93 /* e  48 */,
/* 2**167 = */
928 /* e   0 */, 6550 /* e   4 */, 9593 /* e   8 */, 6515 /* e  12 */,
8422 /* e  16 */, 5876 /* e  20 */, 1658 /* e  24 */, 3007 /* e  28 */,
5735 /* e  32 */, 8355 /* e  36 */,  957 /* e  40 */,  722 /* e  44 */,
187 /* e  48 */,
/* 2**168 = */
1856 /* e   0 */, 3100 /* e   4 */, 9187 /* e   8 */, 3031 /* e  12 */,
6845 /* e  16 */, 1753 /* e  20 */, 3317 /* e  24 */, 6014 /* e  28 */,
1470 /* e  32 */, 6711 /* e  36 */, 1915 /* e  40 */, 1444 /* e  44 */,
374 /* e  48 */,
/* 2**169 = */
3712 /* e   0 */, 6200 /* e   4 */, 8374 /* e   8 */, 6063 /* e  12 */,
3690 /* e  16 */, 3507 /* e  20 */, 6634 /* e  24 */, 2028 /* e  28 */,
2941 /* e  32 */, 3422 /* e  36 */, 3831 /* e  40 */, 2888 /* e  44 */,
748 /* e  48 */,
/* 2**170 = */
7424 /* e   0 */, 2400 /* e   4 */, 6749 /* e   8 */, 2127 /* e  12 */,
7381 /* e  16 */, 7014 /* e  20 */, 3268 /* e  24 */, 4057 /* e  28 */,
5882 /* e  32 */, 6844 /* e  36 */, 7662 /* e  40 */, 5776 /* e  44 */,
1496 /* e  48 */,
/* 2**171 = */
4848 /* e   0 */, 4801 /* e   4 */, 3498 /* e   8 */, 4255 /* e  12 */,
4762 /* e  16 */, 4029 /* e  20 */, 6537 /* e  24 */, 8114 /* e  28 */,
1764 /* e  32 */, 3689 /* e  36 */, 5325 /* e  40 */, 1553 /* e  44 */,
2993 /* e  48 */,
/* 2**172 = */
9696 /* e   0 */, 9602 /* e   4 */, 6996 /* e   8 */, 8510 /* e  12 */,
9524 /* e  16 */, 8058 /* e  20 */, 3074 /* e  24 */, 6229 /* e  28 */,
3529 /* e  32 */, 7378 /* e  36 */,  650 /* e  40 */, 3107 /* e  44 */,
5986 /* e  48 */,
/* 2**173 = */
9392 /* e   0 */, 9205 /* e   4 */, 3993 /* e   8 */, 7021 /* e  12 */,
9049 /* e  16 */, 6117 /* e  20 */, 6149 /* e  24 */, 2458 /* e  28 */,
7059 /* e  32 */, 4756 /* e  36 */, 1301 /* e  40 */, 6214 /* e  44 */,
1972 /* e  48 */,    1 /* e  52 */,
/* 2**174 = */
8784 /* e   0 */, 8411 /* e   4 */, 7987 /* e   8 */, 4042 /* e  12 */,
8099 /* e  16 */, 2235 /* e  20 */, 2299 /* e  24 */, 4917 /* e  28 */,
4118 /* e  32 */, 9513 /* e  36 */, 2602 /* e  40 */, 2428 /* e  44 */,
3945 /* e  48 */,    2 /* e  52 */,
/* 2**175 = */
7568 /* e   0 */, 6823 /* e   4 */, 5975 /* e   8 */, 8085 /* e  12 */,
6198 /* e  16 */, 4471 /* e  20 */, 4598 /* e  24 */, 9834 /* e  28 */,
8236 /* e  32 */, 9026 /* e  36 */, 5205 /* e  40 */, 4856 /* e  44 */,
7890 /* e  48 */,    4 /* e  52 */,
0};

/* table of starting indexes into previous table */
const unsigned short __tbl_2_small_start [] = {
0, 1, 2, 3, 4, 5, 6, 7,
8, 9, 10, 11, 12, 13, 14, 16,
18, 20, 22, 24, 26, 28, 30, 32,
34, 36, 38, 40, 43, 46, 49, 52,
55, 58, 61, 64, 67, 70, 73, 76,
79, 83, 87, 91, 95, 99, 103, 107,
111, 115, 119, 123, 127, 131, 135, 140,
145, 150, 155, 160, 165, 170, 175, 180,
185, 190, 195, 200, 206, 212, 218, 224,
230, 236, 242, 248, 254, 260, 266, 272,
278, 285, 292, 299, 306, 313, 320, 327,
334, 341, 348, 355, 362, 369, 376, 384,
392, 400, 408, 416, 424, 432, 440, 448,
456, 464, 472, 480, 489, 498, 507, 516,
525, 534, 543, 552, 561, 570, 579, 588,
597, 607, 617, 627, 637, 647, 657, 667,
677, 687, 697, 707, 717, 727, 738, 749,
760, 771, 782, 793, 804, 815, 826, 837,
848, 859, 870, 881, 893, 905, 917, 929,
941, 953, 965, 977, 989, 1001, 1013, 1025,
1037, 1050, 1063, 1076, 1089, 1102, 1115, 1128,
1141, 1154, 1167, 1180, 1193, 1206, 1220, 1234,
1248, 0};

\ Copyright (c) 2016 Erigones, s. r. o.
\ All rights reserved.
\

2 brandX ! 1 brandY ! \ Initialize brand placement defaults

: brand+ ( x y c-addr/u -- x y' )
	2swap 2dup at-xy 2swap \ position the cursor
	type \ print to the screen
	1+ \ increase y for next time we're called
;

: brand ( x y -- ) \ "danubecloud" [wide] logo in B/W

	0 70 15 0 0 s" /boot/danubecloud-banner.png" fb-putimage if 2drop exit then

    s"                                                           " brand+
    s"  ___  ____ _  _ _  _ ___  ____    ____ _    ____ _  _ ___ " brand+
    s"  |  \ |__| |\ | |  | |__] |___    |    |    |  | |  | |  \" brand+
    s"  |__/ |  | | \| |__| |__] |___    |___ |___ |__| |__| |__/" brand+

	2drop
;

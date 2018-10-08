# Adding `operator<=>` to C library types

<pre>
Document Number: DXXXXR0
Date: 2018-10-07
Author: David Stone (&#100;&#97;&#118;&#105;&#100;&#109;&#115;&#116;&#111;&#110;&#101;&#64;&#103;&#111;&#111;&#103;&#108;&#101;&#46;&#99;&#111;&#109;, &#100;&#97;&#118;&#105;&#100;&#64;&#100;&#111;&#117;&#98;&#108;&#101;&#119;&#105;&#115;&#101;&#46;&#110;&#101;&#116;)
Audience: Library Evolution Working Group (LEWG)
</pre>

## Proposal

Note: this paper is currently incomplete. I am not sure if I will ever do the work to complete it, given the complexities of calendars and time, and the fairly small payoff associated with it. I would rather spend my effort with integrating better calendar functions.

The following types from C headers do not currently have any comparison operators. This paper proposes the following additions:

* `div_t`: `strong_ordering`
* `ldiv_t`: `strong_ordering`
* `lldiv_t`: `strong_ordering`
* `imaxdiv_t`: `strong_ordering`
* `timespec`: `strong_ordering`
* `tm`: `strong_ordering`
* `lconv`: `strong_equality`
* `fenv_t`: `strong_equality`
* `fpos_t`: `strong_ordering`
* `mbstate_t`: `strong_equality`

## Discussion

`div_t`, `ldivt_t`, `lldivt_t`, and `lmaxdiv_t` are structs that have two data members, `quot` and `rem`, that hold the quotient and remainder of division. The ordering provided would be first by `quot`, then by `rem`.

`timespec` has two data members, `tv_sec` and `tv_nsec`, that represent a time value divided into seconds and nanoseconds. Ordering would be first by `tv_sec`, then by `tv_nsec`.

`tm` has several data members that represent a calendar date + time (TODO: How would we order this when there is a DST flag in there?):
* `tm_year`: years since 1900
* `tm_mon`: months since January
* `tm_mday`: day of the month
* `tm_hour`: hours after midnight
* `tm_min`: minutes after the hour
* `tm_sec`: seconds after the minute
* `tm_isdst`: whether DST is in effect
* `tm_yday`: days since the start of the year (January 1)
* `tm_wday`: days since Sunday

## Where to define them

Because `operator<=>` can be defined outside of the class, it is possible to add these without needing to modify the C header. Do we want to guarantee that `operator<=>` is available when including "thing.h" or only when including "cthing"?

# Types from C that should get operator<=>

<pre>
Document Number: DXXXXR0
Date: 2018-08-22
Author: David Stone (&#x64;&#x61;&#x76;&#x69;&#x64;&#x6D;&#x73;&#x74;&#x6F;&#x6E;&#x65;&#x40;&#x67;&#x6F;&#x6F;&#x67;&#x6C;&#x65;&#x2E;&#x63;&#x6F;&#x6D;)
Audience: Library Evolution Working Group (LEWG)
</pre>

## Proposal

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

Because `operator<=>` can be defined outside of the class, it is possible to add these without needing to modify the C header. Do we want to guarantee that `operator<=>` is available when including "thing.h" or only when including "cthing"?

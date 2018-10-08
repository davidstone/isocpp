# Adding `operator<=>` to types that are not currently comparable

<pre>
Document Number: D1191R0
Date: 2018-08-22
Author: David Stone (&#100;&#97;&#118;&#105;&#100;&#109;&#115;&#116;&#111;&#110;&#101;&#64;&#103;&#111;&#111;&#103;&#108;&#101;&#46;&#99;&#111;&#109;, &#100;&#97;&#118;&#105;&#100;&#64;&#100;&#111;&#117;&#98;&#108;&#101;&#119;&#105;&#115;&#101;&#46;&#110;&#101;&#116;)
Audience: Library Evolution Working Group (LEWG)
</pre>

## Summary

The following types do not currently have comparison operators. They should be modified as follows:

* `filesystem::file_status`: `strong_equality`
* `filesystem::space_info`: `strong_equality`
* `slice`: `strong_equality`
* `to_chars_result`: `strong_equality`
* `from_chars_result`: `strong_equality`

## Discussion

`filesystem::file_status` is conceptually a struct with two enum data members (but with a get / set function interface). It is equal if both the `type` and the `permissions` compare equal.

`filesystem::space_info` is a struct with three `uintmax_t` data members: `capacity`, `free`, and `available`. Two `space_info` objects compare equal if all data members compare equal.

`slice` is conceptually a struct with three `size_t` data members (but with getters only): `start()`, `size()`, and `stride()`. Two `slice` objects compare equal if all three values are equal.

This paper does not propose adding `operator<=>` to `gslice`. This object is much like `slice` except `size` and `stride` are instances of `valarray<size_t>` rather than just `size_t`. Since `valarray` does not have a traditional comparison, we do not attempt to define the equivalent for `gslice`.



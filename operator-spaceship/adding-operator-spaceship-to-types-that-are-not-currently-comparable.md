# Adding `operator<=>` to types that are not currently comparable

<pre>
Document Number: D1191R0
Date: 2018-08-22
Author: David Stone (&#100;&#97;&#118;&#105;&#100;&#109;&#115;&#116;&#111;&#110;&#101;&#64;&#103;&#111;&#111;&#103;&#108;&#101;&#46;&#99;&#111;&#109;, &#100;&#97;&#118;&#105;&#100;&#64;&#100;&#111;&#117;&#98;&#108;&#101;&#119;&#105;&#115;&#101;&#46;&#110;&#101;&#116;)
Audience: Library Evolution Working Group (LEWG)
</pre>

## Proposal

The following types do not currently have comparison operators. They should be modified as follows:

* `filesystem::file_status`: `strong_equality`
* `filesystem::space_info`: `strong_equality`
* `slice`: `strong_equality`
* `gslice`: `strong_equality`
* `to_chars_result`: `strong_equality`
* `from_chars_result`: `strong_equality`

## Discussion



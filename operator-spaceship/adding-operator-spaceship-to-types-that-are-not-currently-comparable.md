# Adding `operator<=>` to types that are not currently comparable

<pre>
Document Number: DXXXXR0
Date: 2018-08-22
Author: David Stone (&#x64;&#x61;&#x76;&#x69;&#x64;&#x6D;&#x73;&#x74;&#x6F;&#x6E;&#x65;&#x40;&#x67;&#x6F;&#x6F;&#x67;&#x6C;&#x65;&#x2E;&#x63;&#x6F;&#x6D;)
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

# Prefer variable templates over structs

<pre>Document Number: D2335R0
Date: 2021-03-11
Author: David Stone (davidfromonline@gmail.com)
Audience: Library Evolution Working Group (LEWG)</pre>

## Abstract

This is not a proposal for a particular feature. Instead, this is a proposal for a new LEWG design policy that future feature proposals would follow.

Type traits in the standard library that produce a value follow a particular pattern: there is one version of the trait that is a struct that has a static `value` data member, and then another version that is a variable template that has the same name as the struct version of the trait but has `_v` appened to its name. For example: `std::is_same<T, U>::value` and `std::is_same_v<T, U>`. This paper proposes that for all new proposals, we do not define the struct version at all and thus dispense with the `_v` suffix on the variable template.

## Why do we do things the way we do them now?

The current design came about as an accident of timing. The `type_traits` header [was proposed for C++11 in 2003](http://open-std.org/jtc1/sc22/wg21/docs/papers/2003/n1424.htm), and borrowed heavily from Boost. Variable templates [were added to C++14 in 2013](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3651.pdf). The functionality simply did not exist when we standardized the majority of the traits.

## Advantages of changing

* Almost all uses of these traits just want the boolean value, so we should give the better name to the thing people actually want to do
* Instantiating a class template takes longer to compile than instantiating a variable template

## Disadvantages of changing

* Inconsistency with the existing traits
* Requires writing a wrapper to make instantiating a trait lazy
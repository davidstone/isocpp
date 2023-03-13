# constexpr static_vector

## Problem

It is not possible to implement `static_vector` that is generically `constexpr`. The primary building block for `static_vector` is `uninitialized_array`, but this also cannot be implemented with generic `constexpr` support.

## Background

For a full background on `static_vector`, including the difficulties in implementing `constexpr`, you can watch [Implementing static_vector: How Hard Could it Be?](https://www.youtube.com/watch?v=I8QJLGI0GOE). The important points from that presentation are summarized in this paper.

`static_vector` has been proposed for standardization, and it has an interface very similar to `std::vector`. However, its template signature is `static_vector<T, capacity>`. The size can change throughout the lifetime of the `static_vector`, but the capacity is fixed at compile-time.

`uninitialized_array` is similar to `std::array`. The difference between the two is that `uninitialized_array<T, size>` provides storage for a statically-sized array of objects but does not manage their lifetime. It is a trivial type, regardless of the element type. The expected interface for `uninitialized_array` is something like

```cpp
template<typename T, std::size_t size_>
struct uninitialized_array {
	uninitialized_array() = default;
	constexpr auto data() const noexcept -> T const *;
	constexpr auto data() noexcept -> T *;
	static constexpr auto size() const -> std::size_t {
		return size_;
	}
};
```

It is possible to [implement this today](https://github.com/davidstone/bounded-integer/blob/main/source/containers/uninitialized_array.cpp), provided `T` is trivially default constructible and trivially destructible. It is possible to remove the trivially destructible requirement, but then `uninitialized_array` ceases to be trivially default constructible, which is the more important property (how many types are trivially default constructible but not trivially destructible?).

Given `uninitialized_array`, `static_vector` can be implemented as:

```cpp
template<typename T, std::size_t capacity>
struct static_vector {
	// Interface
private:
	uninitialized_array<T, capacity> m_storage;
	std::size_t m_size;
};
```

The goal of this paper is to enable `static_vector` to be `constexpr` for any element type (provided the operations on that element are, themselves, `constexpr`). However, given that enabling `uninitialized_array` to be `constexpr` is sufficient to implement `static_vector`, and that there are many tradeoffs that go into `static_vector` and almost none that go into `uninitialized_array`, the rest of this paper will focus entirely on `uninitialized_array`.

## Possible Solutions

### Make `uninitialized_array` a magic library type

We could make `uninitialized_array` known to the compiler. This is probably slightly simpler than `std::initializer_list`, which is also a magic type. This obviously works and solves all problems, but has the downside of introducing a new magic library type. If we can find a more general feature that enables this functionality, that would be better, but at least we have one working solution as an option.

### Make placement new and `std::construct_at` on an array element begin the lifetime of the array

This is essentially extending [existing rules around assigning to inactive members of unions](https://eel.is/c++draft/class.union#general-5). The current rule says that given some union `u`, the expression `u.x = whatever` can begin the lifetime of `u.x` as long as certain rules are followed -- primarily that `u.x` must be trivially default constructible and trivially assignable, and it's as if the trivial default constructor were called before the assignment.

This is the solution proposed by [P2747](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2747r0.html).

Under this solution, `uninitialized_array` could possibly be implemented with a union data member. `data` would just return a pointer to the inactive member, and any attempt to access elements would first need to construct an element, which would implicitly create the array object.

There are a few problems with this approach.

First, it cannot directly extend the assignment rules in unions if we want to allow this layering. The assignment rules require that the expression is literally typed as an assignment expression in which the union access is visible. It does not work for a function returning a reference to a union member. This means that `static_vector` would need to implement `uninitialized_array` functionality inline. Not the end of the world, but unfortunate that we cannot separate the concerns of providing storage and managing lifetime.

Second, and more seriously, the array object is frequently constructed too late. Consider the following code pattern, which is essential for maximum efficiency:

```cpp
auto static_vector<T>::append(std::ranges::range auto && other) {
	std::copy(std::begin(other), std::end(other), m_storage.data() + m_size);
	m_size += std::size(other);
}
```

If this function were called before we have constructed any elements, we've never called placement new on any elements and thus never changed thea active member of the union and thus never begun the lifetime of the array. This means that the expression `m_storage.data() + m_size` is undefined behavior. I do not know if it is possible to resolve this issue, and I do not see any reasonable (something that does not cause sub-optimal code generation just to satisfy language rules) workarounds.

### Create a single_element_storage<T> type, and allow an array of those to be converted to an array of T

One solution would be to allow users to write

```cpp
single_element_storage<T> array[size];
auto ptr = static_cast<T *>(array);
```

and then use `ptr` as though it pointed to an array of `T`. Other than the array interaction, `single_element_storage` can be implemented as something like

```cpp
template<typename T>
union single_element_storage {
	constexpr single_element_storage() noexcept {
	}
	single_element_storage() requires std::is_trivially_default_constructible_v<T> = default;
	constexpr ~single_element_storage() {
	}
	~single_element_storage() requires std::is_trivially_destructible_v<T> = default;

	[[no_unique_address]] T data;
};
```

with the other special member functions added in the obvious way.

If we create an exception to the aliasing rule for `single_element_storage`, we have in fact created a new magic library type, which we were hoping to avoid, but this one is even smaller than `uninitialized_array` and has use in several other contexts. A slightly different approach with the same effect is to say that an array of a union can be treated as an array of `T` under certain circumstances (for instance, if `T` is the only member of the union).

### Add a way to turn off initialization and destruction of a variable

This is the concept we are trying to express, so maybe the solution is to just add some magic syntax for that (strawman syntax used here because I have not yet come up with a good name):

```cpp
👨‍🌾 T array[size];
```

There have been some proposals for disabling initialization only, but we need to disable both the constructor and destructor of the array elements. It could be that we want those two features independently, and then we just combine them here. For instance [`[[clang::no_destroy]]`](https://clang.llvm.org/docs/AttributeReference.html#no-destroy) has important use cases, and we might want to standardize that and extend it to variables other than those with static or thread storage duration. [P2723](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2723r1.html#out-out) proposes various syntaxes for avoiding initialization. Then the declaration might look something like

```cpp
[[no_destroy]] do not const struct this T array[size];
```

If those features have sufficient motivation to be added independently of this, that is a strong argument we should go in that direction to give the building blocks we need.

### Allow changing the active member of a union without altering the contents of the union

One cheeky solution is to try to implement `static_vector` with

```cpp
template<typename T, std::size_t capacity>
union array_up_to {
	// In practice you would need a recursive implementation
	T zero[0];
	T one[1];
	T two[2];
	T three[3];
	...
	T n[capacity];
};
```

If you need to resize a `static_vector` from 2 elements to 4 elements, you simply begin the lifetime of the `four` data member of the union, and then construct at `four[2]` and `four[3]`. For this to work, you would need to be able to change the active member of a union without destroying the previous contents. The closest analog we have to this in the standard today is the common initial sequence rule, which states that if you have two structs in a union, you can access from an inactive member of the union as long as both types are standard layout and both begin with the same sequence of data members up to the point you are accessing. This is kind of taking that in the opposite direction -- you want to start using later elements but keep the array version of the common initial sequence around.

Leaving aside the fact that in order for this to make sense, we would also need to add support for zero-sized arrays, it would require some interesting new syntax for constructing the array. Consider something like

```cpp
array_up_to<int, 5> u = make_array_up_to_with_two_elements();
std::construct_at(u.four, prefix_but_do_not_require_move_constructor_or_invalidate_pointers(u.two), x, y);
```

Unless there is an exciting new development in a good syntax here, this does not feel like a promising direction.

### Allow `reinterpret_cast<T *>(array of bytes)`

This is the most general of the options. It adds the biggest piece of functionality that is missing from constexpr programming right now (the other obvious big chunk being exception handling). It has the highly desirable property that no other proposal has, which is that existing `static_vector` implementations likely become `constexpr` just by adding the `constexpr` keyword to their functions.

Unfortunately, I have not done the work in talking to implementors to understand any difficulties in this approach.
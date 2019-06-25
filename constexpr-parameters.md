# `constexpr` Function Parameters

Document Number: D1045R1
Date: 2019-06-25
Author: David Stone (&#x64;&#x61;&#x76;&#x69;&#x64;&#x40;&#x64;&#x6F;&#x75;&#x62;&#x6C;&#x65;&#x77;&#x69;&#x73;&#x65;&#x2E;&#x6E;&#x65;&#x74;, &#x64;&#x61;&#x76;&#x69;&#x64;&#x2E;&#x73;&#x74;&#x6F;&#x6E;&#x65;&#x40;&#x75;&#x62;&#x65;&#x72;&#x2E;&#x63;&#x6F;&#x6D;)
Audience: Evolution Working Group (EWG)

## Abstract

With this proposal, the following code would be valid:

	void f(constexpr int x) {
		static_assert(x == 5);
	}

The parameter is usable in all the same ways as any `constexpr` variable.

Moreover, this paper proposes the introduction of a "maybe constexpr" qualifier, with a strawman syntax of `constexpr?` (this syntax is a placeholder for most of the paper, there is a section on syntax later on). Such a function can accept values that are or are not `constexpr` and maintain that status when passed on to another function. In other words, this is a way to deduce and forward `constexpr`, similar to what "forwarding references" / "universal references" (`T &&`) do in function templates today. This paper proposes adding generally usable functionality to test whether an expression is a constant expression (`is_constexpr`), with the primary use case being to use this test in an `if constexpr` branch in such a function to add in a compile-time-only check.

Finally, this paper proposes allowing overloading on `constexpr` parameters. Whether a parameter is `constexpr` would be used as the final step in function overload resolution to resolve cases that would otherwise be ambiguous. Put another way, we first perform overload resolution on type, then on `constexpr`.

This paper has three primary design goals:

1) Eliminate arcane metaprogramming from modern libraries by allowing them to make use of "regular" programming.
2) Allow library authors to take advantage of known-at-compile-time values to improve the performance of their libraries
3) Allow library authors to take advantage of known-at-compile-time values to improve the diagnostics of their libraries

## Changes In This Revision

R1: The original version proposed a particular model of `static` function variables. R1 discusses the problems in that original model under the section "Function static variables". R1 also adds more detail on how overload resolution is supposed to work under the section "Overload resolution". Added section "Further work needed".

## No Shadow Worlds

Malte Skarupke wrote an article on [language design and "shadow worlds"](https://probablydance.com/2015/02/16/ideas-for-a-programming-language-part-3-no-shadow-worlds/). Shadow worlds are parts of a language which are segregated from the rest of the language. When you are stuck in this "shadow" language, you find yourself wanting to use the full power of the "real" language. C++ contains at least four general purpose languages (three of them shadow languages): "regular" C++, `constexpr` functions, templates, and macros.

Prior to C++14, constexpr functions couldn't have `if` or `for`, leading to contorted recursive code full of `return condition ? complicated_expression1 : complicated_expression2`. Since C++14, we cannot allocate memory in constexpr functions. Once we remove that restriction in C++20, we still cannot `reinterpret_cast`. The trend here is an ever increasing move toward making `constexpr` functions just be regular functions.

Templates are in an even worse spot: another Turing complete language that has no loops, no mutation, and a completely different syntax for everything.

Worse still are macros: no loops, mutation, [recursion](https://stackoverflow.com/a/10526117/852254), scoping, no real functions, and weird if statements.

For this paper, the shadow world of templates is the most relevant. In a strong twist of irony, Malte's article talks about macros being a shadow world in every language, so therefore you want a strong template system that can replace macros. The template system, however, is yet another shadow language. In C++, as in other languages, the compile-time arguments are split from the run-time: we have template parameters and function parameters, so you can write f<0>(1). That seems fine, at first, until you realize that there are many things you cannot do because of that syntactic difference.

You cannot pass template arguments to constructors. You cannot pass template arguments to overloaded operators. You cannot overload on whether a value is known at compile time when they have different syntaxes. You cannot put a compile-time argument after a run-time argument, even if that is where it makes the most sense for it to be. You cannot generically forward a pack of arguments, some of them compile time and some of them run time. All of these limitations lead to us reimplementing basic functionality like loops, partial function application, and sorting for "run time" vs "compile time" values.

These problems motivate this paper. By treating compile-time and run-time values more uniformly, we bring these two worlds together. Large portions of existing metaprogramming libraries going back to Boost.MPL try to work around the problems, but we need a language change to solve it at the root.

## Before and After

<style type="text/css">table {
	padding: 0;
	border-collapse: collapse;
}
table tr {
	border-top: 1px solid #cccccc;
	background-color: white;
	margin: 0;
	padding: 0;
}
table tr:nth-child(2n) {
	background-color: #f8f8f8;
}
table tr th {
	font-weight: bold;
	border: 1px solid #cccccc;
	margin: 0;
	padding: 6px 13px;
}
table tr td {
	border: 1px solid #cccccc;
	margin: 0;
	padding: 6px 13px;
}
table tr th :first-child, table tr td :first-child {
	margin-top: 0;
}
table tr th :last-child, table tr td :last-child {
	margin-bottom: 0;
}</style>

<table>
	<tr>
		<th>Now (without this proposal)</th>
		<th>The future (with this proposal)</th>
	</tr>
	<tr>
		<td>
<pre><code>auto a = std::array&lt;int, 2&gt;{};
a[0] = 1;
a[1] = 5;
a[2] = 3; // undefined behavior
&nbsp;
auto t = std::tuple&lt;int, std::string&gt;{};
std::get&lt;0&gt;(t) = 1;
std::get&lt;1&gt;(t) = "asdf";
std::get&lt;2&gt;(t) = 3;  // compile error</code></pre>
		</td>
		<td>
<pre><code>auto a = std::array&lt;int, 2&gt;{};
a[0] = 1;
a[1] = 5;
a[2] = 3; // compile failure
&nbsp;
auto t = std::tuple&lt;int, std::string&gt;{};
t[0] = 1;
t[1] = "asdf";
t[2] = 3; // compile failure</code></pre>
		</td>
	</tr>
	<tr>
		<td>
			<pre><code>std::true_type{}</code></pre>
		</td>
		<td>
			<pre><code>true</code></pre>
		</td>
	</tr>
	<tr>
		<td>
<pre><code>std::integral_constant&lt;int, 24&gt;{}
std::constant&lt;int, 24&gt;   // proposed
boost::hana::int_c&lt;24&gt;
boost::mpl::int_&lt;24&gt;</code></pre>
		</td>
		<td>
			<pre><code>24</code></pre>
		</td>
	</tr>
	<tr>
		<td>
<pre><code>template&lt;int n&gt;
void f(boost::hana::int_&lt;n&gt;);</code></pre>
		</td>
		<td>
<pre><code>&nbsp;
f(constexpr int x);</code></pre>
		</td>
	</tr>
	<tr>
		<td>
<pre><code>0 &lt;=&gt; 1 == 0;
// valid, as intended
&nbsp;
0 &lt;=&gt; 1 == nullptr;
// valid, due to implementation detail
&nbsp;
</code></pre>
		</td>
		<td>
<pre><code>0 &lt;=&gt; 1 == 0;
// valid, as intended
&nbsp;
0 &lt;=&gt; 1 == nullptr;
// error: no overloaded operator== comparing
// strong_equality with nullptr_t</code></pre>
		</td>
	</tr>
	<tr>
		<td>
<pre><code>// Either all regex constructions have an extra
// pass over the input to determine the parsing
// strategy, or...
auto const pathological = std::regex("(A+|B+)*C");
// exponential time against failed matches
// starting with 'A' and 'B'.
// See https://swtch.com/~rsc/regexp/regexp1.html</code></pre>
		</td>
		<td>
<pre><code>// Scan can be done at compile time, so...
&nbsp;
&nbsp;
auto const pathological = std::regex("(A+|B+)*C");
// linear time against failed matches
// starting with 'A' and 'B'.
&nbsp;
</code></pre>
		</td>
	</tr>
	<tr>
		<td>
<pre><code>auto const glob = std::regex("*");
// throws std::regex_error
&nbsp;
</code></pre>
		</td>
		<td>
<pre><code>auto const glob = std::regex("*");
// static_assert failed "Regular expression token
// '*' must occur after a character to repeat."</code></pre>
		</td>
	</tr>
	<tr>
		<td>
<pre><code>// ???
&nbsp;
&nbsp;
&nbsp;
&nbsp;
&nbsp;</code></pre>
		</td>
		<td>
<pre><code>static_assert(pow(2_meters, 3) == 8_cubic_meters);
meters runtime_value;
std::cin &gt;&gt; runtime_value;
if (pow(runtime_value, 2) &gt; 100_square_meters) {
	throw std::exception{};
}</code></pre>
		</td>
	</tr>
	<tr>
		<td>
<pre><code>// Do I want my code to work at compile time, do I
// want my code to be fast, or do I try to modify
// my compiler to recognize the code and generate
// the right assembly at run time when it write it
// in the `constexpr` style?</code></pre>
		</td>
		<td>
<pre><code>// From the <a href="https://groups.google.com/a/isocpp.org/forum/#!topic/std-proposals/RdAK-0RyiY0">std-proposals forum</a>
std::size_t strlen(constexpr char const * s) {
	for (const char *p = s; ; ++p) {
		if (*p == '\0') {
			return static_cast&lt;std::size_t&gt;(p - s);
		}
	}
}
std::size_t strlen(char const * s) {
	__asm__("SSE 4.2 insanity");
}
</code></pre>
		</td>
	</tr>
	<tr>
		<td>
<pre><code>// Cannot write a function that transparently uses
// this intrinsic where possible. Cannot write a
// function that puts the parameters in the
// correct order without resorting to
// `integral_constant` business.</code></pre>
		</td>
		<td>
			<pre><code>#include &lt;emmintrin.h&gt;
&nbsp;
void f(int something, char y) {
	__m128i x;
	// ...
	// This intrinsic requires the second argument
	// to be a compile-time constant
	x = __builtin_ia32_aeskeygenassist128(x, y);
	// ...
}</code></pre>
		</td>
	</tr>
</table>

## Background

This proposal will reference a few libraries that make heavy use of compile-time parameters. If readers are not familiar with these libraries, that is fine, the below should be sufficient description to understand some of the motivation of this paper. This paper will also reference other papers that are currently under discussion.

### boost::mpl

The [Boost.MPL](https://www.boost.org/doc/libs/1_67_0/libs/mpl/doc/index.html) library is a general-purpose template metaprogramming framework of compile-time algorithms, sequences and metafunctions. It was designed in the constraints of C++03 and organizes itself around passing types to class template parameters.

### boost::hana

[Boost.Hana](https://www.boost.org/doc/libs/1_67_0/libs/hana/doc/html/index.html) is a metaprogramming library that aims to make metaprogramming just "programming", using as much "regular" C++ as possible. It does this by merging types and values -- values are encoded in types, and types are turned into values -- so that most of the user interaction with `boost::hana` is by calling what appear to be regular functions.

### bounded::integer

[`bounded::integer`](https://bitbucket.org/davidstone/bounded_integer) is a library that adds compile-time range checking to integer types. `bounded::integer<5, 1000>` is an integer type that can be between 5 and 1000, inclusive, and the resulting type of `bounded::integer<1, 10> + bounded::integer<2, 5>` is `bounded::integer<3, 15>`.

### Class Types in Non-Type Template Parameters

A feature has been added to C++20 that allows class types as non-type template parameters. To use such a type, it must be a literal type with a defaulted `operator==`, with all bases and data members meeting the same requirement ("strong structural equality").

## Why we need this proposal

`constexpr` parameters have several advantages over existing solutions:
* Better user experience due to uniform syntax on the caller's side: They do not need to remember `f(true_)` or `f(constant<true>)` for `f<true>()` or or anything else like that. The standard and most intuitive way to pass a value to a function is in the function parameter list: `f(true)`.
* Better compile times as a result of being able to just pass things like `true` rather than instantiating a wrapper template. This applies primarily to solutions like `boost::mpl` (where everything has to be a type, including integers, and is the origin of `integral_constant`) and `boost::hana` (where everything is passed as a regular function parameter, even compile-time constants, which uses the equivalent of `integral_constant` but passed by value instead of by type). The cost of time and memory to compile `void f(constexpr int x)` should be essentially the same as `template<int x> void f()`.
* Library authors can add compile-time diagnostics without adding run-time code generation for checking. This is somewhat possible now, but it requires that the caller put the result in a constexpr context and does not work in all cases (for instance, std::regex cannot yet be a literal type due to possible memory allocation). There is a large amount of user confusion today when a `constexpr` function is passed all `constexpr` parameters, but an error is caught at run time instead of compile time because the result of that function was not stored in a `constexpr` result.
* Allows adding a new overload that can take advantage of known-at-compile-time values, without forcing all callers to change
* Allows easier integration of strongly-typed code with existing code. There are many generic libraries that expect to be able to initialize integer types with a literal `0` or `1`. To support this, the `bounded::integer` library would have to accept all arguments of type `int` (and hope that the library never passes `0U`), which removes some of the compile-time safety from the library. If `bounded::integer` could know that it was dealing with a safe value, it would go a long way toward improving interoperability.

## Details

### In which contexts can `constexpr` parameters be used?

This paper proposes allowing a `constexpr` parameter anywhere that any other `constexpr` value could be used in any location that appears lexically after the parameter. In this way, a `constexpr` parameter is usable in much the same way as a template parameter. In particular, the following code is valid:

	void f(constexpr int X, std::array<int, X> const & a)
		noexcept(X > 5)
		requires(X % 2 == 0)
	{
		static_assert(X == 6);
	}

### Function static variables

There are two possible mental models for this. The first mental model is that these are regular functions for which some parameters are available for use at compile time. The second mental model is that this is purely an alternative syntax for function templates and there is a "desugaring" to a function template. Those two models are not in conflict for most cases, but when `static` variables come into play, they suggest very different results.

	int & f(constexpr int) {
		static int x = 0;
		return x;
	}
	++f(0);
	std::cout << f(1);

What is this program guaranteed to print? The "secretly a template" model says it should print "0" because these are two different functions. The "regular function" model says it should print "1" because the variable `x` is declared `static`, and since we have written just one function here, we are using the same variable. One of primary goals of this proposal is to allow a more regular style of programming, rather than separate rules for run time vs. compile time. Therefore, this paper has a goal of forwarding the "regular function" model. Unfortunately, the rules for templates are the way they are for a reason. There are many examples that, without further work on the topic, seem to suggest that the template model is the only reasonable model:

	void f(constexpr int n) {
		static constexpr int x = n;
		static_assert(x == n);
	}

Is this `static_assert` guaranteed to succeed? With the regular function model, this function would not be guaranteed to succeed, because the first invokation of the function determines the value of `x`. However, there is no fixed ordering of "first" at compile time, especially considering that the function can be called from two different translation units with two different arguments. This suggests that it is not workable to have static constexpr variables of dependent value in such functions. We run into the same problems with static variables of dependent type.

	void f(constexpr int n) {
		using T = std::conditional_t<n == 0, int, long>;
		static T x;
	}

The definition of "dependent type" ends up applying to any locally defined type.

	void f(constexpr int n) {
		static struct {
			static int f() {
				return n;
			}
		} s;
	}

The problem here is that if we ever commit to one model of static variables, it is a breaking change to switch to the other. So we could restrict static variables to not have a type or value dependent on any constexpr function parameter, and then the "regular function" model works, but we would have to commit ourselves to that model going forward. The only two reasonable approaches here are to do what templates do (where a given value of a constexpr function parameter gives you the same instance of static variables, but different values give you different instances) or we simply ban the use of static variables in such functions. This paper argues for not allowing static variables at this time, pending further research into this area.

### Overload resolution

Determine which function is called can be divided into two steps: determining viable candidates, and then determining the best match from that set of viable candidates. `constexpr` parameters will change both parts of this equation.

#### Viable candidates

Determining viable candidates follows a simple rule that should mostly match intuition: if a function parameter is marked `constexpr`, the corresponding argument must be able to initialize a constexpr variable of that same type. Note that this is subtly different from another simple rule, which would be that if the parameter is marked `constexpr`, the corresponding argument must also be a constant expression. The following code should highlight the difference between the two options:

	void f(constexpr bool x);
	void f(bool x);
	
	std::true_type x = some_function();
	f(x);

This code works under this proposal, but not under the alternative formulation. `boost::hana` makes heavy use of types like `true_type` and `integral_constant` to turn values into types, and these types have a constexpr conversion operator to the "underlying" type. It is not necessary to declare `x` constexpr in this example because it is stateless. Ideally, such code would continue to work even if the author migrates to a `constexpr` parameter solution where `f` accepts `constexpr bool` instead of being a function template that expects `std::true_type` or `std::false_type`. Prior to the introduction of `constexpr` lambdas in C++17, many of these functions could not be marked `constexpr`, and thus they depend on this behavior.

The "match `constexpr`" rule is even worse in the opposite case:

	struct Thing {
		constexpr Thing(int setting1, int setting2);
		Thing(path const & configuration_file);
		
		// ...
	};
	
	void f(Thing const thing);
	void f(constexpr Thing thing);
	
	constexpr path p = "/home/david/file.txt";
	f(p);

Here, we have a `constexpr` variable as the source, but the only viable candidate is the overload with the non-`constexpr` parameter. It would be quite surprising indeed if we determined that an overload which could not be called was viable to be called.

#### Overload resolution

Overload resolution is a bit more complicated, but once again has a fairly simple intuitive backing. The general idea is that `constexpr` is used only as a tie-breaker for otherwise ambiguous overloads. [Note: this section is still in progress and may change]

	void f0(int); // 1
	void f0(constexpr long); // 2
	f0(42); // calls 1
	
	void f1(int); // 1
	void f1(constexpr int); // 2
	f1(42); // calls 2
	
	void f2(unsigned); // 1
	void f2(constexpr long); // 2
	f2(42); // calls 2

The complications arise when considering overloads differing only by value category. The exact rules of overload resolution will have to be determined after the work on improving support for `constexpr` references.
	
### What are the restrictions on types that can be used as `constexpr` parameters?

There are two possible options here. Conceptually, we would like to be able to use any literal type as a `constexpr` parameter. This gives us the most flexibility and feels most like a complete feature. Again, however, the obvious implementation strategy for this paper would be to reuse the machinery that handles templates now: internally treat `void f(constexpr int x)` much like `template<int x> void f()`. In the "template" model, we would limit `constexpr` parameters to be types which can be passed as template parameters: with P0732, types with a trivial `operator<=>`. For functions with `constexpr` parameters, however, many of the concerns that prompted that requirement go away under the "regular function" model. The importance of "same instantiation" is limited thanks to not trying to conceptually "instantiate" this into a "regular" function. Because we do not propose allowing the formation of pointers to such functions and because we propose not allowing static variables, the problem is avoided entirely. We do not need to decide whether two functions with different `constexpr` variables are "the same" function because there is no way to determine the answer. The only remaining question here comes down to implementability concerns.

### Can you take the address of `constexpr` parameter?

Just like in P0732, we would like to be able to call const member functions on these parameters at run time, but for that to be possible they need to have an address. To satisfy that requirement, constexpr parameters should be const objects, of which there is one instance in the program for every distinct constexpr parameter value. P0732 proposes this only for non-type template parameters of class type, but there seems to be no reason to limit this in that way. For consistency, it seems that this should be allowed for all constexpr parameters, not just those of class type, and the same argument applies to all non-type template parameters.

### Function Pointers

This paper does not propose adding support for pointers to functions accepting `constexpr` parameters simply because the full interactions there have not yet been explored. Theoretically this could be supported if the type of the function pointer was `void(constexpr int)` and the function pointer is formed in a constexpr context, but that last requirement would be a completely new type of restriction.

### Perfect forwarding

It would be nice to be able to write a function template that accepts parameters that might be `constexpr` and forward them on to another function. For example, if I have a class that wraps another type, it would be nice to write a constructor that forwards all arguments on to the wrapped type. To do this, we need some sort of perfect forwarding mechanism. There are three possibilities here:

1) Do not solve this problem. Ignoring this problem causes an exponential explosion on forwarding functions and makes it impossible to write variadic versions of such functions.
2) Follow the lead of rvalue vs. lvalue references and implement a "constexpr deduction" rule. In other words, for `template<typename T> void f(T x)`, the type `T` can be deduced as `constexpr int`. This seems appealing at first, because of the analogy to rvalue references and the fact that existing code would automatically become "aware" of this feature and take advantage of it. However, that is both a benefit and a drawback, because it means that all function templates get this behavior. This could have a very large cost in time and memory at compile time if compilers have to activate much of their current template machinery to implement this. This also requires using a deduced type even when the exact type is known.
3) "Maybe constexpr" parameters. `void f(constexpr? int x)` is a (strawman) syntax to allow users to write a single function overload that can accept parameters that can be used as compile-time constants and parameters which cannot.

Option 3 seems like the best option, but requires some more explanation for how it would work. There are two primary use cases for this functionality. The first use case is to forward an argument to another function that has a `constexpr?` parameter or is overloaded on `constexpr`. The second use case is to write a single function that has one small bit of code that is run only at compile time or only at run time. For instance, `std::array::operator[]` could be implemented like this:

	auto & operator[](constexpr? size_t index) {
		if constexpr(is_constexpr(index)) {
			static_assert(index < size());
		}
		return __data[index];
	}

Here, we put in a compile-time check where possible, but otherwise the behavior is the same. I actually expect this to be the primary way to use `constexpr` parameters when overloading with run-time parameters, as it ensures there are no accidental differences in the definition.

Here we show a sample implementation of `is_constexpr`, defined as a macro, which is possible to write in standard C++ with the introduction of this paper:

	std::true_type is_constexpr_impl(constexpr int);
	std::false_type is_constexpr_impl(int);
	
	#define is_constexpr(...) \
		decltype(is_constexpr_impl( \
			(void(__VA_ARGS__), 0) \
		))::value

This must be a macro so that it does not evaluate the expression and perform side effects. We need to make it unevaluated, and the macro wraps that logic. I believe it shows the strength of this solution that users can use it to portably detect whether an expression is constexpr (a common user request for a feature), rather than needing that as a built-in language feature. If we had lazy function parameters, the implementation would be even more straightforward:

	constexpr bool is_constexpr(constexpr int) { return true; }
	constexpr bool is_constexpr(~LAZY~ int) { return false; }

Regardless of how the feature is specified, it is an essential component of this proposal.

## Effect on the standard library

This paper is expected to have a fairly small impact on the standard library, because we currently make use of few non-type template parameters on function templates. The most obvious change is that we should add `operator[]` to `std::tuple`. If Evolution approves of the direction of this paper, I will also perform a complete analysis of the standard library to determine impact.

## Effect on other parts of the language

We also have `std::get` baked into the language in the form of structured bindings. Users should be able to opt-in to structured bindings with `std::tuple_size`, `std::tuple_element`, and `operator[]` instead of `std::tuple_size`, `std::tuple_element`, and `get<i>`. Right now, the tuple-like protocol first tries member `get`, then `std::get`. We could add `operator[]` into the mix, either as the first option or the last option.

## Related work

It is recommended to go through the list of examples at the beginning and think about how they could be implemented using any of the features listed below. Some of the examples can be implemented in a way that is just as straightforward, but most of them cannot be implemented at all with any other proposed feature. The following features are commonly compared to constexpr function parameters.

### `std::is_constant_evaluated()`

`std::is_constant_evaluated()` allows the user to test whether the current evaluation is in a context that requires constant evaluation, so the user could say `if (is_constant_evaluated()) {} else {}`. This solves only some of the problems addressed by this paper. The general issue is that `std::is_constant_evaluated()` is testing how the expression is used, but `constexpr` function parameters are all about what data is provided. To clarify the difference, consider an alternative universe where we want to restrict memory orders to be compile-time constants, rather than the current state where they could be run-time values. With constexpr parameters, the solution is straightforward:

	void atomic<T>::store(T desired, constexpr std::memory_order order = std::memory_order_seq_cst) noexcept;

This would require the user to pass the memory order as a compile-time constant, but places no constraints on the overall execution being constexpr (and in fact, we know it never will be because atomic is not a literal type). It is not possible to implement this with `std::is_constant_evaluated()`. For a perhaps more compelling example, this is how we likely would have originally designed tuple if we had constexpr parameters:

	constexpr auto && tuple::operator[](constexpr std::size_t index) { ... }

where the tuple variable (`*this`) is (potentially) a run time value. In other words, the code using the index is mixed in with the code using the tuple. `std::is_constant_evaluated()` allows you to write different code depending on whether the entire evaluation is part of a constexpr context. `constexpr` parameters allows you to partially apply certain arguments at compile time without needing to compute the entire expression at compile time.

Another important difference is that `if constexpr (std::is_constant_evaluated())` is meaningless: `if constexpr` is a context that requires a constant expression, and thus `std::is_constant_evaluated()` will always return true. Many of the examples rely on being able to, for instance, return different types or write code in different branches that would not compile if the value is not a constant expression.

### `consteval`

This has many of the same problems as `std::is_constant_evaluated()`. It also does not provide a way to overload on `constexpr` vs. not.

### Parametric expressions: [P1121](http://wg21.link/p1121)

This proosal introduces the concept of "hygienic macros"

	using add(auto a, auto b) { return a + b; }

It supports evaluating exactly once or 0-N times. It proposes allowing constexpr parameters, but just to the parametric expressions. Parametric expressions can solve only the problems that do not require overloading, but it creates yet another shadow world.

## Syntax

There are two language-level features being proposed by this paper: allowing function parameters to be annotated in some way to allow them to be used at compile time within the function and require initialization from a constant expression, and allowing function parameters to be annotated in some way to allow them to be used at compile time if they are initialized with a constant expression. This section will discuss the syntax for those two features.

### Option 1

There is currently an inconsistency in the meaning of `constexpr`. On a variable declaration, it means "must be evaluated at compile time", but on a function declaration, it means "can be evaluated at compile time if it does stuff that can be done at compile time". We recently added a new keyword, `consteval`, that can be applied to only function declarations and it means "must be evaluated at compile time". We could take this as an opportunity to remove this inconsistency. This option would expand the keyword `consteval` to be used on variable declarations with the same meaning that `constexpr` has today. `constexpr` on a function declaration would retain its current meaning, but `constexpr` on a variable declaration would mean that the variable can be used in a constant expression context if it was initialized with a constant expression. The suggested meaning of the keywords would be that `consteval` means the thing is evaluated at compile time and `constexpr` means the thing is evaluated at compile time if the expression it uses can be evaluated at compile time.

This has the advantage of removing an existing inconsistency in the meaning of the `constexpr ` keyword. It is also a backward-compatible change. It has the downside that the intent of users writing `constexpr` today could be to require a diagnostic if code ever changes that does not provide a compile-time constant. The other issue is that one of the design goals of `consteval` is that the compiler does not need to emit certain information, which would imply that `consteval` on a variable would produce something more like a template parameter: that interpretation of `consteval` would give a name to a prvalue, not an object with identity.

### Option 2

We keep things as they are today with regards to the meaning of `constexpr` and `consteval`. We allow annotating a function parameter with `constexpr` with the sdame meaning as a variable declaration: must be initialized with a constant expression. We add a new keyword, `maybe_constexpr`, that deduces whether the parameter is known at compile time. This paper originally proposed the syntax `constexpr?`. `consteval` was originally spelled as `constexpr!`, but `constexpr!` was rejected in favor of `consteval` for a variety of reasons, some of which also apply to `constexpr?`. One of the primary objections to names with punctuation is the question of how you pronounce the keyword. It would make spoken C++ be a tonal language, or else people will come up with a different name, such as "maybe constexpr". If people will come up with a different name for the keyword, we might as well name the keyword that name, thus `maybe_constexpr`.

## Further work needed

References and pointers declared `constexpr` or in template parameters currently have difficult to understand semantics. There is opportunity to simplify these rules through generalization. For example, under the current rules, is the following code valid?

	constexpr int const & x = 42;

The answer is: it depends. If `x` is a global variable, yes. If `x` is a local variable, no. If you add in `static` (any of the meanings), then it becomes valid again. Cleaning this up will be the topic of another paper.
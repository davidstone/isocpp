If we include only the fundamental operations (those which cannot be implemented in terms of other parts of the interface) and ignore overloads that differ only in the allocator provided, we have a declaration that looks something like:

	class string {
	public:
		using const_iterator = char const *;
		using iterator = char *;
		using allocator_type = std::allocator<char>;

		constexpr string(allocator_type alloc) noexcept;
		constexpr string(string && other) noexcept;
		constexpr string & operator=(string && other) noexcept;

		constexpr ~string();

		constexpr allocator_type get_allocator() const;

		constexpr char const * data() const;
		constexpr char * data();
		constexpr std::size_t size() const;
		
		constexpr const_iterator begin() const {
			return data();
		}
		constexpr iterator begin() {
			return data();
		}
		constexpr const_iterator end() const {
			return begin() + size();
		}
		constexpr iterator end() {
			return begin() + size();
		}

		constexpr std::size_t capacity() const;
		constexpr void reserve(std::size_t requested_capacity);
		constexpr void shrink_to_fit();

		constexpr iterator insert(const_iterator position, char value);
		// Excluding range insert and push_back for brevity, even though they
		// technically need to be implemented as well for maximum performance

		constexpr void pop_back();
	};


If this proto-string can define all of its functions as `constexpr`, the all operations on `basic_string` can be made `constexpr`.

There are fundamentally two valid string implementations: those with the small-buffer optimization and those that are esssentially `vector`. The `vector` implementations are trivially `constexpr` with the latest proposal (as it is the motivating use case), so this paper will concern itself only with the small-buffer optimized implementations. All popular implementations are small-buffer optimized implementations.

Conceptually, a small-buffer optimized string has a member of type `variant<static_vector<char, small_buffer_size>, vector<char>>`. If we have a smart `variant` that uses the smallest possible type to store the index, we can set `small_buffer_size` to `23` (all numbers in this paper assume 64-bit systems and include the null terminator is all measurements) and we end up with a `string` that has a 23-byte buffer and occupies 32 bytes of stack space. However, no implementation is actually implemented this way because there are more efficient ways to do it. The main trade-off: time vs. space. The gcc (libstdc++) and Visual Studio implementations have a 16-byte buffer and use 32 bytes of stack space, but most operations do not require a branch to check whether the small-buffer optimization is active. The clang (libc++) implementation has a 23-byte buffer and uses 24 bytes of stack space, but it requires a branch on every access to the data (just like our hypothetical `variant`-based implementation, but 8 bytes smaller). Ideally, we do not force vendors to pick one of these two tradeoffs by specifying `constexpr` on `basic_string`, so we will look into both of these implementations and ensure they are both still valid.

To prove that the current set of proposals are enough to implement `constexpr string` without unnecessarily constraining vendors, I have created several different versions.

* [clang-like string that uses common initial subsequences (not currently allowed in constexpr) to simplify the implementation significantly](https://github.com/davidstone/isocpp/blob/master/constexpr-string/clang-common-initial-subsequence.cpp)
* [clang-like string that uses bitfields to simplify the implementation somewhat](https://github.com/davidstone/isocpp/blob/master/constexpr-string/clang-bit-field.cpp)
* [Proof of ability for any compiler to compile something like the gcc and MSVC string](https://github.com/davidstone/isocpp/blob/master/constexpr-string/gcc-msvc-compat.cpp)
* [Proof of ABI compatibility with clang](https://github.com/davidstone/isocpp/blob/master/constexpr-string/clang-abi-compatible.cpp)
* [Proof of ABI compatibility with gcc and MSVC](https://github.com/davidstone/isocpp/blob/master/constexpr-string/gcc-msvc-abi.cpp)
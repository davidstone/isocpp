// Copyright David Stone 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// This code compiles as-is with clang, but fails to compile under gcc and MSVC
// due to bugs in their implementations or features of C++17 not yet
// implemented. The goal of this version is to show it is possible to make
// something ABI compatible with gcc and MSVC's standard libraries (which use
// essentially the same strategy for the small-string optimization.
//
// The main thing faked on this file is constexpr allocator support, which is
// accomplished by a custom allocator that allocates from a stack buffer. The
// destructor is also commented out, so this would leak memory if used with a
// real allocator.

#include <cassert>
#include <memory>
#include <string>

template<typename T>
struct buffer {
	buffer(buffer &&) = delete;
	buffer(buffer const &) = delete;
	buffer & operator=(buffer &&) = delete;
	buffer & operator=(buffer const &) = delete;

	T data[5000] = {};
	T * pointer = data;
};

template<typename T>
struct allocator {
	using value_type = T;

	explicit constexpr allocator(buffer<T> & buffer):
		buffer_(&buffer)
	{
	}

	constexpr auto allocate(std::size_t size) {
		auto const result = buffer_->pointer;
		buffer_->pointer += size;
		return result;
	}
	constexpr void deallocate(T * ptr, std::size_t size) {
	}

private:
	buffer<T> * buffer_;
};


template<typename Allocator>
struct allocator_traits : private std::allocator_traits<std::decay_t<Allocator>> {
private:
	using base = std::allocator_traits<std::decay_t<Allocator>>;
public:
	using typename base::allocator_type;
	using typename base::value_type;
	using typename base::pointer;
	using typename base::const_pointer;
	using typename base::void_pointer;
	using typename base::const_void_pointer;
	using typename base::difference_type;
	using typename base::size_type;
	using typename base::propagate_on_container_copy_assignment;
	using typename base::propagate_on_container_move_assignment;
	using typename base::propagate_on_container_swap;
	using typename base::is_always_equal;
	
	template<typename T>
	using rebind_alloc = typename base::template rebind_alloc<T>;
	template<typename T>
	using rebind_traits = std::allocator_traits<rebind_alloc<T>>;
	
	using base::max_size;
	using base::select_on_container_copy_construction;

	static constexpr auto allocate(allocator_type & allocator, std::size_t size) {
		return allocator.allocate(size);
	}
	
	template<typename T>
	static constexpr auto deallocate(allocator_type & allocator, T * const ptr, std::size_t size) {
		return allocator.deallocate(ptr, size);
	}
	

	template<typename T, typename... Args>
	static constexpr void construct(allocator_type &, T * const ptr, Args && ... args) {
		*ptr = T(std::forward<Args>(args)...);
	}
	

	template<typename T>
	static constexpr void destroy(allocator_type &, T * const ptr) {
	}
};


template<typename Allocator, typename InputIterator, typename ForwardIterator>
constexpr ForwardIterator uninitialized_copy(Allocator alloc, InputIterator first, InputIterator const last, ForwardIterator out) {
	using Alloc = allocator_traits<Allocator>;
	auto out_first = out;
//	try {
		for (; first != last; ++first) {
			Alloc::construct(alloc, std::addressof(*out), *first);
			++out;
		}
		return out;
#if 0
	} catch (...) {
		for (; out_first != out; ++out_first) {
			Alloc::destroy(allocator, std::addressof(*out_first));
		}
		throw;
	}
#endif
}

template<typename InputIterator, typename OutputIterator>
constexpr auto copy(InputIterator first, InputIterator const last, OutputIterator out) {
	for (; first != last; ++first) {
		*out = *first;
		++out;
	}
	return out;
}

class string {
public:
	using const_iterator = char const *;
	using iterator = char *;
	using allocator_type = allocator<char>;

private:
	[[no_unique_address]] allocator_type allocator_;
	// This could be set to 8 to reduce the size of our string to 24 bytes
	// (just like clang), but 7 characters (plus a null terminator) is likely
	// too small of a buffer for most users.
	static constexpr std::size_t small_buffer_capacity = 16;
	union U{
		constexpr U():
			buffer{}
		{
		}
		constexpr U(std::size_t c):
			capacity(c)
		{
		}

		char buffer[small_buffer_capacity];
		std::size_t capacity;
	} u_;
	char * data_;
	std::size_t size_;
	
	using Alloc = allocator_traits<allocator_type>;

	constexpr bool is_large() const {
		// gcc and MSVC fail here because they do not believe you can take the
		// address of an inactive member. This can be implemented instead
		// using a bitfield in that case.
		return data_ != u_.buffer;
	}

	constexpr void deallocate() {
		if (is_large()) {
			auto alloc = get_allocator();
			Alloc::deallocate(alloc, data_, u_.capacity);
		}
	}
	
	constexpr void relocate(char * new_data, std::size_t new_capacity) {
		deallocate();
		u_ = U(new_capacity);
		data_ = new_data;
	}
	
	constexpr void force_reserve(std::size_t new_capacity) {
		auto alloc = get_allocator();
		char * temp = Alloc::allocate(alloc, new_capacity);
		copy(begin(), end(), temp);
		relocate(temp, new_capacity);
	}
	
public:
	explicit constexpr string(allocator_type alloc) noexcept:
		allocator_(alloc),
		u_{},
		data_(u_.buffer),
		size_(0)
	{
	}

	constexpr string(string && other) noexcept:
		string(other.get_allocator())
	{
		*this = std::move(other);
	}

	constexpr string & operator=(string && other) noexcept {
		deallocate();

		if (other.is_large()) {
			u_ = U(other.u_.capacity);
			other.u_ = U{};
			
			data_ = other.data_;
			other.data_ = other.u_.buffer;
			
			size_ = other.size_;
			other.size_ = 0U;
			
		} else {
			u_ = U{};
			copy(other.data_, other.data_ + other.size_, u_.buffer);
			data_ = u_.buffer;
			size_ = other.size_;
		}

		return *this;
	}

	#if 0
	constexpr ~string() {
		deallocate();
	}
	#endif

	constexpr allocator_type get_allocator() const {
		return allocator_;
	}

	constexpr char const * data() const {
		return data_;
	}
	constexpr char * data() {
		return data_;
	}
	constexpr std::size_t size() const {
		return size_;
	}

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
	
	constexpr std::size_t capacity() const {
		return is_large() ? u_.capacity : small_buffer_capacity;
	}
	constexpr void reserve(std::size_t requested_capacity) {
		if (requested_capacity > capacity()) {
			force_reserve(requested_capacity);
		}
	}
	constexpr void shrink_to_fit() {
		if (is_large() && capacity() > size()) {
			if (size() > small_buffer_capacity) {
				force_reserve(size());
			} else {
				u_ = U{};
				copy(begin(), end(), u_.buffer);
				data_ = u_.buffer;
			}
		}
	}

	constexpr iterator insert(const_iterator const_position, char const value) {
		auto construct = [&](char * position, char const v) {
			auto alloc = get_allocator();
			Alloc::construct(alloc, position, v);
		};

		auto const offset = const_position - begin();
		auto const position = begin() + offset;
		if (size() < capacity()) {
			if (offset == size()) {
				construct(position, value);
			} else {
				auto const prev_end = std::prev(end());
				construct(end(), *prev_end);
				::copy(std::make_reverse_iterator(prev_end), std::make_reverse_iterator(position), std::make_reverse_iterator(end()));
				*position = value;
			}
		} else {
			// There is a reallocation required, so just put everything in the
			// correct place to begin with
			constexpr auto growth_factor = std::size_t{2};
			auto const new_capacity = capacity() * growth_factor;

			auto alloc = get_allocator();
			char * temp = Alloc::allocate(alloc, new_capacity);

			uninitialized_copy(alloc, begin(), position, temp);
			Alloc::construct(alloc, temp + offset, value);
			if (offset != size()) {
				uninitialized_copy(alloc, std::next(position), end(), std::next(temp + offset));
			}
			
			relocate(temp, new_capacity);
		}
		++size_;
		return begin() + offset;
	}

	constexpr void pop_back() {
		--size_;
		auto alloc = get_allocator();
		Alloc::destroy(alloc, end());
	}
};

constexpr void test_individual(string & str, char const * source) {
	string temp(str.get_allocator());
	for (auto it = source; *it != '\0'; ++it) {
		str.insert(str.end(), *it);
		temp.insert(temp.end(), *it);
	}

	temp.insert(temp.begin(), 'a');
	temp.insert(temp.begin() + temp.size() / 2, 'b');

	while (temp.size() != 0) {
		temp.pop_back();
	}
	assert(temp.size() == 0);

	auto temp2 = std::move(str);
	str = std::move(temp2);

	assert(str.data() == str.data());
	assert(str.data() != temp.data());

	assert(str.size() == std::char_traits<char>::length(source));
	assert(std::char_traits<char>::compare(str.data(), source, str.size()) == 0);
	assert(str.capacity() >= str.size());

	str.reserve(50);
	str.shrink_to_fit();
}

constexpr bool test() {
	buffer<char> buff{};
	auto alloc = allocator(buff);

	char const * short_source = "0123";
	char const * long_source =
		"0123456789"
		"0123456789"
		"0123456789"
		"0123456789"
		"0123456789";

	string short_str(alloc);
	test_individual(short_str, short_source);

	string long_str(alloc);
	test_individual(long_str, long_source);

	// This assertion is accepted by clang and MSVC and rejected by gcc
	assert(short_str.data() != long_str.data());

	string temp(alloc);
	temp = std::move(long_str);
	temp = std::move(short_str);

	return true;
}

int main() {
	test();
	static_assert(test());
}

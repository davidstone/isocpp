// Copyright David Stone 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// This code compiles as-is with clang, gcc (except for one assert), and MSVC.
// The goal of this version is to show the simplest implementation of a
// clang-like string.
//
// The main thing faked on this file is constexpr allocator support, which is
// accomplished by a custom allocator that allocates from a stack buffer. The
// destructor is also commented out, so this would leak memory if used with a
// real allocator.

#include <cassert>
#include <climits>
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
	static constexpr std::size_t small_buffer_capacity = 23;

	struct small_t {
		char data[small_buffer_capacity] = {};
	};
	struct [[gnu::packed]] large_t {
		static constexpr std::size_t bytes_remaining = 7;

		template<typename It>
		class range_view {
		public:
			constexpr range_view(It first, It last):
				first_(first),
				last_(last)
			{
			}
			constexpr auto begin() const {
				return first_;
			}
			constexpr auto end() const {
				return last_;
			}
		private:
			It first_;
			It last_;
		};

		constexpr auto little_endian_capacity() {
			return range_view(
				std::rbegin(rest_of_capacity),
				std::rend(rest_of_capacity)
			);
		}
		constexpr auto big_endian_capacity() const {
			return range_view(
				std::begin(rest_of_capacity),
				std::end(rest_of_capacity)
			);
		}

		constexpr large_t(std::size_t set_size, std::size_t capacity, char * pointer) noexcept:
			rest_of_capacity{},
			size(set_size),
			data(pointer)
		{
			assert(data != nullptr);
			for (unsigned char & byte : little_endian_capacity()) {
				byte = capacity;
				capacity >>= CHAR_BIT;
			}
		}

		unsigned char rest_of_capacity[bytes_remaining];
		std::size_t size;
		char * data;
	};

	[[no_unique_address]] allocator_type allocator_;
	// is_large_ exists just to be a bit that's always 0 with the small
	// buffer active and 1 with the large buffer active.
	bool is_large_ : 1;
	unsigned char size_or_first_byte_of_capacity_ : 7;
	// We manually implement visit on this union...
	union U {
		constexpr U() noexcept:
			small{}
		{
		}
		explicit constexpr U(std::size_t size, std::size_t capacity, char * data) noexcept:
			large(size, capacity, data)
		{
		}
		constexpr U(large_t set_large) noexcept:
			large(set_large)
		{
		}

		small_t small;
		large_t large;
	} u_;

	using Alloc = allocator_traits<allocator_type>;

	constexpr bool is_large() const {
		return is_large_;
	}

	constexpr void increment_size() {
		if (is_large()) {
			++u_.large.size;
		} else {
			++size_or_first_byte_of_capacity_;
		}
	}
	constexpr void decrement_size() {
		if (is_large()) {
			--u_.large.size;
		} else {
			--size_or_first_byte_of_capacity_;
		}
	}

	constexpr void deallocate() {
		if (is_large()) {
			auto alloc = get_allocator();
			Alloc::deallocate(alloc, u_.large.data, capacity());
		}
	}
	
	constexpr void relocate(char * new_data, std::size_t new_capacity) {
		deallocate();
		u_ = U(size(), new_capacity, new_data);
		is_large_ = true;
		size_or_first_byte_of_capacity_ = (new_capacity >> (CHAR_BIT * 7));
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
		is_large_(false),
		size_or_first_byte_of_capacity_(0),
		u_{}
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
			u_ = U(other.u_.large);
			is_large_ = true;
			size_or_first_byte_of_capacity_ = other.size_or_first_byte_of_capacity_;

			other.u_ = U{};
			other.is_large_ = false;
			other.size_or_first_byte_of_capacity_ = 0;
		} else {
			auto & osmall = other.u_.small;
			u_ = U{};
			is_large_ = false;
			copy(osmall.data, osmall.data + other.size_or_first_byte_of_capacity_, u_.small.data);
			size_or_first_byte_of_capacity_ = other.size_or_first_byte_of_capacity_;
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
		return is_large() ? u_.large.data : u_.small.data;
	}
	constexpr char * data() {
		return is_large() ? u_.large.data : u_.small.data;
	}
	constexpr std::size_t size() const {
		return is_large() ? u_.large.size : size_or_first_byte_of_capacity_;
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
		if (!is_large()) {
			return small_buffer_capacity;
		}
		std::size_t result = size_or_first_byte_of_capacity_;
		for (unsigned char const byte : u_.large.big_endian_capacity()) {
			result <<= CHAR_BIT;
			result |= byte;
		}
		return result;
	}
	constexpr void reserve(std::size_t requested_capacity) {
		if (requested_capacity > capacity()) {
			force_reserve(requested_capacity);
		}
	}
	constexpr void shrink_to_fit() {
		auto const local_size = size();
		if (is_large() && capacity() > local_size) {
			if (local_size > small_buffer_capacity) {
				force_reserve(local_size);
			} else {
				auto const data = u_.large.data;
				auto const original_capacity = capacity();
				u_ = U{};
				is_large_ = false;
				size_or_first_byte_of_capacity_ = local_size;
				copy(data, data + local_size, u_.small.data);
				auto alloc = get_allocator();
				Alloc::deallocate(alloc, data, original_capacity);
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
		increment_size();
		return begin() + offset;
	}

	constexpr void pop_back() {
		decrement_size();
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


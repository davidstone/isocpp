// Copyright David Stone 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// The goal of this version is to show the simplest implementation of a
// clang-like string if we were allowed to examine the common initial
// subsequence of a standard layout union in constexpr.
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
	static constexpr std::size_t small_buffer_capacity = 23;

	// force_large_ exists just to be a bit that's always 0 with the small
	// buffer active and 1 with the large buffer active.
	class small_t {
	public:
		constexpr small_t() noexcept:
			force_large_(false),
			size_(0),
			data_{}
		{
		}
		
		constexpr bool is_large() const noexcept {
			return force_large_;
		}
		static constexpr std::size_t capacity() noexcept {
			return small_buffer_capacity;
		}

		constexpr std::size_t size() const noexcept {
			return size_;
		}
		constexpr void set_size(std::size_t const size) noexcept {
			size_ = size;
		}

		constexpr char const * data() const noexcept {
			return data_;
		}
		constexpr char * data() noexcept {
			return data_;
		}

	private:
		bool force_large_ : 1;
		char size_ : 7;
		char data_[small_buffer_capacity];
	};

	class large_t {
	public:
		constexpr large_t(std::size_t size, std::size_t capacity, char * pointer) noexcept:
			force_large_(true),
			size_(size),
			data_(pointer),
			capacity_(capacity)
		{
			assert(data() != nullptr);
		}

		constexpr bool is_large() const noexcept {
			return force_large_;
		}
		constexpr std::size_t capacity() const noexcept {
			return capacity_;
		}

		constexpr std::size_t size() const noexcept {
			return size_;
		}
		constexpr void set_size(std::size_t const size) noexcept {
			size_ = size;
		}

		constexpr char const * data() const noexcept {
			return data_;
		}
		constexpr char * data() noexcept {
			return data_;
		}


	private:
		bool force_large_ : 1;
		std::size_t size_ : 63;
		char * data_;
		std::size_t capacity_;
	};
	static_assert(std::is_standard_layout<small_t>{});
	static_assert(std::is_standard_layout<large_t>{});

	[[no_unique_address]] allocator_type allocator_;
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
		return u_.small.is_large();
	}

	constexpr void increment_size() {
		if (is_large()) {
			u_.large.set_size(u_.large.size() + 1);
		} else {
			u_.small.set_size(u_.small.size() + 1);
		}
	}

	constexpr void decrement_size() {
		if (is_large()) {
			u_.large.set_size(u_.large.size() - 1);
		} else {
			u_.small.set_size(u_.small.size() - 1);
		}
	}

	constexpr void deallocate() {
		if (is_large()) {
			auto alloc = get_allocator();
			Alloc::deallocate(alloc, u_.large.data(), u_.large.capacity());
		}
	}
	
	constexpr void relocate(char * new_data, std::size_t new_capacity) {
		deallocate();
		u_ = U(size(), new_capacity, new_data);
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
			other.u_ = U{};
		} else {
			auto & osmall = other.u_.small;
			u_ = U{};
			copy(osmall.data(), osmall.data() + osmall.size(), u_.small.data());
			u_.small.set_size(osmall.size());
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
		return is_large() ? u_.large.data() : u_.small.data();
	}
	constexpr char * data() {
		return is_large() ? u_.large.data() : u_.small.data();
	}
	constexpr std::size_t size() const {
		return is_large() ? u_.large.size() : u_.small.size();
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
		return is_large() ? u_.large.capacity() : small_buffer_capacity;
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
				auto const data = u_.large.data();
				u_ = U{};
				copy(data, data + local_size, u_.small.data());
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

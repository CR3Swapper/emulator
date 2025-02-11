#pragma once
#include "memory_utils.hpp"

template <typename T>
class emulator_object
{
public:
	using value_type = T;

	emulator_object() = default;

	emulator_object(emulator& emu, const uint64_t address = 0)
		: emu_(&emu)
		  , address_(address)
	{
	}

	emulator_object(emulator& emu, const void* address)
		: emulator_object(emu, reinterpret_cast<uint64_t>(address))
	{
	}

	uint64_t value() const
	{
		return this->address_;
	}

	uint64_t size() const
	{
		return sizeof(T);
	}

	uint64_t end() const
	{
		return this->value() + this->size();
	}

	T* ptr() const
	{
		return reinterpret_cast<T*>(this->address_);
	}

	operator bool() const
	{
		return this->address_ != 0;
	}

	T read(const size_t index = 0) const
	{
		T obj{};
		this->emu_->read_memory(this->address_ + index * this->size(), &obj, sizeof(obj));
		return obj;
	}

	void write(const T& value, const size_t index = 0) const
	{
		this->emu_->write_memory(this->address_ + index * this->size(), &value, sizeof(value));
	}

	template <typename F>
	void access(const F& accessor, const size_t index = 0) const
	{
		T obj{};
		this->emu_->read_memory(this->address_ + index * this->size(), &obj, sizeof(obj));

		accessor(obj);

		this->write(obj, index);
	}

	void serialize(utils::buffer_serializer& buffer) const
	{
		buffer.write(this->address_);
	}

	void deserialize(utils::buffer_deserializer& buffer)
	{
		buffer.read(this->address_);
	}

private:
	emulator* emu_{};
	uint64_t address_{};
};

class emulator_allocator
{
public:
	emulator_allocator(emulator& emu)
		: emu_(&emu)
	{
	}

	emulator_allocator(emulator& emu, const uint64_t address, const uint64_t size)
		: emu_(&emu)
		  , address_(address)
		  , size_(size)
		  , active_address_(address)
	{
	}

	uint64_t reserve(const uint64_t count, const uint64_t alignment = 1)
	{
		const auto potential_start = align_up(this->active_address_, alignment);
		const auto potential_end = potential_start + count;
		const auto total_end = this->address_ + this->size_;

		if (potential_end > total_end)
		{
			throw std::runtime_error("Out of memory");
		}

		this->active_address_ = potential_end;

		return potential_start;
	}

	template <typename T>
	emulator_object<T> reserve(const size_t count = 1)
	{
		const auto potential_start = this->reserve(sizeof(T) * count, alignof(T));
		return emulator_object<T>(*this->emu_, potential_start);
	}

	void make_unicode_string(UNICODE_STRING& result, const std::wstring_view str)
	{
		constexpr auto element_size = sizeof(str[0]);
		constexpr auto required_alignment = alignof(decltype(str[0]));
		const auto total_length = str.size() * element_size;

		const auto string_buffer = this->reserve(total_length + element_size, required_alignment);

		this->emu_->write_memory(string_buffer, str.data(), total_length);

		constexpr std::array<char, element_size> nullbyte{};
		this->emu_->write_memory(string_buffer + total_length, nullbyte.data(), nullbyte.size());

		result.Buffer = reinterpret_cast<PWCH>(string_buffer);
		result.Length = static_cast<USHORT>(total_length);
		result.MaximumLength = result.Length;
	}

	emulator_object<UNICODE_STRING> make_unicode_string(const std::wstring_view str)
	{
		const auto unicode_string = this->reserve<UNICODE_STRING>();

		unicode_string.access([&](UNICODE_STRING& unicode_str)
		{
			this->make_unicode_string(unicode_str, str);
		});

		return unicode_string;
	}

	uint64_t get_base() const
	{
		return this->address_;
	}

	uint64_t get_size() const
	{
		return this->size_;
	}

	uint64_t get_next_address() const
	{
		return this->active_address_;
	}

	void serialize(utils::buffer_serializer& buffer) const
	{
		buffer.write(this->address_);
		buffer.write(this->size_);
		buffer.write(this->active_address_);
	}

	void deserialize(utils::buffer_deserializer& buffer)
	{
		buffer.read(this->address_);
		buffer.read(this->size_);
		buffer.read(this->active_address_);
	}

	void release()
	{
		if (this->emu_ && this->address_ && this->size_)
		{
			this->emu_->release_memory(this->address_, this->size_);
			this->address_ = 0;
			this->size_ = 0;
		}
	}

private:
	emulator* emu_{};
	uint64_t address_{};
	uint64_t size_{};
	uint64_t active_address_{0};
};

inline std::wstring read_unicode_string(const emulator& emu, const UNICODE_STRING ucs)
{
	static_assert(offsetof(UNICODE_STRING, Length) == 0);
	static_assert(offsetof(UNICODE_STRING, MaximumLength) == 2);
	static_assert(offsetof(UNICODE_STRING, Buffer) == 8);
	static_assert(sizeof(UNICODE_STRING) == 16);

	std::wstring result{};
	result.resize(ucs.Length / 2);

	emu.read_memory(reinterpret_cast<uint64_t>(ucs.Buffer), result.data(), ucs.Length);

	return result;
}


inline std::wstring read_unicode_string(const emulator& emu, const emulator_object<UNICODE_STRING> uc_string)
{
	const auto ucs = uc_string.read();
	return read_unicode_string(emu, ucs);
}

inline std::wstring read_unicode_string(emulator& emu, const UNICODE_STRING* uc_string)
{
	return read_unicode_string(emu, emulator_object<UNICODE_STRING>{emu, uc_string});
}

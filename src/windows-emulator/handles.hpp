#pragma once

struct handle_types
{
	enum type : uint16_t
	{
		file,
		event,
		section,
		symlink,
		directory,
		semaphore,
		port,
		thread,
	};
};

#pragma pack(push)
#pragma pack(1)
struct handle_value
{
	uint64_t id : 32;
	uint64_t type : 16;
	uint64_t padding : 15;
	uint64_t is_pseudo : 1;
};
#pragma pack(pop)

static_assert(sizeof(handle_value) == 8);

union handle
{
	handle_value value;
	uint64_t bits;
	HANDLE h;
};

inline bool operator==(const handle& h1, const handle& h2)
{
	return h1.bits == h2.bits;
}

inline bool operator==(const handle& h1, const uint64_t& h2)
{
	return h1.bits == h2;
}

inline handle_value get_handle_value(const uint64_t h)
{
	handle hh{};
	hh.bits = h;
	return hh.value;
}

constexpr handle make_handle(const uint32_t id, const handle_types::type type, const bool is_pseudo)
{
	handle_value value{};

	value.padding = 0;
	value.id = id;
	value.type = type;
	value.is_pseudo = is_pseudo;

	return {value};
}

constexpr handle make_pseudo_handle(const uint32_t id, const handle_types::type type)
{
	return make_handle(id, type, true);
}

namespace handle_detail
{
	template <typename, typename = void>
	struct has_deleter_function : std::false_type
	{
	};

	template <typename T>
	struct has_deleter_function<T, std::void_t<decltype(T::deleter(std::declval<T&>()))>>
		: std::is_same<decltype(T::deleter(std::declval<T&>())), bool>
	{
	};
}

template <handle_types::type Type, typename T>
	requires(utils::Serializable<T>)
class handle_store
{
public:
	using value_map = std::map<uint32_t, T>;

	handle store(T value)
	{
		auto index = this->find_free_index();
		this->store_[index] = std::move(value);

		return make_handle(index);
	}

	handle make_handle(const uint32_t index)
	{
		handle h{};
		h.bits = 0;
		h.value.is_pseudo = false;
		h.value.type = Type;
		h.value.id = index;

		return h;
	}

	T* get(const handle_value h)
	{
		const auto entry = this->get_iterator(h);
		if (entry == this->store_.end())
		{
			return nullptr;
		}

		return &entry->second;
	}

	T* get(const handle h)
	{
		return this->get(h.value);
	}

	T* get(const uint64_t h)
	{
		handle hh{};
		hh.bits = h;

		return this->get(hh);
	}

	size_t size() const
	{
		return this->store_.size();
	}

	bool erase(const typename value_map::iterator& entry)
	{
		if (entry == this->store_.end())
		{
			return false;
		}

		if constexpr (handle_detail::has_deleter_function<T>())
		{
			if (!T::deleter(entry->second))
			{
				return false;
			}
		}

		this->store_.erase(entry);
		return true;
	}

	bool erase(const handle_value h)
	{
		const auto entry = this->get_iterator(h);
		return this->erase(entry);
	}

	bool erase(const handle h)
	{
		return this->erase(h.value);
	}

	bool erase(const uint64_t h)
	{
		handle hh{};
		hh.bits = h;

		return this->erase(hh);
	}

	bool erase(const T& value)
	{
		const auto entry = this->find(value);
		return this->erase(entry);
	}

	void serialize(utils::buffer_serializer& buffer) const
	{
		buffer.write_map(this->store_);
	}

	void deserialize(utils::buffer_deserializer& buffer)
	{
		buffer.read_map(this->store_);
	}

	typename value_map::iterator find(const T& value)
	{
		auto i = this->store_.begin();
		for (; i != this->store_.end(); ++i)
		{
			if (&i->second == &value)
			{
				break;
			}
		}

		return i;
	}

	typename value_map::const_iterator find(const T& value) const
	{
		auto i = this->store_.begin();
		for (; i != this->store_.end(); ++i)
		{
			if (&i->second == &value)
			{
				break;
			}
		}

		return i;
	}

	typename value_map::iterator begin()
	{
		return this->store_.begin();
	}

	typename value_map::const_iterator begin() const
	{
		return this->store_.begin();
	}

	typename value_map::iterator end()
	{
		return this->store_.end();
	}

	typename value_map::const_iterator end() const
	{
		return this->store_.end();
	}

private:
	typename value_map::iterator get_iterator(const handle_value h)
	{
		if (h.type != Type || h.is_pseudo)
		{
			return this->store_.end();
		}

		return this->store_.find(h.id);
	}

	uint32_t find_free_index()
	{
		uint32_t index = 1;
		for (; index > 0; ++index)
		{
			if (!this->store_.contains(index))
			{
				break;
			}
		}

		return index;
	}


	value_map store_{};
};

constexpr auto KNOWN_DLLS_DIRECTORY = make_pseudo_handle(0x1337, handle_types::directory);
constexpr auto KNOWN_DLLS_SYMLINK = make_pseudo_handle(0x1337, handle_types::symlink);
constexpr auto SHARED_SECTION = make_pseudo_handle(0x1337, handle_types::section);
constexpr auto CONSOLE_SERVER = make_pseudo_handle(0x1338, handle_types::section);
constexpr auto CM_API = make_pseudo_handle(0x1338, handle_types::file);

constexpr auto CONSOLE_HANDLE = make_pseudo_handle(0x1, handle_types::file);
constexpr auto STDOUT_HANDLE = make_pseudo_handle(0x2, handle_types::file);
constexpr auto STDIN_HANDLE = make_pseudo_handle(0x3, handle_types::file);

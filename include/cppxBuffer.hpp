#ifndef CPPX_BUFFER_H
#define CPPX_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace cppx {
struct BufferFlags {
	std::uint8_t memory : 1;
	std::uint8_t modify : 1;
};

struct BufferManager {
	using AllocateFunction = std::function<void *(std::size_t)>;
	using DeallocateFunction = std::function<void(void *, std::size_t)>;

	static const AllocateFunction defaultAllocateFunction;
	static const DeallocateFunction defaultReleaseFunction;

	const char *const name;
	BufferFlags flags;
	AllocateFunction alloc;
	DeallocateFunction release;

	std::string toString() const;
};

class BufferCore {
public:
	std::uint16_t m_refcount;
	std::uint16_t m_preall;
	std::uint32_t m_size;
	std::uint8_t *m_address;

	const BufferManager *m_manager;

public:
	constexpr static const std::size_t max_size = std::uint32_t(~0);
	constexpr static const std::size_t max_preall = std::uint16_t(~0);

private:
	BufferCore(const BufferManager *manager, std::uint16_t preall = 0, std::uint32_t size = 0, std::uint8_t *address = nullptr);

public:
	~BufferCore() = default;

	std::uint8_t *tryAllocateRaw(std::size_t bytes);
	bool tryDeallocateRaw();
	bool tryShare();
	bool tryAllocate(std::size_t bytes);
	bool tryDeallocate();

	static void shareOrDetach(BufferCore *&core);

	static void detach(BufferCore *&core);
	static void create(BufferCore *&core, const BufferManager *manager, std::uint16_t preall = 0, std::uint32_t size = 0, std::uint8_t *address = nullptr);
	static void release(BufferCore *&core);
	static void change(BufferCore *&core, BufferCore *const newcore);
};

class Buffer {
public:
	typedef std::uint8_t byte_t;

public:
	static const BufferManager staticManager;
	static const BufferManager stackManager;
	static const BufferManager heapManager;

	static constexpr const BufferManager *onStatic = &staticManager;
	static constexpr const BufferManager *onStack = &stackManager;
	static constexpr const BufferManager *onHeap = &heapManager;

private:
	BufferCore *m_core;

public:
	Buffer();
	Buffer(const BufferManager *manager, std::size_t size = 0);
	Buffer(const BufferManager *manager, void *pointer, std::size_t size);
	Buffer(const Buffer &other);
	Buffer(Buffer &&other);
	~Buffer();

	[[nodiscard]] static Buffer Heap(std::size_t size);
	[[nodiscard]] static Buffer HeapPreall(std::size_t size);
	[[nodiscard]] static Buffer HeapFrom(void *ptr, std::size_t size);
	[[nodiscard]] static Buffer Stack(void *ptr, std::size_t size);
	[[nodiscard]] static const Buffer Static(void *ptr, std::size_t size);

	Buffer &operator=(const Buffer &other);
	Buffer &operator=(Buffer &&other);

	int compare(const Buffer &other) const noexcept;

	operator bool() const;
	bool operator!() const;

	bool operator==(const Buffer &other) const;
	bool operator!=(const Buffer &other) const;
	bool operator>(const Buffer &other) const;
	bool operator<(const Buffer &other) const;
	bool operator>=(const Buffer &other) const;
	bool operator<=(const Buffer &other) const;

	[[nodiscard]] void *data() noexcept;
	[[nodiscard]] void *data() const noexcept;

	std::size_t size() const noexcept;
	std::size_t preallocated() const noexcept;
	std::size_t totalsize() const noexcept;
	const BufferManager *manager() const noexcept;

	byte_t at(std::size_t i) const;
	byte_t &at(std::size_t i);

#ifdef CPPX_BUFFER_DEBUG
	inline std::uint16_t refcount() const
	{
		return m_core ? m_core->m_refcount : 0;
	}
#endif

	inline byte_t operator[](std::size_t i) const
	{
		return at(i);
	}
	inline byte_t &operator[](std::size_t i) { return at(i); }

public:
	//! @brief Random access iterator class
	class Iterator {
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = byte_t;
		using pointer = value_type *;
		using reference = value_type &;
		using iterator_category = std::random_access_iterator_tag;

	private:
		//! @brief Current index in the buffer
		std::uint32_t m_index;

		//! @brief Buffer data
		BufferCore *m_data;

		friend class Buffer;

	private:
		Iterator(BufferCore *const data, std::uint32_t index);

	public:
		Iterator() = default;
		Iterator(const Iterator &other);
		~Iterator();

		std::size_t maxIndex() const noexcept;

		std::size_t index() const noexcept;

		/**
		 * @brief Returns the current value
		 * @throw Exception if the current index is beyond the end of the data
		 */
		byte_t value() const;

		/**
		 * @brief Returns a reference to the current value
		 * @throw Exception if the current index is beyond the end of the data
		 */
		byte_t &value();

		[[nodiscard]] Iterator step(std::int32_t amount) const;
		Iterator &stepSelf(std::int32_t amount);

		inline byte_t operator*() const { return value(); }
		inline byte_t &operator*() { return value(); }

		inline byte_t operator[](std::int32_t offset) const { return step(offset).value(); }
		inline byte_t &operator[](std::int32_t offset) { return step(offset).value(); }

		inline Iterator &operator++() { return stepSelf(1); }
		inline Iterator &operator--() { return stepSelf(-1); }

		/**
		 * @brief Steps the iterator; post-increment
		 * @throw Exception if the current iterator is an end iterator
		 */
		Iterator operator++(int);

		/**
		 * @brief Steps the iterator backwards; post-decrement
		 * @throw Exception if the current iterator is a begin iterator
		 */
		Iterator operator--(int);

		inline Iterator operator+(std::int32_t amount) const { return step(amount); }
		inline Iterator operator-(std::int32_t amount) const { return step(-amount); }

		/**
		 * @brief Returns the distance between the two iterators
		 * @throw Exception if the current iterator and |Other| don't refer to the same buffer
		 */
		std::ptrdiff_t operator-(const Iterator &other) const;

		inline Iterator &operator+=(std::int32_t amount) { return stepSelf(amount); }
		inline Iterator &operator-=(std::int32_t amount) { return stepSelf(-amount); }

		Iterator &operator=(const Iterator &other);
		Iterator &operator=(Iterator &&other);

		bool operator==(const Iterator &other) const;
		bool operator!=(const Iterator &other) const;

		std::string toString() const;
	};

public:
	Iterator begin() const;
	Iterator end() const;

	Buffer &selfPreallocate(std::size_t extra, const BufferManager *manager = nullptr);

	[[nodiscard]] Buffer clone(const BufferManager *manager = nullptr) const;
	Buffer &selfClone(const Buffer &other, const BufferManager *manager = nullptr);

	[[nodiscard]] Buffer range(std::size_t start, std::size_t end, const BufferManager *manager = nullptr) const;
	[[nodiscard]] Buffer range(Iterator start, Iterator end, const BufferManager *manager = nullptr) const;

	[[nodiscard]] Buffer reverse(const BufferManager *manager = nullptr) const;
	[[nodiscard]] Buffer reverse(std::size_t start, std::size_t end, const BufferManager *manager = nullptr) const;
	[[nodiscard]] Buffer reverse(Iterator start, Iterator end, const BufferManager *manager = nullptr) const;

	Buffer &selfReverse();
	Buffer &selfReverse(std::size_t start, std::size_t end);
	Buffer &selfReverse(Iterator start, Iterator end);

	[[nodiscard]] Buffer insert(std::size_t index, const Buffer &value, const BufferManager *manager = nullptr) const;
	[[nodiscard]] Buffer insert(Iterator index, const Buffer &value, const BufferManager *manager = nullptr) const;
	[[nodiscard]] Buffer append(const Buffer &right, const BufferManager *manager = nullptr) const;

	Buffer &selfInsert(std::size_t index, const Buffer &value);
	Buffer &selfInsert(Iterator index, const Buffer &value);
	Buffer &selfAppend(const Buffer &right);

	[[nodiscard]] Buffer erase(std::size_t start, std::size_t end, const BufferManager *manager = nullptr) const;
	[[nodiscard]] Buffer erase(Iterator start, Iterator end, const BufferManager *manager = nullptr) const;

	Buffer &selfErase(std::size_t start, std::size_t end);
	Buffer &selfErase(Iterator start, Iterator end);

	enum Representation : std::uint8_t {
		HEX = 0x01,
		BINARY = 0x02,
		LOWERCASE = 0x04,
		PREFIXED = 0x08
	};
	std::string represent(std::uint8_t form = Representation::HEX) const;
	inline std::string toString() const { return represent(Representation::HEX | Representation::PREFIXED); }
};
} // namespace cppx

#endif // !defined(CPPX_BUFFER_H)
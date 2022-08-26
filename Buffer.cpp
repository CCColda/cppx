#include "Buffer.hpp"
#include "Exception.hpp"

#include <iomanip>
#include <sstream>

#define BUFFER_COPY(dest, src, size) memcpy(dest, src, size)
#define BUFFER_MOVE(dest, src, size) memmove(dest, src, size)

#if defined(BUFFER_NO_BUILTINS)
#define BUFFER_SAFE_INCREASE(x, onoverflow) \
	{                                       \
		const auto __orig_x__ = x;          \
		x += 1;                             \
		if (x <= __orig_x__) [[unlikely]] { \
			x = __orig_x__;                 \
			onoverflow;                     \
		}                                   \
	}

#define BUFFER_SAFE_DECREASE(x, onoverflow) \
	{                                       \
		const auto __orig_x__ = x;          \
		x -= 1;                             \
		if (x >= __orig_x__) [[unlikely]] { \
			x = __orig_x__;                 \
			onoverflow;                     \
		}                                   \
	}
#else // defined(BUFFER_NO_BUILTINS)
#define BUFFER_SAFE_INCREASE(x, onoverflow)                  \
	{                                                        \
		const auto __orig_x__ = x;                           \
		if (__builtin_add_overflow(x, 1, &x)) [[unlikely]] { \
			x = __orig_x__;                                  \
			onoverflow;                                      \
		}                                                    \
	}

#define BUFFER_SAFE_DECREASE(x, onoverflow)                  \
	{                                                        \
		const auto __orig_x__ = x;                           \
		if (__builtin_sub_overflow(x, 1, &x)) [[unlikely]] { \
			x = __orig_x__;                                  \
			onoverflow;                                      \
		}                                                    \
	}
#endif // defined(BUFFER_NO_BUILTINS)

namespace {
namespace bufexc {
constexpr const char *buf_no_alloc = "Can't create buffer: manager has allocations disallowed";
constexpr const char *buf_fail_alloc = "Allocation failed";
constexpr const char *buf_fail_release = "Failed releasing buffer data";
constexpr const char *buf_ref_empty = "Can't get reference: The buffer is empty";
constexpr const char *buf_ref_index_invalid = "Can't get reference: Invalid index";
constexpr const char *bufcore_fail_detach = "Failed detaching buffer";
constexpr const char *buf_readonly = "Buffer cannot be modified";
constexpr const char *buf_data_not_owned = "Cannot use modified data";
constexpr const char *buf_insufficient = "Insufficient buffer storage";
constexpr const char *buf_no_manager = "No suitable data manager";

constexpr const char *buf_fail_ref_overflow = "Reference count overflow";
constexpr const char *buf_fail_ref_underflow = "Reference count underflow";

constexpr const char *iter_instantiation_fail_ref_overflow = "Iterator could not be instantiated; reference count overflow";
constexpr const char *iter_invalid = "Invalid iterator";
constexpr const char *iter_invalid_sub = "Invalid iterator subtraction";
constexpr const char *iter_end_increment = "Can't increment beyond end iterator";
constexpr const char *iter_begin_decrement = "Can't decrement beyond begin iterator";
constexpr const char *invalid_range = "Invalid range";
constexpr const char *invalid_index = "Invalid index";
constexpr const char *no_data = "Data not avaliable";
} // namespace bufexc
} // namespace

namespace Cold {
#pragma region BufferManager

/** @static */
const BufferManager::AllocateFunction BufferManager::defaultAllocateFunction = [](std::size_t) -> void * { return nullptr; };

/** @static */
const BufferManager::DeallocateFunction BufferManager::defaultReleaseFunction = [](void *, std::size_t) -> void { return; };

std::string BufferManager::toString() const
{
	using namespace std::string_literals;

	return "{\"name\"=\""s + name +
	       "\", \"flags\"="s +
	       (flags.memory ? "m"s : ""s) +
	       (flags.modify ? "w"s : ""s) +
	       "}"s;
}

// BufferManager
#pragma endregion
#pragma region BufferCore

BufferCore::BufferCore(
    const BufferManager *manager,
    std::uint16_t preall, std::uint32_t size,
    std::uint8_t *address)
    : m_manager(manager), m_refcount(1),
      m_preall(preall), m_size(size), m_address(address)
{
}

bool BufferCore::tryShare()
{
	BUFFER_SAFE_INCREASE(m_refcount, { return false; })
	return true;
}

bool BufferCore::tryAllocate(std::size_t bytes)
{
	if (!m_manager->flags.memory)
		return false;

	m_address = reinterpret_cast<std::uint8_t *>(m_manager->alloc(bytes));
	m_preall = bytes;
	m_size = 0;
	return bool(m_address);
}

bool BufferCore::tryDeallocate()
{
	if (!m_manager->flags.memory)
		return false;

	m_manager->release(m_address, m_size + m_preall);
	m_address = nullptr;
	m_size = 0;
	m_preall = 0;

	return true;
}

/** @static */
void BufferCore::shareOrDetach(BufferCore *&core)
{
	if (!core->tryShare())
		detach(core);
}

/** @static */
void BufferCore::detach(BufferCore *&core)
{
	if (core->m_refcount > 1) {
		auto newCore = new BufferCore(core->m_manager, core->m_preall, core->m_size, core->m_address);

		if (core->m_manager->flags.modify) {
			if (!newCore->tryAllocate(core->m_size + core->m_preall)) {
				delete newCore;
				throw Exception(__FUNCTION__, bufexc::bufcore_fail_detach);
			}
		}

		release(core);
		core = newCore;
	}
}

/** @static */
void BufferCore::release(BufferCore *&core)
{
	if (core->m_address && core->m_refcount <= 1 && core->m_manager->flags.memory) {
		core->m_manager->release(core->m_address, core->m_size + core->m_preall);

		delete core;
	}
	else {
		BUFFER_SAFE_DECREASE(core->m_refcount, {});
	}

	core = nullptr;
}

/** @static */
void BufferCore::change(BufferCore *&core, BufferCore *const newcore)
{
	release(core);

	core = newcore;
	shareOrDetach(core);
}

/** @static */
void BufferCore::swap(BufferCore *&core, BufferCore *&newcore)
{
	auto &tempcore = newcore;

	newcore = core;
	core = tempcore;
}

// BufferCore
#pragma endregion

#pragma region Buffer

/** @static */
const BufferManager Buffer::staticManager = {
    "staticManager",
    {0, 0},
    BufferManager::defaultAllocateFunction,
    BufferManager::defaultReleaseFunction};

/** @static */
const BufferManager Buffer::stackManager = {
    "stackManager",
    {0, 1},
    BufferManager::defaultAllocateFunction,
    BufferManager::defaultReleaseFunction};

/** @static */
const BufferManager Buffer::heapManager = {
    "heapManager",
    {1, 1},
    [](std::size_t size) -> void * { return new std::uint8_t[size]; },
    [](void *ptr, std::size_t size) -> void { delete[] reinterpret_cast<std::uint8_t *>(ptr); }};

Buffer::Buffer() : m_core(nullptr) {}

Buffer::Buffer(const BufferManager *manager, std::size_t size)
    : m_core(nullptr)
{
	if (!manager->flags.memory)
		throw Exception(
		    Exception::makeCallString(__FUNCTION__, manager, size),
		    bufexc::buf_no_alloc);

	m_core = new BufferCore(manager);

	// TODO?
	if (size)
		if (!m_core->tryAllocate(size))
			throw Exception(
			    Exception::makeCallString(__FUNCTION__, manager, size),
			    bufexc::buf_fail_alloc);
}

Buffer::Buffer(const BufferManager *manager, void *pointer, std::size_t size)
{
	if (manager->flags.memory)
		throw Exception(
		    Exception::makeCallString(__FUNCTION__, manager, pointer, size),
		    bufexc::buf_data_not_owned);

	m_core = new BufferCore(manager, 0, size, reinterpret_cast<std::uint8_t *>(pointer));
}

Buffer::Buffer(const Buffer &other)
    : m_core(other.m_core)
{
	BufferCore::shareOrDetach(m_core);
}

Buffer::Buffer(Buffer &&other)
    : m_core(other.m_core)
{
	BufferCore::shareOrDetach(m_core);
}

Buffer::~Buffer()
{
	if (m_core)
		BufferCore::release(m_core);
}

/** @static */ [[nodiscard]] Buffer Buffer::Heap(std::size_t size)
{
	return Buffer(&heapManager, size);
}

/** @static */ [[nodiscard]] Buffer Buffer::HeapFrom(void *ptr, std::size_t size)
{
	auto result = Buffer(&heapManager, size);

	result.m_core->m_size = result.m_core->m_preall;
	result.m_core->m_preall = 0;

	BUFFER_COPY(result.m_core->m_address, ptr, result.m_core->m_size);

	return result;
}

/** @static */ [[nodiscard]] Buffer Buffer::Stack(void *ptr, std::size_t size)
{
	return Buffer(&stackManager, ptr, size);
}

/** @static */ [[nodiscard]] const Buffer Buffer::Static(void *ptr, std::size_t size)
{
	return Buffer(&staticManager, ptr, size);
}

Buffer &Buffer::operator=(const Buffer &other)
{
	if (m_core) {
		BufferCore::change(m_core, other.m_core);
	}
	else {
		m_core = other.m_core;
		BufferCore::shareOrDetach(m_core);
	}

	return *this;
}

Buffer &Buffer::operator=(Buffer &&other)
{
	if (m_core) {
		BufferCore::change(m_core, other.m_core);
	}
	else {
		m_core = other.m_core;
		BufferCore::shareOrDetach(m_core);
	}

	return *this;
}

int Buffer::compare(const Buffer &other) const noexcept
{
	const auto thissize = size(), othersize = other.size();

	if (thissize < othersize)
		return -1;
	else if (thissize > othersize)
		return 1;

	for (std::size_t i = 0; i < thissize; ++i) {
		const auto thisival = at(i), otherival = other.at(i);

		if (thisival < otherival)
			return -1;
		else if (thisival > otherival)
			return 1;
	}

	return 0;
}

Buffer::operator bool() const { return m_core ? bool(m_core->m_address) : false; }
bool Buffer::operator!() const { return m_core ? !m_core->m_address : true; }

bool Buffer::operator==(const Buffer &other) const { return compare(other) == 0; }
bool Buffer::operator!=(const Buffer &other) const { return compare(other) != 0; }
bool Buffer::operator>(const Buffer &other) const { return compare(other) > 0; }
bool Buffer::operator<(const Buffer &other) const { return compare(other) < 0; }
bool Buffer::operator>=(const Buffer &other) const { return compare(other) >= 0; }
bool Buffer::operator<=(const Buffer &other) const { return compare(other) <= 0; }

[[nodiscard]] void *Buffer::data() noexcept
{
	return m_core
	           ? m_core->m_address
	           : nullptr;
}

[[nodiscard]] void *Buffer::data() const noexcept
{
	return m_core
	           ? m_core->m_address
	           : nullptr;
}

std::size_t Buffer::size() const noexcept
{
	return m_core ? m_core->m_size : 0u;
}

std::size_t Buffer::preallocated() const noexcept
{
	return m_core ? m_core->m_preall : 0u;
}

std::size_t Buffer::totalsize() const noexcept
{
	return m_core ? m_core->m_size + m_core->m_preall : 0u;
}

const BufferManager *Buffer::manager() const noexcept
{
	return m_core ? m_core->m_manager : nullptr;
}

Buffer::byte_t Buffer::at(std::size_t i) const
{
	if (m_core) {
		if (m_core->m_size <= i)
			throw Exception(Exception::makeCallString(__FUNCTION__, i), bufexc::buf_ref_index_invalid);

		return m_core->m_address[i];
	}
	else {
		throw Exception(Exception::makeCallString(__FUNCTION__, i), bufexc::buf_ref_empty);
	}
}

Buffer::byte_t &Buffer::at(std::size_t i)
{
	if (m_core) {
		if (!m_core->m_manager->flags.modify)
			throw Exception(Exception::makeCallString(__FUNCTION__, i), bufexc::buf_readonly);

		if (m_core->m_size <= i)
			throw Exception(Exception::makeCallString(__FUNCTION__, i), bufexc::buf_ref_index_invalid);

		return m_core->m_address[i];
	}
	else {
		throw Exception(Exception::makeCallString(__FUNCTION__, i), bufexc::buf_ref_empty);
	}
}

// Buffer
#pragma endregion

#pragma region BufferIterator

Buffer::Iterator::Iterator(BufferCore *const data, std::uint32_t index)
    : m_data(data), m_index(index)
{

	if (m_data)
		if (!m_data->tryShare())
			throw Exception(__FUNCTION__, bufexc::iter_instantiation_fail_ref_overflow);
}

Buffer::Iterator::Iterator(const Iterator &other)
    : m_data(other.m_data), m_index(other.m_index)
{

	if (m_data)
		if (!m_data->tryShare())
			throw Exception(__FUNCTION__, bufexc::iter_instantiation_fail_ref_overflow);
}

Buffer::Iterator::~Iterator()
{
	if (m_data)
		BufferCore::release(m_data);
}

std::size_t Buffer::Iterator::maxIndex() const noexcept
{
	return m_data ? m_data->m_size : 0;
}

std::size_t Buffer::Iterator::index() const noexcept
{
	return m_index;
}

Buffer::byte_t Buffer::Iterator::value() const
{
	if (!m_data)
		throw Exception(__FUNCTION__, bufexc::iter_invalid);

	if (m_index >= m_data->m_size)
		throw Exception(__FUNCTION__, bufexc::iter_invalid);

	return m_data->m_address[m_index];
}

Buffer::byte_t &Buffer::Iterator::value()
{
	if (!m_data)
		throw Exception(__FUNCTION__, bufexc::iter_invalid);

	if (m_index >= m_data->m_size)
		throw Exception(__FUNCTION__, bufexc::iter_invalid);

	return m_data->m_address[m_index];
}

[[nodiscard]] Buffer::Iterator Buffer::Iterator::step(std::int32_t amount) const
{
	if (!m_data)
		throw Exception(Exception::makeCallString(__FUNCTION__, amount), bufexc::iter_invalid);

	if (m_index + amount > m_data->m_size)
		throw Exception(Exception::makeCallString(__FUNCTION__, amount), bufexc::iter_end_increment);

	if (static_cast<std::int64_t>(m_index) + amount < 0)
		throw Exception(Exception::makeCallString(__FUNCTION__, amount), bufexc::iter_begin_decrement);

	return Iterator(m_data, m_index + amount);
}

Buffer::Iterator &Buffer::Iterator::stepSelf(std::int32_t amount)
{
	if (!m_data)
		throw Exception(Exception::makeCallString(__FUNCTION__, amount), bufexc::iter_invalid);

	if (m_index + amount > m_data->m_size)
		throw Exception(Exception::makeCallString(__FUNCTION__, amount), bufexc::iter_end_increment);

	if (static_cast<std::int64_t>(m_index) + amount < 0)
		throw Exception(Exception::makeCallString(__FUNCTION__, amount), bufexc::iter_begin_decrement);

	m_index += amount;
	return *this;
}

Buffer::Iterator Buffer::Iterator::operator++(int)
{
	if (!m_data)
		throw Exception(__FUNCTION__, bufexc::iter_invalid);

	if (m_index + 1 > m_data->m_size)
		throw Exception(__FUNCTION__, bufexc::iter_end_increment);

	return Iterator(m_data, m_index++);
}

Buffer::Iterator Buffer::Iterator::operator--(int)
{
	if (!m_data)
		throw Exception(__FUNCTION__, bufexc::iter_invalid);

	if (m_index == 0)
		throw Exception(__FUNCTION__, bufexc::iter_begin_decrement);

	return Iterator(m_data, m_index--);
}

std::ptrdiff_t Buffer::Iterator::operator-(const Iterator &other) const
{
	if (m_data == nullptr || m_data != other.m_data)
		throw Exception(Exception::makeCallString(__FUNCTION__, other), bufexc::iter_invalid_sub);

	return std::ptrdiff_t(m_index - other.m_index);
}

Buffer::Iterator &Buffer::Iterator::operator=(const Iterator &other)
{
	if (m_data) {
		BufferCore::change(m_data, other.m_data);
	}
	else {
		m_data = other.m_data;
		BufferCore::shareOrDetach(m_data);
	}

	return *this;
}

Buffer::Iterator &Buffer::Iterator::operator=(Iterator &&other)
{
	if (m_data) {
		BufferCore::change(m_data, other.m_data);
	}
	else {
		m_data = other.m_data;
		BufferCore::shareOrDetach(m_data);
	}

	return *this;
}

bool Buffer::Iterator::operator==(const Iterator &other) const
{
	return (m_data == other.m_data && m_index == other.m_index);
}

bool Buffer::Iterator::operator!=(const Iterator &other) const
{
	return (m_data != other.m_data || m_index != other.m_index);
}

[[noexcept]] std::string Buffer::Iterator::toString() const
{
	std::stringstream stream;

	stream
	    << "{index=" << m_index
	    << " max_index=" << (m_data ? m_data->m_size : 0)
	    << "}";

	return stream.str();
}

// BufferIterator
#pragma endregion

#pragma region BufferOperations

Buffer::Iterator Buffer::begin() const
{
	return Iterator(m_core, 0);
}

Buffer::Iterator Buffer::end() const
{
	return Iterator(m_core, m_core ? m_core->m_size : 0);
}

[[nodiscard]] Buffer Buffer::clone(const BufferManager *manager) const
{
	if (!m_core)
		return Buffer();

	const BufferManager *resultManager = manager ? manager : m_core->m_manager;

	if (!resultManager)
		throw Exception(Exception::makeCallString(__FUNCTION__, manager), bufexc::buf_no_manager);

	if (!(resultManager->flags.memory && resultManager->flags.modify))
		throw Exception(Exception::makeCallString(__FUNCTION__, manager), bufexc::buf_no_alloc);

	Buffer result = Buffer(resultManager, m_core->m_size);
	m_core->m_size = m_core->m_preall;
	m_core->m_preall = 0;

	BUFFER_COPY(result.m_core->m_address, m_core->m_address, m_core->m_size);

	return result;
}

Buffer &Buffer::selfClone(const Buffer &other, const BufferManager *imanager)
{
	if (!other)
		return *this;

	const BufferManager *resultManager = imanager ? imanager : manager();

	if (!resultManager)
		throw Exception(Exception::makeCallString(__FUNCTION__, other, imanager), bufexc::buf_no_manager);

	if (!(resultManager->flags.memory && resultManager->flags.modify))
		throw Exception(Exception::makeCallString(__FUNCTION__, other, imanager), bufexc::buf_no_alloc);

	if (m_core)
		BufferCore::release(m_core);

	m_core = new BufferCore(resultManager);
	if (!m_core->tryAllocate(other.m_core->m_size))
		throw Exception(Exception::makeCallString(__FUNCTION__, other, imanager), bufexc::buf_fail_alloc);

	m_core->m_size = m_core->m_preall;
	m_core->m_preall = 0;

	BUFFER_COPY(m_core->m_address, other.m_core->m_address, m_core->m_size);

	return *this;
}

[[nodiscard]] Buffer Buffer::range(std::size_t start, std::size_t end, bool constant) const
{
	if (end < start || end >= size())
		throw Exception(Exception::makeCallString(__FUNCTION__, start, end, constant), bufexc::invalid_range);

	// m_core is defined - if undefined, (end >= 0, size = 0 -> throw)

	if (constant)
		return Buffer::Static(m_core->m_address + start, end - start);

	// TODO
	auto result = Buffer(&heapManager, end - start);
	result.m_core->m_size = result.m_core->m_preall;
	result.m_core->m_preall = 0;

	BUFFER_COPY(result.m_core->m_address, m_core->m_address + start, result.m_core->m_size);

	return result;
}

[[nodiscard]] Buffer Buffer::range(Iterator start, Iterator end, bool constant) const
{
	if (start.m_data != end.m_data || start.m_data != m_core || end.m_index < start.m_index)
		throw Exception(Exception::makeCallString(__FUNCTION__, start, end, constant), bufexc::invalid_range);

	return range(start.m_index, end.m_index, constant);
}

[[nodiscard]] Buffer Buffer::reverse(std::size_t start, std::size_t end, const BufferManager *imanager) const
{
	if (end < start || end > size())
		throw Exception(Exception::makeCallString(__FUNCTION__, start, end), bufexc::invalid_range);

	const BufferManager *newManager = imanager ? imanager : manager();

	auto result = Buffer(newManager, m_core->m_size);
	result.m_core->m_size = result.m_core->m_preall;
	result.m_core->m_preall = 0;

	BUFFER_COPY(
	    result.m_core->m_address,
	    m_core->m_address,
	    start);

	BUFFER_COPY(
	    result.m_core->m_address + end,
	    m_core->m_address + end,
	    m_core->m_size - end);

	for (std::size_t i = start, j = end - 1; i < end; ++i, --j)
		result.m_core->m_address[i] = m_core->m_address[j];

	return result;
}

[[nodiscard]] Buffer Buffer::reverse(Iterator start, Iterator end, const BufferManager *imanager) const
{
	if (start.m_data != end.m_data || start.m_data != m_core || end.m_index < start.m_index)
		throw Exception(Exception::makeCallString(__FUNCTION__, start, end), bufexc::invalid_range);

	return reverse(start.m_index, end.m_index, imanager);
}

[[nodiscard]] Buffer Buffer::reverse(const BufferManager *imanager) const
{
	return reverse(0, size(), imanager);
}

Buffer &Buffer::selfReverse(std::size_t start, std::size_t end)
{
	if (end < start || end > size())
		throw Exception(Exception::makeCallString(__FUNCTION__, start, end), bufexc::invalid_range);

	if (!m_core->m_manager->flags.modify)
		throw Exception(Exception::makeCallString(__FUNCTION__, start, end), bufexc::buf_readonly);

	const std::size_t halfway = start + (end - start) / 2;

	for (std::size_t i = start, j = end - 1; i < halfway; ++i, --j) {
		const std::uint8_t left = m_core->m_address[i];

		m_core->m_address[i] = m_core->m_address[j];
		m_core->m_address[j] = left;
	}

	return *this;
}

Buffer &Buffer::selfReverse(Iterator start, Iterator end)
{
	if (start.m_data != end.m_data || start.m_data != m_core || end.m_index < start.m_index)
		throw Exception(Exception::makeCallString(__FUNCTION__, start, end), bufexc::invalid_range);

	return selfReverse(start.m_index, end.m_index);
}

Buffer &Buffer::selfReverse()
{
	return selfReverse(0, size());
}

[[nodiscard]] Buffer Buffer::insert(std::size_t index, const Buffer &value) const
{
	if (index > size())
		throw Exception(Exception::makeCallString(__FUNCTION__, end, value), bufexc::iter_invalid);

	const BufferManager *newManager = manager() ? manager() : &heapManager;

	Buffer newBuffer = Buffer(newManager, size() + value.size());
	newBuffer.m_core->m_size = newBuffer.m_core->m_preall;
	newBuffer.m_core->m_preall = 0;

	if (m_core) {
		BUFFER_COPY(newBuffer.m_core->m_address, m_core->m_address, index);
		BUFFER_COPY(newBuffer.m_core->m_address + index + value.size(), m_core->m_address + index, size() - index);
	}

	if (value.m_core)
		BUFFER_COPY(newBuffer.m_core->m_address + index, value.m_core->m_address, value.size());

	return newBuffer;
}

[[nodiscard]] Buffer Buffer::insert(Iterator index, const Buffer &value) const
{
	if (index.m_data != m_core)
		throw Exception(Exception::makeCallString(__FUNCTION__, index, value), bufexc::iter_invalid);

	return insert(index.m_index, value);
}

[[nodiscard]] Buffer Buffer::append(const Buffer &right) const
{
	return insert(size(), right);
}

Buffer &Buffer::selfInsert(std::size_t index, const Buffer &value)
{
	if (!m_core)
		return selfClone(value);

	if (index > size())
		throw Exception(Exception::makeCallString(__FUNCTION__, index, value), bufexc::invalid_range);

	if (!m_core->m_manager->flags.modify)
		throw Exception(Exception::makeCallString(__FUNCTION__, index, value), bufexc::buf_readonly);

	if (!value)
		return *this;

	const auto newSize = m_core->m_size + value.m_core->m_size;

	if (newSize > m_core->m_preall || m_core->m_refcount > 1) {
		if (!m_core->m_manager->flags.memory)
			throw Exception(Exception::makeCallString(__FUNCTION__, index, value), bufexc::buf_insufficient);

		BufferCore *newCore = new BufferCore(m_core->m_manager);

		if (!newCore->tryAllocate(newSize))
			throw Exception(Exception::makeCallString(__FUNCTION__, index, value), bufexc::buf_fail_alloc);

		newCore->m_size = newCore->m_preall;
		newCore->m_preall = 0;

		BUFFER_COPY(newCore->m_address, m_core->m_address, index);
		BUFFER_COPY(newCore->m_address + index, value.m_core->m_address, value.m_core->m_size);
		BUFFER_COPY(newCore->m_address + index + value.m_core->m_size, m_core->m_address + value.m_core->m_size, m_core->m_size - index);

		BufferCore::change(m_core, newCore);

		return *this;
	}
	else {
		BUFFER_MOVE(m_core->m_address + index + value.m_core->m_size, m_core->m_address + index, value.m_core->m_size);
		BUFFER_COPY(m_core->m_address + index, value.m_core->m_address, value.m_core->m_size);
	}

	return *this;
}

Buffer &Buffer::selfInsert(Iterator index, const Buffer &value)
{
	if (index.m_data != m_core)
		throw Exception(Exception::makeCallString(__FUNCTION__, index, value), bufexc::invalid_range);

	return selfInsert(index.m_index, value);
}

Buffer &Buffer::selfAppend(const Buffer &right)
{
	return selfInsert(size(), right);
}

std::string Buffer::represent(std::uint8_t form) const
{
	if (!m_core)
		return "null";

	if (m_core->m_size == 0)
		return "null";

	std::ostringstream stream;

	if ((form & Representation::HEX) == Representation::HEX) {
		if ((form & Representation::LOWERCASE) == 0)
			stream << std::uppercase;

		if ((form & Representation::PREFIXED) == Representation::PREFIXED)
			stream << "0x";

		stream << std::hex << std::noshowbase;
		for (std::size_t index = 0; index < m_core->m_size; ++index)
			stream << std::setfill('0') << std::setw(2)
			       << static_cast<std::uint32_t>(m_core->m_address[index]);
	}
	else if ((form & Representation::BINARY) == Representation::BINARY) {
		if ((form & Representation::LOWERCASE) == 0)
			stream << std::uppercase;

		if ((form & Representation::PREFIXED) == Representation::PREFIXED)
			stream << "0b";

		for (std::size_t index = 0; index < m_core->m_size; ++index) {
			const auto v = m_core->m_address[index];

			for (std::size_t bit = 0x80; bit > 0; bit >>= 1)
				stream << ((v & bit) == bit) ? '1' : '0';
		}
	}
	else {
		stream << "null";
	}

	return stream.str();
}

std::string Buffer::toString() const
{
	return represent(Representation::HEX | Representation::PREFIXED);
}

// BufferOperations
#pragma endregion

} // namespace Cold
#include <catch2/catch_all.hpp>
#include <execution>
#include <numeric>

#include "cppxBuffer.hpp"
#include "cppxException.hpp"

namespace Catch {
template <>
struct StringMaker<cppx::Buffer> {
	static std::string convert(cppx::Buffer const &value)
	{
		return value.toString();
	}
};
} // namespace Catch

template <std::size_t N = 16>
const auto randomStackData()
{
	static_assert(N > 0, "N must be greater than 0");

	struct {
		std::uint8_t data[N];
		std::size_t size;
	} result = {{}, N};

	for (std::size_t i = 0; i < result.size; ++i)
		result.data[i] = rand() % 0x100;

	return result;
}

template <std::size_t N = 16>
const auto randomStaticData()
{
	static_assert(N > 0, "N must be greater than 0");

	static std::uint8_t data[N] = {};

	struct {
		std::uint8_t *data;
		std::size_t size;
	} result = {(uint8_t *)&data, N};

	for (std::size_t i = 0; i < result.size; ++i)
		data[i] = rand() % 0x100;

	return result;
}

TEST_CASE("cppx::Buffer", "[Buffer]")
{
	using cppx::Buffer;

	static const char s_staticData[] = "i am static data";

	SECTION("constructor")
	{
		SECTION("empty constructor")
		{
			const auto emptyBuffer = Buffer();
			REQUIRE(emptyBuffer.size() == 0);
			REQUIRE(emptyBuffer.preallocated() == 0);
			REQUIRE(emptyBuffer.data() == nullptr);
		}

		SECTION("size constructor")
		{
			SECTION("throws on size initialization")
			{
				REQUIRE_THROWS(Buffer(&Buffer::stackManager, 4));
				REQUIRE_THROWS(Buffer(&Buffer::staticManager, 4));
			}

			const auto heapBuffer = Buffer(&Buffer::heapManager, 4);
			REQUIRE(heapBuffer.size() == 4);
			REQUIRE(heapBuffer.preallocated() == 0);
			REQUIRE(heapBuffer.data() != nullptr);

			const auto largeBuffer = Buffer(&Buffer::heapManager, 0x1000F);
			REQUIRE(largeBuffer.size() == 0x1000F);
			REQUIRE(largeBuffer.preallocated() == 0);
			REQUIRE(largeBuffer.data() != nullptr);
		}

		SECTION("data constructor")
		{
			const auto stackData = randomStackData();
			const auto staticData = randomStaticData();

			SECTION("throws on static initialization of heap")
			{
				REQUIRE_THROWS(Buffer(&Buffer::heapManager, (void *)"ABCD", 4));
			}

			const auto stackBuffer = Buffer(&Buffer::stackManager, (void *)stackData.data, stackData.size);
			REQUIRE(stackBuffer.data() == stackData.data);
			REQUIRE(stackBuffer.preallocated() == 0);
			REQUIRE(stackBuffer.size() == stackData.size);

			const auto staticBuffer = Buffer(&Buffer::staticManager, (void *)staticData.data, staticData.size);
			REQUIRE(staticBuffer.data() == staticData.data);
			REQUIRE(staticBuffer.preallocated() == 0);
			REQUIRE(staticBuffer.size() == staticData.size);
		}

		SECTION("HeapPreall constructor")
		{
			const auto heapBuffer = Buffer::HeapPreall(4);

			REQUIRE(heapBuffer.size() == 0);
			REQUIRE(heapBuffer.preallocated() == 4);
			REQUIRE(heapBuffer.data() != nullptr);
		}

		SECTION("Heap constructor")
		{
			const auto heapBuffer = Buffer::Heap(4);

			REQUIRE(heapBuffer.size() == 4);
			REQUIRE(heapBuffer.preallocated() == 0);
			REQUIRE(heapBuffer.data() != nullptr);
		}

		SECTION("HeapFrom constructor")
		{
			const auto data = randomStackData();
			const auto heapBuffer = Buffer::HeapFrom((void *)data.data, data.size);

			REQUIRE(heapBuffer.size() == data.size);
			REQUIRE(heapBuffer.preallocated() == 0);

			REQUIRE_THAT(heapBuffer.data(), Catch::Matchers::Predicate<void *>([data](void *bufdata) {
				             for (std::size_t i = 0; i < data.size; ++i)
					             if (data.data[i] != ((std::uint8_t *)bufdata)[i])
						             return false;
				             return true;
			             }));
		}

		SECTION("Stack constructor")
		{
			const auto stackData = randomStackData();
			const auto stackBuffer = Buffer::Stack((void *)stackData.data, stackData.size);

			REQUIRE(stackBuffer.size() == stackData.size);
			REQUIRE(stackBuffer.preallocated() == 0);
			REQUIRE(stackBuffer.data() == stackData.data);
		}

		SECTION("Static constructor")
		{
			const auto staticData = randomStaticData();
			const auto staticBuffer = Buffer::Stack((void *)staticData.data, staticData.size);

			REQUIRE(staticBuffer.size() == staticData.size);
			REQUIRE(staticBuffer.preallocated() == 0);
			REQUIRE(staticBuffer.data() == staticData.data);
		}
	}

	const auto s_staticbuf = Buffer::Static((void *)s_staticData, sizeof(s_staticData));
	auto s_heapbuf = Buffer::Heap(sizeof(int));

	// TODO

	SECTION("comparisons")
	{
		const std::uint8_t data[] = {0x00, 0x01, 0x02};
		const std::uint8_t data2[] = {0x00, 0x01, 0x02, 0x03};
		const std::uint8_t data3[] = {0x10, 0x20, 0x30, 0x40};

		const auto buf_data = Buffer::Stack((void *)data, sizeof(data)),
		           buf_data2 = Buffer::Stack((void *)data2, sizeof(data2)),
		           buf_data3 = Buffer::Stack((void *)data3, sizeof(data3));

		SECTION("truth comparisons")
		{
			REQUIRE(buf_data);
			REQUIRE_FALSE(!buf_data);
		}

		SECTION("equality comparisons")
		{
			REQUIRE(buf_data == buf_data);
			REQUIRE(buf_data != buf_data2);
		}

		SECTION("relative comparisons")
		{
			REQUIRE(buf_data < buf_data2);
			REQUIRE(buf_data <= buf_data2);
			REQUIRE(buf_data2 < buf_data3);
			REQUIRE(buf_data2 <= buf_data3);
		}
	}

	SECTION("modifying data")
	{
		SECTION("modifying data on static")
		{
			static std::uint8_t data[] = {0x00, 0x01, 0x02};
			const auto staticbuf = Buffer::Static((void *)data, sizeof(data));

			REQUIRE(staticbuf[1] == 0x01);
		}

		SECTION("modifying data on stack")
		{
			std::uint8_t data[] = {0x00, 0x01, 0x02};
			auto stackbuf = Buffer::Stack((void *)data, sizeof(data));

			stackbuf[1] = 0x10;

			REQUIRE(stackbuf[1] == 0x10);
			REQUIRE(data[1] == 0x10);
		}

		SECTION("modifying data on heap")
		{
			const std::uint8_t data[] = {0x00, 0x01, 0x02};
			auto heapbuf = Buffer::HeapFrom((void *)data, sizeof(data));

			heapbuf[1] = 0x10;

			REQUIRE(heapbuf[1] == 0x10);
		}
	}

	SECTION("assignment")
	{
		Buffer buf2(s_staticbuf);

		REQUIRE(buf2 == s_staticbuf);
		REQUIRE(s_staticbuf.refcount() == 2);

		buf2 = s_heapbuf;

		REQUIRE(s_heapbuf.refcount() == 2);
		REQUIRE(s_staticbuf.refcount() == 1);
		REQUIRE(buf2 == s_heapbuf);
	}

	SECTION("iterators")
	{
		std::size_t traditionalReduce = 0;

		for (std::size_t i = 0; i < s_staticbuf.size(); ++i) {
			traditionalReduce += s_staticbuf.at(i);
		}

		std::size_t iteratorReduce = 0;
		std::size_t iteratorReduceCounter = 0;

		for (auto it : s_staticbuf) {
			iteratorReduce += it;
			++iteratorReduceCounter;
		}

		std::size_t stdReduce = std::reduce(std::execution::par, s_staticbuf.begin(), s_staticbuf.end(), 0);

		REQUIRE(iteratorReduceCounter == s_staticbuf.size());
		REQUIRE(traditionalReduce == iteratorReduce);
		REQUIRE(iteratorReduce == stdReduce);
	}

	SECTION("preall")
	{
	}

	SECTION("clone")
	{
		REQUIRE(Buffer().clone() == Buffer());

		REQUIRE_THROWS(s_staticbuf.clone());
		REQUIRE_THROWS(s_staticbuf.clone(&Buffer::stackManager));
		REQUIRE_THROWS(s_staticbuf.clone(&Buffer::staticManager));

		auto staticbuf_heap_clone = s_staticbuf.clone(&Buffer::heapManager);
		REQUIRE(staticbuf_heap_clone == s_staticbuf);
		REQUIRE(staticbuf_heap_clone.data() != s_staticbuf.data());
		REQUIRE(staticbuf_heap_clone.manager() != s_staticbuf.manager());
	}

	SECTION("selfClone")
	{
		REQUIRE(Buffer().selfClone(Buffer()) == Buffer());

		REQUIRE_THROWS(Buffer().selfClone(s_staticbuf));
		REQUIRE_THROWS(Buffer().selfClone(s_staticbuf, &Buffer::stackManager));
		REQUIRE_THROWS(Buffer().selfClone(s_staticbuf, &Buffer::staticManager));
		REQUIRE_NOTHROW(Buffer(&Buffer::heapManager).selfClone(s_staticbuf));

		auto staticbuf_heap_clone = Buffer().selfClone(s_staticbuf, &Buffer::heapManager);

		REQUIRE(staticbuf_heap_clone == s_staticbuf);
		REQUIRE(staticbuf_heap_clone.data() != s_staticbuf.data());
		REQUIRE(staticbuf_heap_clone.manager() != s_staticbuf.manager());
	}

	SECTION("ranges")
	{
		const auto range1 = s_staticbuf.range(3, 5);
		const auto range2 = s_staticbuf.range(s_staticbuf.begin() + 3, s_staticbuf.begin() + 5);
		auto range3 = s_staticbuf.range(s_staticbuf.begin() + 3, s_staticbuf.begin() + 5, Buffer::onHeap);

		REQUIRE(range1 == range2);
		REQUIRE(range1[0] == s_staticbuf[3]);

		REQUIRE(range1.manager() == &Buffer::staticManager);
		REQUIRE(range2.manager() == &Buffer::staticManager);

		REQUIRE_NOTHROW(range3[1] = 0x99);
		REQUIRE(range3[1] == 0x99);
		REQUIRE(s_staticbuf[1] != 0x99);
	}

	SECTION("representation")
	{
		REQUIRE(Buffer().represent() == "null");

		REQUIRE_THAT(s_staticbuf.represent(Buffer::HEX | Buffer::PREFIXED), Catch::Matchers::StartsWith("0x"));
		REQUIRE_THAT(s_staticbuf.represent(Buffer::HEX | Buffer::LOWERCASE), Catch::Matchers::Matches("[a-f0-9]+", Catch::CaseSensitive::Yes));
		REQUIRE_THAT(s_staticbuf.represent(Buffer::HEX), Catch::Matchers::Matches("[A-F0-9]+", Catch::CaseSensitive::Yes));

		REQUIRE_THAT(s_staticbuf.represent(Buffer::BINARY | Buffer::PREFIXED), Catch::Matchers::StartsWith("0b"));

		REQUIRE(Buffer::Static((void *)"\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8).represent(Buffer::HEX) == "0123456789ABCDEF");
		REQUIRE(Buffer::Static((void *)"\x93", 1).represent(Buffer::BINARY) == "10010011");
	}

	SECTION("reverse")
	{
		SECTION("throws without manager")
		{
			REQUIRE_THROWS(s_staticbuf.reverse());
		}

		SECTION("length and data match")
		{
			REQUIRE(s_staticbuf.reverse(&Buffer::heapManager).size() == s_staticbuf.size());

			REQUIRE(s_staticbuf.reverse(&Buffer::heapManager)[0] == s_staticbuf[s_staticbuf.size() - 1]);
			REQUIRE(s_staticbuf.reverse(&Buffer::heapManager).reverse() == s_staticbuf);

			REQUIRE(Buffer::Static((void *)"\x01\x23\x45\x67\x89\xAB", 6).reverse(1, 4, &Buffer::heapManager) == Buffer::Static((void *)"\x01\x67\x45\x23\x89\xAB", 6));
		}
	}

	SECTION("selfReverse")
	{
		std::uint8_t stackdata[8] = {0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87};
		Buffer stackbuf = Buffer::Stack(stackdata, sizeof(stackdata));
		REQUIRE(stackbuf.selfReverse(2, 6) == stackbuf);

		REQUIRE(stackbuf == Buffer::Static((void *)"\xF0\xE1\xA5\xB4\xC3\xD2\x96\x87", 8));
	}

	SECTION("insert")
	{
		std::uint8_t stackdata[8] = {0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87};
		std::uint8_t stackdata2[4] = {0x01, 0x02, 0x03, 0x04};
		std::uint8_t stackdata3[4] = {0xA0, 0xB0, 0xC0, 0xD0};

		REQUIRE_THROWS(Buffer().insert(0, Buffer::Stack(stackdata, sizeof(stackdata))));
		REQUIRE_NOTHROW(Buffer().insert(0, Buffer::Stack(stackdata, sizeof(stackdata)), Buffer::onHeap));

		auto insertBuffer = Buffer(Buffer::onHeap).insert(0, Buffer::Stack(stackdata, sizeof(stackdata)));

		REQUIRE(insertBuffer.size() == sizeof(stackdata));
		REQUIRE(insertBuffer.preallocated() == 0);
		REQUIRE(insertBuffer.data() != stackdata);

		auto insertBuffer2 = insertBuffer.insert(3, Buffer::Stack(stackdata2, sizeof(stackdata2)));

		REQUIRE(insertBuffer2.size() == sizeof(stackdata) + sizeof(stackdata2));
		REQUIRE(insertBuffer2.preallocated() == 0);
		REQUIRE(insertBuffer2 == Buffer::Static((void *)"\xF0\xE1\xD2\x01\x02\x03\x04\xC3\xB4\xA5\x96\x87", 12));

		auto insertBuffer3 = insertBuffer2.insert(insertBuffer2.size(), Buffer::Stack(stackdata3, sizeof(stackdata3)));

		REQUIRE(insertBuffer3.size() == sizeof(stackdata) + sizeof(stackdata2) + sizeof(stackdata3));
		REQUIRE(insertBuffer3.preallocated() == 0);
		REQUIRE(insertBuffer3 == Buffer::Static((void *)"\xF0\xE1\xD2\x01\x02\x03\x04\xC3\xB4\xA5\x96\x87\xA0\xB0\xC0\xD0", 16));
	}

	// TODO check that metadata changed correctly
	SECTION("selfInsert")
	{
		std::uint8_t stackdata[8] = {0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87};
		std::uint8_t stackdata2[4] = {0x01, 0x02, 0x03, 0x04};
		std::uint8_t stackdata3[4] = {0xA0, 0xB0, 0xC0, 0xD0};

		auto insertBuffer = Buffer(&Buffer::heapManager);
		auto invalidInsertBuffer = Buffer();

		REQUIRE_THROWS(invalidInsertBuffer.selfInsert(0, Buffer::Stack(stackdata, sizeof(stackdata))));

		insertBuffer.selfInsert(0, Buffer::Stack(stackdata, sizeof(stackdata)));

		REQUIRE(insertBuffer.size() == sizeof(stackdata));
		REQUIRE(insertBuffer.preallocated() == 0);
		REQUIRE(insertBuffer.data() != stackdata);

		insertBuffer.selfInsert(3, Buffer::Stack(stackdata2, sizeof(stackdata2)));

		REQUIRE(insertBuffer.size() == sizeof(stackdata) + sizeof(stackdata2));
		REQUIRE(insertBuffer.preallocated() == 0);
		REQUIRE(insertBuffer == Buffer::Static((void *)"\xF0\xE1\xD2\x01\x02\x03\x04\xC3\xB4\xA5\x96\x87", 12));

		insertBuffer.selfInsert(insertBuffer.size(), Buffer::Stack(stackdata3, sizeof(stackdata3)));

		REQUIRE(insertBuffer.size() == sizeof(stackdata) + sizeof(stackdata2) + sizeof(stackdata3));
		REQUIRE(insertBuffer.preallocated() == 0);
		REQUIRE(insertBuffer == Buffer::Static((void *)"\xF0\xE1\xD2\x01\x02\x03\x04\xC3\xB4\xA5\x96\x87\xA0\xB0\xC0\xD0", 16));
	}

	SECTION("erase")
	{
		REQUIRE_THROWS(s_staticbuf.erase(4, s_staticbuf.size()));
		REQUIRE(s_staticbuf.erase(4, s_staticbuf.size(), Buffer::onHeap) == Buffer::Static((void *)"i am", 4));
	}

	SECTION("selfErase")
	{
		REQUIRE(s_staticbuf.clone(Buffer::onHeap).selfErase(4, s_staticbuf.size()) == Buffer::Static((void *)"i am", 4));
		REQUIRE(s_staticbuf.clone(Buffer::onHeap).selfErase(0, s_staticbuf.size()) == Buffer());
	}
}

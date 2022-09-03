#include <catch2/catch_all.hpp>
#include <string>

#include "cppxException.hpp"

CATCH_TRANSLATE_EXCEPTION(cppx::Exception const &exc)
{
	return exc.getString();
}

TEST_CASE("cppx::Exception", "[Exception]")
{
	constexpr const char *functions[] = {
	    "function1",
	    "function2",
	    "function3",
	    "function4"};

	constexpr const auto functions_size = sizeof(functions) / sizeof(functions[0]);

	constexpr const char *description = "Exception test";

	cppx::Exception exc(functions[0], description);

	REQUIRE(exc.getDescription() == description);
	REQUIRE(exc.getCallstack().size() == 1);
	REQUIRE(exc.getCallstack()[0] == functions[0]);

	SECTION("increasing the stored call stack")
	{
		for (int i = 1; i < functions_size; ++i) {
			exc = cppx::Exception(functions[i], exc);
		}

		REQUIRE(exc.getCallstack().size() == functions_size);
		REQUIRE(exc.getCallstack()[0] == functions[0]);
		REQUIRE(exc.getCallstack()[exc.getCallstack().size() - 1] == functions[functions_size - 1]);
	}

	SECTION("creating function argument lists")
	{
		using namespace std::string_literals;

		const auto callstring = cppx::Exception::makeCallString(
		    "function_with_arguments", 9, "a const char*", 'c', 0.8f, 0.888);

		const auto expected =
		    "function_with_arguments(["s +
		    typeid(int(9)).name() + "] 9, ["s +
		    typeid((const char *)("")).name() + "] a const char*, ["s +
		    typeid(char('c')).name() + "] c, ["s +
		    typeid(float(0.8f)).name() + "] 0.8, [" +
		    typeid(double(0.888)).name() + "] 0.888)"s;

		REQUIRE(callstring == expected);
	}
}

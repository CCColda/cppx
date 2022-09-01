#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <sstream>
#include <string>
#include <vector>

#include <type_traits>
#include <typeinfo>

namespace colda {

/** @brief Class representing callstack-tracking exceptions */
class Exception {
private:
	std::string m_description;
	std::vector<std::string> m_callstack;

public:
	Exception() noexcept;
	Exception(const Exception &other) noexcept;
	Exception(const std::string &function, const std::string &description) noexcept;
	Exception(const std::string &function, const Exception &lastInStack) noexcept;
	~Exception() noexcept;

	std::string getDescription() const noexcept;
	std::vector<std::string> getCallstack() const noexcept;

	std::string getCallstackString(int base = 0, int firstLine = 0) const noexcept;

	std::string getString() const noexcept;

private:
	template <typename T>
	class HasOstreamOperator {
		template <
		    typename U,
		    typename = decltype(std::declval<std::ostream>() << (std::declval<U>()))>
		constexpr static std::true_type test(U *);

		template <typename U>
		constexpr static std::false_type test(...);

	public:
		static constexpr const bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
	};

	template <typename T>
	class HasToString {
		template <
		    typename U,
		    typename = typename std::enable_if_t<
		        std::is_same_v<decltype(U::toString), std::string (U::*)() const>>>
		static std::true_type test(decltype(U::toString));
		template <typename U>
		static std::false_type test(...);

	public:
		static constexpr const bool value = std::is_same<decltype(test<T>(0)), std::true_type>::value;
	};

public:
	template <typename... Types>
	static std::string makeCallString(const char *function, Types... values)
	{
		constexpr std::size_t Args = sizeof...(Types);

		std::stringstream Stream;

		Stream << function << "(";

		std::size_t Counter = 0;

		([&Stream, values, &Counter, Args]() {
			if constexpr (HasToString<Types>::value) {
				Stream << "[" << typeid(Types).name() << "] " << values.toString();
			}
			else if constexpr (HasOstreamOperator<Types>::value) {
				Stream << "[" << typeid(Types).name() << "] " << values;
			}
			else {
				Stream << "[" << typeid(Types).name() << "] " << &values;
			}

			if (Counter < (Args - 1)) {
				Stream << ", ";
			}

			++Counter;
		}(),
		 ...);

		Stream << ")";

		return Stream.str();
	}
};

} // namespace colda

#endif // !EXCEPTION_H
#include "Exception.hpp"

#include <iomanip>

namespace Cold {

Exception::Exception() noexcept
    : m_callstack(), m_description() {}

Exception::Exception(const Exception &other) noexcept
    : m_callstack(other.m_callstack), m_description(other.m_description) {}

Exception::Exception(const std::string &function, const std::string &description) noexcept
    : m_callstack{function}, m_description(description) {}

Exception::Exception(const std::string &function, const Exception &lastInStack) noexcept
    : m_callstack{lastInStack.m_callstack}, m_description(lastInStack.m_description)
{
	m_callstack.push_back(function);
}

Exception::~Exception() noexcept {}

std::string Exception::getDescription() const noexcept
{
	return m_description;
}

std::vector<std::string> Exception::getCallstack() const noexcept
{
	return m_callstack;
}

std::string Exception::getCallstackString(int base, int firstLine) const noexcept
{
	switch (m_callstack.size()) {
		case 0: {
			return "";
		}
		case 1: {
			return std::string(base + firstLine, ' ') + m_callstack.at(0);
		}
		default: {
			const std::string baseIndentString = std::string(base, ' ');
			const std::string firstLineIndentString = std::string(base + firstLine, ' ');

			std::stringstream output;

			output << firstLineIndentString << "000 " << m_callstack.at(0) << '\n';

			for (std::size_t Index = 1; Index < m_callstack.size(); ++Index) {
				output << baseIndentString << std::setw(3) << std::setfill('0') << Index << " " << m_callstack.at(Index) << '\n';
			}

			return output.str();
		}
	}
}

std::string Exception::getString() const noexcept
{
	using namespace std::string_literals;

	return "Description: \""s + m_description + "\"\nCallstack: "s + getCallstackString(11, -11);
}

} // namespace Cold

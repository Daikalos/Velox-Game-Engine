#pragma once

#include <typeinfo>
#include <string>

#include <Velox/Config.hpp>

namespace vlx::id
{
	// Thanks to: https://skypjack.github.io/2020-03-14-ecs-baf-part-8/
	// and https://stackoverflow.com/questions/48896142/is-it-possible-to-get-hash-values-as-compile-time-constants
	//
	// Generating unique ids that work across different boundaries is tricky (furthermore in compile time), however, these
	// sources have provided solutions that should solve this problem. The solution below enables for unique ids to be generated 
	// at compile-time using the full signature of a function. The signature is altered depending on the template and thus 
	// generates a new id.

	static consteval size_t HashFunction(const std::string_view func_name)
	{
		size_t result = 0xcbf29ce484222325; // FNV offset basis

		for (const char c : func_name)
		{
			result ^= static_cast<size_t>(c);
			result *= 1099511628211; // FNV prime
		}

		return result;
	}

	struct Generator
	{
		static size_t Next()
		{
			static size_t value{};
			return value++;
		}
	};

	template<typename T>
	struct Type
	{
#if defined VELOX_PRETTY_FUNCTION
		static consteval size_t ID()
		{
			return HashFunction(VELOX_PRETTY_FUNCTION);
		}
#else
		static size_t ID()
		{
			static const size_t value = Generator::Next();
			return value;
		}
#endif
	};
}
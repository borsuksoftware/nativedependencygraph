#pragma once

#include <vector>
#include <unordered_map>

namespace dependencygraph {

	template <class TKeyType, class TValueType>
	class IObjectBuilder {
	public:
		virtual std::vector<TKeyType> GetDependencies(const TKeyType& address) = 0;
		virtual TValueType BuildObject(const TKeyType& address, const std::unordered_map<TKeyType, TValueType>& dependencies) = 0;
	};
}
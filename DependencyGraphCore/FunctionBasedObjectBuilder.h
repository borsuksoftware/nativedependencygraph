#pragma once

#include "IObjectBuilder.h"

namespace dependencygraph {

	template <class TKeyType, class TValueType>
	class FunctionBasedObjectBuilder : public IObjectBuilder<TKeyType, TValueType> {
	public:
		std::function<std::vector<TKeyType>(const TKeyType& address)> GetDependenciesFunc;
		std::function<TValueType(const TKeyType& address, const std::unordered_map<TKeyType, TValueType>& dependencies)> BuildObjectFunc;

		FunctionBasedObjectBuilder(
			std::function<std::vector<TKeyType>(const TKeyType& address)> GetDependenciesFunc,
			std::function<TValueType(const TKeyType& address, const std::unordered_map<TKeyType, TValueType>& dependencies)> BuildObjectFunc);

		std::vector<TKeyType> GetDependencies(const TKeyType& address) override;
		TValueType BuildObject(const TKeyType& address, const std::unordered_map<TKeyType, TValueType>& dependencies) override;
	};

	template <class TKeyType, class TValueType>
	FunctionBasedObjectBuilder<TKeyType, TValueType>::FunctionBasedObjectBuilder(std::function<std::vector<TKeyType>(const TKeyType& address)> getDependenciesFunc,
		std::function<TValueType(const TKeyType& address, const std::unordered_map<TKeyType, TValueType>& dependencies)> buildObjectFunc) :
		GetDependenciesFunc(getDependenciesFunc),
		BuildObjectFunc(buildObjectFunc) { }

	template <class TKeyType, class TValueType>
	std::vector<TKeyType> FunctionBasedObjectBuilder<TKeyType, TValueType>::GetDependencies(const TKeyType& address) {
		if (this->GetDependenciesFunc) {
			return this->GetDependenciesFunc(address);
		}

		return std::vector<TKeyType>();
	}

	template <class TKeyType, class TValueType>
	TValueType FunctionBasedObjectBuilder<TKeyType, TValueType>::BuildObject(const TKeyType& address, const std::unordered_map<TKeyType, TValueType>& dependencies) {
		if (this->BuildObjectFunc) {
			return this->BuildObjectFunc(address, dependencies);
		}

		TValueType defaultRetValue = TValueType(); // = default(TValueType);
		return defaultRetValue;
	}
}
#pragma once

#include <functional>
#include <unordered_map>

#include "IObjectBuilder.h"

namespace dependencygraph {
	template <class TKeyType, class TValueType>
	class ObjectBuilderProvider : public IObjectBuilderProvider<TKeyType, TValueType> {
	public:
		std::unordered_map<TKeyType, std::shared_ptr<IObjectBuilder<TKeyType, TValueType>>> addressSpecificBuilders;
		std::unordered_map<TKeyType, TValueType> addressSpecificOverrides;
		std::function<bool(const TKeyType&, std::shared_ptr<IObjectBuilder<TKeyType, TValueType>>&)> builderProviderFunc;

		// IObjectBuilderProvider functionality
		bool TryGetObjectBuilder(const TKeyType& address, std::shared_ptr<IObjectBuilder<TKeyType, TValueType>>& objectBuilder) override;

	private:

	};

	template <class TKeyType, class TValueType>
	bool ObjectBuilderProvider<TKeyType, TValueType>::TryGetObjectBuilder(const TKeyType& address, std::shared_ptr<IObjectBuilder<TKeyType, TValueType>>& objectBuilder) {
		{
			auto itr = this->addressSpecificOverrides.find(address);
			if (itr != this->addressSpecificOverrides.end()) {
				// TODO - wrap and return
			}
		}

		{
			auto itr = this->addressSpecificBuilders.find(address);
			if (itr != this->addressSpecificBuilders.end()) {
				objectBuilder = itr->second;
				return true;
			}
		}

		if (this->builderProviderFunc)
		{
			auto retValue = this->builderProviderFunc(address, objectBuilder);
			if (retValue)
				return true;
		}

		objectBuilder = nullptr;
		return false;
	}
}
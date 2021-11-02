#pragma once

#include <memory>

#include "IObjectBuilder.h"

namespace dependencygraph {

	template <class TKeyType, class TValueType>
	class IObjectBuilderProvider {
	public:
		virtual bool TryGetObjectBuilder(const TKeyType& address, std::shared_ptr<IObjectBuilder<TKeyType, TValueType>>& objectBuilder) = 0;
	};
}
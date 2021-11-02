#pragma once

namespace dependencygraph {
	enum class ObjectBuildingState {
		Starting,
		NoBuilderAvailable,
		DependenciesKnown,
		ObjectBuilt,
		Failure,
	};


	std::wstring ToString(ObjectBuildingState state) {
		switch (state) {
		case ObjectBuildingState::Starting:
			return L"Starting";

		case ObjectBuildingState::NoBuilderAvailable:
			return L"NoBuilderAvailable";

		case ObjectBuildingState::DependenciesKnown:
			return L"DependenciesKnown";

		case ObjectBuildingState::ObjectBuilt:
			return L"ObjectBuilt";

		case ObjectBuildingState::Failure:
			return L"Failure";

		default:
			return L"Unknown";
		}
	}
}
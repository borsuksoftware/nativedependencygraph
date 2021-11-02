#pragma once

#include <functional>

namespace dependencygraph {

	enum class DependencyGraphJobStyle {
		other,
		objectBuilding,
		discovery,
	};

	// Wrapper class to define a job
	// Note that we don't simply use a std::function<..> object here because
	// we will extend it to include additional information which job executors
	// can use to decide upon precisely how to orchestrate the call.
	// 
	// This would typically be the case where different thread pools would be appropriate
	// based off properties of the underlying object builder
	struct DependencyGraphJob {
	public:
		DependencyGraphJobStyle style;
		std::function<void()> func;

		DependencyGraphJob() : style(DependencyGraphJobStyle::other) { }
		DependencyGraphJob(DependencyGraphJobStyle style, std::function<void()>&& func) : style(style), func(func) {};
	};

	class IDependencyGraphJobQueue {

	public:
		virtual void RegisterJob(DependencyGraphJob&& job) = 0;
	};
}
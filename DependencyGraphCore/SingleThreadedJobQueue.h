#pragma once

#include "ObjectContext.h"

// TODO - This looks odd because IJobQueue isn't currently templatised, but it will be shortly...

namespace dependencygraph {
	class SingleThreadedJobQueue : public IDependencyGraphJobQueue {
	public:
		void RegisterJob(DependencyGraphJob&& job) override {
			if (job.func)
				job.func();
		}
	};
}
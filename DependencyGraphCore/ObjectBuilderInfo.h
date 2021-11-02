#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "ObjectBuildingState.h"
#include "ObjectBuildingMode.h"
#include "WaitHandle.h"

namespace dependencygraph {

	// Forward definition
	template <class TKeyType, class TValueType> class ObjectContext;

	// Object representing a node within an object context
	template <class TKeyType, class TValueType>
	class ObjectBuilderInfo {

	private:
		std::atomic<int> _buildRequestCount;

		std::vector<std::function<void(ObjectBuilderInfo<TKeyType, TValueType>&)>> _postDependenciesKnownCallBacks;
		std::vector<std::function<void(ObjectBuilderInfo<TKeyType, TValueType>&)>> _postBuildCallBacks;

		std::atomic<int> _outstandingDependenciesCount;

		void launchPostDependenciesKnownCallBacks();
		void launchPostBuildCallBacks();

		void buildObject();

		std::atomic<ObjectBuildingState> _state;

	public:
		ObjectBuildingState getState() const volatile {
			return _state.load();
		}

		ObjectContext<TKeyType, TValueType>* objectContext;
		const TKeyType key;
		std::shared_ptr<IObjectBuilder<TKeyType, TValueType>> objectBuilder;

		std::vector<TKeyType> dependencies;
		dependencygraph::WaitHandle dependenciesKnownWaitHandle;
		std::mutex dependenciesKnownMutex;
		std::condition_variable dependenciesKnownCV;

		dependencygraph::WaitHandle objectBuiltOrFailureWaitHandle;
		std::mutex objectBuiltOrFailureMutex;
		std::condition_variable objectBuiltOrFailureCV;

		TValueType builtObject;
		std::shared_ptr<std::exception> exception;

		ObjectBuilderInfo(ObjectContext<TKeyType, TValueType>* objectContext,
			const TKeyType& key) :
			objectContext(objectContext),
			key(key),
			_buildRequestCount(0),
			builtObject(TValueType()),
			_state(ObjectBuildingState::Starting),
			objectBuiltOrFailureWaitHandle(&_state, &objectBuiltOrFailureMutex, &objectBuiltOrFailureCV, { ObjectBuildingState::Failure, ObjectBuildingState::NoBuilderAvailable, ObjectBuildingState::ObjectBuilt }),
			dependenciesKnownWaitHandle(&_state, &dependenciesKnownMutex, &dependenciesKnownCV, { ObjectBuildingState::Failure, ObjectBuildingState::NoBuilderAvailable, ObjectBuildingState::ObjectBuilt, ObjectBuildingState::DependenciesKnown }) {
		}

		// Set the object builder to be used (but do nothing with it for now)
		void SetObjectBuilder(std::shared_ptr<IObjectBuilder<TKeyType, TValueType>>& objectBuilder) {
			this->objectBuilder = objectBuilder;
		}

		void SetRequestedDependencies(std::vector<TKeyType>&& dependencies) {
			this->dependencies = std::move(dependencies);
			this->_state = ObjectBuildingState::DependenciesKnown;
			{
				std::unique_lock<std::mutex> accessor(this->dependenciesKnownMutex);
				this->dependenciesKnownCV.notify_all();
			}

			this->launchPostDependenciesKnownCallBacks();
		}

		void SetRequestedDependencies(std::vector<TKeyType>& dependencies) {
			auto localCopy = dependencies;
			this->SetRequestedDependencies(std::move(localCopy));
		}

		void SetObjectBuilt(TValueType& builtObject) {
			this->builtObject = builtObject;
			this->_state = ObjectBuildingState::ObjectBuilt;

			{
				std::unique_lock<std::mutex> accessor(this->objectBuiltOrFailureMutex);
				this->objectBuiltOrFailureCV.notify_all();
			}

			this->launchPostBuildCallBacks();
		}

		void SetObjectFailed(std::shared_ptr<std::exception>& exception) {
			this->exception = exception;
			this->_state = ObjectBuildingState::Failure;

			{
				std::unique_lock<std::mutex> accessor(this->dependenciesKnownMutex);
				this->dependenciesKnownCV.notify_all();
			}
			{
				std::unique_lock<std::mutex> accessor(this->objectBuiltOrFailureMutex);
				this->objectBuiltOrFailureCV.notify_all();
			}

			this->launchPostDependenciesKnownCallBacks();
			this->launchPostBuildCallBacks();
		}

		void SetNoBuilderFound() {
			this->_state = ObjectBuildingState::NoBuilderAvailable;
			{
				std::unique_lock<std::mutex> accessor(this->dependenciesKnownMutex);
				this->dependenciesKnownCV.notify_all();
			}
			{
				std::unique_lock<std::mutex> accessor(this->objectBuiltOrFailureMutex);
				this->objectBuiltOrFailureCV.notify_all();
			}

			this->launchPostDependenciesKnownCallBacks();
			this->launchPostBuildCallBacks();
		}

		void RegisterPostDependenciesKnownCallBack(std::function<void(ObjectBuilderInfo<TKeyType, TValueType>&)>&& callBackFunc);
		void RegisterPostBuildCallBack(std::function<void(ObjectBuilderInfo<TKeyType, TValueType>&)>&& callBackFunc);

		void RequestBuildObject(std::shared_ptr<IDependencyGraphJobQueue> jobQueue) {
			auto originalValue = _buildRequestCount.exchange(1);
			if (originalValue == 0) {
				// This was the actual build....

				switch (this->getState()) {
				case ObjectBuildingState::Failure:
				case ObjectBuildingState::NoBuilderAvailable:
					return;
				}

				this->RegisterPostDependenciesKnownCallBack([this, jobQueue](ObjectBuilderInfo<TKeyType, TValueType>& address) {
					// This method will be called once we know all of the dependencies that this
					// particular object will depend upon

					// TODO 
					// At this point, there are a few possibilities:
					//  1. We're clearly responsible for building the object
					//  2. That the object should simply be brought forward
					//  3. That the object should be built for us, after we've checked that the dependencies have been rebuilt
					//
					// The annoyance factor is that it's actually quite hard to work out whether we're in #2 or #3 right now,
					// with the one special case being when there are explicitly no dependencies to bring forwards
					try
					{
						_outstandingDependenciesCount.store((int)address.dependencies.size());

						// We can request actual building immediately and cannot actually be triggered from a post build call back anyway...
						if (address.dependencies.size() == 0) {
							DependencyGraphJob job(DependencyGraphJobStyle::objectBuilding, std::bind(&ObjectBuilderInfo<TKeyType, TValueType>::buildObject, this));
							jobQueue->RegisterJob(std::move(job));
						}
						else {
							for (auto& dependency : address.dependencies) {
								auto dependencyOBI = this->objectContext->BuildObject(dependency);

								dependencyOBI->RegisterPostBuildCallBack([this, jobQueue](ObjectBuilderInfo<TKeyType, TValueType>& builtDependency) {
									int previousCount = _outstandingDependenciesCount.fetch_sub(1);
									if (previousCount > 1)
										return;

									// At this point, we know that we need to actually build the object....
									DependencyGraphJob job(DependencyGraphJobStyle::objectBuilding, std::bind(&ObjectBuilderInfo<TKeyType, TValueType>::buildObject, this));
									jobQueue->RegisterJob(std::move(job));
									});
							}
						}
					}
					catch (...) {
						std::wcout << L"Random failure(" << address.key << L")" << std::endl;

						auto exception = std::make_shared<std::exception>("Failed");
						this->SetObjectFailed(exception);
					}
					});

			}
		}
	};

	template <class TKeyType, class TValueType>
	void ObjectBuilderInfo<TKeyType, TValueType>::buildObject() {

		try
		{
			std::unordered_map<TKeyType, TValueType> builtDependencies;
			int failureCount(0);
			for (auto& dependency : this->dependencies) {
				auto dependencyOBI = this->objectContext->BuildObject(dependency);
				if (dependencyOBI->getState() != ObjectBuildingState::ObjectBuilt) {
					failureCount++;
					break;
				}

				builtDependencies[dependency] = dependencyOBI->builtObject;
			}

			if (failureCount > 0) {
				std::wcout << L"Failed to source built dependencies for #" << this->key << std::endl;

				auto exception = std::make_shared<std::exception>("Failed to source dependency");
				this->SetObjectFailed(exception);
				return;
			}

			auto builtObject = this->objectBuilder->BuildObject(this->key, builtDependencies);
			this->SetObjectBuilt(builtObject);
		}
		catch (...)
		{
			std::wcout << L"Failed to build object #" << this->key << std::endl;
			auto exception = std::make_shared<std::exception>("Failed to build object dependency");
			this->SetObjectFailed(exception);
		}
	}

	template <class TKeyType, class TValueType>
	void ObjectBuilderInfo<TKeyType, TValueType>::RegisterPostDependenciesKnownCallBack(std::function<void(ObjectBuilderInfo<TKeyType, TValueType>&)>&& callBackFunc) {
		switch (this->getState()) {
		case ObjectBuildingState::DependenciesKnown:
		case ObjectBuildingState::Failure:
		case ObjectBuildingState::NoBuilderAvailable:
		case ObjectBuildingState::ObjectBuilt:
			// Can run immediately...
			if (callBackFunc)
				callBackFunc(*this);
			break;

		default: {
			std::unique_lock<std::mutex> lock(this->dependenciesKnownMutex);
			switch (this->getState()) {
			case ObjectBuildingState::DependenciesKnown:
			case ObjectBuildingState::Failure:
			case ObjectBuildingState::NoBuilderAvailable:
			case ObjectBuildingState::ObjectBuilt:
				// Can run immediately...
				lock.unlock();
				if (callBackFunc)
					callBackFunc(*this);
				break;

			default:
				this->_postDependenciesKnownCallBacks.push_back(callBackFunc);
				break;
			}

			break;
		}
		}
	}

	template <class TKeyType, class TValueType>
	void ObjectBuilderInfo<TKeyType, TValueType>::RegisterPostBuildCallBack(std::function<void(ObjectBuilderInfo<TKeyType, TValueType>&)>&& callBackFunc) {
		switch (this->getState()) {
		case ObjectBuildingState::Failure:
		case ObjectBuildingState::NoBuilderAvailable:
		case ObjectBuildingState::ObjectBuilt:
			// Can run immediately...
			if (callBackFunc)
				callBackFunc(*this);
			break;

		default: {

			// We need to take a lock...
			std::unique_lock<std::mutex> lock(this->objectBuiltOrFailureMutex);
			// At this stage, we could have been marked as having been completed...
			switch (this->getState()) {
			case ObjectBuildingState::Failure:
			case ObjectBuildingState::NoBuilderAvailable:
			case ObjectBuildingState::ObjectBuilt:
				// Can run immediately...
				lock.unlock();
				if (callBackFunc)
					callBackFunc(*this);
				break;

			default:
				this->_postBuildCallBacks.push_back(callBackFunc);
				break;
			}
			break;
		}
		}
	}

	template <class TKeyType, class TValueType>
	void ObjectBuilderInfo<TKeyType, TValueType>::launchPostDependenciesKnownCallBacks() {
		for (auto& callBack : this->_postDependenciesKnownCallBacks) {
			try {
				callBack(*this);
			}
			catch (...) {

			}
		}

		this->_postDependenciesKnownCallBacks.clear();
	}

	template <class TKeyType, class TValueType>
	void ObjectBuilderInfo<TKeyType, TValueType>::launchPostBuildCallBacks() {
		for (auto& callBack : this->_postBuildCallBacks) {
			try {
				callBack(*this);
			}
			catch (...) {

			}
		}

		this->_postBuildCallBacks.clear();
	}
}
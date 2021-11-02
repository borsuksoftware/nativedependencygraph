#pragma once


#include <functional>

#include "IDependencyGraphJobQueue.h"
#include "IObjectBuilderProvider.h"
#include "ObjectBuilderInfo.h"

namespace dependencygraph {

	/// <summary>
	/// Object representing the actual dependency graph
	/// </summary>
	/// <typeparam name="TKeyType"The type of the address></typeparam>
	/// <typeparam name="TValueType">The type of nodes within the graph</typeparam>
	template <class TKeyType, class TValueType>
	class ObjectContext {
	public:
		ObjectContext(
			std::shared_ptr<IObjectBuilderProvider<TKeyType, TValueType>> objectBuilderProvider,
			std::shared_ptr<IDependencyGraphJobQueue> jobQueue);

		std::shared_ptr<ObjectBuilderInfo<TKeyType, TValueType>> GetDependencies(const TKeyType& address);
		std::shared_ptr<ObjectBuilderInfo<TKeyType, TValueType>> BuildObject(const TKeyType& address);

	protected:
		std::shared_ptr<ObjectBuilderInfo<TKeyType, TValueType>> GetDependenciesInt(const TKeyType& address);

	private:
		std::shared_ptr<IDependencyGraphJobQueue> _jobQueue;
		std::shared_ptr<IObjectBuilderProvider<TKeyType, TValueType>> _objectBuilderProvider;

		std::unordered_map<TKeyType, std::shared_ptr<ObjectBuilderInfo<TKeyType, TValueType>>> _values;
		std::mutex _valuesDictionaryAccessMutex;
	};

	template <class TKeyType, class TValueType>
	ObjectContext<TKeyType, TValueType>::ObjectContext(
		std::shared_ptr<IObjectBuilderProvider<TKeyType, TValueType>> objectBuilderProvider,
		std::shared_ptr<IDependencyGraphJobQueue> jobQueue) :
		_objectBuilderProvider(objectBuilderProvider),
		_jobQueue(jobQueue){
	}

	template <class TKeyType, class TValueType>
	std::shared_ptr<ObjectBuilderInfo<TKeyType, TValueType>> ObjectContext<TKeyType, TValueType>::GetDependencies(const TKeyType& address) {
		return this->GetDependenciesInt(address);
	}

	template <class TKeyType, class TValueType>
	std::shared_ptr<ObjectBuilderInfo<TKeyType, TValueType>>  ObjectContext<TKeyType, TValueType>::GetDependenciesInt(const TKeyType& address) {
		{
			std::unique_lock<std::mutex> lock(this->_valuesDictionaryAccessMutex);
			auto itr = this->_values.find(address);
			if (itr != this->_values.end())
				return itr->second;

			// Create a new entry and store this...
			auto ptr = std::make_shared<ObjectBuilderInfo<TKeyType, TValueType>>(this, address);
			this->_values[address] = ptr;

			// At this point, we now need to kick off the rest of the population work, but we do not 
			// have to be locked as this is not a requirement anymore...
			lock.unlock();

			// Check to see if this is an object which we think we can build at this specific ObjectContext
			// level, otherwise look to our parents to see if we can do it.

			std::shared_ptr<IObjectBuilder<TKeyType, TValueType>> objectBuilder;
			if (!this->_objectBuilderProvider->TryGetObjectBuilder(address, objectBuilder)) {

				// TODO - Update this to allow for inheriting items / item specifications from a parent context
				// Register unable to do anything here...
				ptr->SetNoBuilderFound();
			}
			else {
				// Register the object builder and start the discovery process
				ptr->SetObjectBuilder(objectBuilder);

				try
				{
					auto dependencies = objectBuilder->GetDependencies(address);
					ptr->SetRequestedDependencies(std::move(dependencies));
				}
				catch (...)
				{
					std::wcout << L"Dependency Failed(" << address << L")" << std::endl;
					auto exception = std::make_shared<std::exception>("Discovery failed");
					ptr->SetObjectFailed(exception);
				}
			}

			return ptr;
		}
	}

	template <class TKeyType, class TValueType>
	std::shared_ptr<ObjectBuilderInfo<TKeyType, TValueType>> ObjectContext<TKeyType, TValueType>::BuildObject(const TKeyType& address) {
		auto obi = this->GetDependenciesInt(address);
		obi->RequestBuildObject(this->_jobQueue);
		return obi;
	}
}
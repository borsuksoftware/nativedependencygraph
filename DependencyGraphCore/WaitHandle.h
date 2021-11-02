#pragma once

#include "ObjectBuildingState.h"
#include <mutex>

namespace dependencygraph {
	class WaitHandle {
	private:
		std::atomic<ObjectBuildingState>* _stateAtomic;
		std::mutex* _stateMutex;
		std::condition_variable* _stateCV;

		int _acceptableValuesMask;

	protected:
		bool isCriteriaMatch(dependencygraph::ObjectBuildingState state) {
			bool bitmaskFlag = _acceptableValuesMask & (1 << (int)state);
			return bitmaskFlag;
		}

	public:
		WaitHandle(std::atomic<ObjectBuildingState>* stateAtomic,
			std::mutex* stateMutex,
			std::condition_variable* stateCV,
			const std::initializer_list<ObjectBuildingState>& acceptableValues) :
			_stateAtomic(stateAtomic),
			_stateMutex(stateMutex),
			_stateCV(stateCV),
			_acceptableValuesMask(0) {

			for (auto acceptableValue : acceptableValues)
				_acceptableValuesMask |= 1 << (int)acceptableValue;
		}

		WaitHandle(std::atomic<ObjectBuildingState>* stateAtomic,
			std::mutex* stateMutex,
			std::condition_variable* stateCV,
			std::initializer_list<ObjectBuildingState>&& acceptableValues) :
			_stateAtomic(stateAtomic),
			_stateMutex(stateMutex),
			_stateCV(stateCV),
			_acceptableValuesMask(0) {

			for (auto acceptableValue : acceptableValues)
				_acceptableValuesMask |= 1 << (int)acceptableValue;
		}

		void wait();

		template <class _Rep, class _Period>
		std::cv_status wait_for(const std::chrono::duration<_Rep, _Period> duration);

		std::cv_status wait_until(const xtime* absTime);
	};
}

template <class _Rep, class _Period>
std::cv_status dependencygraph::WaitHandle::wait_for(const std::chrono::duration<_Rep, _Period> duration) {
	auto state = _stateAtomic->load();
	if (this->isCriteriaMatch(state))
		return std::cv_status::no_timeout;

	std::unique_lock<std::mutex> lock(*_stateMutex);
	state = _stateAtomic->load();
	if (this->isCriteriaMatch(state))
		return std::cv_status::no_timeout;

	// At this point, we know that we can't be side-swiped as the CV will always get notified....
	auto result = _stateCV->wait_for(lock, duration);
	return result;
}

void dependencygraph::WaitHandle::wait() {
	auto state = _stateAtomic->load();
	if (this->isCriteriaMatch(state))
		return;

	std::unique_lock<std::mutex> lock(*_stateMutex);
	state = _stateAtomic->load();
	if (this->isCriteriaMatch(state))
		return;

	// At this point, we know that we can't be side-swiped as the CV will always get notified....
	_stateCV->wait(lock);
}

std::cv_status dependencygraph::WaitHandle::wait_until(const xtime* absTime) {

	auto state = _stateAtomic->load();
	if (this->isCriteriaMatch(state))
		return std::cv_status::no_timeout;

	std::unique_lock<std::mutex> lock(*_stateMutex);
	state = _stateAtomic->load();
	if (this->isCriteriaMatch(state))
		return std::cv_status::no_timeout;

	// At this point, we know that we can't be side-swiped as the CV will always get notified....
	auto result = _stateCV->wait_until(lock, absTime);
	return result;
}
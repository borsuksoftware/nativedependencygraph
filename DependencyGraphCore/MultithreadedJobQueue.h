#pragma once

#include <atomic>
#include <mutex>
#include <queue>
#include <vector>

#include "IDependencyGraphJobQueue.h"

// TODO - This looks odd because IJobQueue isn't currently templatised, but it will be shortly...

namespace dependencygraph {

	class MultithreadedJobQueue : public IDependencyGraphJobQueue {
	private:
		std::vector<std::thread> _threads;

		std::mutex _queueAccessMutex;
		std::condition_variable _queueAccessCV;
	public:
		std::queue<DependencyGraphJob> _jobs;
		std::atomic<int> totalRequests;
	private:
		volatile bool _stopRequested;

	public:
		MultithreadedJobQueue(int threadCount);

		void RegisterJob(DependencyGraphJob&& job) override;

		~MultithreadedJobQueue();
		void StopThreads();
	};

	void MultithreadedJobQueue::RegisterJob(DependencyGraphJob&& job) {

		this->totalRequests.fetch_add(1);

		std::unique_lock<std::mutex> lock(this->_queueAccessMutex);
		this->_jobs.push(job);

		this->_queueAccessCV.notify_all();
	}

	MultithreadedJobQueue::MultithreadedJobQueue(int threadCount) :
		_stopRequested(false),
		totalRequests(0) {
		if (threadCount == 0)
			throw std::exception("Invalid thread count specified");

		if (threadCount < 0) {
			// TODO - Read this value in from somewhere
			threadCount = 16;
		}

		for (int i(0); i < threadCount; ++i) {
			_threads.push_back(std::thread([this]() -> void {

				while (true) {
					try {
						if (_stopRequested)
							return;

						std::unique_lock<std::mutex> lock(this->_queueAccessMutex);
						if (this->_jobs.empty()) {
							// Nothing to do
							if (_stopRequested)
								return;

							this->_queueAccessCV.wait(lock);
						}
						else {
							auto job = this->_jobs.front();
							_jobs.pop();
							lock.unlock();

							try
							{
								job.func();
							}
							catch (...) {
								// What to do here?
							}
						}

					}
					catch (...) {

					}
				}

				}));
		}
	}

	void MultithreadedJobQueue::StopThreads() {
		this->_stopRequested = true;
		this->_queueAccessCV.notify_all();

		for (auto& t : this->_threads) {
			t.join();
		}

		this->_threads.clear();
	}
	MultithreadedJobQueue::~MultithreadedJobQueue() {
		this->StopThreads();
	}
}
#pragma once

#include "IDependencyGraphJobQueue.h"

#include <mutex>
#include <queue>
#include <vector>

namespace dependencygraph {

	class PriorityBasedMultithreadedJobQueueJobQueue : public IDependencyGraphJobQueue {
	private:
		std::mutex* _pQueueAccessMutex;
		std::condition_variable* _pConditionVariable;
		std::queue<DependencyGraphJob>* _pJobQueue;

	public:
		PriorityBasedMultithreadedJobQueueJobQueue(std::queue<DependencyGraphJob>* pJobQueue,
			std::mutex* pQueueAccessMutex,
			std::condition_variable* pConditionVariable) :
			_pQueueAccessMutex(pQueueAccessMutex),
			_pConditionVariable(pConditionVariable),
			_pJobQueue(pJobQueue) {
		}

		void RegisterJob(DependencyGraphJob&& job) override
		{
			std::unique_lock<std::mutex> lock(*_pQueueAccessMutex);
			this->_pJobQueue->push(job);
			this->_pConditionVariable->notify_all();
		}
	};

	// Class to demonstrate the separation between a job queue as far as an object context is concerned
	// and the actual running of the supplied jobs.
	//
	// This class allows for the sharing of a single thread pool, but with multiple effective job runners
	// with jobs submitted through the highPriorityJobQueue taking priority over jobs submitted through 
	// lowPriorityJobQueue 
	//
	// This class can be thought of as a template to show how one might have more complicated job 
	// execution code, e.g. one based off properties of the object builder being used (IO vs. CPU bound)
	class PriorityBasedMultithreadedJobQueue {
	private:
		std::vector<std::thread> _threads;

		std::mutex _queueAccessMutex;
		std::condition_variable _queueAccessCV;
	public:
		std::queue<DependencyGraphJob> _jobsHP, _jobsLP;
		std::atomic<int> totalRequests;
	private:
		volatile bool _stopRequested;

	public:
		PriorityBasedMultithreadedJobQueue(int threadCount);

		~PriorityBasedMultithreadedJobQueue();
		void StopThreads();

		std::shared_ptr<IDependencyGraphJobQueue> lowPriorityJobQueue;
		std::shared_ptr<IDependencyGraphJobQueue> highPriorityJobQueue;
	};

	PriorityBasedMultithreadedJobQueue::PriorityBasedMultithreadedJobQueue(int threadCount) :
		_stopRequested(false),
		totalRequests(0) {
		if (threadCount == 0)
			throw std::exception("Invalid thread count specified");

		if (threadCount < 0) {
			// TODO - Read this value in from somewhere
			threadCount = 16;
		}

		this->highPriorityJobQueue = std::make_shared<PriorityBasedMultithreadedJobQueueJobQueue>(&this->_jobsHP, &this->_queueAccessMutex, &this->_queueAccessCV);
		this->lowPriorityJobQueue = std::make_shared<PriorityBasedMultithreadedJobQueueJobQueue>(&this->_jobsHP, &this->_queueAccessMutex, &this->_queueAccessCV);

		for (int i(0); i < threadCount; ++i) {
			_threads.push_back(std::thread([this]() -> void {

				while (true) {
					try {
						if (_stopRequested)
							return;

						std::unique_lock<std::mutex> lock(this->_queueAccessMutex);

						if (!this->_jobsHP.empty()) {
							// We have a high priority job
							auto job = std::move(this->_jobsHP.front());
							this->_jobsHP.pop();
							lock.unlock();

							try
							{
								job.func();
							}
							catch (...) {
								// What to do here?
							}
						}
						else if (!this->_jobsLP.empty()) {
							// We have a low priority job
							auto job = std::move(this->_jobsLP.front());
							this->_jobsLP.pop();
							lock.unlock();

							try
							{
								job.func();
							}
							catch (...) {
								// What to do here?
							}
						}
						else {
							// Nothing to do
							if (_stopRequested)
								return;

							this->_queueAccessCV.wait(lock);
						}
					}
					catch (...) {

					}
				}

				}));
		}
	}

	void PriorityBasedMultithreadedJobQueue::StopThreads() {
		this->_stopRequested = true;
		this->_queueAccessCV.notify_all();

		for (auto& t : this->_threads) {
			t.join();
		}

		this->_threads.clear();
	}

	PriorityBasedMultithreadedJobQueue::~PriorityBasedMultithreadedJobQueue() {
		this->StopThreads();
	}
}
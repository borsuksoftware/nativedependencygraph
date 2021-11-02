// DependencyGraph.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "Dictionary.h"

#include "ObjectContext.h"

#include "FunctionBasedObjectBuilder.h"
#include "ObjectBuilderProvider.h"

// Have a choice of which job queue to use
#include "SingleThreadedJobQueue.h"
#include "MultithreadedJobQueue.h"
#include "PriorityBasedMultithreadedJobQueue.h"

using namespace std::chrono_literals;

#define ITERATIONCOUNT 20000

int main()
{
	//
	// Simple example showing how to use the library 
	//
	// This uses an arbitrary function to perform the 'build operation'. This is desinged to burn compute cycles so that the parallelism can be seen
	std::cout << "Building graph" << std::endl;

	auto obp = std::make_shared<dependencygraph::ObjectBuilderProvider<int, double>>();
	obp->builderProviderFunc = [](const int& address, std::shared_ptr<dependencygraph::IObjectBuilder<int, double>>& pObjectBuilder) -> bool {

		auto funcBasedObjectBuilder = std::make_shared<dependencygraph::FunctionBasedObjectBuilder<int, double>>(
			[](const int& address) {
				std::vector<int> dependencies;

				int dependencyAddress = address / 2;
				while (dependencyAddress > 0) {

					dependencies.push_back(dependencyAddress);
					dependencyAddress /= 2;
				}
				return dependencies;
			},
			[](const int& address, const std::unordered_map<int, double>& dependencies) {
				double result = 0;
				for (int i = 0; i < ITERATIONCOUNT; ++i) {
					auto radians = (double)(((long long)address) * (long long)i);
					result += sin(radians);
				}
				return result;
			});

		pObjectBuilder = std::dynamic_pointer_cast<dependencygraph::IObjectBuilder<int, double>>(funcBasedObjectBuilder);
		return true;
	};


	{
		auto totalStartTime = std::chrono::high_resolution_clock::now();

		// We've chosen to use the priority based approach here so that people can see how they can use more complicated
		// job scheduling algorithms if they like.
		dependencygraph::PriorityBasedMultithreadedJobQueue priorityBasedMultithreadedJobQueue(16);
		auto jobQueue = priorityBasedMultithreadedJobQueue.highPriorityJobQueue;

		dependencygraph::ObjectContext<int, double> objectContext(
			std::dynamic_pointer_cast<dependencygraph::IObjectBuilderProvider<int, double>>(obp),
			jobQueue);

		auto submissionStart = std::chrono::high_resolution_clock::now();
		std::wcout << L"Starting to build objects" << std::endl;
		for (int i = 0; i < 256 * 1024; ++i) {
			objectContext.BuildObject(i);
		}
		auto submissionEnd = std::chrono::high_resolution_clock::now();
		auto submissionTime = submissionEnd - submissionStart;
		std::wcout << L"Requests submitted - " << (submissionTime.count() / 1000000) << "ms" << std::endl;


		std::wcout << L"Starting use of wait handles" << std::endl;
		{
			for (int i = 0; i < 256 * 1024; ++i) {
				auto pOBI = objectContext.BuildObject(i);
				while (true) {
					auto result = pOBI->objectBuiltOrFailureWaitHandle.wait_for(1s);
					if (result == std::cv_status::timeout) {
						std::wcout << L"Waiting on " << i << std::endl;
						std::wcout << L"State " << dependencygraph::ToString(pOBI->getState()) << std::endl;
					}
					else
						break;
				}
			}
		}
		std::wcout << L"Completed wait through wait handles" << std::endl;



		int outstandingCount = 0;
		int handledCount = 0;
		do
		{
			outstandingCount = 0;
			handledCount = 0;

			for (int i = 0; i < 256 * 1024; ++i) {

				auto pOBI = objectContext.GetDependencies(i);
				switch (pOBI->getState()) {
				case dependencygraph::ObjectBuildingState::ObjectBuilt:
				case dependencygraph::ObjectBuildingState::Failure:
					++handledCount;
					break;

				default:
					++outstandingCount;
				}
			}

			if (outstandingCount == 0)
				break;


			std::wcout << L"Job status: " << outstandingCount << L" outstanding job(s); " << handledCount << " processed job(s)." << std::endl;
			if (outstandingCount > 33000)
				std::this_thread::sleep_for(2000ms);
			else
				std::this_thread::sleep_for(500ms);
		} while (outstandingCount > 0);

		auto totalEnd = std::chrono::high_resolution_clock::now();
		auto waitingTime = totalEnd - submissionEnd;
		auto totalTimeTaken = totalEnd - totalStartTime;
		std::wcout << L"Waiting time: " << (waitingTime.count() / 1000000) << L"ms" << std::endl;
		std::wcout << L"Total time taken: " << (totalTimeTaken.count() / 1000000) << L"ms" << std::endl;

		// std::wcout << L"Total jobs submitted: " << jobQueue->totalRequests.load() << std::endl;

		// stop.store(true);
		// t.join();
	}

	std::wcout << L"Object Context gone" << std::endl;

	if (true)
	{
		std::wcout << std::endl;
		std::wcout << L"Single threaded mode" << std::endl;

		auto now = std::chrono::high_resolution_clock::now();
		double output(0);
		for (int i = 0; i < 256 * 1024; ++i) {
			double result = 0;
			for (int loopIdx = 0; loopIdx < ITERATIONCOUNT; ++loopIdx) {
				auto radians = (double)(((long long)i) * (long long)loopIdx);
				result += sin(radians);
			}
			output += result;
		}
		auto endTime = std::chrono::high_resolution_clock::now();
		auto timeTaken = endTime - now;
		std::wcout << L"Total: " << output << std::endl;
		std::wcout << L"Time taken: " << (timeTaken.count() / 1000000) << L"ms" << std::endl;
		std::wcout << L"Time taken (/16): " << (timeTaken.count() / 16000000) << L"ms" << std::endl;
	}

	obp = nullptr;
}
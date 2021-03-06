/*
 * SpotifyRunner.cpp
 *
 *  Created on: Dec 24, 2013
 *      Author: rob
 */

#include "SpotifyRunner.hpp"

#include <stdio.h>
#include <cerrno>

#include "ThreadingUtils.hpp"
#include "TcpConnection.hpp"

namespace fambogie {

SpotifyRunner::SpotifyRunner(sp_session_config& config) :
		mainLoopConditionMutex(true), mainLoopCondition(mainLoopConditionMutex) {
	spotifySession = new SpotifySession(config, this);

	threadShouldStop = false;
	nextTimeout = 0;

	spotifySession->login("USERNAME", "PASSWORD", true);
}

void SpotifyRunner::run() {
	if (mainLoopConditionMutex.lock() != 0) {
		logError("Could not aquire lock: %s %d", __FILE__, __LINE__);
		pthread_exit(0);
	}

	while (!threadShouldStop) {
		int loopTimeout = 100;
		spotifySession->processEvents();
		if (loopTimeout == 0) {
			mainLoopCondition.wait();
			processTasks();
		} else {
			timespec timeout = fambogie::getPthreadTimeout(loopTimeout);
			switch (mainLoopCondition.wait(&timeout)) {
			case 0:
				//Since we got a notification, we try to process everything from the jobqueue.
				processTasks();
				break;
			default:
				break;
			}
		}
	}

	if (mainLoopConditionMutex.unlock() != 0) {
		logError("Could not release lock: %s %d", __FILE__, __LINE__);
		pthread_exit(0);
	}
}

void SpotifyRunner::stop() {
	this->threadShouldStop = true;
	this->notify();
}

void SpotifyRunner::addTask(Task* task) {
	if (mainLoopConditionMutex.lock() != 0) {
		logError("Could not aquire lock: %s %d", __FILE__, __LINE__);
		pthread_exit(0);
	}

	taskList.push_back(task);

	this->notify();

	if (mainLoopConditionMutex.unlock() != 0) {
		logError("Could not release lock: %s %d", __FILE__, __LINE__);
		pthread_exit(0);
	}
}

void SpotifyRunner::processTasks() {
	if (mainLoopConditionMutex.lock() != 0) {
		logError("Could not aquire lock: %s %d", __FILE__, __LINE__);
		pthread_exit(0);
	}

	while (taskList.size() > 0) {
		Task* task = taskList.front();
		taskList.pop_front();
		TcpConnection* client = task->getClient();
		ClientResponse* response = spotifySession->processTask(task);
		if (response != nullptr) {
			client->sendResponse(response);
		}
	}

	if (mainLoopConditionMutex.unlock() != 0) {
		logError("Could not release lock: %s %d", __FILE__, __LINE__);
		pthread_exit(0);
	}
}

void SpotifyRunner::setTcpServer(TcpServer* server) {
	this->tcpServer = server;
}

void SpotifyRunner::broadcastMessage(ClientResponse* response) {
	if(tcpServer != nullptr) {
		tcpServer->broadcastMessage(response);
	}
}

void SpotifyRunner::notify() {
	if (mainLoopCondition.signal() != 0) {
		logError("pthread_cond_signal");
		pthread_exit(0);
	}
}

SpotifyRunner::~SpotifyRunner() {
	if (isRunning()) {
		this->stop();
		this->join();
	}

	delete spotifySession;
}

} /* namespace fambogie */

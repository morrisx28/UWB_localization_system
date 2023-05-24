#ifndef __BLOCK_QUEUE_H_
#define __BLOCK_QUEUE_H_
#include <stdlib.h>
#include <stdio.h>
#include <queue>
#include <mutex>
#include <condition_variable>

#define DELAY_START_BY_CALL -1


template < typename T >
class BlockQueue {
public:
	BlockQueue() : BlockQueue(1000){}
	BlockQueue(int max_size);
	void startPop();
	bool isStartByCall();
	~BlockQueue();
	void push(T& item);
	T pop();
	int size();
	void releaseAllCV();	
private:
	int m_max_size;
	std::queue<T> m_queue;
	bool bExit;
	bool bDelayConsume;
	bool bStartByCall;
	size_t delayNum;
	
	std::condition_variable bufferNotEmpty;
	std::condition_variable bufferNotFull;
	std::mutex bufferLock;
};

template < typename T >
bool BlockQueue<T>::isStartByCall()
{
	return bStartByCall;
}

template < typename T >
BlockQueue<T>::BlockQueue(int max_size)
{
	bExit = false;
	m_max_size = max_size;

	bDelayConsume = false;
	bStartByCall = false;
	delayNum = 0;
}

template < typename T >
void BlockQueue<T>::releaseAllCV()
{
	bExit = true;
	bufferNotFull.notify_one();
	bufferNotEmpty.notify_one();
}

template < typename T>
void BlockQueue<T>::startPop(){
	bufferNotEmpty.notify_one();
}

template < typename T >
int BlockQueue<T>::size()
{
	int queue_size;
	bufferLock.lock();
	queue_size = m_queue.size();
	bufferLock.unlock();
	return queue_size;

}

template < typename T >
BlockQueue<T>::~BlockQueue()
{
	releaseAllCV();
}

template < typename T >
void BlockQueue<T>::push(T& item)
{
	std::unique_lock<std::mutex> lock(bufferLock);

	while (m_queue.size() == m_max_size && bExit == false){
		// Buffer is full - sleep so consumers can get items.
		bufferNotFull.wait(lock);
	}

	if (bExit){
		lock.unlock();
		return;
	}


	// Insert the item at the end of the queue and increment size.
	m_queue.push(item);
	int queue_size = m_queue.size();
	lock.unlock();
	if (bDelayConsume){
		if ( queue_size >= delayNum ){
			// If a consumer is waiting, wake it.
			bufferNotEmpty.notify_one();
		}	
	} else {
		bufferNotEmpty.notify_one();
	}
}

template < typename T >
T BlockQueue<T>::pop()
{
	std::unique_lock<std::mutex> lock(bufferLock);
	
	while (m_queue.size() == 0 && bExit == false){		
		// Buffer is empty - sleep so producers can create items.
		bufferNotEmpty.wait(lock);
	}

	if (bExit && m_queue.size() == 0){
		lock.unlock();
		T item{}; 
		return item;
	} 

	// Consume the first available item.
	T item = m_queue.front();
	m_queue.pop();	

	lock.unlock();

	// If a producer is waiting, wake it.
	bufferNotFull.notify_one();
	return item;
}
#endif //__BLOCK_QUEUE_H_
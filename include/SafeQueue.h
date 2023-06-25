#ifndef _SAFE_QUEUE_H_
#define _SAFE_QUEUE_H_

#include <queue>
#include <mutex>
#include <condition_variable>


template <typename T>
class SafeQueue{
public:
	bool empty() {
		std::unique_lock<std::mutex> lock(mutex_);
		return queue_.empty();
	}
	size_t size() {
		std::unique_lock<std::mutex> lock(mutex_);
		return queue_.size();
	}
	void push(const T & item) {
		{
			std::unique_lock<std::mutex> lock(mutex_);
			queue_.emplace(item);
		}
		condition_.notify_one();
	}
	void push(T && item){
		{
			std::unique_lock<std::mutex> lock(mutex_);
			queue_.emplace(std::move(item));
		}
		condition_.notify_one();
	}
	bool pop(T & item) {
		std::unique_lock<std::mutex> lock(mutex_); 
        if (queue_.empty()){
            return false;
        }
        item = std::move(queue_.front()); 
        queue_.pop(); 
        return true;
	}

	bool wait_pop(T & item) {
		std::unique_lock<std::mutex> lock(mutex_); 
		condition_.wait(lock, [this](){ return !queue_.empty() || stop; });
        if (queue_.empty()){
            return false;
        }
        item = std::move(queue_.front()); 
        queue_.pop(); 
        return true;
	}

	bool try_pop(T & item) {
		std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
		if(!lock.owns_lock() || queue_.empty()) {
			return false;
		}
		item = std::move(queue_.front());
		queue_.pop();
		return true;
	}

	void destory(){
		{
			std::unique_lock<std::mutex> lock(mutex_);
			stop = true;
		}
		condition_.notify_all();
	}
private:
	std::queue<T> queue_;
	std::mutex mutex_;
	std::condition_variable condition_;
	bool stop = false;
};

#endif
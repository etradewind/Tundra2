// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "Foundation.h"
#include "ThreadTask.h"
#include "ThreadTaskManager.h"

namespace Foundation
{
    ThreadTask::ThreadTask(const std::string& task_description) :
        task_description_(task_description),
        task_manager_(0),
        keep_running_(true),
        running_(false),
        finished_(false)
    {
    }

    ThreadTask::~ThreadTask()
    {
        Stop();
    }

    void ThreadTask::Stop()
    {
        keep_running_ = false;
        request_condition_.notify_one();
        
        thread_.join();
    }

    void ThreadTask::AddRequest(ThreadTaskRequestPtr request)
    {
        if (request)
        {
            if (!running_)
            {
                thread_.join(); // Make sure it's really stopped, not just set the flag to false
                requests_.push_back(request);
                running_ = true;
                finished_ = false;
                thread_ = boost::thread(boost::ref(*this));
            }
            else
            {
                Core::MutexLock lock(request_mutex_);
                requests_.push_back(request);
            }
            request_condition_.notify_one();
        }
    }

    ThreadTaskResultPtr ThreadTask::GetResult() const
    {
        if (!finished_)
            return ThreadTaskResultPtr();
        
        return result_;
    }

    void ThreadTask::operator() ()
    {
        Work();
        running_ = false;
        finished_ = true;
    }
    
    bool ThreadTask::WaitForRequests()
    {
        Core::ScopedLock lock(request_mutex_);
        while (requests_.empty() && keep_running_)
        {
            request_condition_.wait(lock);
        }
        
        return (!requests_.empty());
    }
    
    ThreadTaskRequestPtr ThreadTask::GetNextRequest()
    {
        ThreadTaskRequestPtr request;
        
        Core::MutexLock lock(request_mutex_);
        if (!requests_.empty())
        {
            request = *requests_.begin();
            requests_.pop_front();
        }
        
        return request;
    }
    
    void ThreadTask::SetResult(ThreadTaskResultPtr result)
    {
        if (result)
        {
            Core::MutexLock lock(result_mutex_);
            result->task_description_ = task_description_;
            result_ = result;
        }
    }
    void ThreadTask::QueueResult(ThreadTaskResultPtr result)
    {
        if (result)
        {
            if (task_manager_)
            {
                result->task_description_ = task_description_;
                task_manager_->QueueResult(result);
            }
            else
            {
                RootLogError("Thread task manager not set, can not queue result");
            }
        }
    }
}

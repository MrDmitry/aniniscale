/*
* Copyright (c) 2017 Dmitry Odintsov
* This code is licensed under the MIT license (MIT)
* (http://opensource.org/licenses/MIT)
*/

#ifndef ANINISCALE_WORKER_POOL_HPP
#define ANINISCALE_WORKER_POOL_HPP

#include "WorkerPool.hpp"

#include <vips/vips8>

#include <functional>
#include <list>
#include <mutex>
#include <vector>

//! Described pool of thread workers
class WorkerPool
{
public:
    //! Shortcut to worker's routine task type
    typedef std::function<void(WorkerPool&)> Task;

    WorkerPool(uint32_t bandCount, uint32_t x_blockSize, uint32_t y_blockSize);

    //! Worker's routine
    void Worker();

    /** @brief  Process portion of an image
     *
     *  @param[in]  img image to process
     *  @param[out] out buffer to store processing results
     */
    void ProcessImage(vips::VImage img, std::vector<uint8_t>& out);

    /** @brief  Pushes task to queue
     *
     *  @attention  this method is not thread safe and shall be called
     *              before any worker is initialized
     *
     *  @param  task    task to be performed by a worker
     */
    void PushTask(Task task);

private:
    //! Mutex protecting access to @p m_tasks
    std::mutex m_mutex;

    //! A list of tasks to be processed
    std::list<Task> m_tasks;

    //! Image band count
    uint32_t m_bandCount;

    //! Block size
    uint32_t m_x_blockSize;
    uint32_t m_y_blockSize;
};

#endif // ANINISCALE_WORKER_POOL_HPP

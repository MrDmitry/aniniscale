/*
* Copyright (c) 2017 Dmitry Odintsov
* This code is licensed under the MIT license (MIT)
* (http://opensource.org/licenses/MIT)
*/

#include "WorkerPool.hpp"
#include "Reporter.hpp"

#include <map>

WorkerPool::WorkerPool(uint32_t bandCount, uint32_t x_blockSize, uint32_t y_blockSize)
    : m_bandCount(bandCount)
    , m_x_blockSize(x_blockSize)
    , m_y_blockSize(y_blockSize)
{

}

void WorkerPool::Worker()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    while (!m_tasks.empty())
    {
        //! Before you start working - report current time status
        Reporter::ProgressReport(m_tasks.size());

        //! Take a task
        Task task = m_tasks.front();
        m_tasks.pop_front();

        //! Let other workers take tasks as well
        lock.unlock();

        //! Complete your task
        task(*this);

        //! Rinse and repeat
        lock.lock();
    }
}

void WorkerPool::ProcessImage(vips::VImage img, std::vector<uint8_t>& out)
{
    //! Get image dimensions
    const uint32_t width = img.width();
    const uint32_t height = img.height();

    //! Calculate tile count (== pixels in the end result)
    const uint32_t x_tiles = width / m_x_blockSize;
    const uint32_t y_tiles = height / m_y_blockSize;

    //! Calculate size of each tile
    const uint32_t size = m_x_blockSize * m_y_blockSize;

    //! Calculate color threshold - if color has this much, it is dominating
    const uint32_t win = size / 2;

    //! Get image pixel data
    const uint8_t* imgPixels = reinterpret_cast<const uint8_t*>(img.data());

    //! Iterate over all tiles
    for (uint32_t x = 0; x < x_tiles; ++x)
    {
        for (uint32_t y = 0; y < y_tiles; ++y)
        {
            //! Find dominant color
            // @TODO: there must be some better way to do this. Histograms?
            std::map<uint32_t, uint32_t> colors;
            const uint8_t* dominant = 0;
            uint32_t domCount = 0;

            //! Iterate over all pixels in original area
            for (uint32_t areaX = 0; areaX < m_x_blockSize; ++areaX)
            {
                for (uint32_t areaY = 0; areaY < m_y_blockSize; ++areaY)
                {
                    // Get current pixel data
                    const uint8_t* pixel = imgPixels + (
                        (areaY + y * m_y_blockSize) * width + areaX + x * m_x_blockSize
                    ) * m_bandCount;

                    uint32_t color = 0;

                    //! Get color value
                    for (uint32_t b = 0; b < m_bandCount; ++b)
                    {
                        color |= pixel[b] << ((m_bandCount - 1 - b) * 8);
                    }

                    //! Increase the number of votes for that color and check if it's dominating
                    colors[color] += 1;

                    if (domCount < colors[color])
                    {
                        domCount = colors[color];
                        dominant = pixel;

                        if (domCount >= win)
                        {
                            break;
                        }
                    }
                }
            }

            //! Paint the resulting pixel with dominant color
            for (uint32_t b = 0; b < m_bandCount; ++b)
            {
                out[(y * x_tiles + x) * m_bandCount + b] = *(dominant + b);
            }
        }
    }
}

void WorkerPool::PushTask(Task task)
{
    m_tasks.push_back(task);
}

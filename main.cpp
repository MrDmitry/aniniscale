/*
* Copyright (c) 2017 Dmitry Odintsov
* This code is licensed under the MIT license (MIT)
* (http://opensource.org/licenses/MIT)
*/

/*
 *  Usage: aniniscale <x_factor> <y_factor> <in_image> <out_image>
 */

#include <vips/vips8>

#include <cstdlib>
#include <ctime>
#include <future>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <thread>

using namespace vips;

static int x_factor = 1;
static int y_factor = 1;

static int bandCount = 3;

static int taskPixels = 0;
static int totalPixels = 0;

//! Each task shall be comprised of no more than 64 by 64 sections
static int taskSectionSide = 64;

static const std::time_t startTime = std::time(0);
static volatile std::time_t progressReport = std::time(0);

//! Prints out elapsed time at most once per 5 seconds
void ReportElapsedTime()
{
    std::time_t elapsed = std::time(0) - startTime;

    char buffer[9];
    if (std::strftime(buffer, sizeof(buffer), "%H:%M:%S", std::gmtime(&elapsed)))
    {
        std::cout << "/" << std::setfill('-') << std::setw(25) << "\\" << std::endl;
        std::cout << "| Time elapsed: " << buffer << " |" << std::endl;
        std::cout << "\\" << std::setfill('-') << std::setw(25) << "/" << std::endl;
    }
    else
    {
        std::cout << "Failed to convert data ¯\\_(ツ)_/¯" << std::endl;
    }
}

//! Prints out ETA at most once per 5 seconds
void EstimateTimeLeft(uint32_t tasksLeft)
{
    std::time_t elapsed = std::time(0) - startTime;

    uint32_t pixelsLeft = tasksLeft * taskPixels;

    double perSecond = ((double)(totalPixels - pixelsLeft)) / ((double)elapsed);

    std::time_t eta = static_cast<uint32_t>(((double)pixelsLeft) / perSecond);

    char buffer[9];
    if (std::strftime(buffer, sizeof(buffer), "%H:%M:%S", std::gmtime(&eta)))
    {
        std::cout << "/" << std::setfill('-') << std::setw(16) << "\\" << std::endl;
        std::cout << "| ETA: " << buffer << " |" << std::endl;
        std::cout << "\\" << std::setfill('-') << std::setw(16) << "/" << std::endl;
    }
    else
    {
        std::cout << "Failed to convert data ¯\\_(ツ)_/¯" << std::endl;
    }
}

void ProgressReport(uint32_t tasksLeft)
{
    if (std::time(0) - progressReport < 5)
    {
        return;
    }

    std::cout << std::endl;

    ReportElapsedTime();
    EstimateTimeLeft(tasksLeft);

    progressReport = std::time(0);

    std::cout << "Tasks left: " << tasksLeft << " (" << tasksLeft * taskPixels << "px)" << std::endl;
}

//! Described pool of thread workers
class WorkerPool
{
public:
    typedef std::function<void()> Task;

    //! Worker's routine
    void Worker()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        std::cout << "Worker ready " << std::this_thread::get_id() << std::endl;

        while (!m_tasks.empty())
        {
            //! Before you start working - report current time status
            ProgressReport(m_tasks.size());

            //! Take a task
            Task task = m_tasks.front();
            m_tasks.pop_front();

            //! Let other workers take tasks as well
            lock.unlock();

            //! Complete your task
            task();

            //! Rinse and repeat
            lock.lock();
        }

        std::cout << "Worker done " << std::this_thread::get_id() << std::endl;
    }

    //! Pushes task to queue (not thread safe, shall be done before workers are initialized)
    void PushTask(Task task)
    {
        m_tasks.push_back(task);
    }

private:
    std::mutex m_mutex;

    std::list<Task> m_tasks;
};

//! Task at hand - process portion of an image
void work(VImage img, std::vector<uint8_t>& out)
{
    //! Get image dimensions
    const uint32_t width = img.width();
    const uint32_t height = img.height();

    //! Calculate tile count (== pixels in the end result)
    const uint32_t x_tiles = width / x_factor;
    const uint32_t y_tiles = height / y_factor;

    //! Calculate size of each tile
    const uint32_t size = x_factor * y_factor;

    //! Calculate color threshold - if color has this much, it is dominating
    const uint32_t win = size / 2;

    //! Get pixel data
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
            for (int areaX = 0; areaX < x_factor; ++areaX)
            {
                for (int areaY = 0; areaY < y_factor; ++areaY)
                {
                    const uint8_t* pixel = imgPixels + (
                        (areaY + y * y_factor) * width + areaX + x * x_factor
                        ) * bandCount;

                    uint32_t color = 0;

                    //! Get color value
                    for (int b = 0; b < bandCount; ++b)
                    {
                        color |= pixel[b] << ((bandCount - 1 - b) * 8);
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
            for (int b = 0; b < bandCount; ++b)
            {
                out[(y * x_tiles + x) * bandCount + b] = *(dominant + b);
            }
        }
    }
}

int main(int argc, char** argv)
{
    //! Initialize VIPS library
    if( VIPS_INIT( argv[0] ) )
    {
        return -1;
    }

    //! Check if we have enough arguments
    // @TODO: Argument correctness check? If arguments are messed up it may behave unexpectedly
    if (argc < 5)
    {
        std::cout << "Not enough arguments" << std::endl;
        std::cout << "Usage: aniniscale <x_factor> <y_factor> <in_image> <out_image> [<task_side>]" << std::endl;
        return -1;
    }

    //! Get factors
    x_factor = atoi(argv[1]);
    y_factor = atoi(argv[2]);

    //! If any factor is < 1 we're not going to do a thing
    if (x_factor < 1 || y_factor < 1)
    {
        return -1;
    }

    if (argc > 5)
    {
        taskSectionSide = atoi(argv[5]);
    }

    //! Open the image and check channel count
    VImage img = VImage::new_from_file( argv[3] );
    bandCount = img.bands();

    //! If both factors are 1, we can just save the image
    if (x_factor == 1 && y_factor == 1)
    {
        img.pngsave(argv[4]);
        return 0;
    }

    //! Get image information and estimate how it will be divided
    const uint32_t width = img.width();
    const uint32_t height = img.height();
    const uint32_t x_tiles = width / x_factor;
    const uint32_t y_tiles = height / y_factor;

    totalPixels = width * height;

    //! Check how many threads we can run
    uint32_t workerCount = std::thread::hardware_concurrency();

    //! Make sure it's a multiple of 2
    if (workerCount % 2 != 0)
    {
        workerCount += workerCount % 2;
    }

    //! If we have too much workers, cut their number until we have enough work
    while (workerCount > x_tiles || workerCount > y_tiles)
    {
        workerCount -= 2;
    }

    //! If we ended up without workers, bring back one
    if (0 == workerCount)
    {
        workerCount = 1;
    }

    //! Tasks shall not be too big, so we keep them manageable
    int x_section = x_tiles / workerCount;

    while (x_section > taskSectionSide)
    {
        x_section /= 2;
    }

    int y_section = y_tiles / workerCount;

    while (y_section > taskSectionSide)
    {
        y_section /= 2;
    }

    const uint32_t x_sectionSize = x_section * x_factor;
    const uint32_t y_sectionSize = y_section * y_factor;

    const uint32_t x_sectionCount = width / x_sectionSize;
    const uint32_t y_sectionCount = height / y_sectionSize;

    //! Store total number of pixels on the image for the reporting
    taskPixels = x_sectionSize * y_sectionSize;

    WorkerPool pool;

    std::cout << "Creating " << x_sectionCount * y_sectionCount << " tasks of size " << x_sectionSize << "x" << y_sectionSize << std::endl;

    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> result;

    //! Allocate all needed memory
    for (uint32_t x_task = 0; x_task < x_sectionCount; ++x_task)
    {
        for (uint32_t y_task = 0; y_task < y_sectionCount; ++y_task)
        {
            result[std::pair<uint32_t, uint32_t>(x_task, y_task)].resize(x_section * y_section * bandCount);
        }
    }

    auto ReportTaskCreationProgress = [&](uint32_t count){
        static std::time_t lastTime = std::time(0);
        const std::time_t now = std::time(0);

        if (now - lastTime > 5)
        {
            std::cout << "Progress: " << count << "/" << result.size() << std::endl;
            lastTime = now;
        }
    };

    //! Create tasks to process each section
    for (uint32_t x_task = 0; x_task < x_sectionCount; ++x_task)
    {
        for (uint32_t y_task = 0; y_task < y_sectionCount; ++y_task)
        {
            std::pair<uint32_t, uint32_t> coords(x_task, y_task);

            VImage area = img.extract_area(coords.first * x_sectionSize,
                coords.second * y_sectionSize,
                x_sectionSize,
                y_sectionSize);

            pool.PushTask([=, &result](){ work(area, result[std::pair<uint32_t, uint32_t>(x_task, y_task)]); });

            ReportTaskCreationProgress(x_task * y_sectionCount + y_task);
        }
    }

    std::cout << "Task creation complete" << std::endl;

    std::cout << "Total area to be processed: " << width << "x" << height << " (" << totalPixels << "px)" << std::endl;

    //! Spawn workers
    std::vector<std::thread> workers;
    workers.resize(workerCount);
    for (uint32_t i = 0; i < workerCount; ++i)
    {
        workers[i] = std::thread(&WorkerPool::Worker, &pool);
    }

    //! Wait for all workers to finish
    for (uint32_t i = 0; i < workerCount; ++i)
    {
        workers[i].join();
    }

    workers.clear();

    //! Prepare the buffer to store final output
    std::vector<uint8_t> outBuffer;
    outBuffer.resize(x_tiles * y_tiles * bandCount);

    VImage out = VImage::new_from_memory(outBuffer.data(), outBuffer.size(),
        x_tiles, y_tiles, bandCount, img.format());

    //! Go through worker results and place them in resulting image
    for (auto& r : result)
    {
        const std::pair<uint32_t, uint32_t>& coords = r.first;
        std::vector<uint8_t>& buffer = r.second;
        VImage block = VImage::new_from_memory(buffer.data(), buffer.size(),
            x_section, y_section, bandCount, img.format());

        out = out.insert(block, coords.first * x_section, coords.second * y_section);
    }

    //! Save the image
    out.pngsave(argv[4]);

    //! Deinitialize
    vips_shutdown();

    //! Report elapsed time
    ReportElapsedTime();

    return 0;
}

/*
* Copyright (c) 2017 Dmitry Odintsov
* This code is licensed under the MIT license (MIT)
* (http://opensource.org/licenses/MIT)
*/

#include <vips/vips8>

#include <cstdlib>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

#include "Reporter.hpp"
#include "WorkerPool.hpp"

static const char* s_appName = "aniniscale";
static const char* s_versionInfo = "1.0.1";

struct Arguments
{
    // Required
    std::string in;
    std::string out;

    // Optional
    int x_blockSize = 8;
    int y_blockSize = 8;
    int taskBlockSide = 64;
    int reportingTimeout = 5;
    bool help = false;

    //! Returns the validity of argument set
    bool IsValid() const
    {
        // arguments are valid only if:
        return !in.empty() && !out.empty() &&   // both in and out are set
            !help &&                            // -h/--help is not set
            x_blockSize >= 1 && y_blockSize >= 1;       // x and y block sizes are positive integers
    }
};

void PrintUsage(const Arguments& arguments)
{
    if (!arguments.help && !arguments.IsValid())
    {
        if (arguments.in.empty())
        {
            std::cout << "Input image path is required!" << std::endl;
        }

        if (arguments.out.empty())
        {
            std::cout << "Output image path is required!" << std::endl;
        }

        if (arguments.x_blockSize < 1)
        {
            std::cout << "-x/--x-block must be a positive integer" << std::endl;
        }

        if (arguments.y_blockSize < 1)
        {
            std::cout << "-y/--y-block must be a positive integer" << std::endl;
        }

        std::cout << std::endl;
    }

    if (arguments.help)
    {
        std::cout << s_appName << " v" << s_versionInfo << std::endl;
        std::cout << "Downscales image by reducing blocks in original image to a single pixel of dominant color." << std::endl;
        std::cout << std::endl;

        std::cout << "Divides given image into multiple task areas. The size of each area is determined by a number of factors:" << std::endl;
        std::cout << "  - initial image size;" << std::endl;
        std::cout << "  - number of processing threads;" << std::endl;
        std::cout << "  - block size;" << std::endl;
        std::cout << "  - number of blocks inside each task." << std::endl;
        std::cout << std::endl;
        std::cout << "Each task consists of the following steps:" << std::endl;
        std::cout << "  1. Pick a block of pixels" << std::endl;
        std::cout << "  2. Find dominant color in this block" << std::endl;
        std::cout << "  3. Write dominant color to resulting image" << std::endl;
        std::cout << "After all tasks are complete, resulting image is saved as png" << std::endl;
        std::cout << std::endl;
    }

    std::cout << "Usage:" << std::endl;
    std::cout << s_appName << " [options] -i/--input INPUT -o/--output OUTPUT" << std::endl;
    std::cout << std::endl;

    const uint32_t requiredWidth = 32;
    std::cout << "Required arguments:" << std::endl;
    std::cout << "  " << std::left << std::setw(requiredWidth) << "-i INPUT, --input=INPUT" << "path to input image" << std::endl;
    std::cout << "  " << std::left << std::setw(requiredWidth) << "-o OUTPUT, --output=OUTPUT" << "path to output image" << std::endl;
    std::cout << std::endl;

    Arguments defaultArgs;

    const uint32_t optionalWidth = 32;
    std::cout << "Optional arguments:" << std::endl;
    std::cout << "  " << std::left << std::setw(optionalWidth) << "-h, --help" << "prints detailed help message" << std::endl;
    std::cout << "  " << std::left << std::setw(optionalWidth) << "-x NUM, --x-block=NUM" << "block size on X axis [default " << defaultArgs.x_blockSize << "]" << std::endl;
    std::cout << "  " << std::left << std::setw(optionalWidth) << "-y NUM, --y-block=NUM" << "block size on Y axis [default " <<  defaultArgs.y_blockSize << "]" << std::endl;
    std::cout << "  " << std::left << std::setw(optionalWidth) << "-t NUM, --task-block-side=NUM" << "maximum number of blocks in any processing task [default " << defaultArgs.taskBlockSide << "]" << std::endl;
    std::cout << "  " << std::left << std::setw(optionalWidth) << "-r NUM, --reporting-timeout=NUM" << "minimum timeout between log reports in seconds [default " << defaultArgs.reportingTimeout << "]" << std::endl;
}

Arguments ProcessArgs(int argc, char** argv)
{
    static struct option options[] = {
        {"x-block", required_argument, 0, 'x'},
        {"y-block", required_argument, 0, 'y'},

        {"input", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},

        {"task-block-side", required_argument, 0, 't'},

        {"reporting-timeout", required_argument, 0, 'r'},

        {"help", no_argument, 0, 'h'},

        {0, 0, 0, 0}
    };

    Arguments arguments;

    while (true)
    {
        int c = getopt_long(argc, argv, "x:y:i:o:t:r:h", options, 0);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'x': // x-block
            {
                arguments.x_blockSize = atoi(optarg);
                break;
            }
            case 'y': // y-block
            {
                arguments.y_blockSize = atoi(optarg);
                break;
            }
            case 'i': // input
            {
                arguments.in = std::string(optarg);
                break;
            }
            case 'o': // output
            {
                arguments.out = std::string(optarg);
                break;
            }
            case 't': // task-block-side
            {
                arguments.taskBlockSide = atoi(optarg);
                break;
            }
            case 'r': // reporting-timeout
            {
                arguments.reportingTimeout = atoi(optarg);
                break;
            }
            case 'h': // help
            {
                arguments.help = true;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return arguments;
}

int Process(const Arguments& arguments)
{
    //! Open the image and check channel count
    vips::VImage img;

    try
    {
        img = vips::VImage::new_from_file( arguments.in.c_str() );
    }
    catch( vips::VError& e )
    {
        std::cout << "Error occured while opening image " << arguments.in.c_str() << std::endl;
        std::cerr << e.what() << std::endl;
        return -1;
    }

    int bandCount = img.bands();

    //! If both blocks are 1, we can just save the image
    if (arguments.x_blockSize == 1 && arguments.y_blockSize == 1)
    {
        img.pngsave( (char*) arguments.out.c_str() );
        return 0;
    }

    //! Get image information and estimate how it will be divided
    const uint32_t width = img.width();
    const uint32_t height = img.height();

    const uint32_t x_tiles = width / arguments.x_blockSize;
    const uint32_t y_tiles = height / arguments.y_blockSize;

    uint32_t totalPixels = width * height;

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
    int x_sectionsInTask = x_tiles / workerCount;

    while (x_sectionsInTask > arguments.taskBlockSide)
    {
        x_sectionsInTask /= 2;
    }

    int y_sectionsInTask = y_tiles / workerCount;

    while (y_sectionsInTask > arguments.taskBlockSide)
    {
        y_sectionsInTask /= 2;
    }

    const uint32_t x_taskSize = x_sectionsInTask * arguments.x_blockSize;
    const uint32_t y_taskSize = y_sectionsInTask * arguments.y_blockSize;

    const uint32_t x_taskCount = width / x_taskSize;
    const uint32_t y_taskCount = height / y_taskSize;

    //! Store total number of pixels on the image for the reporting
    Reporter::s_taskPixels = x_taskSize * y_taskSize;
    Reporter::s_tasksTotal = x_taskCount * y_taskCount;

    WorkerPool pool(bandCount, arguments.x_blockSize, arguments.y_blockSize);

    std::cout << "Creating " << x_taskCount * y_taskCount << " tasks of size " << x_taskSize << "x" << y_taskSize << std::endl;

    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> result;

    //! Allocate all needed memory
    for (uint32_t x_task = 0; x_task < x_taskCount; ++x_task)
    {
        for (uint32_t y_task = 0; y_task < y_taskCount; ++y_task)
        {
            result[std::pair<uint32_t, uint32_t>(x_task, y_task)].resize(x_sectionsInTask * y_sectionsInTask * bandCount);
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
    for (uint32_t x_task = 0; x_task < x_taskCount; ++x_task)
    {
        for (uint32_t y_task = 0; y_task < y_taskCount; ++y_task)
        {
            std::pair<uint32_t, uint32_t> coords(x_task, y_task);

            vips::VImage area = img.extract_area(coords.first * x_taskSize,
                coords.second * y_taskSize,
                x_taskSize,
                y_taskSize);

            pool.PushTask([=, &result](WorkerPool& worker){ worker.ProcessImage(area, result[std::pair<uint32_t, uint32_t>(x_task, y_task)]); });

            ReportTaskCreationProgress(x_task * y_taskCount + y_task);
        }
    }

    std::cout << "Task creation complete" << std::endl;

    std::cout << "Total area to be processed: " << width << "x" << height << " (" << totalPixels << "px)" << std::endl;

    //! Spawn workers
    std::vector<std::thread> workers;
    workers.resize(workerCount);

    std::cout << "Initializing " << workerCount << " workers" << std::endl;

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

    std::cout << "Processing complete, preparing resulting image" << std::endl;

    //! Prepare the buffer to store final output
    std::vector<uint8_t> outBuffer;
    outBuffer.resize(x_tiles * y_tiles * bandCount);

    vips::VImage outImg = vips::VImage::new_from_memory(outBuffer.data(), outBuffer.size(),
        x_tiles, y_tiles, bandCount, img.format());

    try
    {
        //! Go through worker results and place them in resulting image
        for (auto& r : result)
        {
            const std::pair<uint32_t, uint32_t>& coords = r.first;
            std::vector<uint8_t>& buffer = r.second;
            vips::VImage block = vips::VImage::new_from_memory(buffer.data(), buffer.size(),
                x_sectionsInTask, y_sectionsInTask, bandCount, img.format());

            outImg = outImg.insert(block, coords.first * x_sectionsInTask, coords.second * y_sectionsInTask);
        }
    }
    catch( vips::VError& e )
    {
        std::cout << "Error occured while preparing resulting image" << std::endl;
        std::cerr << e.what() << std::endl;
        return -1;
    }

    try
    {
        std::cout << "Saving resulting image" << std::endl;

        //! Save the image
        outImg.pngsave( (char*) arguments.out.c_str() );
    }
    catch( vips::VError& e )
    {
        std::cout << "Error occured while saving resulting image to " << arguments.out.c_str() << std::endl;
        std::cerr << e.what() << std::endl;
        return -1;
    }

    return 0;
}

int main(int argc, char** argv)
{
    //! Initialize VIPS library
    if( VIPS_INIT( argv[0] ) )
    {
        return -1;
    }

    std::string in;
    std::string out;
    bool help = false;

    Arguments arguments = ProcessArgs(argc, argv);

    if (!arguments.IsValid())
    {
        PrintUsage(arguments);

        return help ? 0 : -1;
    }

    int retVal = Process(arguments);

    //! Deinitialize
    vips_shutdown();

    if (0 == retVal)
    {
        //! Report elapsed time
        Reporter::ReportElapsedTime();
    }

    return retVal;
}

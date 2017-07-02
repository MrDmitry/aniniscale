/*
* Copyright (c) 2017 Dmitry Odintsov
* This code is licensed under the MIT license (MIT)
* (http://opensource.org/licenses/MIT)
*/

#include "Reporter.hpp"

#include <ctime>
#include <iostream>
#include <iomanip>

static const std::time_t startTime = std::time(0);
static volatile std::time_t progressReport = std::time(0);

uint32_t Reporter::s_minTimeout = 5;
uint32_t Reporter::s_tasksTotal = 0;
uint32_t Reporter::s_taskPixels = 0;

void Reporter::ReportElapsedTime()
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

void Reporter::EstimateTimeLeft(uint32_t tasksLeft)
{
    std::time_t elapsed = std::time(0) - startTime;

    double perSecond = ((double)(s_tasksTotal - tasksLeft)) / ((double)elapsed);

    std::time_t eta = static_cast<uint32_t>(((double)tasksLeft) / perSecond);

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

void Reporter::ProgressReport(uint32_t tasksLeft)
{
    if (std::time(0) - progressReport < s_minTimeout)
    {
        return;
    }

    std::cout << std::endl;

    ReportElapsedTime();
    EstimateTimeLeft(tasksLeft);

    progressReport = std::time(0);

    std::cout << "Tasks left: " << tasksLeft << " (" << tasksLeft * s_taskPixels << "px)" << std::endl;
}

/*
* Copyright (c) 2017 Dmitry Odintsov
* This code is licensed under the MIT license (MIT)
* (http://opensource.org/licenses/MIT)
*/

#ifndef ANINISCALE_REPORTER_HPP
#define ANINISCALE_REPORTER_HPP

#include <cstdint>

/** @brief  Prints information regarding runtime to std::cout */
class Reporter
{
public:
    //! Minimum timeout between ProgressReport() printouts
    static uint32_t s_minTimeout;

    //! Total number of tasks
    static uint32_t s_tasksTotal;

    //! Number of pixels in one task
    static uint32_t s_taskPixels;

    //! Prints elapsed time
    static void ReportElapsedTime();

    /** @brief  Prints ETA
     *
     *  Estimates how many time is required to finish remaining tasks
     *
     *  @param  tasksLeft   amount of remaining tasks
     */
    static void EstimateTimeLeft(uint32_t tasksLeft);

    /** @brief  Reports current progress
     *
     *  Calls ReportElapsedTime(), EstimateTimeLeft() and prints how many
     *  tasks are left to be processed
     *
     *  @param  tasksLeft   amount of remaining tasks
     */
    static void ProgressReport(uint32_t tasksLeft);
};

#endif // ANINISCALE_REPORTER_HPP

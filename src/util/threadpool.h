/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#ifndef NGSTHREADPOOL_H
#define NGSTHREADPOOL_H

#include <list>

#include "cpl_multiproc.h"

namespace ngs {

/**
 * @brief The ThreadData class Data for thread
 */
class ThreadData
{
public:
    ThreadData(bool own);
    bool isOwn() const { return m_own; }

protected:
    bool m_own;
};



/**
 * @brief The ThreadPool class Pool of threads simultaniously executed
 */
class ThreadPool
{
    typedef void (*poolThreadFunction)(ThreadData*);
public:
    ThreadPool();
    ~ThreadPool();
    void init(unsigned char numThreads, poolThreadFunction function);
    void addThreadData(ThreadData* data);
    void clearThreadData();

protected:
    bool process();
    void finished();
    void newWorker();

    // static
    static void threadFunction(void *threadData);

protected:
    std::list<ThreadData*> m_threadData;
    CPLMutex *m_dataMutex, *m_threadMutex;
    poolThreadFunction m_function;
    unsigned char m_maxThreadCount, m_threadCount;
};

}

#endif // NGSTHREADPOOL_H
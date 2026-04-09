/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifdef _LINUX_VENV_
#include <unistd.h>
#endif

#include "EventHandler.h"
#include "VidcLog.h"

using namespace early_video_app;
using namespace std::chrono_literals;

EventHandler::EventHandler(std::shared_ptr<V4l2Codec> codec) : mV4l2Codec(codec) {
    VIDC_MED("EventHandler, contructor\n");
}

EventHandler::~EventHandler() {
    VIDC_MED("EventHandler, destructor\n");
}

void EventHandler::threadLoop() {
    int rc = 0;

    VIDC_MED("EventHandler::threadLoop, enter\n");
    mEventThreadRunning = true;

    while (!mEventThreadExit) {
        std::shared_ptr<Event> event = nullptr;
        {
            std::unique_lock<std::mutex> lock(mEventQueueLock);
            if (mEvents.size() == 0) {
                std::cv_status ret = mEventQueueCondition.wait_for(lock, std::chrono::seconds(2));
                if (ret == std::cv_status::timeout)
                    continue;
                }
            if (mEvents.size() == 0) {
                VIDC_MED("EventHandler::threadLoop, No events available to process\n");
                continue;
            }
            event = mEvents.front();
            mEvents.pop_front();
        }
        if (!event) {
            VIDC_ERR("EventHandler::threadLoop, invalid event\n");
            break;
        }
        VIDC_MED("EventHandler::threadLoop, processing event %ld\n", event->id);
        switch (event->id) {
            case EVENT_CONFIGURE_INPUT: {
                rc = mV4l2Codec->configureInput();
                if (rc)
                    break;
                break;
            }
            case EVENT_CONFIGURE_OUTPUT: {
                rc = mV4l2Codec->configureOutput();
                if (rc)
                    break;
                break;
            }
            case EVENT_RECONFIGURE: {
                rc = mV4l2Codec->reconfigureOutput();
                if (rc)
                    break;
                break;
            }
            case EVENT_START_INPUT: {
                rc = mV4l2Codec->startInput();
                if (rc)
                    break;
                break;
            }
            case EVENT_START_OUTPUT: {
                rc = mV4l2Codec->startOutput();
                if (rc)
                    break;
                break;
            }
            case EVENT_STOP_INPUT: {
                rc = mV4l2Codec->stopInput();
                if (rc)
                    break;
                break;
            }
            case EVENT_STOP_OUTPUT: {
                rc = mV4l2Codec->stopOutput();
                if (rc)
                    break;
                break;
            }
            case EVENT_START_META_INPUT: {
                rc = mV4l2Codec->startMetaInput();
                if (rc)
                    break;
                break;
            }
            case EVENT_START_META_OUTPUT: {
                rc = mV4l2Codec->startMetaOutput();
                if (rc)
                    break;
                break;
            }
            case EVENT_STOP_META_INPUT: {
                rc = mV4l2Codec->stopMetaInput();
                if (rc)
                    break;
                break;
            }
            case EVENT_STOP_META_OUTPUT: {
                rc = mV4l2Codec->stopMetaOutput();
                if (rc)
                    break;
                break;
            }
            case EVENT_QBUF: {
                rc = mV4l2Codec->queueBuffer(event->buffer);
                if (rc)
                    break;
                break;
            }
            default: {
                VIDC_ERR("EventHandler::threadLoop, unknown event %ld\n", event->id);
                rc = -EINVAL;
                break;
            }
        }

        if (!rc) {
            std::unique_lock<std::mutex> lock(mEventWaitLock);
            mEventWaitNotified = true;
            mEventWaitCondition.notify_one();
        }
    }

    VIDC_MED("EventHandler::threadLoop, end\n");
}

void ThreadFunc(EventHandler& handler) {
    handler.threadLoop();
}

int EventHandler::createEventThread() {
    mEventThreadExit = false;
    mEventThreadRunning = false;
    mEventThread = std::make_shared<std::thread>(ThreadFunc, std::ref(*this));
    if (!mEventThread) {
        VIDC_ERR("EventHandler::createEventThread, thread create failed\n");
        return -EINVAL;
    }
    else {
        int count = 0;
        while (!mEventThreadRunning) {
            VIDC_MED("EventHandler::createEventThread, wait for thread running\n");
            usleep(5 * 1000);
            count++;
            if (count >= 100)
                break;
        }
        if (!mEventThreadRunning) {
            VIDC_ERR("EventHandler::createEventThread, thread not running\n");
            return -EINVAL;
        }
    }
    VIDC_MED("EventHandler::createEventThread, thread started\n");
    return 0;
}

int EventHandler::stopEventThread() {
    if (!mEventThread  || mEventThreadExit) {
        VIDC_MED("EventHandler::stopEventThread, invalid event thread. exit %d\n",
            mEventThreadExit);
        return -EINVAL;
    }

    mEventThreadExit = true;
    VIDC_MED("EventHandler::stopEventThread, join thread\n");
    if (mEventThread != nullptr and mEventThread->joinable()) {
        mEventThread->join();
    }
    mEventThread = nullptr;

    VIDC_MED("EventHandler::stopEventThread, exit event thread\n");
    return 0;
}

int EventHandler::queueEvent(enum event_id eventId, bool blocking) {
    std::shared_ptr<Event> event = std::make_shared<Event>();
    if (event == nullptr)
    {
        return -EINVAL;
    }
    event->id = eventId;
    {
        std::unique_lock<std::mutex> lock(mEventQueueLock);
        mEvents.push_back(event);
        mEventQueueCondition.notify_one();
    }
    {
        if (blocking) {
            std::unique_lock<std::mutex> lock(mEventWaitLock);
            bool notified = mEventWaitCondition.wait_for(
                lock,
                std::chrono::seconds(15),
                [this] { return mEventWaitNotified; });

            if (!notified) {
                VIDC_MED("EventHandler::queueEvent, timeout for eventId %d\n", eventId);
                return -EINVAL;
            }
            mEventWaitNotified = false;
        }
    }
    return 0;
}

int EventHandler::queueBuffer(enum event_id eventId, std::shared_ptr<v4l2_buffer> buffer) {
    std::shared_ptr<Event> event = std::make_shared<Event>();
    if (event == nullptr)
    {
        return -EINVAL;
    }
    event->id = eventId;
    event->buffer = buffer;
    {
        std::unique_lock<std::mutex> lock(mEventQueueLock);
        mEvents.push_back(event);
        mEventQueueCondition.notify_one();
    }
    {
        std::unique_lock<std::mutex> lock(mEventWaitLock);
        bool notified = mEventWaitCondition.wait_for(
            lock,
            std::chrono::seconds(15),
            [this] { return mEventWaitNotified; });
        if (!notified) {
            VIDC_MED("EventHandler::queueBuffer, timeout for eventId %d\n", eventId);
            return -EINVAL;
        }
        mEventWaitNotified = false;
    }
    return 0;
}

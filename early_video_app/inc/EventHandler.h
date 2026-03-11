/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __MSM_EVENT_HANDLER_H__
#define __MSM_EVENT_HANDLER_H__

#include <list>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include "V4l2Codec.h"

enum event_id {
    EVENT_CONFIGURE_INPUT,
    EVENT_CONFIGURE_OUTPUT,
    EVENT_START_INPUT,
    EVENT_START_OUTPUT,
    EVENT_STOP_INPUT,
    EVENT_STOP_OUTPUT,
    EVENT_START_META_INPUT,
    EVENT_START_META_OUTPUT,
    EVENT_STOP_META_INPUT,
    EVENT_STOP_META_OUTPUT,
    EVENT_RECONFIGURE,
    EVENT_QBUF,
    EVENT_DQBUF,
};

struct Event {
    enum event_id id;
    std::shared_ptr<struct v4l2_buffer> buffer;
};

class EventHandler {
    public:
        EventHandler(std::shared_ptr<V4l2Codec> codec);
        ~EventHandler();
        void threadLoop();
        int createEventThread();
        int stopEventThread();
        int queueEvent(enum event_id eventId, bool blocking);
        int queueBuffer(enum event_id eventId, std::shared_ptr<v4l2_buffer> buffer);

        std::list<std::shared_ptr<Event>> mEvents;
        std::mutex mEventWaitLock;
        std::condition_variable mEventWaitCondition; // signal to wake up the caller by event handler
        std::mutex mEventQueueLock;
        std::condition_variable mEventQueueCondition; // signal to wake up event handler

    private:
        std::shared_ptr<V4l2Codec> mV4l2Codec;
        std::shared_ptr<std::thread> mEventThread;
        bool mEventThreadRunning = false;
        bool mEventThreadExit = false;
        bool mEventWaitNotified = false;
};

#endif

/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef VIDCLOG_H_
#define VIDCLOG_H_

#ifdef ANDROID
#include <utils/Log.h>
#include <stdio.h>
#else
#ifdef _LINUX_VENV_
typedef unsigned int uint32_t;
#endif
#endif

namespace early_video_app {

	enum VidcLogLevels : uint32_t {
		VIDC_MSGLEVEL_ERROR     = 0x01, //< error logs
		VIDC_MSGLEVEL_HIGH      = 0x02, //< high logs
		VIDC_MSGLEVEL_INFO      = 0x04, //< info logs
		VIDC_MSGLEVEL_LOW       = 0x08, //< low logs
	};

	static const char *kDebugLogsLevelProperty = "vendor.earlyvideoapp.log.msg";
	extern uint32_t gVidcLogLevel;

	void vidcUpdateLogLevel();
	void VIDC_ERR(const char* format, ...);
	void VIDC_INFO(const char* format, ...);
	void VIDC_HIGH(const char* format, ...);
	void VIDC_LOW(const char* format, ...);
	void printLogToKMsg(const char* logFormat, ...);
	void printLogToLocal(const char* format, ...);
	void closeLogInstance();

	static void printLogToKMsgInternal(const char* logFormat, va_list args);
	static void printLogToLocalInternal(const char* format, va_list args);

};
//namespace early_video_app

#endif /* VIDCLOG_H_ */

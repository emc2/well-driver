/* Copyright (c) 2011 Eric McCorkle.  All rights reserved. */
#ifndef _LOGGING_H_
#define _LOGGING_H_

/* Define seven logging levels.  The meanings are as follows:
 *
 * FATAL- Messages indicating immanent failure.
 * ERROR- Messagues indicating a serious error condition, though not
 * necessarily fatal.
 * WARN- Messages indicating an unusual condition, which may result in
 * error or hindered performance.
 * MESSAGE- Normal messaging output.  In the context of a kernel, this
 * would correspond to device driver probing and other messages.
 * INFO- Verbose messaging output.
 * DEBUG- Debugging-level output, not particularly verbose.  Should
 * augment the INFO level with additional information, but not disrupt
 * the functioning of the program too much.
 * VERBOSE- Verbose debugging output, should allow a programmer to
 * diagnose most bugs.  May disrupt the usability of the program.
 * TRACE- Most verbose level, logs function calls and other events.
 * Should allow a programmer to locate crash points.  Will likely
 * render the program unusable.
 */
#define LVL_NONE -1
#define LVL_FATAL 0
#define LVL_ERROR 1
#define LVL_WARN 2
#define LVL_MESSAGE 3
#define LVL_INFO 4
#define LVL_DEBUG 5
#define LVL_VERBOSE 6
#define LVL_TRACE 7

/* This is the maximum level of logging that will not be deleted from
 * the code at compile time.  This is used to avoid excessive runtime
 * checks.
 */
#ifndef LOG_LVL_MAX
/* By default, set this to the message level. */
#define LOG_LVL_MAX 5
#endif

/* This is the minimum level of logging.  Logging messages below this
 * will be hardwired into place.  Set this to -1 to allow tuning to
 * every level.
 */
#ifndef LOG_LVL_MIN
#define LOG_LVL_MIN 0
#endif

/* This allows us to set the logging print function. */
#ifndef LOG_PRINTF
#define LOG_PRINTF(args...) printf(args)
#endif

/* This decides whether or not the logging level can be tuned at
 * runtime.  Disallowing this will improve performance, but at the
 * cost of flexibility.
 */
#ifndef TUNABLE_LOG_LVL
#define TUNABLE_LOG_LVL 1
#endif

#if LOG_LVL_MAX >= LVL_FATAL
#if TUNABLE_LOG_LVL && (LOG_LVL_MIN < LVL_FATAL)
#define LOG_FATAL(system, args...)		\
  {						\
    if(system ## _log_lvl >= LVL_FATAL) {	\
      LOG_PRINTF(args);				\
    }						\
  }
#define LOG_FATAL_PREFIX(system, args...)	\
  {						\
    if(system ## _log_lvl >= LVL_FATAL) {	\
      LOG_PRINTF("FATAL(" #system "): " args);	\
    }						\
  }
#else
#define LOG_FATAL(system, args...) \
  { LOG_PRINTF(args); }
#define LOG_FATAL_PREFIX(system, args...) \
  { LOG_PRINTF("FATAL(" #system "): " args); }
#endif
#else
#define LOG_FATAL(system, args...)
#define LOG_FATAL_PREFIX(system, args...)
#endif

#if LOG_LVL_MAX >= LVL_ERROR
#if TUNABLE_LOG_LVL && (LOG_LVL_MIN < LVL_ERROR)
#define LOG_ERROR(system, args...)		\
  {						\
    if(system ## _log_lvl >= LVL_ERROR) {	\
      LOG_PRINTF(args);				\
    }						\
  }
#define LOG_ERROR_PREFIX(system, args...)	\
  {						\
    if(system ## _log_lvl >= LVL_ERROR) {	\
      LOG_PRINTF("ERROR(" #system "): " args);	\
    }						\
  }
#else
#define LOG_ERROR(system, args...) \
  { LOG_PRINTF(args); }
#define LOG_ERROR_PREFIX(system, args...)	\
  { LOG_PRINTF("ERROR(" #system "): " args); }
#endif
#else
#define LOG_ERROR(system, args...)
#define LOG_ERROR_PREFIX(system, args...)
#endif

#if LOG_LVL_MAX >= LVL_WARN
#if TUNABLE_LOG_LVL && (LOG_LVL_MIN < LVL_WARN)
#define LOG_WARN(system, args...)		\
  {						\
    if(system ## _log_lvl >= LVL_WARN) {	\
      LOG_PRINTF(args);				\
    }						\
  }
#define LOG_WARN_PREFIX(system, args...)	\
  {						\
    if(system ## _log_lvl >= LVL_WARN) {	\
      LOG_PRINTF("WARN(" #system "): " args);	\
    }						\
  }
#else
#define LOG_WARN(system, args...)	\
  { LOG_PRINTF(args); }
#define LOG_WARN_PREFIX(system, args...)	\
  { LOG_PRINTF("WARN(" #system "): " args); }
#endif
#else
#define LOG_WARN(system, args...)
#define LOG_WARN_PREFIX(system, args...)
#endif

#if LOG_LVL_MAX >= LVL_MESSAGE
#if TUNABLE_LOG_LVL && (LOG_LVL_MIN < LVL_MESSAGE)
#define LOG_MESSAGE(system, args...)		\
  {						\
    if(system ## _log_lvl >= LVL_MESSAGE) {	\
      LOG_PRINTF(args);				\
    }						\
  }
#define LOG_MESSAGE_PREFIX(system, args...)	\
  {						\
    if(system ## _log_lvl >= LVL_MESSAGE) {	\
      LOG_PRINTF("MESSAGE(" #system "): " args);\
    }						\
  }
#else
#define LOG_MESSAGE(system, args...)	\
  { LOG_PRINTF(args); }
#define LOG_MESSAGE_PREFIX(system, args...)	\
  { LOG_PRINTF("MESSAGE(" #system "): " args); }
#endif
#else
#define LOG_MESSAGE(system, args...)
#define LOG_MESSAGE_PREFIX(system, args...)
#endif

#if LOG_LVL_MAX >= LVL_INFO
#if TUNABLE_LOG_LVL && (LOG_LVL_MIN < LVL_INFO)
#define LOG_INFO(system, args...)	\
  {					\
    if(system ## _log_lvl >= LVL_INFO) {\
      LOG_PRINTF(args);			\
    }					\
  }
#define LOG_INFO_PREFIX(system, args...)	\
  {						\
    if(system ## _log_lvl >= LVL_INFO) {	\
      LOG_PRINTF("INFO(" #system "): " args);	\
    }						\
  }
#else
#define LOG_INFO(system, args...)	\
  { LOG_PRINTF(args); }
#define LOG_INFO_PREFIX(system, args...)	\
  { LOG_PRINTF("INFO(" #system "): " args); }
#endif
#else
#define LOG_INFO(system, args...)
#define LOG_INFO_PREFIX(system, args...)
#endif

#if LOG_LVL_MAX >= LVL_DEBUG
#if TUNABLE_LOG_LVL && (LOG_LVL_MIN < LVL_DEBUG)
#define LOG_DEBUG(system, args...)		\
  {						\
    if(system ## _log_lvl >= LVL_DEBUG) {	\
      LOG_PRINTF(args);				\
    }						\
  }
#define LOG_DEBUG_PREFIX(system, args...)	\
  {						\
    if(system ## _log_lvl >= LVL_DEBUG) {	\
      LOG_PRINTF("DEBUG(" #system "): " args);	\
    }						\
  }
#else
#define LOG_DEBUG(system, args...)	\
  { LOG_PRINTF(args); }
#define LOG_DEBUG_PREFIX(system, args...)	\
  { LOG_PRINTF("DEBUG(" #system "): " args); }
#endif
#else
#define LOG_DEBUG(system, args...)
#define LOG_DEBUG_PREFIX(system, args...)
#endif

#if LOG_LVL_MAX >= LVL_VERBOSE
#if TUNABLE_LOG_LVL && (LOG_LVL_MIN < LVL_VERBOSE)
#define LOG_VERBOSE(system, args...)	\
  {						\
    if(system ## _log_lvl >= LVL_VERBOSE) {	\
      LOG_PRINTF(args);				\
    }						\
  }
#define LOG_VERBOSE_PREFIX(system, args...)	\
  {						\
    if(system ## _log_lvl >= LVL_VERBOSE) {	\
      LOG_PRINTF("VERBOSE(" #system "): " args);\
    }						\
  }
#else
#define LOG_VERBOSE(system, args...)	\
  { LOG_PRINTF(args); }
#define LOG_VERBOSE_PREFIX(system, args...)	\
  { LOG_PRINTF("VERBOSE(" #system "): " args); }
#endif
#else
#define LOG_VERBOSE(system, args...)
#define LOG_VERBOSE_PREFIX(system, args...)
#endif

#if LOG_LVL_MAX >= LVL_TRACE
#if TUNABLE_LOG_LVL && (LOG_LVL_MIN < LVL_TRACE)
#define LOG_TRACE(system, args...)		\
  {						\
    if(system ## _log_lvl >= LVL_TRACE) {	\
      LOG_PRINTF(args);				\
    }						\
  }
#define LOG_TRACE_PREFIX(system, args...)	\
  {						\
    if(system ## _log_lvl >= LVL_TRACE) {	\
      LOG_PRINTF("TRACE(" #system "): " args);	\
    }						\
  }
#else
#define LOG_TRACE(system, args...)	\
  { LOG_PRINTF(args); }
#define LOG_TRACE_PREFIX(system, args...)	\
  { LOG_PRINTF("TRACE(" #system "): " args); }
#endif
#else
#define LOG_TRACE(system, args...)
#define LOG_TRACE_PREFIX(system, args...)
#endif

#define DECLARE_LOG_SYSTEM(name)		\
  extern unsigned int name ## _log_lvl;
#define DEFINE_LOG_SYSTEM(name, init)					\
  unsigned int name ## _log_lvl = (init > LOG_LVL_MAX ? LOG_LVL_MAX :	\
				   (init < LOG_LVL_MIN ? LOG_LVL_MIN :	\
				    init))

#define SYSTEM_LOG_LVL(name) (name ## _log_lvl)
#define SET_SYSTEM_LOG_LVL(name, lvl)					\
  (name ## _log_lvl = (lvl > LOG_LVL_MAX ? LOG_LVL_MAX :		\
		       (lvl < LOG_LVL_MIN ? LOG_LVL_MIN : lvl)))

#endif

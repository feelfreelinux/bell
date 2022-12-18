#pragma once

/**
 * @brief No log messages.
 *
 * When @ref LIBRARY_LOG_LEVEL is #LOG_NONE, logging is disabled and no
 * logging messages are printed.
 */
#define LOG_NONE     0

/**
 * @brief Represents erroneous application state or event.
 *
 * These messages describe the situations when a library encounters an error from
 * which it cannot recover.
 *
 * These messages are printed when @ref LIBRARY_LOG_LEVEL is defined as either
 * of #LOG_ERROR, #LOG_WARN, #LOG_INFO or #LOG_DEBUG.
 */
#define LOG_ERROR    1

/**
 * @brief Message about an abnormal event.
 *
 * These messages describe the situations when a library encounters
 * abnormal event that may be indicative of an error. Libraries continue
 * execution after logging a warning.
 *
 * These messages are printed when @ref LIBRARY_LOG_LEVEL is defined as either
 * of #LOG_WARN, #LOG_INFO or #LOG_DEBUG.
 */
#define LOG_WARN     2

/**
 * @brief A helpful, informational message.
 *
 * These messages describe normal execution of a library. They provide
 * the progress of the program at a coarse-grained level.
 *
 * These messages are printed when @ref LIBRARY_LOG_LEVEL is defined as either
 * of #LOG_INFO or #LOG_DEBUG.
 */
#define LOG_INFO     3

/**
 * @brief Detailed and excessive debug information.
 *
 * Debug log messages are used to provide the
 * progress of the program at a fine-grained level. These are mostly used
 * for debugging and may contain excessive information such as internal
 * variables, buffers, or other specific information.
 *
 * These messages are only printed when @ref LIBRARY_LOG_LEVEL is defined as
 * #LOG_DEBUG.
 */
#define LOG_DEBUG    4

#define LogError(message)
#define LogWarn(message)
#define LogInfo(message)
#define LogDebug(message)
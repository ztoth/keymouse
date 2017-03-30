/*
 *------------------------------------------------------------------------------
 *
 * framework.h
 *
 * Simple C++ project framework for small projects and quick prototyping
 *
 * Copyright (c) 2017 Zoltan Toth <ztoth AT thetothfamily DOT net>
 * All rights reserved.
 *
 *------------------------------------------------------------------------------
 */
#ifndef FRAMEWORK_H_
#define FRAMEWORK_H_

#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <ctime>
#include <syslog.h>

/* global variables and macros */

/** return codes */
typedef enum return_code {
    RC_OK,
    RC_MAIN_SIGNAL_ERROR,
    RC_MAIN_LOGFILE_ERROR,
    RC_CONFIG_FILE_NOT_FOUND,
    RC_CONFIG_MISSING_SECTION
} return_code_en;

/** debug types */
typedef enum debug_type {
    DEBUG_TYPE_INVALID = 0,
    DEBUG_TYPE_FRAMEWORK
} debug_type_en;

/** debug levels */
typedef enum debug_level {
    DEBUG_LEVEL_ERROR,
    DEBUG_LEVEL_WARNING,
    DEBUG_LEVEL_NORMAL,
    DEBUG_LEVEL_VERBOSE,
    DEBUG_LEVEL_VERY_VERBOSE
} debug_level_en;

/** logging is only enabled when code is compiled with DEBUG flag */
#ifdef DEBUG

/** print the name of the variable, as it appears in source code */
#define VNAME(v) #v

/** cut off the newline character from the output of ctime() */
#define TIMESTAMP(t) (t).substr(0, (t).length() - 1)

/** debug macro alias, used when code is compiled with the DEBUG flag */
#define dbug(l, t, s)                                                          \
    do {                                                                       \
        if (((debug_level_en)(l) <= framework::debug_level) &&                 \
            (debug_type_en)(t)) {                                              \
            std::stringstream strstr;                                          \
            time_t timestamp = time(NULL);                                     \
            if (!framework::log_to_syslog) {                                   \
                strstr << TIMESTAMP(std::string(ctime(&timestamp)))            \
                       << " " << framework::project_name << " ";               \
            }                                                                  \
            strstr << std::string(VNAME(l)).substr(12) << " ";                 \
            if (framework::debug_level >= DEBUG_LEVEL_VERY_VERBOSE) {          \
                strstr << __PRETTY_FUNCTION__ << " [" << __FILE__ << ":"       \
                          << __LINE__ << "]: ";                                \
            } else {                                                           \
                strstr << "{" << std::string(VNAME(t)).substr(11) << "} "      \
                          << __FUNCTION__ << "[" << __LINE__ << "]: ";         \
            }                                                                  \
            strstr << s << std::endl;                                          \
            if (framework::log_to_syslog) {                                    \
                syslog(LOG_INFO, strstr.str().c_str());                        \
            } else {                                                           \
                std::cout << strstr.str();                                     \
            }                                                                  \
        }                                                                      \
    } while (0);

#else /* compiled without the DEBUG flag */
#define dbug(l, t, s) ((void) 0)
#endif

namespace framework {

/** name of the project, used in debug messages and syslog */
extern const std::string project_name;

/** config file location */
extern std::string config_file;

/** track debug level from command line arg */
extern debug_level_en debug_level;

/** true means log to syslog */
extern bool log_to_syslog;

/**
 * Config class
 *
 * This is a very basic JSON-style config file parser. It expects everything as
 * string (between quotes), which you can later reinterpret by calling the
 * corresponding conversion member function. The configs must be grouped into
 * sections (e.g. one section per module), and the key-value pairs must be
 * separated with a colon, that is surrounded with spaces. However, please note
 * that the class does not perform any syntax checking. Please refer to the
 * example config file.
 */
class Config {
  public:
    /** constructor, pass the section you want to parse */
    Config (const char *section);

    /** default destructor */
    virtual ~Config (void);

    /** get a string value for the given keyword */
    std::string get_string (const std::string &keyword);

    /** get a float value for the given keyword */
    float get_float (const std::string &keyword);

    /** get an int value for the given keyword */
    int get_int (const std::string &keyword);

    /** get a boolean value for the given keyword */
    bool get_bool (const std::string &keyword);

  private:
    std::map<std::string, std::string> configs;   /** map stores key-value pairs */

    /** get a word between quotes */
    std::string get_word (const std::string &text);
};

} /* namespace framework */

#endif /* FRAMEWORK_H_ */

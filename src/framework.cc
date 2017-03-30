/*
 *------------------------------------------------------------------------------
 *
 * framework.cc
 *
 * Simple C++ project framework for small projects and quick prototyping
 *
 * Copyright (c) 2017 Zoltan Toth <ztoth AT thetothfamily DOT net>
 * All rights reserved.
 *
 *------------------------------------------------------------------------------
 */
#include <fstream>
#include <sstream>
#include <iterator>
#include <ios>

#include "framework.h"

namespace framework {

/** name of the project, used in debug/syslog messages */
const std::string project_name = "framework";

/** config file location */
std::string config_file = "cfg/default.cfg";

/** debug level */
debug_level_en debug_level = DEBUG_LEVEL_NORMAL;

/** true means log to syslog */
bool log_to_syslog = false;


/**
 * Constructor with the section name to be parsed
 */
Config::Config (const char *section)
{
    std::ifstream file;

    /* attempt to open config file */
    file.open(config_file.c_str());
    if (file.std::ios::fail()) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
             "could not open file " << config_file << " to look for section " <<
             section);
        throw RC_CONFIG_FILE_NOT_FOUND;
    }

    /* look for the given section */
    bool found = false;
    while (!file.std::ios::eof()) {
        char line[512];
        file.getline(line, 512);

        /* skip to the given section */
        if (std::string(line).find(section) != (size_t)(-1)) {
            found = true;

            dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
                 "parsing section '" << section << "' in file " << config_file);

            /* read the entire section at once */
            file.getline(line, 512, '}');
            std::istringstream iss(line);
            std::istream_iterator<std::string> begin(iss);
            std::istream_iterator<std::string> end;
            std::string key;
            int i = 0;

            /* iterate through the words and save the config pairs in the map */
            for (; begin != end; begin++) {
                std::string s = get_word(*begin);

                /* ignore the delimiter */
                if (":" == s) {
                    continue;
                }

                /* we look for string pairs for the map */
                if (i % 2) {
                    /* odd means we have a value for the previous key */
                    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
                         "parsed " << section << " pair: " << key << " = " << s);
                    configs[key] = s;
                } else {
                    /* even means we have a new key */
                    key = s;
                }
                i++;
            }

            break;
        }
    }

    /* close config file */
    file.close();

    /* throw an error if the section is not found */
    if (!found) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
             "could not find section " << section << " in file " << config_file);
        throw RC_CONFIG_MISSING_SECTION;
    }
}

/**
 * Config destructor
 */
Config::~Config (void)
{
    configs.clear();
}

/**
 * Get a word between quotes
 */
std::string
Config::get_word (const std::string &text)
{
    std::string::size_type begin = 0;
    std::string::size_type end   = text.size();

    /* find the opening quote's position */
    while ((begin != text.size()) && (text[begin] == '"')) {
        ++begin;
    }

    /* find the closing quote's position */
    while ((end != 0) && (text[end - 1] != '"')) {
        --end;
    }

    /* return the substring between the quotes */
    return (text.substr(begin, end - 1 - begin));
}

/**
 * Get a string value for the given keyword
 */
std::string
Config::get_string (const std::string &keyword)
{
    return configs[keyword];
}

/**
 * Get a float value for the given keyword
 */
float
Config::get_float (const std::string &keyword)
{
    return (std::atof(configs[keyword].c_str()));
}

/**
 * Get an int value for the given keyword
 */
int
Config::get_int (const std::string &keyword)
{
    return (std::atoi(configs[keyword].c_str()));
}

/**
 * Get a boolean value for the given keyword
 */
bool
Config::get_bool (const std::string &keyword)
{
    if (("yes" == configs[keyword]) ||
        ("true" == configs[keyword]) ||
        ("t" == configs[keyword]) ||
        ("1" == configs[keyword])) {
        return true;
    }
    return false;
}

} /* namespace framework */

/*
 *------------------------------------------------------------------------------
 *
 * main.cc
 *
 * Simple C++ project framework for small projects and quick prototyping
 *
 * Currently the following (optional) command line args are supported:
 *   -v                verbose debug level
 *   -vv               very verbose debug level
 *   -c <configfile>   use the given config file (default is cfg/default.cfg)
 *   -l <logfile>      redirect std::cout to the given file
 *   -s                log messages to syslog
 *
 * Copyright (c) 2017 Zoltan Toth <ztoth AT thetothfamily DOT net>
 * All rights reserved.
 *
 *------------------------------------------------------------------------------
 */
#include <iostream>
#include <string>
#include <fstream>
#include <csignal>
#include <pthread.h>

#include "framework.h"

/**
 * Signal handler thread
 */
void*
signal_thread (void *arg)
{
    sigset_t *sigset = (sigset_t*)arg;
    int signal;

    pthread_setname_np(pthread_self(), "signal handler");
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    /* wait for asynchronous OS signals */
    while (true) {
        if (sigwait(sigset, &signal) != 0) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
                 "sigwait() returned error");
            break;
        }

        dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
             "signal " << signal << " received, exiting");
        break;
    }

    pthread_exit(NULL);
}

/**
 * Main function
 */
int
main (int argc, char *argv[])
{
    /* init local and global variables */
    std::streambuf *cout = std::cout.rdbuf();
    std::ofstream logfile;
    std::string logfile_name;
    bool log_to_file = false;

    /* process command line arguments */
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);

        /* debug level */
        if ("-v" == arg) {
            framework::debug_level = DEBUG_LEVEL_VERBOSE;
        } else if ("-vv" == arg) {
            framework::debug_level = DEBUG_LEVEL_VERY_VERBOSE;
        }

        /* configuration file */
        if ("-c" == arg) {
            framework::config_file = argv[++i];
        }

        /* check if we need to log to file */
        if ("-l" == arg) {
            logfile_name = std::string(argv[++i]);
            log_to_file = true;
        }

        /* check if we need to log to syslog */
        if ("-s" == arg) {
            framework::log_to_syslog = true;
        }
    }

    if (framework::log_to_syslog) {
        /* don't log to file if syslog is enabled */
        log_to_file = false;
        openlog(framework::project_name.c_str(), 0, 0);
    }

    if (log_to_file) {
        /* redirect cout to file */
        logfile.open(logfile_name);
        if (logfile.std::ios::fail()) {
            dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
                 "could not open logfile" << logfile_name);
            return RC_MAIN_LOGFILE_ERROR;
        }
        std::cout.rdbuf(logfile.rdbuf());
    }

    if (DEBUG_LEVEL_VERBOSE == framework::debug_level) {
        dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
             "verbose mode enabled");
    } else if (DEBUG_LEVEL_VERY_VERBOSE == framework::debug_level) {
        dbug(DEBUG_LEVEL_WARNING, DEBUG_TYPE_FRAMEWORK,
             "very verbose mode enabled!");
    }
    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
         "using config file " << framework::config_file);

    /* set the name of the main thread */
    pthread_setname_np(pthread_self(), framework::project_name.c_str());

    /* block every signal in the main and its child threads */
    sigset_t sigset;
    sigfillset(&sigset);
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL) != 0) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
             "unable to set sigmask");
        return RC_MAIN_SIGNAL_ERROR;
    }

    /* spawn a signal handler thread to catch asynchronous signals from the OS */
    pthread_t signal_thrd;
    if (pthread_create(&signal_thrd, 0, signal_thread, (void*)&sigset) != 0) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
             "unable to start signal handler thread");
        return RC_MAIN_SIGNAL_ERROR;
    }


    /* TODO: do your stuff here */


    /* cleanup */
    pthread_cancel(signal_thrd);
    pthread_join(signal_thrd, NULL);

    /* close logfile if we used one */
    if (logfile.is_open()) {
        std::cout.rdbuf(cout);
        logfile.close();
    }

    return 0;
}

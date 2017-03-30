/*
 *------------------------------------------------------------------------------
 *
 * keymouse.cc
 *
 * Project keymouse
 *
 * Currently the following (optional) command line args are supported:
 *   -v                verbose debug level
 *   -vv               very verbose debug level
 *   -c <configfile>   use the given config file (default is cfg/default.cfg)
 *   -l <logfile>      redirect std::cout to the given file
 *   -s                log messages to syslog
 *
 * Copyright (c) 2017 Zoltan Toth <ztoth AT thetothfamily DOT net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 *------------------------------------------------------------------------------
 */
#include <iostream>
#include <string>
#include <fstream>
#include <csignal>
#include <pthread.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include "framework.h"

/** mouse movement direction flags */
typedef enum move_flag_e {
    STOP  = 0,
    UP    = 0x1,
    DOWN  = 0x2,
    LEFT  = 0x4,
    RIGHT = 0x8
} move_flag_t;

/** configuration */
typedef struct config_s {
    KeyCode trigger;
    KeyCode up;
    KeyCode down;
    KeyCode left;
    KeyCode right;
    KeyCode click;
    KeyCode paste;
    KeyCode slow;
    int speed;
    int sleep;
    int numlock;
} config_t;

/**
 * Update mouse global coordinates
 */
static void
update_mouse_coordinates (Display *display, Window &root, int &mouse_x, int &mouse_y)
{
    /* query window properties */
    Atom atom;
    int format;
    unsigned long items, bytes;
    Window *properties;
    XGetWindowProperty(display, root, XInternAtom(display, "_NET_ACTIVE_WINDOW",
                                                  True),
                       0, 1, False, AnyPropertyType, &atom, &format, &items,
                       &bytes, (unsigned char**)&properties);

    /* get mouse position */
    Window tmpwin1, tmpwin2;
    int tmp_x, tmp_y;
    unsigned int mask;
    XQueryPointer(display, properties[0], &tmpwin1, &tmpwin2,
                  &mouse_x, &mouse_y, &tmp_x, &tmp_y, &mask);

    /* cleanup */
    XFree(properties);
}

/**
 * Main loop processes key events
 */
static void
main_loop (Display *display, Window &root, config_t &cfg)
{
    XEvent event;
    bool mouse_grab_active = false;
    bool clicking = false;
    bool pasting = false;
    int move_state = STOP;
    int mouse_x;
    int mouse_y;

    while (true) {
        /*
         * Waiting for the next event blocks execution, which is okay if we
         * don't have the mouse, but otherwise we must make sure there is an
         * event waiting in the queue to be processed
         */
        if (!mouse_grab_active || XPending(display)) {
            XNextEvent(display, &event);
            if ((KeyPress == event.type) && (event.xkey.keycode == cfg.trigger)) {
                if (mouse_grab_active) {
                    /* release the mouse */
                    XUngrabKey(display, cfg.up, cfg.numlock, root);
                    XUngrabKey(display, cfg.left, cfg.numlock, root);
                    XUngrabKey(display, cfg.down, cfg.numlock, root);
                    XUngrabKey(display, cfg.right, cfg.numlock, root);
                    XUngrabKey(display, cfg.click, cfg.numlock, root);
                    XUngrabKey(display, cfg.paste, cfg.numlock, root);
                    XUngrabKey(display, cfg.slow, cfg.numlock, root);

                    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
                         "mouse released");
                } else {
                    /* grab the mouse */
                    XGrabKey(display, cfg.up, cfg.numlock, root, False,
                             GrabModeAsync, GrabModeAsync);
                    XGrabKey(display, cfg.left, cfg.numlock, root, False,
                             GrabModeAsync, GrabModeAsync);
                    XGrabKey(display, cfg.down, cfg.numlock, root, False,
                             GrabModeAsync, GrabModeAsync);
                    XGrabKey(display, cfg.right, cfg.numlock, root, False,
                             GrabModeAsync, GrabModeAsync);
                    XGrabKey(display, cfg.click, cfg.numlock, root, False,
                             GrabModeAsync, GrabModeAsync);
                    XGrabKey(display, cfg.paste, cfg.numlock, root, False,
                             GrabModeAsync, GrabModeAsync);
                    XGrabKey(display, cfg.slow, cfg.numlock, root, False,
                             GrabModeAsync, GrabModeAsync);

                    /* update mouse coordinates */
                    update_mouse_coordinates(display, root, mouse_x, mouse_y);

                    dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
                         "mouse grabbed");
                }
                mouse_grab_active = !mouse_grab_active;
            }
        }

        /*
         * If the mouse is ours, query the pressed keys and update movements and
         * clicks as necessary
         */
        if (mouse_grab_active) {
            char pressed_keys[32];
            XQueryKeymap(display, pressed_keys);

            /* update UP direction flag */
            if ((pressed_keys[cfg.up >> 3] >> (cfg.up & 0x07)) & 0x01) {
                move_state |= UP;
            } else {
                move_state &= ~UP;
            }

            /* update LEFT direction flag */
            if ((pressed_keys[cfg.left >> 3] >> (cfg.left & 0x07)) & 0x01) {
                move_state |= LEFT;
            } else {
                move_state &= ~LEFT;
            }

            /* update DOWN direction flag */
            if ((pressed_keys[cfg.down >> 3] >> (cfg.down & 0x07)) & 0x01) {
                move_state |= DOWN;
            } else {
                move_state &= ~DOWN;
            }

            /* update RIGHT direction flag */
            if ((pressed_keys[cfg.right >> 3] >> (cfg.right & 0x07)) & 0x01) {
                move_state |= RIGHT;
            } else {
                move_state &= ~RIGHT;
            }

            /* update click states */
            bool send_click_event = false;
            if (clicking != pressed_keys[cfg.click >> 3] >> (cfg.click & 0x07)) {
                /* send corresponding left click event */
                clicking = pressed_keys[cfg.click >> 3] >> (cfg.click & 0x07);
                send_click_event = true;
            }
            bool send_paste_event = false;
            if (pasting != pressed_keys[cfg.paste >> 3] >> (cfg.paste & 0x07)) {
                /* send paste event only on keyrelease */
                if (pasting) {
                    send_paste_event = true;
                }
                pasting = pressed_keys[cfg.paste >> 3] >> (cfg.paste & 0x07);
            }

            /* set speed */
            int pixels = cfg.speed;
            if ((pressed_keys[cfg.slow >> 3] >> (cfg.slow & 0x07)) & 0x01) {
                pixels = 1;
            }

            /* move mouse up or down */
            if (UP & move_state) {
                if (mouse_y >= pixels) {
                    mouse_y -= pixels;
                }
            } else if (DOWN & move_state) {
                /* TODO: get window size and limit movement */
                mouse_y += pixels;
            }

            /* move mouse left or right */
            if (LEFT & move_state) {
                if (mouse_x >= pixels) {
                    mouse_x -= pixels;
                }
            } else if (RIGHT & move_state) {
                /* TODO: get window size and limit movement */
                mouse_x += pixels;
            }

            /* put the mouse in its new position */
            XWarpPointer(display, None, root, 0, 0, 0, 0, mouse_x, mouse_y);

            /* send left click event */
            if (send_click_event) {
                if (clicking) {
                    /* mouse left button down event */
                    XTestFakeButtonEvent(display, 1, True, CurrentTime);
                } else {
                    /* mouse left button up event */
                    XTestFakeButtonEvent(display, 1, False, CurrentTime);
                }
            }

            /* send paste event */
            if (send_paste_event) {
                /* mouse middle button down-up event */
                XTestFakeButtonEvent(display, 2, True, CurrentTime);
                XTestFakeButtonEvent(display, 2, False, CurrentTime);
            }

            /* refresh the screen */
            XFlush(display);

            /* take a break */
            usleep(cfg.sleep);
        }
    }
}

/**
 * Parse configuration
 */
static config_t
parse_config (Display *display)
{
    config_t cfg;
    framework::Config *config = new framework::Config("keymouse");

    /* read values to the config structure */
    cfg.trigger = XKeysymToKeycode(
        display, XStringToKeysym(config->get_string("trigger").c_str()));
    cfg.up = XKeysymToKeycode(
        display, XStringToKeysym(config->get_string("up").c_str()));
    cfg.left = XKeysymToKeycode(
        display, XStringToKeysym(config->get_string("left").c_str()));
    cfg.down = XKeysymToKeycode(
        display, XStringToKeysym(config->get_string("down").c_str()));
    cfg.right = XKeysymToKeycode(
        display, XStringToKeysym(config->get_string("right").c_str()));
    cfg.click = XKeysymToKeycode(
        display, XStringToKeysym(config->get_string("click").c_str()));
    cfg.paste = XKeysymToKeycode(
        display, XStringToKeysym(config->get_string("paste").c_str()));
    cfg.slow = XKeysymToKeycode(
        display, XStringToKeysym(config->get_string("slow").c_str()));
    cfg.speed = config->get_int("speed");
    cfg.sleep = config->get_int("sleep");
    cfg.numlock = config->get_bool("numlock") ? Mod2Mask : 0;

    delete config;
    return cfg;
}

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

        /* catch CTRL-C */
        if (SIGINT == signal) {
            dbug(DEBUG_LEVEL_NORMAL, DEBUG_TYPE_FRAMEWORK,
                 "SIGINT received, exiting");
            exit(0);
        }
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

    /* open the display */
    Display *display = XOpenDisplay(NULL);
    if (NULL == display) {
        dbug(DEBUG_LEVEL_ERROR, DEBUG_TYPE_FRAMEWORK,
             "cannot open display");
        return RC_MAIN_DISPLAY_ERROR;
    }

    /* get root window */
    Window root = XDefaultRootWindow(display);

    /* parse configuration */
    config_t cfg = parse_config(display);

    /* disable keyboard auto-repeat */
    XkbSetDetectableAutoRepeat(display, True, NULL);

    /* we are listening on key events */
    XSelectInput(display, root, KeyPressMask | KeyReleaseMask);

    /* grab the menu key */
    XGrabKey(display, cfg.trigger, cfg.numlock, root, False, GrabModeAsync,
             GrabModeAsync);

    /* loop forever */
    main_loop(display, root, cfg);

    /* cleanup */
    XUngrabKey(display, cfg.trigger, cfg.numlock, root);
    XCloseDisplay(display);
    pthread_cancel(signal_thrd);
    pthread_join(signal_thrd, NULL);

    /* close logfile if we used one */
    if (logfile.is_open()) {
        std::cout.rdbuf(cout);
        logfile.close();
    }

    return 0;
}

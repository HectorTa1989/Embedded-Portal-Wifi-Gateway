// Wifi monitoring thread

#ifndef _WDCTL_THREAD_H_
#define _WDCTL_THREAD_H_

#define DEFAULT_WDCTL_SOCK	"/tmp/wdctl.sock"

/** @brief Listen for WiFiDog control messages on a unix domain socket */
void thread_wdctl(void *arg);

#endif

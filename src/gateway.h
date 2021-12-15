#ifndef _GATEWAY_H_
#define _GATEWAY_H_

#include <stdio.h>

#include "httpd.h"

extern time_t started_time;

/** @brief The internal web server */
extern httpd *webserver;

/** @brief actual program entry point. */
int gw_main(int, char **);

/** @brief exits cleanly and clear the firewall rules. */
void termination_handler(int s);

#endif                          /* _GATEWAY_H_ */

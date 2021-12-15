// Firewall update functions header

#ifndef _FIREWALL_H_
#define _FIREWALL_H_

#include "client_list.h"

/** Used by fw_iptables.c */
typedef enum _t_fw_marks {
    FW_MARK_NONE = 0, /**< @brief No mark set. */
    FW_MARK_PROBATION = 1, /**< @brief The client is in probation period and must be authenticated 
			    @todo: VERIFY THAT THIS IS ACCURATE*/
    FW_MARK_KNOWN = 2,  /**< @brief The client is known to the firewall */
    FW_MARK_AUTH_IS_DOWN = 253, /**< @brief The auth servers are down */
    FW_MARK_LOCKED = 254 /**< @brief The client has been locked out */
} t_fw_marks;

/** @brief Initialize the firewall */
int fw_init(void);

/** @brief Clears the authservers list */
void fw_clear_authservers(void);

/** @brief Sets the authservers list */
void fw_set_authservers(void);

/** @brief Destroy the firewall */
int fw_destroy(void);

/** @brief Allow a user through the firewall*/
int fw_allow(t_client *, int);

/** @brief Allow a host through the firewall*/
int fw_allow_host(const char *);

/** @brief Deny a client access through the firewall*/
int fw_deny(t_client *);

/** @brief Passthrough for clients when auth server is down */
int fw_set_authdown(void);

/** @brief Remove passthrough for clients when auth server is up */
int fw_set_authup(void);

/** @brief Refreshes the entire client list */
void fw_sync_with_authserver(void);

/** @brief Get an IP's MAC address from the ARP cache.*/
char *arp_get(const char *);

#endif                          /* _FIREWALL_H_ */

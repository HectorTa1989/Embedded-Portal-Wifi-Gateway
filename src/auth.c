#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <syslog.h>

#include "httpd.h"
#include "http.h"
#include "safe.h"
#include "conf.h"
#include "debug.h"
#include "auth.h"
#include "centralserver.h"
#include "fw_iptables.h"
#include "firewall.h"
#include "client_list.h"
#include "util.h"
#include "wd_util.h"

/** Launches a thread that periodically checks if any of the connections has timed out
@param arg Must contain a pointer to a string containing the IP adress of the client to check to check
@todo Also pass MAC adress? 
@todo This thread loops infinitely, need a watchdog to verify that it is still running?
*/
void
thread_client_timeout_check(const void *arg)
{
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
    struct timespec timeout;

    while (1) {
        /* Sleep for config.checkinterval seconds... */
        timeout.tv_sec = time(NULL) + config_get_config()->checkinterval;
        timeout.tv_nsec = 0;

        /* Mutex must be locked for pthread_cond_timedwait... */
        pthread_mutex_lock(&cond_mutex);

        /* Thread safe "sleep" */
        pthread_cond_timedwait(&cond, &cond_mutex, &timeout);

        /* No longer needs to be locked */
        pthread_mutex_unlock(&cond_mutex);

        debug(LOG_DEBUG, "Running fw_counter()");

        fw_sync_with_authserver();
    }
}

/**
 * @brief Logout a client and report to auth server.
 *
 * This function assumes it is being called with the client lock held! This
 * function remove the client from the client list and free its memory, so
 * client is no langer valid when this method returns.
 *
 * @param client Points to the client to be logged out
 */
void
logout_client(t_client * client)
{
    t_authresponse authresponse;
    const s_config *config = config_get_config();
    fw_deny(client);
    client_list_remove(client);

    /* Advertise the logout if we have an auth server */
    if (config->auth_servers != NULL) {
        UNLOCK_CLIENT_LIST();
        auth_server_request(&authresponse, REQUEST_TYPE_LOGOUT,
                            client->ip, client->mac, client->token,
                            client->counters.incoming, client->counters.outgoing, client->counters.incoming_delta, client->counters.outgoing_delta);

        if (authresponse.authcode == AUTH_ERROR)
            debug(LOG_WARNING, "Auth server error when reporting logout");
        LOCK_CLIENT_LIST();
    }

    client_free_node(client);
}

/** Authenticates a single client against the central server and returns when done
 * Alters the firewall rules depending on what the auth server says
@param r httpd request struct
*/
void
authenticate_client(request * r)
{
    t_client *client, *tmp;
    t_authresponse auth_response;
    char *token;
    httpVar *var;
    char *urlFragment = NULL;
    s_config *config = NULL;
    t_auth_serv *auth_server = NULL;

    LOCK_CLIENT_LIST();

    client = client_dup(client_list_find_by_ip(r->clientAddr));

    UNLOCK_CLIENT_LIST();

    if (client == NULL) {
        debug(LOG_ERR, "authenticate_client(): Could not find client for %s", r->clientAddr);
        return;
    }

    /* Users could try to log in(so there is a valid token in
     * request) even after they have logged in, try to deal with
     * this */
    if ((var = httpdGetVariableByName(r, "token")) != NULL) {
        token = safe_strdup(var->value);
    } else {
        token = safe_strdup(client->token);
    }

    /* 
     * At this point we've released the lock while we do an HTTP request since it could
     * take multiple seconds to do and the gateway would effectively be frozen if we
     * kept the lock.
     */
    auth_server_request(&auth_response, REQUEST_TYPE_LOGIN, client->ip, client->mac, token, 0, 0, 0, 0);

    LOCK_CLIENT_LIST();

    /* can't trust the client to still exist after n seconds have passed */
    tmp = client_list_find_by_client(client);

    if (NULL == tmp) {
        debug(LOG_ERR, "authenticate_client(): Could not find client node for %s (%s)", client->ip, client->mac);
        UNLOCK_CLIENT_LIST();
        client_list_destroy(client);    /* Free the cloned client */
        free(token);
        return;
    }

    client_list_destroy(client);        /* Free the cloned client */
    client = tmp;

    if (strcmp(token, client->token) != 0) {
        /* If token changed, save it. */
        free(client->token);
        client->token = token;
    } else {
        free(token);
    }

    /* Prepare some variables we'll need below */
    config = config_get_config();
    auth_server = get_auth_server();

    switch (auth_response.authcode) {

    case AUTH_ERROR:
        /* Error talking to central server */
        debug(LOG_ERR, "Got ERROR from central server authenticating token %s from %s at %s", client->token, client->ip,
              client->mac);
        send_http_page(r, "Error!", "Error: We did not get a valid answer from the central server");
        break;

    case AUTH_DENIED:
        /* Central server said invalid token */
        debug(LOG_INFO,
              "Got DENIED from central server authenticating token %s from %s at %s - deleting from firewall and redirecting them to denied message",
              client->token, client->ip, client->mac);
        fw_deny(client);
        safe_asprintf(&urlFragment, "%smessage=%s&token=%s",
                      auth_server->authserv_msg_script_path_fragment, GATEWAY_MESSAGE_DENIED, client->token);
        http_send_redirect_to_auth(r, urlFragment, "Redirect to denied message");
        free(urlFragment);
        break;

    case AUTH_VALIDATION:
        /* They just got validated for X minutes to check their email */
        debug(LOG_INFO, "Got VALIDATION from central server authenticating token %s from %s at %s"
              "- adding to firewall and redirecting them to activate message", client->token, client->ip, client->mac);
        fw_allow(client, FW_MARK_PROBATION);
        safe_asprintf(&urlFragment, "%smessage=%s&token=%s",
                      auth_server->authserv_msg_script_path_fragment, GATEWAY_MESSAGE_ACTIVATE_ACCOUNT, client->token);
        http_send_redirect_to_auth(r, urlFragment, "Redirect to activate message");
        free(urlFragment);
        break;

    case AUTH_ALLOWED:
        /* Logged in successfully as a regular account */
        debug(LOG_INFO, "Got ALLOWED from central server authenticating token %s from %s at %s - "
              "adding to firewall and redirecting them to portal", client->token, client->ip, client->mac);
        fw_allow(client, FW_MARK_KNOWN);
        served_this_session++;
        safe_asprintf(&urlFragment, "%sgw_id=%s&token=%s", auth_server->authserv_portal_script_path_fragment,
                      config->gw_id, client->token);
        http_send_redirect_to_auth(r, urlFragment, "Redirect to portal");
        free(urlFragment);
        break;

    case AUTH_VALIDATION_FAILED:
        /* Client had X minutes to validate account by email and didn't = too late */
        debug(LOG_INFO, "Got VALIDATION_FAILED from central server authenticating token %s from %s at %s "
              "- redirecting them to failed_validation message", client->token, client->ip, client->mac);
        safe_asprintf(&urlFragment, "%smessage=%s&token=%s",
                      auth_server->authserv_msg_script_path_fragment, GATEWAY_MESSAGE_ACCOUNT_VALIDATION_FAILED,
                      client->token);
        http_send_redirect_to_auth(r, urlFragment, "Redirect to failed validation message");
        free(urlFragment);
        break;

    default:
        debug(LOG_WARNING,
              "I don't know what the validation code %d means for token %s from %s at %s - sending error message",
              auth_response.authcode, client->token, client->ip, client->mac);
        send_http_page(r, "Internal Error", "We can not validate your request at this time");
        break;

    }

    UNLOCK_CLIENT_LIST();
    return;
}

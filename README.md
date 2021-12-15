# Embedded Portal Wifi Gateway

This project is a complete and embeddable captive portal
solution for wireless community groups or individuals who wish to open a
free hotspot while still preventing abuse of their Internet connection.

## Components ##

Embedded Portal Wifi Gateway consists of two parts:

* auth server
* client daemon (the gateway)

This repository contains the client daemon. The client typically runs on
embedded hardware, e.g. the hotspot itself. The client is responsible for
redirecting the user to the auth server where they may authenticate
themselves. Depending on the response of the auth server, the client
lifts the access restrictions for the user.
Client and server speak the WifiDog Protocol Version 1,
with Version 2 being a draft which has not been implemented so far.
A detailed description of the login process involving user,
client and server is available as a flow diagram.

## Install ##

See the [FAQ][https://github.com/HectorTa1989/Embedded-Portal-Wifi-Gateway/blob/master/FAQ].


## License ##
The project's software is released under the GPL license and is copyrighted
by its respective owners. See COPYING for details.


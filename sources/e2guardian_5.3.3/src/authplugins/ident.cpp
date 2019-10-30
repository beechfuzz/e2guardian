// Ident server auth plugin

// For all support, instructions and copyright go to:
// http://e2guardian.org/
// Released under the GPL v2, with the OpenSSL exception described in the README file.

// INCLUDES
#ifdef HAVE_CONFIG_H
#include "dgconfig.h"
#endif

#include "../Auth.hpp"
#include "../OptionContainer.hpp"

#include <syslog.h>

// GLOBALS

extern OptionContainer o;
extern thread_local std::string thread_id;

// DECLARATIONS

// class name is relevant!
class identinstance : public AuthPlugin
{
    public:
    identinstance(ConfigVar &definition)
        : AuthPlugin(definition){
        client_ip_based = true;    // not sure if this is correct!!
    };
    int identify(Socket &peercon, Socket &proxycon, HTTPHeader &h, std::string &string, bool &is_real_user);
};

// IMPLEMENTATION

// class factory code *MUST* be included in every plugin

AuthPlugin *identcreate(ConfigVar &definition)
{
    return new identinstance(definition);
}

// end of Class factory

// ident server username extraction
// checkme: needs better error reporting
int identinstance::identify(Socket &peercon, Socket &proxycon, HTTPHeader &h, std::string &string, bool &is_real_user)
{
    std::string clientip;
    bool use_xforwardedfor;
    use_xforwardedfor = false;
    if (o.use_xforwardedfor == 1) {
        if (o.xforwardedfor_filter_ip.size() > 0) {
            const char *ip = peercon.getPeerIP().c_str();
            for (unsigned int i = 0; i < o.xforwardedfor_filter_ip.size(); i++) {
                if (strcmp(ip, o.xforwardedfor_filter_ip[i].c_str()) == 0) {
                    use_xforwardedfor = true;
                    break;
                }
            }
        } else {
            use_xforwardedfor = true;
        }
    }

    if (use_xforwardedfor) {
        // grab the X-Forwarded-For IP if available
        clientip = h.getXForwardedForIP();
        // otherwise, grab the IP directly from the client connection
        if (clientip.length() == 0)
            clientip = peercon.getPeerIP();
    } else {
        clientip = peercon.getPeerIP();
    }
    int clientport = peercon.getPeerSourcePort();
    int serverport = peercon.getPort();
#ifdef DGDEBUG
    std::cerr << thread_id << "Connecting to: " << clientip << std::endl;
    std::cerr << thread_id << "to ask about: " << clientport << std::endl;
#endif
    Socket iq;
    iq.setTimeout(5000);
    int rc = iq.connect(clientip.c_str(), 113); // ident port
    if (rc) {
#ifdef DGDEBUG
        std::cerr << thread_id << "Error connecting to obtain ident from: " << clientip << std::endl;
#endif
        return DGAUTH_NOMATCH;
    }
#ifdef DGDEBUG
    std::cerr << thread_id << "Connected to:" << clientip << std::endl;
#endif
    std::string request;
    request = String(clientport).toCharArray();
    request += ", ";
    request += String(serverport).toCharArray();
    request += "\r\n";
#ifdef DGDEBUG
    std::cerr << thread_id << "About to send:" << request << std::endl;
#endif
    if (!iq.writeToSocket((char *)request.c_str(), request.length(), 0, 5000)) {
#ifdef DGDEBUG
        std::cerr << thread_id << "Error writing to ident connection to: " << clientip << std::endl;
#endif
        iq.close(); // close conection to client
        return -1;
    }
#ifdef DGDEBUG
    std::cerr << thread_id << "wrote ident request to:" << clientip << std::endl;
#endif
    char buff[8192];
    try {
        iq.getLine(buff, 8192, 5000);
    } catch (std::exception &e) {
        return -2;
    }
    String temp;
    temp = buff; // convert to String
#ifdef DGDEBUG
    std::cerr << thread_id << "got ident reply: " << temp << " from: " << clientip << std::endl;
#endif
    iq.close(); // close conection to client
    temp = temp.after(":");
    if (!temp.before(":").contains("USERID")) {
        return -3;
    }
    temp = temp.after(":");
    temp = temp.after(":");
    temp.removeWhiteSpace();
    if (temp.length() > 0) {
        string = temp.toCharArray();
        return DGAUTH_OK;
    }
    return DGAUTH_NOMATCH;
}

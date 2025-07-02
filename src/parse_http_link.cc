// thanks gemini.

#include <stdexcept>
#include <algorithm> // For std::transform and std::tolower
#include "parse_http_link.h"


int parseUrlManual(UrlComponents* dst, const std::string& url) {
    // if 
    if (dst == nullptr) {
        return ERR_URL_DEST_NULL;
    }

    dst->port = 0; // Default port

    std::string temp_url = url;

    // 1. Parse Scheme (http:// or https://)
    size_t scheme_end = temp_url.find("://");
    if (scheme_end != std::string::npos) {
        dst->scheme = temp_url.substr(0, scheme_end);
        std::transform(dst->scheme.begin(), dst->scheme.end(), dst->scheme.begin(), ::tolower);
        temp_url = temp_url.substr(scheme_end + 3); // Remove "scheme://"
    }
    else {
        // Assume default scheme if not present (e.g., "example.com:8080")
        dst->scheme = "http";
    }

    // 2. Parse Host and Port
    size_t path_start = temp_url.find('/');
    std::string host_port_str;
    if (path_start != std::string::npos) {
        host_port_str = temp_url.substr(0, path_start);
        dst->path = temp_url.substr(path_start);
    }
    else {
        host_port_str = temp_url;
        dst->path = "/"; // Default path
    }

    size_t port_delimiter = host_port_str.find(':');
    if (port_delimiter != std::string::npos) {
        dst->host = host_port_str.substr(0, port_delimiter);
        std::string port_str = host_port_str.substr(port_delimiter + 1);
        try {
            dst->port = std::stoi(port_str);
        }
        catch (const std::invalid_argument& e) {
            return ERR_URL_INVALID_PORT_STR;
        }
        catch (const std::out_of_range& e) {
            return ERR_URL_PORT_OUTOFRANGE;
        }
    }
    else {  // port havn't been set
        dst->host = host_port_str;
        // Assign default port based on scheme if no port specified
        if (dst->scheme == "https") {
            dst->port = 443;
        }
        else if (dst->scheme == "http") {
            dst->port = 80;
        }
    }

    // Handle IPv6 addresses in host (e.g., [::1]:8080)
    if (dst->host.length() > 0 && dst->host[0] == '[' && dst->host.back() == ']') {
        dst->host = dst->host.substr(1, dst->host.length() - 2);
    }

    return 0;
}
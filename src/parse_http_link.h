#include <string>

struct UrlComponents {
    std::string scheme;
    std::string host;
    uint16_t port;
    std::string path;
};

#define ERR_URL_DEST_NULL 1
#define ERR_URL_INVALID_PORT_STR 1
#define ERR_URL_PORT_OUTOFRANGE 1

int parseUrlManual(UrlComponents* dst, const std::string& url);
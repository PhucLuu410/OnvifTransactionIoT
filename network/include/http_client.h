#ifndef NETWORK_HTTP_CLIENT_H
#define NETWORK_HTTP_CLIENT_H

#include <string>

namespace Eyerus
{

    struct HttpResponse
    {
        int statusCode = 0;  // HTTP Status code: 200, 401, 500,...
        std::string header;  // Raw HTTP headers
        std::string body;    // Payload XML response
        int roundTripMs = 0; // Latency / Ping time (milliseconds)
    };

    class HttpClient
    {
    public:
        // Gửi SOAP request chuẩn qua HTTP/1.1 POST đến ONVIF service URL
        static bool postSoap(
            const std::string &url,
            const std::string &soapBody,
            HttpResponse &outResponse,
            int timeoutMs = 4000);
    };

} // namespace Eyerus

#endif // NETWORK_HTTP_CLIENT_H

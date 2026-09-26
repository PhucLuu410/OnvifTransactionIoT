#include "ws_security.h"
#include "sha1.h"
#include "base64.h"
#include <chrono>
#include <ctime>
#include <random>
#include <sstream>
#include <iomanip>

namespace Eyerus
{

    std::string WsSecurity::getUtcIso8601()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm gmt;
#if defined(_WIN32)
        gmtime_s(&gmt, &tt);
#else
        gmtime_r(&tt, &gmt);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &gmt);
        return std::string(buf);
    }

    std::string WsSecurity::generateNonce(size_t length)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(0, 255);

        std::string nonce;
        nonce.reserve(length);
        for (size_t i = 0; i < length; ++i)
        {
            nonce.push_back(static_cast<char>(dis(gen)));
        }
        return nonce;
    }

    std::string WsSecurity::buildSecurityHeader(const std::string &username, const std::string &password)
    {
        if (username.empty())
        {
            return "";
        }

        std::string rawNonce = generateNonce(16);
        std::string createdTime = getUtcIso8601();

        // Chuẩn ONVIF: PasswordDigest = Base64( SHA1( RawNonce + CreatedUtc + PlainPassword ) )
        std::string toHash = rawNonce + createdTime + password;
        std::vector<uint8_t> sha1Result = SHA1::hash(toHash);

        std::string passwordDigest = Base64::encode(sha1Result);
        std::string nonceBase64 = Base64::encode(reinterpret_cast<const uint8_t *>(rawNonce.data()), rawNonce.size());

        std::ostringstream ss;
        ss << "    <wsse:Security xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\" "
           << "xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">\r\n"
           << "      <wsse:UsernameToken>\r\n"
           << "        <wsse:Username>" << username << "</wsse:Username>\r\n"
           << "        <wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">"
           << passwordDigest << "</wsse:Password>\r\n"
           << "        <wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">"
           << nonceBase64 << "</wsse:Nonce>\r\n"
           << "        <wsu:Created>" << createdTime << "</wsu:Created>\r\n"
           << "      </wsse:UsernameToken>\r\n"
           << "    </wsse:Security>\r\n";

        return ss.str();
    }

} // namespace Eyerus

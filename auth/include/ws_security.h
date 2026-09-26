#ifndef AUTH_WS_SECURITY_H
#define AUTH_WS_SECURITY_H

#include <string>

namespace Eyerus
{

    class WsSecurity
    {
    public:
        // Tạo XML SOAP Security Header theo chuẩn ONVIF WS-Security UsernameToken (PasswordDigest)
        static std::string buildSecurityHeader(const std::string &username, const std::string &password);

        // Lấy thời gian UTC định dạng ISO 8601 (vd: 2026-09-26T07:30:00Z)
        static std::string getUtcIso8601();

        // Sinh chuỗi bytes ngẫu nhiên làm Nonce
        static std::string generateNonce(size_t length = 16);
    };

} // namespace Eyerus

#endif // AUTH_WS_SECURITY_H

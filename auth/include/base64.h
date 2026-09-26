#ifndef AUTH_BASE64_H
#define AUTH_BASE64_H

#include <string>
#include <vector>
#include <cstdint>

namespace Eyerus
{

    class Base64
    {
    public:
        static std::string encode(const uint8_t *data, size_t length);
        static std::string encode(const std::vector<uint8_t> &data);
        static std::string encode(const std::string &input);
    };

} // namespace Eyerus

#endif // AUTH_BASE64_H

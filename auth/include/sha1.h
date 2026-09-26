#ifndef AUTH_SHA1_H
#define AUTH_SHA1_H

#include <string>
#include <vector>
#include <cstdint>

namespace Eyerus
{

    class SHA1
    {
    public:
        SHA1();
        void update(const uint8_t *data, size_t length);
        void update(const std::string &s);

        // Trả về 20 bytes hash nhị phân (raw binary digest)
        std::vector<uint8_t> finalBytes();

        // Tiện ích băm trực tiếp chuỗi trả về 20 bytes
        static std::vector<uint8_t> hash(const uint8_t *data, size_t length);
        static std::vector<uint8_t> hash(const std::string &input);

    private:
        uint32_t state[5];
        uint32_t count[2];
        uint8_t buffer[64];

        void transform(const uint8_t data[64]);
    };

} // namespace Eyerus

#endif // AUTH_SHA1_H

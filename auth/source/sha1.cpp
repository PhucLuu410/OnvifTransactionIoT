#include "sha1.h"
#include <cstring>

namespace Eyerus
{

    namespace
    {

        inline uint32_t rol(uint32_t value, size_t bits)
        {
            return (value << bits) | (value >> (32 - bits));
        }

        inline uint32_t blk0(const uint8_t *block, size_t i)
        {
            return (static_cast<uint32_t>(block[i * 4]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(block[i * 4 + 3]));
        }

        inline uint32_t blk(const uint32_t *block, size_t i)
        {
            return rol(block[(i + 13) & 15] ^ block[(i + 8) & 15] ^ block[(i + 2) & 15] ^ block[i & 15], 1);
        }

        void r0(const uint32_t *block, uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i)
        {
            z += ((w & (x ^ y)) ^ y) + block[i] + 0x5a827999 + rol(v, 5);
            w = rol(w, 30);
        }

        void r1(uint32_t *block, uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i)
        {
            block[i & 15] = blk(block, i);
            z += ((w & (x ^ y)) ^ y) + block[i & 15] + 0x5a827999 + rol(v, 5);
            w = rol(w, 30);
        }

        void r2(uint32_t *block, uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i)
        {
            block[i & 15] = blk(block, i);
            z += (w ^ x ^ y) + block[i & 15] + 0x6ed9eba1 + rol(v, 5);
            w = rol(w, 30);
        }

        void r3(uint32_t *block, uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i)
        {
            block[i & 15] = blk(block, i);
            z += (((w | x) & y) | (w & x)) + block[i & 15] + 0x8f1bbcdc + rol(v, 5);
            w = rol(w, 30);
        }

        void r4(uint32_t *block, uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, size_t i)
        {
            block[i & 15] = blk(block, i);
            z += (w ^ x ^ y) + block[i & 15] + 0xca62c1d6 + rol(v, 5);
            w = rol(w, 30);
        }

    } // anonymous namespace

    SHA1::SHA1()
    {
        state[0] = 0x67452301;
        state[1] = 0xefcdab89;
        state[2] = 0x98badcfe;
        state[3] = 0x10325476;
        state[4] = 0xc3d2e1f0;
        count[0] = count[1] = 0;
    }

    void SHA1::transform(const uint8_t data[64])
    {
        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];
        uint32_t block[16];

        for (size_t i = 0; i < 16; ++i)
        {
            block[i] = blk0(data, i);
        }

        r0(block, a, b, c, d, e, 0);
        r0(block, e, a, b, c, d, 1);
        r0(block, d, e, a, b, c, 2);
        r0(block, c, d, e, a, b, 3);
        r0(block, b, c, d, e, a, 4);
        r0(block, a, b, c, d, e, 5);
        r0(block, e, a, b, c, d, 6);
        r0(block, d, e, a, b, c, 7);
        r0(block, c, d, e, a, b, 8);
        r0(block, b, c, d, e, a, 9);
        r0(block, a, b, c, d, e, 10);
        r0(block, e, a, b, c, d, 11);
        r0(block, d, e, a, b, c, 12);
        r0(block, c, d, e, a, b, 13);
        r0(block, b, c, d, e, a, 14);
        r0(block, a, b, c, d, e, 15);

        r1(block, e, a, b, c, d, 16);
        r1(block, d, e, a, b, c, 17);
        r1(block, c, d, e, a, b, 18);
        r1(block, b, c, d, e, a, 19);

        r2(block, a, b, c, d, e, 20);
        r2(block, e, a, b, c, d, 21);
        r2(block, d, e, a, b, c, 22);
        r2(block, c, d, e, a, b, 23);
        r2(block, b, c, d, e, a, 24);
        r2(block, a, b, c, d, e, 25);
        r2(block, e, a, b, c, d, 26);
        r2(block, d, e, a, b, c, 27);
        r2(block, c, d, e, a, b, 28);
        r2(block, b, c, d, e, a, 29);
        r2(block, a, b, c, d, e, 30);
        r2(block, e, a, b, c, d, 31);
        r2(block, d, e, a, b, c, 32);
        r2(block, c, d, e, a, b, 33);
        r2(block, b, c, d, e, a, 34);
        r2(block, a, b, c, d, e, 35);
        r2(block, e, a, b, c, d, 36);
        r2(block, d, e, a, b, c, 37);
        r2(block, c, d, e, a, b, 38);
        r2(block, b, c, d, e, a, 39);

        r3(block, a, b, c, d, e, 40);
        r3(block, e, a, b, c, d, 41);
        r3(block, d, e, a, b, c, 42);
        r3(block, c, d, e, a, b, 43);
        r3(block, b, c, d, e, a, 44);
        r3(block, a, b, c, d, e, 45);
        r3(block, e, a, b, c, d, 46);
        r3(block, d, e, a, b, c, 47);
        r3(block, c, d, e, a, b, 48);
        r3(block, b, c, d, e, a, 49);
        r3(block, a, b, c, d, e, 50);
        r3(block, e, a, b, c, d, 51);
        r3(block, d, e, a, b, c, 52);
        r3(block, c, d, e, a, b, 53);
        r3(block, b, c, d, e, a, 54);
        r3(block, a, b, c, d, e, 55);
        r3(block, e, a, b, c, d, 56);
        r3(block, d, e, a, b, c, 57);
        r3(block, c, d, e, a, b, 58);
        r3(block, b, c, d, e, a, 59);

        r4(block, a, b, c, d, e, 60);
        r4(block, e, a, b, c, d, 61);
        r4(block, d, e, a, b, c, 62);
        r4(block, c, d, e, a, b, 63);
        r4(block, b, c, d, e, a, 64);
        r4(block, a, b, c, d, e, 65);
        r4(block, e, a, b, c, d, 66);
        r4(block, d, e, a, b, c, 67);
        r4(block, c, d, e, a, b, 68);
        r4(block, b, c, d, e, a, 69);
        r4(block, a, b, c, d, e, 70);
        r4(block, e, a, b, c, d, 71);
        r4(block, d, e, a, b, c, 72);
        r4(block, c, d, e, a, b, 73);
        r4(block, b, c, d, e, a, 74);
        r4(block, a, b, c, d, e, 75);
        r4(block, e, a, b, c, d, 76);
        r4(block, d, e, a, b, c, 77);
        r4(block, c, d, e, a, b, 78);
        r4(block, b, c, d, e, a, 79);

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }

    void SHA1::update(const uint8_t *data, size_t length)
    {
        size_t i = 0;
        size_t j = (count[0] >> 3) & 63;
        if ((count[0] += static_cast<uint32_t>(length << 3)) < static_cast<uint32_t>(length << 3))
        {
            count[1]++;
        }
        count[1] += static_cast<uint32_t>(length >> 29);

        if ((j + length) > 63)
        {
            std::memcpy(&buffer[j], data, (i = 64 - j));
            transform(buffer);
            for (; i + 63 < length; i += 64)
            {
                transform(&data[i]);
            }
            j = 0;
        }
        std::memcpy(&buffer[j], &data[i], length - i);
    }

    void SHA1::update(const std::string &s)
    {
        update(reinterpret_cast<const uint8_t *>(s.data()), s.length());
    }

    std::vector<uint8_t> SHA1::finalBytes()
    {
        uint8_t finalcount[8];
        for (size_t i = 0; i < 8; ++i)
        {
            finalcount[i] = static_cast<uint8_t>((count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
        }
        uint8_t c = 0200;
        update(&c, 1);
        while ((count[0] & 504) != 448)
        {
            c = 0000;
            update(&c, 1);
        }
        update(finalcount, 8);

        std::vector<uint8_t> digest(20);
        for (size_t i = 0; i < 20; ++i)
        {
            digest[i] = static_cast<uint8_t>((state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
        }
        return digest;
    }

    std::vector<uint8_t> SHA1::hash(const uint8_t *data, size_t length)
    {
        SHA1 checksum;
        checksum.update(data, length);
        return checksum.finalBytes();
    }

    std::vector<uint8_t> SHA1::hash(const std::string &input)
    {
        return hash(reinterpret_cast<const uint8_t *>(input.data()), input.length());
    }

} // namespace Eyerus

#include "Sha256.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr std::array<std::uint32_t, 64> kRound = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

std::uint32_t rotateRight(std::uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32 - bits));
}

class Sha256 {
public:
    void update(const unsigned char* data, std::size_t length) {
        for (std::size_t i = 0; i < length; ++i) {
            buffer_[bufferLength_++] = data[i];
            if (bufferLength_ == buffer_.size()) {
                compress(buffer_.data());
                bufferLength_ = 0;
            }
        }
        totalBytes_ += length;
    }

    std::string finish() {
        const std::uint64_t bits = totalBytes_ * 8;
        const unsigned char one = 0x80;
        update(&one, 1);
        const unsigned char zero = 0;
        while (bufferLength_ != 56) {
            update(&zero, 1);
        }
        std::array<unsigned char, 8> length{};
        for (std::size_t i = 0; i < length.size(); ++i) {
            length[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
        }
        update(length.data(), length.size());

        std::string hex;
        char digits[3];
        for (std::uint32_t word : state_) {
            for (int shift = 24; shift >= 0; shift -= 8) {
                std::snprintf(digits, sizeof digits, "%02x", (word >> shift) & 0xff);
                hex += digits;
            }
        }
        return hex;
    }

private:
    void compress(const unsigned char* block) {
        std::array<std::uint32_t, 64> schedule{};
        for (std::size_t i = 0; i < 16; ++i) {
            schedule[i] = (std::uint32_t(block[4 * i]) << 24) | (std::uint32_t(block[4 * i + 1]) << 16) |
                          (std::uint32_t(block[4 * i + 2]) << 8) | std::uint32_t(block[4 * i + 3]);
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotateRight(schedule[i - 15], 7) ^ rotateRight(schedule[i - 15], 18) ^ (schedule[i - 15] >> 3);
            const std::uint32_t s1 = rotateRight(schedule[i - 2], 17) ^ rotateRight(schedule[i - 2], 19) ^ (schedule[i - 2] >> 10);
            schedule[i] = schedule[i - 16] + s0 + schedule[i - 7] + s1;
        }

        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
            const std::uint32_t choose = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = h + sum1 + choose + kRound[i] + schedule[i];
            const std::uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                           0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<unsigned char, 64> buffer_{};
    std::size_t bufferLength_ = 0;
    std::uint64_t totalBytes_ = 0;
};

}  // namespace

std::optional<std::string> sha256OfFile(const std::filesystem::path& file) {
    std::FILE* in = std::fopen(file.string().c_str(), "rb");
    if (!in) {
        return std::nullopt;
    }
    Sha256 hash;
    std::vector<unsigned char> chunk(1 << 16);
    std::size_t count = 0;
    while ((count = std::fread(chunk.data(), 1, chunk.size(), in)) > 0) {
        hash.update(chunk.data(), count);
    }
    const bool failed = std::ferror(in) != 0;
    std::fclose(in);
    if (failed) {
        return std::nullopt;
    }
    return hash.finish();
}

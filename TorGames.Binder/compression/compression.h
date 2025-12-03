// ============================================================================
// File: src/compression/Compression.h
// Purpose: LZ4 compression wrapper
// ============================================================================

#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace Compression {

    // Error codes
    enum class CompressionError {
        Success = 0,
        CompressionFailed,
        DecompressionFailed,
        InvalidData,
        BufferTooSmall,
        InvalidParameter
    };

    // Result structure
    struct CompressionResult {
        CompressionError error;
        std::string errorMessage;
        std::vector<uint8_t> data;
        size_t originalSize;
    };

    // Compress data using LZ4
    // Output format: [4 bytes original size] + [compressed data]
    CompressionResult Compress(
        const uint8_t* data,
        size_t dataSize
    );

    // Decompress LZ4 data
    // Input format: [4 bytes original size] + [compressed data]
    CompressionResult Decompress(
        const uint8_t* compressedData,
        size_t compressedSize
    );

    // Convenience overloads
    CompressionResult Compress(const std::vector<uint8_t>& data);
    CompressionResult Decompress(const std::vector<uint8_t>& compressedData);

    // Get maximum compressed size for given input size
    size_t GetMaxCompressedSize(size_t inputSize);

    // Check if data appears to be LZ4 compressed (basic heuristic)
    bool IsCompressed(const uint8_t* data, size_t dataSize);

} // namespace Compression
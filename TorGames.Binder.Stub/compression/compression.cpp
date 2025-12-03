// ============================================================================
// File: src/compression/Compression.cpp
// Purpose: LZ4 compression implementation
// ============================================================================

#include "Compression.h"

// Include LZ4 - adjust path as needed
#include "../lib/lz4.h"

#include <cstring>
#include <algorithm>

namespace Compression {

// Header structure: 4 bytes for original size
static const size_t HEADER_SIZE = 4;

CompressionResult Compress(const uint8_t* data, size_t dataSize) {
    CompressionResult result;
    result.error = CompressionError::Success;
    result.originalSize = dataSize;

    if (!data || dataSize == 0) {
        result.error = CompressionError::InvalidParameter;
        result.errorMessage = "Invalid input data";
        return result;
    }

    // Calculate max compressed size
    int maxCompressedSize = LZ4_compressBound(static_cast<int>(dataSize));
    if (maxCompressedSize <= 0) {
        result.error = CompressionError::CompressionFailed;
        result.errorMessage = "Failed to calculate compressed size bound";
        return result;
    }

    // Allocate buffer: header + compressed data
    result.data.resize(HEADER_SIZE + maxCompressedSize);

    // Store original size in header (little-endian)
    result.data[0] = static_cast<uint8_t>(dataSize & 0xFF);
    result.data[1] = static_cast<uint8_t>((dataSize >> 8) & 0xFF);
    result.data[2] = static_cast<uint8_t>((dataSize >> 16) & 0xFF);
    result.data[3] = static_cast<uint8_t>((dataSize >> 24) & 0xFF);

    // Compress
    int compressedSize = LZ4_compress_default(
        reinterpret_cast<const char*>(data),
        reinterpret_cast<char*>(result.data.data() + HEADER_SIZE),
        static_cast<int>(dataSize),
        maxCompressedSize
    );

    if (compressedSize <= 0) {
        result.error = CompressionError::CompressionFailed;
        result.errorMessage = "LZ4 compression failed";
        result.data.clear();
        return result;
    }

    // Resize to actual size
    result.data.resize(HEADER_SIZE + compressedSize);

    return result;
}

CompressionResult Decompress(const uint8_t* compressedData, size_t compressedSize) {
    CompressionResult result;
    result.error = CompressionError::Success;

    if (!compressedData || compressedSize < HEADER_SIZE) {
        result.error = CompressionError::InvalidParameter;
        result.errorMessage = "Invalid compressed data";
        return result;
    }

    // Read original size from header (little-endian)
    size_t originalSize =
        static_cast<size_t>(compressedData[0]) |
        (static_cast<size_t>(compressedData[1]) << 8) |
        (static_cast<size_t>(compressedData[2]) << 16) |
        (static_cast<size_t>(compressedData[3]) << 24);

    result.originalSize = originalSize;

    // Sanity check - prevent excessive allocation
    if (originalSize > 1024 * 1024 * 1024) { // 1GB max
        result.error = CompressionError::InvalidData;
        result.errorMessage = "Original size exceeds maximum (1GB)";
        return result;
    }

    // Allocate output buffer
    result.data.resize(originalSize);

    // Decompress
    int decompressedSize = LZ4_decompress_safe(
        reinterpret_cast<const char*>(compressedData + HEADER_SIZE),
        reinterpret_cast<char*>(result.data.data()),
        static_cast<int>(compressedSize - HEADER_SIZE),
        static_cast<int>(originalSize)
    );

    if (decompressedSize < 0) {
        result.error = CompressionError::DecompressionFailed;
        result.errorMessage = "LZ4 decompression failed - data may be corrupted";
        result.data.clear();
        return result;
    }

    if (static_cast<size_t>(decompressedSize) != originalSize) {
        result.error = CompressionError::InvalidData;
        result.errorMessage = "Decompressed size mismatch";
        result.data.clear();
        return result;
    }

    return result;
}

CompressionResult Compress(const std::vector<uint8_t>& data) {
    return Compress(data.data(), data.size());
}

CompressionResult Decompress(const std::vector<uint8_t>& compressedData) {
    return Decompress(compressedData.data(), compressedData.size());
}

size_t GetMaxCompressedSize(size_t inputSize) {
    return HEADER_SIZE + LZ4_compressBound(static_cast<int>(inputSize));
}

bool IsCompressed(const uint8_t* data, size_t dataSize) {
    if (!data || dataSize < HEADER_SIZE) {
        return false;
    }

    // Read claimed original size
    size_t claimedSize =
        static_cast<size_t>(data[0]) |
        (static_cast<size_t>(data[1]) << 8) |
        (static_cast<size_t>(data[2]) << 16) |
        (static_cast<size_t>(data[3]) << 24);

    // Basic sanity check: original size should be reasonable
    // and compressed size should be smaller or similar
    return claimedSize > 0 &&
           claimedSize < 1024 * 1024 * 1024 &&
           dataSize > HEADER_SIZE;
}

} // namespace Compression
// //////////////////////////////////////////////////////////
// sha512.h
// Copyright (c) 2014,2015,2026 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
//

#pragma once

#include <string>

// define fixed size integer types
#ifdef _MSC_VER
typedef unsigned __int8  uint8_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

/// compute SHA512 hash
class SHA512
{
public:
    enum { BlockSize = 128, HashBytes = 64 };

    SHA512();

    std::string operator()(const void* data, size_t numBytes);
    std::string operator()(const std::string& text);

    void add(const void* data, size_t numBytes);
    std::string getHash();
    void getHash(unsigned char buffer[HashBytes]);
    void reset();

private:
    void processBlock(const void* data);
    void processBuffer();

private:
    uint64_t m_numBytes;
    size_t   m_bufferSize;
    uint8_t  m_buffer[BlockSize];
    uint64_t m_hash[8];
};
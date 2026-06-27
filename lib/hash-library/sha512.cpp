// //////////////////////////////////////////////////////////
// sha512.cpp
// Copyright (c) 2014,2015,2026 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
//

#include "sha512.h"

// Windows 平台不需要 endian.h
#if defined(_WIN32) || defined(_WIN64)
// Windows 字节序处理
#define __BYTE_ORDER __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#else
#include <endian.h>
#endif

namespace
{
  inline uint64_t rotate(uint64_t x, uint64_t bits)
  {
    return (x >> bits) | (x << (64 - bits));
  }

  inline uint64_t swap(uint64_t x)
  {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(x);
#elif defined(_MSC_VER)
    return _byteswap_uint64(x);
#else
    return
      ((x & 0x00000000000000FFULL) << 56) |
      ((x & 0x000000000000FF00ULL) << 40) |
      ((x & 0x0000000000FF0000ULL) << 24) |
      ((x & 0x00000000FF000000ULL) << 8 ) |
      ((x & 0x000000FF00000000ULL) >> 8 ) |
      ((x & 0x0000FF0000000000ULL) >> 24) |
      ((x & 0x00FF000000000000ULL) >> 40) |
      ((x & 0xFF00000000000000ULL) >> 56);
#endif
  }

  inline uint64_t Ch(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (~x & z); }
  inline uint64_t Maj(uint64_t x, uint64_t y, uint64_t z){ return (x & y) ^ (x & z) ^ (y & z); }
  inline uint64_t SIGMA0(uint64_t x) { return rotate(x,28) ^ rotate(x,34) ^ rotate(x,39); }
  inline uint64_t SIGMA1(uint64_t x) { return rotate(x,14) ^ rotate(x,18) ^ rotate(x,41); }
  inline uint64_t sigma0(uint64_t x) { return rotate(x,1) ^ rotate(x,8) ^ (x>>7); }
  inline uint64_t sigma1(uint64_t x) { return rotate(x,19) ^ rotate(x,61) ^ (x>>6); }

  static const uint64_t K[80] =
  {
    0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
    0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
    0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
    0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL
  };
}

// ---------------------------------------
SHA512::SHA512()
{
  reset();
}

void SHA512::reset()
{
  m_numBytes = 0;
  m_bufferSize = 0;

  m_hash[0] = 0x6a09e667f3bcc908ULL;
  m_hash[1] = 0xbb67ae8584caa73bULL;
  m_hash[2] = 0x3c6ef372fe94f82bULL;
  m_hash[3] = 0xa54ff53a5f1d36f1ULL;
  m_hash[4] = 0x510e527fade682d1ULL;
  m_hash[5] = 0x9b05688c2b3e6c1fULL;
  m_hash[6] = 0x1f83d9abfb41bd6bULL;
  m_hash[7] = 0x5be0cd19137e2179ULL;
}

// ---------------------------------------
void SHA512::processBlock(const void* data)
{
  const uint64_t* input = (const uint64_t*)data;
  uint64_t W[80];
  for (int i=0;i<16;i++)
    W[i] = swap(input[i]);
  for (int i=16;i<80;i++)
    W[i] = sigma1(W[i-2]) + W[i-7] + sigma0(W[i-15]) + W[i-16];

  uint64_t a = m_hash[0];
  uint64_t b = m_hash[1];
  uint64_t c = m_hash[2];
  uint64_t d = m_hash[3];
  uint64_t e = m_hash[4];
  uint64_t f = m_hash[5];
  uint64_t g = m_hash[6];
  uint64_t h = m_hash[7];

  for (int i=0;i<80;i++)
  {
    uint64_t T1 = h + SIGMA1(e) + Ch(e,f,g) + K[i] + W[i];
    uint64_t T2 = SIGMA0(a) + Maj(a,b,c);
    h=g; g=f; f=e; e=d+T1; d=c; c=b; b=a; a=T1+T2;
  }

  m_hash[0]+=a;
  m_hash[1]+=b;
  m_hash[2]+=c;
  m_hash[3]+=d;
  m_hash[4]+=e;
  m_hash[5]+=f;
  m_hash[6]+=g;
  m_hash[7]+=h;
}

// ---------------------------------------
void SHA512::add(const void* data, size_t numBytes)
{
  const uint8_t* current = (const uint8_t*)data;

  if (m_bufferSize > 0)
  {
    while (numBytes>0 && m_bufferSize<BlockSize)
    {
      m_buffer[m_bufferSize++] = *current++;
      numBytes--;
    }
  }

  if (m_bufferSize==BlockSize)
  {
    processBlock(m_buffer);
    m_numBytes += BlockSize;
    m_bufferSize = 0;
  }

  while (numBytes >= BlockSize)
  {
    processBlock(current);
    current += BlockSize;
    m_numBytes += BlockSize;
    numBytes -= BlockSize;
  }

  while (numBytes>0)
  {
    m_buffer[m_bufferSize++] = *current++;
    numBytes--;
  }
}

// ---------------------------------------
void SHA512::processBuffer()
{
  uint64_t bitLen = m_numBytes*8 + m_bufferSize*8;

  size_t padSize = BlockSize - m_bufferSize;
  if (padSize < 17) padSize += BlockSize;
  unsigned char pad[256] = {0};
  pad[0] = 0x80;
  uint64_t high = 0;
  uint64_t low = bitLen;
  // SHA512 uses 128-bit length (high+low), here assume high=0 for simplicity
  for (int i=0;i<8;i++)
    pad[padSize-16+i] = (high >> (56-8*i)) & 0xFF;
  for (int i=0;i<8;i++)
    pad[padSize-8+i] = (low >> (56-8*i)) & 0xFF;

  add(pad, padSize);
}

// ---------------------------------------
void SHA512::getHash(unsigned char buffer[SHA512::HashBytes])
{
  uint64_t oldHash[8];
  for (int i=0;i<8;i++) oldHash[i] = m_hash[i];

  processBuffer();

  for (int i=0;i<8;i++)
  {
    buffer[i*8+0] = (m_hash[i]>>56) & 0xFF;
    buffer[i*8+1] = (m_hash[i]>>48) & 0xFF;
    buffer[i*8+2] = (m_hash[i]>>40) & 0xFF;
    buffer[i*8+3] = (m_hash[i]>>32) & 0xFF;
    buffer[i*8+4] = (m_hash[i]>>24) & 0xFF;
    buffer[i*8+5] = (m_hash[i]>>16) & 0xFF;
    buffer[i*8+6] = (m_hash[i]>>8 ) & 0xFF;
    buffer[i*8+7] =  m_hash[i]      & 0xFF;
    m_hash[i] = oldHash[i];
  }
}

// ---------------------------------------
std::string SHA512::getHash()
{
  unsigned char raw[HashBytes];
  getHash(raw);
  static const char hex[]="0123456789abcdef";
  std::string result;
  result.reserve(2*HashBytes);
  for (int i=0;i<HashBytes;i++)
  {
    result += hex[(raw[i]>>4)&0xF];
    result += hex[ raw[i]    &0xF];
  }
  return result;
}

// ---------------------------------------
std::string SHA512::operator()(const void* data, size_t numBytes)
{
  reset();
  add(data,numBytes);
  return getHash();
}

std::string SHA512::operator()(const std::string& text)
{
  reset();
  add(text.c_str(), text.size());
  return getHash();
}
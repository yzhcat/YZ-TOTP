// SHA512 test program
// g++ test_sha512.cpp ../sha512.cpp -o test_sha512 && ./test_sha512
#include "../sha512.h"
#include <iostream> // for std::cout only, not needed for hashing library

int main(int, char**)
{
  // create a new hashing object
  SHA512 sha512;

  // hashing an std::string
  std::cout << "expected: \n2c74fd17edafd80e8447b0d46741ee243b7eb74dd2149a0ab1b9246fb30382f27e853d8585719e0e67cbda0daa8f51671064615d645ae27acb15bfb1447f459b\n";
  std::cout << sha512("Hello World") << std::endl;

  // hashing a buffer of bytes
  std::cout << "expected: \n903f05ea13c15c1ced5f279bfeeb69d8a072d66644a7c12c27baa5de89523acca2e23ee2f12d384cc87cd2064d0cfe9e79d3cfdb500cf54f64b02565712a56a5\n";
  const char* buffer = "How are you";
  std::cout << sha512(buffer, 11) << std::endl;

  // or in a streaming fashion (re-use "How are you")
  SHA512 sha512stream;
  const char* url = "create.stephan-brumme.com"; // 25 bytes
  int step = 5;
  for (int i = 0; i < 25; i += step)
    sha512stream.add(url + i, step); // add five bytes at a time
  std::cout << "expected: \n087446740b59a861720ea7f9ac4f80961aa5a3bd48c694629af0aca8a991abe6e64b4e866093d2721e7e6085f2b9842c37f24a4e586cd178c742046ccbbfa6fb\n";
  std::cout << sha512stream.getHash() << std::endl;

  return 0;
}
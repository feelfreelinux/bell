#pragma once

#include <stdexcept>
#include <vector>
#include "BellLogger.h"
#include "mbedtls/ssl.h"

namespace bell::X509Bundle {

typedef struct crt_bundle_t {
  const uint8_t** crts;
  uint16_t num_certs;
  size_t x509_crt_bundle_len;
} crt_bundle_t;

static crt_bundle_t s_crt_bundle;
static std::vector<uint8_t> bundleBytes;

static constexpr auto TAG = "X509Bundle";
static constexpr auto CRT_HEADER_OFFSET = 4;
static constexpr auto BUNDLE_HEADER_OFFSET = 2;

int crtCheckCertificate(mbedtls_x509_crt* child, const uint8_t* pub_key_buf,
                        size_t pub_key_len);
/* This callback is called for every certificate in the chain. If the chain
 * is proper each intermediate certificate is validated through its parent
 * in the x509_crt_verify_chain() function. So this callback should
 * only verify the first untrusted link in the chain is signed by the
 * root certificate in the trusted bundle
*/
int crtVerifyCallback(void* buf, mbedtls_x509_crt* crt, int depth,
                      uint32_t* flags);

/* Initialize the bundle into an array so we can do binary search for certs,
   the bundle generated by the python utility is already presorted by subject name
 */
void init(const uint8_t* x509_bundle, size_t bundle_size);

void attach(mbedtls_ssl_config* conf);

bool shouldVerify();
};  // namespace bell::X509Bundle
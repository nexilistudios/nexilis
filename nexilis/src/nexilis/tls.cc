/* Copyright (C) 2026 Valtteri Viirret
   This file is part of the Nexilis Project.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this file.  If not, see <https://gnu.org>. */

#include <nexilis/tls.hh>

#include <nexilis/crypto.hh>

#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <array>
#include <cstring>
#include <ctime>
#include <memory>
#include <optional>
#include <vector>

namespace nexilis::tls
{
namespace
{

void freePskExData(void* /*parent*/,
                   void* ptr,
                   CRYPTO_EX_DATA* /*ad*/,
                   int /*idx*/,
                   long /*argl*/,
                   void* /*argp*/)
{
    delete static_cast<std::vector<unsigned char>*>(ptr);
}

int pskExDataIndex()
{
    static const int index = SSL_CTX_get_ex_new_index(0, nullptr, nullptr, nullptr, freePskExData);
    return index;
}

const std::vector<unsigned char>* pskFromContext(SSL* ssl)
{
    SSL_CTX* ctx = SSL_get_SSL_CTX(ssl);
    if (!ctx)
    {
        return nullptr;
    }
    return static_cast<const std::vector<unsigned char>*>(SSL_CTX_get_ex_data(ctx, pskExDataIndex()));
}

std::optional<std::array<unsigned char, 32>> publicKeyDigest(X509* certificate)
{
    std::array<unsigned char, 32> digest{};
    if (!certificate)
        return std::nullopt;
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(X509_get_pubkey(certificate), EVP_PKEY_free);
    if (!key)
        return std::nullopt;
    const int length = i2d_PUBKEY(key.get(), nullptr);
    if (length <= 0)
        return std::nullopt;
    std::vector<unsigned char> der(static_cast<std::size_t>(length));
    auto* output = der.data();
    if (i2d_PUBKEY(key.get(), &output) != length ||
        EVP_Digest(der.data(), der.size(), digest.data(), nullptr, EVP_sha256(), nullptr) != 1)
        return std::nullopt;
    return digest;
}

std::string toHex(const std::array<unsigned char, 32>& digest)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(digest.size() * 2);
    for (auto byte : digest)
    {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 15]);
    }
    return result;
}

int hexDigit(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

bool parsePin(std::string_view hex, std::array<unsigned char, 32>& pin)
{
    if (hex.size() != pin.size() * 2)
        return false;
    for (std::size_t i = 0; i < pin.size(); ++i)
    {
        const int high = hexDigit(hex[i * 2]);
        const int low = hexDigit(hex[i * 2 + 1]);
        if (high < 0 || low < 0)
            return false;
        pin[i] = static_cast<unsigned char>((high << 4) | low);
    }
    return true;
}

bool configureCertificateTls(SSL_CTX* context)
{
    return SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) == 1 &&
           SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION) != 0;
}

unsigned int pskClientCallback(SSL* ssl,
                               const char* /*hint*/,
                               char* identity,
                               unsigned int max_identity_len,
                               unsigned char* psk,
                               unsigned int max_psk_len)
{
    const auto* key = pskFromContext(ssl);
    if (!key || key->size() > max_psk_len)
    {
        return 0;
    }

    constexpr std::size_t identity_len = sizeof(kPskIdentity); // includes the NUL terminator.
    if (max_identity_len < identity_len)
    {
        return 0;
    }
    std::memcpy(identity, kPskIdentity, identity_len);
    std::memcpy(psk, key->data(), key->size());
    return static_cast<unsigned int>(key->size());
}

unsigned int pskServerCallback(SSL* ssl,
                               const char* /*identity*/,
                               unsigned char* psk,
                               unsigned int max_psk_len)
{
    const auto* key = pskFromContext(ssl);
    if (!key || key->size() > max_psk_len)
    {
        return 0;
    }
    std::memcpy(psk, key->data(), key->size());
    return static_cast<unsigned int>(key->size());
}

} // namespace

std::string derivePsk(const std::string& password)
{
    // Domain-separated from the room-message keys: the same password produces
    // unrelated keys for message encryption and for the TLS transport.
    return crypto::deriveKey(password, "nexilis-tls-psk-v1:");
}

std::shared_ptr<boost::asio::ssl::context> createPskContext(const std::string& password, bool server)
{
    const std::string psk = derivePsk(password);
    if (psk.size() != kPskLength)
    {
        return nullptr;
    }

    auto ctx = std::make_shared<boost::asio::ssl::context>(
            server ? boost::asio::ssl::context::tls_server
                   : boost::asio::ssl::context::tls_client);

    ctx->set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::no_tlsv1 |
            boost::asio::ssl::context::no_tlsv1_1);

    SSL_CTX* native = ctx->native_handle();

    // Restrict the ciphers to PSK key exchange so no certificates are needed,
    // and to SHA-256 based suites so the 32-byte key always matches the PRF.
    if (SSL_CTX_set_cipher_list(native, "PSK-AES128-GCM-SHA256:PSK-CHACHA20-POLY1305") != 1)
    {
        return nullptr;
    }
    if (SSL_CTX_set_ciphersuites(native, "TLS_AES_128_GCM_SHA256:TLS_CHACHA20_POLY1305_SHA256") != 1)
    {
        return nullptr;
    }

    // Store the key against the context so multiple servers/clients with
    // different passwords can coexist; OpenSSL calls the free function when
    // the context is destroyed.
    auto* keyStorage = new std::vector<unsigned char>(psk.begin(), psk.end());
    SSL_CTX_set_ex_data(native, pskExDataIndex(), keyStorage);

    if (server)
    {
        SSL_CTX_set_psk_server_callback(native, pskServerCallback);
    }
    else
    {
        SSL_CTX_set_psk_client_callback(native, pskClientCallback);
    }

    return ctx;
}

std::string publicKeySha256(std::string_view certificatePem)
{
    if (certificatePem.empty())
        return {};
    std::unique_ptr<BIO, decltype(&BIO_free)> input(
            BIO_new_mem_buf(certificatePem.data(), static_cast<int>(certificatePem.size())), BIO_free);
    if (!input)
        return {};
    std::unique_ptr<X509, decltype(&X509_free)> certificate(
            PEM_read_bio_X509(input.get(), nullptr, nullptr, nullptr), X509_free);
    const auto digest = publicKeyDigest(certificate.get());
    return digest ? toHex(*digest) : std::string();
}

std::shared_ptr<boost::asio::ssl::context> createServerContext(
        std::string_view certificatePem, std::string_view privateKeyPem)
{
    if (certificatePem.empty() || privateKeyPem.empty())
        return nullptr;
    try
    {
        auto context = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_server);
        if (!configureCertificateTls(context->native_handle()))
            return nullptr;
        context->use_certificate_chain(boost::asio::buffer(certificatePem.data(), certificatePem.size()));
        context->use_private_key(boost::asio::buffer(privateKeyPem.data(), privateKeyPem.size()),
                                 boost::asio::ssl::context::pem);
        if (SSL_CTX_check_private_key(context->native_handle()) != 1)
            return nullptr;
        return context;
    }
    catch (...)
    {
        return nullptr;
    }
}

std::shared_ptr<boost::asio::ssl::context> createClientContext()
{
    auto context = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_client);
    return configureCertificateTls(context->native_handle()) ? context : nullptr;
}

bool validPublicKeySha256(std::string_view expectedHex)
{
    std::array<unsigned char, 32> pin{};
    return parsePin(expectedHex, pin);
}

bool verifyServerKey(SSL* ssl, std::string_view expectedHex)
{
    std::array<unsigned char, 32> expected{};
    if (!ssl || !parsePin(expectedHex, expected))
        return false;
    std::unique_ptr<X509, decltype(&X509_free)> certificate(
            SSL_get_peer_certificate(ssl), X509_free);
    std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)> now(
            ASN1_TIME_set(nullptr, std::time(nullptr)), ASN1_TIME_free);
    if (!certificate || !now)
        return false;
    const int starts = ASN1_TIME_compare(X509_get0_notBefore(certificate.get()), now.get());
    const int ends = ASN1_TIME_compare(X509_get0_notAfter(certificate.get()), now.get());
    if (starts == -2 || ends == -2 || starts > 0 || ends <= 0)
        return false;
    const auto actual = publicKeyDigest(certificate.get());
    return actual && CRYPTO_memcmp(actual->data(), expected.data(), expected.size()) == 0;
}

ServerIdentity generateSelfSignedIdentity()
{
    ServerIdentity identity;
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> keyContext(
            EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
    if (!keyContext || EVP_PKEY_keygen_init(keyContext.get()) != 1 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(keyContext.get(), 3072) != 1)
        return identity;
    EVP_PKEY* rawKey = nullptr;
    if (EVP_PKEY_keygen(keyContext.get(), &rawKey) != 1)
        return identity;
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(rawKey, EVP_PKEY_free);
    std::unique_ptr<X509, decltype(&X509_free)> certificate(X509_new(), X509_free);
    if (!certificate || X509_set_version(certificate.get(), 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 1) != 1 ||
        !X509_gmtime_adj(X509_getm_notBefore(certificate.get()), 0) ||
        !X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 365L * 24 * 60 * 60) ||
        X509_set_pubkey(certificate.get(), key.get()) != 1)
        return identity;
    std::unique_ptr<X509_NAME, decltype(&X509_NAME_free)> name(X509_NAME_new(), X509_NAME_free);
    if (!name || X509_NAME_add_entry_by_txt(name.get(), "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Nexilis server"), -1, -1, 0) != 1 ||
        X509_set_subject_name(certificate.get(), name.get()) != 1 ||
        X509_set_issuer_name(certificate.get(), name.get()) != 1 ||
        X509_sign(certificate.get(), key.get(), EVP_sha256()) <= 0)
        return identity;
    std::unique_ptr<BIO, decltype(&BIO_free)> certOutput(BIO_new(BIO_s_mem()), BIO_free);
    std::unique_ptr<BIO, decltype(&BIO_free)> keyOutput(BIO_new(BIO_s_mem()), BIO_free);
    if (!certOutput || !keyOutput || PEM_write_bio_X509(certOutput.get(), certificate.get()) != 1 ||
        PEM_write_bio_PrivateKey(keyOutput.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1)
        return identity;
    char* certData = nullptr;
    char* keyData = nullptr;
    const auto certSize = BIO_get_mem_data(certOutput.get(), &certData);
    const auto keySize = BIO_get_mem_data(keyOutput.get(), &keyData);
    if (certSize <= 0 || keySize <= 0)
        return identity;
    identity.certificatePem.assign(certData, static_cast<std::size_t>(certSize));
    identity.privateKeyPem.assign(keyData, static_cast<std::size_t>(keySize));
    const auto digest = publicKeyDigest(certificate.get());
    if (!digest)
        return {};
    identity.publicKeySha256 = toHex(*digest);
    return identity;
}

} // namespace nexilis::tls

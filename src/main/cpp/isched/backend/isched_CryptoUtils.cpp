// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_CryptoUtils.cpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief AES-256-GCM + HKDF implementation (T048-001a).
 */
#include "isched_CryptoUtils.hpp"

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace isched::v0_0_1::backend {

// ── RAII helpers ─────────────────────────────────────────────────────────────

namespace {

struct EvpCtxDeleter {
    void operator()(EVP_CIPHER_CTX* p) const noexcept { EVP_CIPHER_CTX_free(p); }
};
using EvpCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, EvpCtxDeleter>;

struct EvpPkeyCtxDeleter {
    void operator()(EVP_PKEY_CTX* p) const noexcept { EVP_PKEY_CTX_free(p); }
};
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter>;

// ── Base-64 helpers (OpenSSL EVP — no newlines) ───────────────────────────────

[[nodiscard]] std::string b64_encode(const unsigned char* data, std::size_t len) {
    const std::size_t enc_len = ((len + 2) / 3) * 4 + 1;
    std::vector<unsigned char> out(enc_len);
    const int written =
        EVP_EncodeBlock(out.data(), data, static_cast<int>(len));
    return {reinterpret_cast<const char*>(out.data()), static_cast<std::size_t>(written)};
}

[[nodiscard]] std::vector<unsigned char> b64_decode(const std::string& encoded) {
    if (encoded.empty()) return {};

    const std::size_t max_len = ((encoded.size() + 3) / 4) * 3;
    std::vector<unsigned char> out(max_len);
    const int decoded = EVP_DecodeBlock(
        out.data(),
        reinterpret_cast<const unsigned char*>(encoded.c_str()),
        static_cast<int>(encoded.size()));
    if (decoded < 0)
        throw std::runtime_error("decrypt_secret: base64 decode failed");

    std::size_t padding = 0;
    for (auto it = encoded.rbegin();
         it != encoded.rend() && *it == '='; ++it) {
        ++padding;
    }
    out.resize(static_cast<std::size_t>(decoded) - padding);
    return out;
}

// ── Key derivation: HKDF-SHA256 ──────────────────────────────────────────────

[[nodiscard]] std::vector<unsigned char> derive_key(const std::string& master_secret,
                                                     const std::string& tenant_id) {
    EvpPkeyCtxPtr pctx{EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr)};
    if (!pctx)
        throw std::runtime_error("derive_key: EVP_PKEY_CTX_new_id failed");

    if (EVP_PKEY_derive_init(pctx.get()) <= 0)
        throw std::runtime_error("derive_key: EVP_PKEY_derive_init failed");

    if (EVP_PKEY_CTX_set_hkdf_md(pctx.get(), EVP_sha256()) <= 0)
        throw std::runtime_error("derive_key: set_hkdf_md failed");

    if (EVP_PKEY_CTX_set1_hkdf_key(
            pctx.get(),
            reinterpret_cast<const unsigned char*>(master_secret.c_str()),
            static_cast<int>(master_secret.size())) <= 0)
        throw std::runtime_error("derive_key: set1_hkdf_key failed");

    if (EVP_PKEY_CTX_add1_hkdf_info(
            pctx.get(),
            reinterpret_cast<const unsigned char*>(tenant_id.c_str()),
            static_cast<int>(tenant_id.size())) <= 0)
        throw std::runtime_error("derive_key: add1_hkdf_info failed");

    std::vector<unsigned char> key(32);
    std::size_t key_len = 32;
    if (EVP_PKEY_derive(pctx.get(), key.data(), &key_len) <= 0)
        throw std::runtime_error("derive_key: EVP_PKEY_derive failed");

    return key;
}

} // anonymous namespace

// ── Public API ────────────────────────────────────────────────────────────────

std::string encrypt_secret(const std::string& plaintext,
                            const std::string& tenant_id,
                            const std::string& master_secret) {
    if (master_secret.empty())
        throw std::runtime_error("encrypt_secret: master_secret is empty");

    const auto key = derive_key(master_secret, tenant_id);

    // Random 12-byte IV / nonce for AES-256-GCM
    unsigned char nonce[12];
    if (RAND_bytes(nonce, 12) != 1)
        throw std::runtime_error("encrypt_secret: RAND_bytes failed");

    EvpCtxPtr ctx{EVP_CIPHER_CTX_new()};
    if (!ctx)
        throw std::runtime_error("encrypt_secret: EVP_CIPHER_CTX_new failed");

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        throw std::runtime_error("encrypt_secret: EVP_EncryptInit_ex (alg) failed");

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1)
        throw std::runtime_error("encrypt_secret: EVP_CTRL_GCM_SET_IVLEN failed");

    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), nonce) != 1)
        throw std::runtime_error("encrypt_secret: EVP_EncryptInit_ex (key/iv) failed");

    std::vector<unsigned char> ciphertext(plaintext.size() + 16 /*extra for GCM*/);
    int enc_len = 0;
    if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &enc_len,
                          reinterpret_cast<const unsigned char*>(plaintext.c_str()),
                          static_cast<int>(plaintext.size())) != 1)
        throw std::runtime_error("encrypt_secret: EVP_EncryptUpdate failed");

    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + enc_len, &final_len) != 1)
        throw std::runtime_error("encrypt_secret: EVP_EncryptFinal_ex failed");

    unsigned char tag[16];
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag) != 1)
        throw std::runtime_error("encrypt_secret: get GCM tag failed");

    // Wire format: nonce (12) | tag (16) | ciphertext
    const std::size_t ct_len = static_cast<std::size_t>(enc_len + final_len);
    std::vector<unsigned char> blob;
    blob.reserve(12 + 16 + ct_len);
    blob.insert(blob.end(), nonce, nonce + 12);
    blob.insert(blob.end(), tag, tag + 16);
    blob.insert(blob.end(), ciphertext.begin(),
                ciphertext.begin() + static_cast<std::ptrdiff_t>(ct_len));

    return b64_encode(blob.data(), blob.size());
}

std::string decrypt_secret(const std::string& base64_ciphertext,
                            const std::string& tenant_id,
                            const std::string& master_secret) {
    if (master_secret.empty())
        throw std::runtime_error("decrypt_secret: master_secret is empty");

    const auto blob = b64_decode(base64_ciphertext);
    if (blob.size() < 12 + 16)
        throw std::runtime_error("decrypt_secret: ciphertext blob too short");

    const auto key = derive_key(master_secret, tenant_id);

    const unsigned char* nonce_ptr = blob.data();
    const unsigned char* tag_ptr   = blob.data() + 12;
    const unsigned char* ct_ptr    = blob.data() + 28;
    const std::size_t    ct_len    = blob.size() - 28;

    EvpCtxPtr ctx{EVP_CIPHER_CTX_new()};
    if (!ctx)
        throw std::runtime_error("decrypt_secret: EVP_CIPHER_CTX_new failed");

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        throw std::runtime_error("decrypt_secret: EVP_DecryptInit_ex (alg) failed");

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1)
        throw std::runtime_error("decrypt_secret: EVP_CTRL_GCM_SET_IVLEN failed");

    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), nonce_ptr) != 1)
        throw std::runtime_error("decrypt_secret: EVP_DecryptInit_ex (key/iv) failed");

    std::vector<unsigned char> plaintext(ct_len);
    int dec_len = 0;
    if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &dec_len, ct_ptr,
                          static_cast<int>(ct_len)) != 1)
        throw std::runtime_error("decrypt_secret: EVP_DecryptUpdate failed");

    // Must set tag BEFORE calling EVP_DecryptFinal_ex
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast) — required by OpenSSL API
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16,
                             const_cast<unsigned char*>(tag_ptr)) != 1)
        throw std::runtime_error("decrypt_secret: failed to set GCM tag");

    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + dec_len, &final_len) != 1)
        throw std::runtime_error(
            "decrypt_secret: GCM authentication tag mismatch — data corrupted or tampered");

    return {reinterpret_cast<const char*>(plaintext.data()),
            static_cast<std::size_t>(dec_len + final_len)};
}

} // namespace isched::v0_0_1::backend

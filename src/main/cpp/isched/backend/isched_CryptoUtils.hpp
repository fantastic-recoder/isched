// SPDX-License-Identifier: MPL-2.0
/**
 * @file isched_CryptoUtils.hpp
 * @copyright Copyright (c) 2024-2026 isched contributors
 * @see LICENSE.md — Mozilla Public License 2.0
 * @brief AES-256-GCM encrypt/decrypt helpers with HKDF-derived per-tenant key (T048-001a).
 *
 * Wire format (base64-encoded): nonce (12 B) | GCM auth-tag (16 B) | ciphertext.
 * Key derivation: HKDF-SHA256(ikm=master_secret, info=tenant_id) → 32-byte AES-256 key.
 * No new dependencies — uses OpenSSL 3.x EVP API already present in the project.
 */
#pragma once

#include <string>

namespace isched::v0_0_1::backend {

/**
 * @brief Encrypt @p plaintext for @p tenant_id using AES-256-GCM.
 *
 * @param plaintext      Secret value to protect (e.g. raw API key).
 * @param tenant_id      Tenant identifier used to derive the encryption key.
 * @param master_secret  Server-level secret seed (≥ 16 bytes recommended).
 * @return Base64-encoded blob: nonce (12 B) | GCM tag (16 B) | ciphertext.
 * @throws std::runtime_error on OpenSSL failure or empty @p master_secret.
 */
[[nodiscard]] std::string encrypt_secret(const std::string& plaintext,
                                          const std::string& tenant_id,
                                          const std::string& master_secret);

/**
 * @brief Decrypt a blob previously produced by @c encrypt_secret.
 *
 * @throws std::runtime_error on GCM authentication failure, malformed input,
 *         or empty @p master_secret.
 */
[[nodiscard]] std::string decrypt_secret(const std::string& base64_ciphertext,
                                          const std::string& tenant_id,
                                          const std::string& master_secret);

} // namespace isched::v0_0_1::backend

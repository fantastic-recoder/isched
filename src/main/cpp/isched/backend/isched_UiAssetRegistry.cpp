// SPDX-License-Identifier: MPL-2.0
#include "isched_UiAssetRegistry.hpp"

// Include the generated asset map.  This file is produced at build time by
// tools/embed_ui_assets.py (CMake target: isched_ui_embed).
#include "isched_ui_assets.hpp"

namespace isched::v0_0_1::backend {

// ── UiAssetRegistry implementation ──────────────────────────────────────────

UiAssetRegistry::UiAssetRegistry() {
    for (std::size_t i = 0; i < ISCHED_UI_ASSET_MAP_SIZE; ++i) {
        const auto& entry = ISCHED_UI_ASSET_MAP[i];
        m_assets.emplace(
            std::string{entry.url_path},
            UiAssetView{entry.asset.data, entry.asset.mime_type, entry.asset.etag});
    }
    m_has_index_html = m_assets.count("/index.html") > 0;
}

const UiAssetRegistry& UiAssetRegistry::instance() {
    static const UiAssetRegistry s_instance;
    return s_instance;
}

std::optional<UiAssetView> UiAssetRegistry::find(std::string_view url_path) const {
    auto it = m_assets.find(std::string{url_path});
    if (it == m_assets.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool UiAssetRegistry::has_index_html() const noexcept {
    return m_has_index_html;
}

std::size_t UiAssetRegistry::size() const noexcept {
    return m_assets.size();
}

} // namespace isched::v0_0_1::backend

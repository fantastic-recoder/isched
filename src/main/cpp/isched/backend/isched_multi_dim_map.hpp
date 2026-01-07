/**
 * @file isched_multi_dim_map.hpp
 * @brief Multidimensional associative container based on std::map
 * @author isched Development Team
 * @version 1.0.0
 */

#pragma once

#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace isched {
namespace v0_0_1 {
namespace backend {

/**
 * @brief A multidimensional associative container based on std::map
 * 
 * This container allows for nested indexing and storing values at any level.
 * Example:
 * @code
 * multi_dim_map<std::string, int> mmap;
 * mmap["a"] = 1;
 * mmap["a"]["b"] = 2;
 * @endcode
 */
template<typename T_key, typename T_value>
class multi_dim_map {
public:
    using key_type = T_key;
    using mapped_type = multi_dim_map;
    using value_type = std::pair<const T_key, multi_dim_map>;
    using key_compare = std::less<T_key>;
    using allocator_type = std::allocator<value_type>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = typename std::map<T_key, multi_dim_map>::iterator;
    using const_iterator = typename std::map<T_key, multi_dim_map>::const_iterator;
    using reverse_iterator = typename std::map<T_key, multi_dim_map>::reverse_iterator;
    using const_reverse_iterator = typename std::map<T_key, multi_dim_map>::const_reverse_iterator;

    multi_dim_map() = default;

    // Iterators
    iterator begin() noexcept { return m_children.begin(); }
    const_iterator begin() const noexcept { return m_children.begin(); }
    iterator end() noexcept { return m_children.end(); }
    const_iterator end() const noexcept { return m_children.end(); }

    const_iterator cbegin() const noexcept { return m_children.cbegin(); }
    const_iterator cend() const noexcept { return m_children.cend(); }

    reverse_iterator rbegin() noexcept { return m_children.rbegin(); }
    const_reverse_iterator rbegin() const noexcept { return m_children.rbegin(); }
    reverse_iterator rend() noexcept { return m_children.rend(); }
    const_reverse_iterator rend() const noexcept { return m_children.rend(); }

    const_reverse_iterator crbegin() const noexcept { return m_children.crbegin(); }
    const_reverse_iterator crend() const noexcept { return m_children.crend(); }

    // Capacity
    [[nodiscard]] bool empty() const noexcept { return m_children.empty(); }
    size_type size() const noexcept { return m_children.size(); }
    size_type max_size() const noexcept { return m_children.max_size(); }

    // Modifiers
    void clear() noexcept { m_children.clear(); }
    
    iterator erase(iterator pos) { return m_children.erase(pos); }
    iterator erase(const_iterator pos) { return m_children.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return m_children.erase(first, last); }
    size_type erase(const key_type& key) { return m_children.erase(key); }

    void swap(multi_dim_map& other) noexcept {
        using std::swap;
        swap(m_value, other.m_value);
        swap(m_has_value, other.m_has_value);
        swap(m_children, other.m_children);
    }

    // Lookup
    size_type count(const key_type& key) const { return m_children.count(key); }
    iterator find(const key_type& key) { return m_children.find(key); }
    const_iterator find(const key_type& key) const { return m_children.find(key); }

    bool contains(const key_type& key) const { return m_children.find(key) != m_children.end(); }

    iterator lower_bound(const key_type& key) { return m_children.lower_bound(key); }
    const_iterator lower_bound(const key_type& key) const { return m_children.lower_bound(key); }
    iterator upper_bound(const key_type& key) { return m_children.upper_bound(key); }
    const_iterator upper_bound(const key_type& key) const { return m_children.upper_bound(key); }

    std::pair<iterator, iterator> equal_range(const key_type& key) { return m_children.equal_range(key); }
    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const { return m_children.equal_range(key); }
    
    // Support assignment: my_mmap[0] = 0
    multi_dim_map& operator=(const T_value& val) {
        m_value = val;
        m_has_value = true;
        return *this;
    }

    // Support indexing: my_mmap[0][0]
    // We use a helper to handle potential conversion from int to std::string if needed by the requirement
    template<typename K>
    multi_dim_map& operator[](K&& key) {
        if constexpr (std::is_same_v<std::decay_t<T_key>, std::string> && std::is_integral_v<std::decay_t<K>>) {
            return m_children[std::to_string(std::forward<K>(key))];
        } else if constexpr (std::is_same_v<std::decay_t<K>, std::vector<T_key>>) {
            multi_dim_map* current = this;
            for (const auto& k : key) {
                current = &((*current)[k]);
            }
            return *current;
        } else {
            return m_children[static_cast<T_key>(std::forward<K>(key))];
        }
    }

    const multi_dim_map& operator[](const T_key& key) const {
        return m_children.at(key);
    }

    const multi_dim_map& operator[](const std::vector<T_key>& p_path) const {
        const multi_dim_map* current = this;
        for (const auto& key : p_path) {
            current = &((*current)[key]);
        }
        return *current;
    }

    multi_dim_map& at(const std::vector<T_key>& p_path) {
        multi_dim_map* current = this;
        for (const auto& key : p_path) {
            current = &(current->m_children.at(key));
        }
        return *current;
    }

    const multi_dim_map& at(const std::vector<T_key>& p_path) const {
        const multi_dim_map* current = this;
        for (const auto& key : p_path) {
            current = &(current->m_children.at(key));
        }
        return *current;
    }

    // Support comparison: REQUIRE(my_mmap[0][0] == 3)
    bool operator==(const T_value& other) const {
        return m_has_value && (m_value == other);
    }

    // Conversion to T_value
    operator T_value() const {
        return m_value;
    }

    [[nodiscard]] bool has_value() const noexcept {
        return m_has_value;
    }

    [[nodiscard]] const T_value& get_value() const noexcept {
        return m_value;
    }

private:
    T_value m_value{};
    bool m_has_value = false;
    std::map<T_key, multi_dim_map> m_children;
};

} // namespace backend
} // namespace v0_0_1
} // namespace isched

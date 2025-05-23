#pragma once
#include <bits/stdc++.h>

/*================================================*\

  B+*-tree,
  A B*-Tree based on B+-Tree,
  Support duplicated keys.

  Each node has up to `M` branches,
  For index and leaf node, up to `M - 1` keys.

\*================================================*/

template<typename node_type, typename Requires = void>
struct is_a_node : std::false_type {};

template<typename node_type>
struct is_a_node<
    node_type,
    std::void_t<decltype(std::declval<typename node_type::key_t>() < std::declval<typename node_type::key_t>()),
                decltype(typename node_type::key_t{}),
                decltype(typename node_type::key_t{std::declval<const typename node_type::key_t &>()})>>
    : std::true_type {};

template<typename key_type, typename val_type, std::size_t M, typename Derived>
struct b_base_node {

  using key_t = key_type;
  using val_t = val_type;

  std::size_t key_cnt;
  bool is_leaf;
  key_type key[M - 1];
  union {
    struct {
      val_type *data_ptr[M - 1];
      Derived *sib;
    } leaf;
    struct {
      Derived *key_ptr[M];
    } idx;
  };

  std::size_t find_idx_ptr_index_(const key_type &k) noexcept {
    std::size_t l{0}, r{key_cnt};
    std::size_t mid{};
    // key[-1, l) <= val, key[r, key_cnt + 1) > val.
    while (r > l) {
      mid = (r - l) / 2 + l;
      if (!(k < key[mid])) {
        l = mid + 1;
      } else {
        r = mid;
      }
    }
    return r;
  }

  std::size_t find_data_ptr_index_(const key_type &k) noexcept {
    std::size_t l{0}, r{key_cnt};
    std::size_t mid{};
    // key[-1, l) < val, key[r, key_cnt + 1) >= val.
    while (r > l) {
      mid = (r - l) / 2 + l;
      if (key[mid] < k) {
        l = mid + 1;
      } else {
        r = mid;
      }
    }
    return r;
  }
};

template<typename key_type, typename val_type, std::size_t M>
struct b_star_node : public b_base_node<key_type, val_type, M, b_star_node<key_type, val_type, M>> {};

template<typename key_type, typename val_type, std::size_t M, typename node_type = b_star_node<key_type, val_type, M>,
         typename Requires = std::void_t<std::enable_if_t<is_a_node<node_type>::value && M >= 7>>>
class b_star_tree {
protected:
  node_type *root{};
  static constexpr std::size_t KEY_SLOTS = M - 1;
  static constexpr std::size_t MAX_KEYS = KEY_SLOTS;

  // this `MIN_KEYS` comes from:
  // `ceil( (MIN + MIN + MIN+2) + 1 ) / 2 <= MAX` .
  // 2-3 split MUST be successful if equal-split-3 is failed.
  static constexpr std::size_t MIN_KEYS = (2 * MAX_KEYS - 5) / 3;

private:
  bool is_overflow_(const node_type *n) noexcept {
    return n->key_cnt >= MAX_KEYS;
  }

  bool root_overflow_(const node_type *root) noexcept {
    return root->key_cnt >= MAX_KEYS;
  }

  bool average_2_overflow_(const node_type *a, const node_type *b) noexcept {
    return ((a->key_cnt + b->key_cnt + 1) / 2 >= MAX_KEYS);
  }
  bool average_3_overflow_(const node_type *a, const node_type *b, const node_type *c) noexcept {
    return ((a->key_cnt + b->key_cnt + c->key_cnt + 2) / 3) >= MAX_KEYS;
  }

  // UNUSED
  bool merge_2_1_overflow_(const node_type *a, const node_type *b) noexcept {
    std::size_t total{a->key_cnt + b->key_cnt};
    if (!a->is_leaf) total++;
    return (total + 1) / 2 >= MAX_KEYS;
  }

  // UNUSED
  bool merge_3_2_overflow_(const node_type *a, const node_type *b, const node_type *c) noexcept {
    std::size_t total{a->key_cnt + b->key_cnt + c->key_cnt};
    if (!a->is_leaf) total++;
    return (total + 2) / 3 >= MAX_KEYS;
  }

  bool is_underflow_(const node_type *n) noexcept {
    return (n->key_cnt <= MIN_KEYS);
  }

  bool root_underflow_() noexcept {
    return !root->is_leaf && root->key_cnt == 1;
  }

  bool average_2_underflow_(const node_type *a, const node_type *b) noexcept {
    return ((a->key_cnt + b->key_cnt) / 2) <= MIN_KEYS;
  }
  bool average_3_underflow_(const node_type *a, const node_type *b, const node_type *c) noexcept {
    return ((a->key_cnt + b->key_cnt + c->key_cnt) / 3) <= MIN_KEYS;
  }

  // UNUSED
  bool split_1_2_underflow_(const node_type *a) noexcept {
    std::size_t total{a->key_cnt};
    if (!a->is_leaf) total--;
    return total / 2 <= MIN_KEYS;
  }

  // UNUSED
  bool split_2_3_underflow_(const node_type *a, const node_type *b) noexcept {
    std::size_t total{a->key_cnt + b->key_cnt};
    if (!a->is_leaf) total--;
    return total / 3 <= MIN_KEYS;
  }

  // parent != nullptr
  key_type redistribute_keys_(node_type *node1, node_type *node2, std::size_t need1, std::size_t need2,
                              node_type *parent, std::size_t idx1) noexcept {

    std::size_t total{node1->key_cnt + node2->key_cnt};
    if (node1->key_cnt > need1) {
      if (node1->is_leaf) {
        std::size_t key_move{node1->key_cnt - need1};
        std::size_t ptr_move{key_move};
        // adjust keys
        std::memmove(node2->key + key_move, node2->key, node2->key_cnt * sizeof(key_type));
        // move keys
        std::memcpy(node2->key, node1->key + need1, key_move * sizeof(key_type));
        // adjust ptrs
        std::memmove(node2->leaf.data_ptr + ptr_move, node2->leaf.data_ptr, node2->key_cnt * sizeof(val_type *));
        // move ptrs
        std::memcpy(node2->leaf.data_ptr, node1->leaf.data_ptr + need1, ptr_move * sizeof(val_type *));
        node1->key_cnt = need1;
        node2->key_cnt = need2;
        return node2->key[0];
      } else {
        std::size_t key_move{node1->key_cnt - need1};
        std::size_t ptr_move{key_move};
        key_type new_delim{node1->key[need1]};
        // adjust
        std::memmove(node2->key + key_move, node2->key, node2->key_cnt * sizeof(key_type));
        // move
        std::memcpy(node2->key, node1->key + need1 + 1, (key_move - 1) * sizeof(key_type));
        node2->key[key_move - 1] = parent->key[idx1];
        // adjust
        std::memmove(node2->idx.key_ptr + ptr_move, node2->idx.key_ptr, (node2->key_cnt + 1) * sizeof(node_type *));
        // move
        std::memcpy(node2->idx.key_ptr, node1->idx.key_ptr + need1 + 1, ptr_move * sizeof(node_type *));
        node1->key_cnt = need1;
        node2->key_cnt = need2;
        return new_delim;
      }
    } else if (node1->key_cnt < need1) {
      if (node1->is_leaf) {
        std::size_t key_move{need1 - node1->key_cnt};
        std::size_t ptr_move{key_move};
        // move
        std::memcpy(node1->key + node1->key_cnt, node2->key, (key_move) * sizeof(key_type));
        // adjust
        std::memmove(node2->key, node2->key + key_move, (node2->key_cnt - key_move) * sizeof(key_type));
        // move
        std::memcpy(node1->leaf.data_ptr + node1->key_cnt, node2->leaf.data_ptr, (ptr_move) * sizeof(val_type *));
        // adjust
        std::memmove(node2->leaf.data_ptr, node2->leaf.data_ptr + ptr_move,
                     (node2->key_cnt - ptr_move) * sizeof(val_type *));
        node1->key_cnt = need1;
        node2->key_cnt = need2;
        return node2->key[0];
      } else {
        std::size_t key_move{need1 - node1->key_cnt};
        std::size_t ptr_move{key_move};
        key_type new_delim{node2->key[node2->key_cnt - need2 - 1]};
        // move
        node1->key[node1->key_cnt] = parent->key[idx1];
        std::memcpy(node1->key + node1->key_cnt + 1, node2->key, (key_move - 1) * sizeof(key_type));
        // adjust
        std::memmove(node2->key, node2->key + key_move,
                     (node2->key_cnt - key_move) * sizeof(key_type)); // BUG
        // move
        std::memcpy(node1->idx.key_ptr + node1->key_cnt + 1, node2->idx.key_ptr,
                    ptr_move * sizeof(node_type *)); // ?
        // adjust
        std::memmove(node2->idx.key_ptr, node2->idx.key_ptr + ptr_move,
                     (node2->key_cnt + 1 - ptr_move) * sizeof(node_type *));
        node1->key_cnt = need1;
        node2->key_cnt = need2;
        return new_delim;
      }
    }

    // nothing should be changed.
    return parent->key[idx1];
  }

  void new_key_in_parent_(node_type *node1, node_type *node2, node_type *parent, std::size_t idx1) noexcept {
    std::memmove(parent->key + idx1 + 1, parent->key + idx1, (parent->key_cnt - idx1) * sizeof(key_type));
    std::memmove(parent->idx.key_ptr + idx1 + 2, parent->idx.key_ptr + idx1 + 1,
                 (parent->key_cnt - idx1) * sizeof(node_type *));
    // trick
    if (!node1->is_leaf) {
      node2->idx.key_ptr[0] = node1->idx.key_ptr[node1->key_cnt];
      parent->key[idx1] = node1->key[--node1->key_cnt];
    }
    parent->idx.key_ptr[idx1] = node1;
    parent->idx.key_ptr[idx1 + 1] = node2;
    parent->key_cnt++;
  }

  void modify_key_in_parent_(node_type *node1, node_type *node2, node_type *parent, std::size_t idx1,
                             const key_type &new_key) noexcept {
    if (node1->is_leaf) {
      parent->key[idx1] = node2->key[0];
    } else {
      parent->key[idx1] = new_key;
    }
    parent->idx.key_ptr[idx1] = node1;
    parent->idx.key_ptr[idx1 + 1] = node2;
  }

  void delete_key_in_parent_(node_type *node1, node_type *node2, node_type *parent, std::size_t idx1) noexcept {
    // trick
    if (!node1->is_leaf) {
      node1->key[node1->key_cnt++] = parent->key[idx1];
      node1->idx.key_ptr[node1->key_cnt] = node2->idx.key_ptr[0];
    }
    // ???
    std::memmove(parent->key + idx1, parent->key + idx1 + 1, (parent->key_cnt - (idx1 + 1)) * sizeof(key_type));
    std::memmove(parent->idx.key_ptr + idx1 + 1, parent->idx.key_ptr + idx1 + 2,
                 (parent->key_cnt - (idx1 + 1)) * sizeof(node_type *));
    parent->key_cnt--;
  }

  void link_split_leaf(node_type *node1, node_type *new_node) {
    new_node->leaf.sib = node1->leaf.sib;
    node1->leaf.sib = new_node;
  }

  void link_merge_leaf(node_type *node1, node_type *delete_node) {
    node1->leaf.sib = delete_node->leaf.sib;
  }

  void do_1_2_split_(node_type *node1, node_type *parent, std::size_t idx1) noexcept {
    node_type *node2{new node_type{}};
    node2->key_cnt = 0;
    node2->is_leaf = root->is_leaf;

    if (node1->is_leaf) {
      link_split_leaf(node1, node2);
    }
    new_key_in_parent_(node1, node2, parent, idx1);

    std::size_t total{node1->key_cnt};
    std::size_t need1{(total + 1) / 2};
    std::size_t need2{total / 2};

    key_type new_key{redistribute_keys_(node1, node2, need1, need2, parent, 0)};

    modify_key_in_parent_(node1, node2, parent, 0, new_key);
  }

  void do_2_1_merge_(node_type *node1, node_type *node2, node_type *parent, std::size_t idx1) noexcept {
    std::size_t total{node1->key_cnt + node2->key_cnt};
    key_type new_key{redistribute_keys_(node1, node2, total, 0, parent, idx1)};
    modify_key_in_parent_(node1, node2, parent, idx1, new_key);
    delete_key_in_parent_(node1, node2, parent, idx1);

    if (node1->is_leaf) {
      link_merge_leaf(node1, node2);
    }

    delete node2;
  }

  // PASSED FAST_TEST
  void do_2_3_split_(node_type *node1, node_type *node2, node_type *parent, std::size_t idx1,
                     std::size_t idx2) noexcept {
    node_type *node3{new node_type{}};
    node3->key_cnt = 0;
    node3->is_leaf = node1->is_leaf;

    new_key_in_parent_(node2, node3, parent, idx2);
    if (node1->is_leaf) {
      link_split_leaf(node2, node3);
    }

    std::size_t total{node1->key_cnt + node2->key_cnt};
    std::size_t need1{(total + 2) / 3};
    std::size_t need2{(total + 1) / 3};
    std::size_t need3{total / 3};

    key_type new_key2{redistribute_keys_(node2, node3, node2->key_cnt - need3, need3, parent, idx2)};
    key_type new_key1{redistribute_keys_(node1, node2, need1, need2, parent, idx1)};
    modify_key_in_parent_(node2, node3, parent, idx2, new_key2);
    modify_key_in_parent_(node1, node2, parent, idx1, new_key1);
  }

  // PASSED FAST_TEST
  void do_3_2_merge_(node_type *node1, node_type *node2, node_type *node3, node_type *parent, std::size_t idx1,
                     std::size_t idx2, std::size_t idx3) noexcept {
    std::size_t total{node1->key_cnt + node2->key_cnt + node3->key_cnt};
    std::size_t need1{(total + 1) / 2};
    std::size_t need2{total / 2};

    key_type new_key1{redistribute_keys_(node1, node2, need1, node1->key_cnt + node2->key_cnt - need1, parent, idx1)};
    key_type new_key2{redistribute_keys_(node2, node3, need2, 0, parent, idx2)};
    modify_key_in_parent_(node1, node2, parent, idx1, new_key1);
    modify_key_in_parent_(node2, node3, parent, idx2, new_key2);
    delete_key_in_parent_(node2, node3, parent, idx2);

    if (node1->is_leaf) {
      link_merge_leaf(node2, node3);
    }

    delete node3;
  }

  // PASSED FAST_TEST
  void do_2_equal_split_(node_type *node1, node_type *node2, node_type *parent, std::size_t idx1) noexcept {
    std::size_t total{node1->key_cnt + node2->key_cnt};
    std::size_t need1{(total + 1) / 2};
    std::size_t need2{total / 2};

    key_type new_key{redistribute_keys_(node1, node2, need1, need2, parent, idx1)};
    modify_key_in_parent_(node1, node2, parent, idx1, new_key);
  }

  void do_3_equal_split_(node_type *node1, node_type *node2, node_type *node3, node_type *parent, std::size_t idx1,
                         std::size_t idx2) noexcept {
    std::size_t total{node1->key_cnt + node2->key_cnt + node3->key_cnt};
    std::size_t need1{(total + 2) / 3};
    std::size_t need2{(total + 1) / 3};
    std::size_t need3{total / 3};

    // this algorithm comes up with waterflow.

    std::size_t tmp{};

    if ((tmp = node1->key_cnt + node2->key_cnt) <= need1 + need2) {
      key_type new_key{redistribute_keys_(node1, node2, need1, tmp - need1, parent, idx1)};
      modify_key_in_parent_(node1, node2, parent, idx1, new_key);
    }

    if ((tmp = node2->key_cnt + node3->key_cnt) <= need2 + need3) {
      key_type new_key{redistribute_keys_(node2, node3, tmp - need3, need3, parent, idx2)};
      modify_key_in_parent_(node2, node3, parent, idx2, new_key);
    }

    if ((tmp = node1->key_cnt + node2->key_cnt) <= need1 + need2) {
      key_type new_key{redistribute_keys_(node1, node2, tmp - need2, need2, parent, idx1)};
      modify_key_in_parent_(node1, node2, parent, idx1, new_key);
    }
  }

  bool pair_left(node_type *&node_l, node_type *&node_r, std::size_t &idx_l, std::size_t &idx_r,
                 node_type *parent) noexcept {
    if (idx_r > 0) {
      idx_l = idx_r - 1;
      node_l = parent->idx.key_ptr[idx_l];
      return true;
    }
    return false;
  }

  bool pair_right(node_type *&node_l, node_type *&node_r, std::size_t &idx_l, std::size_t &idx_r,
                  node_type *parent) noexcept {
    if (idx_l + 1 <= parent->key_cnt) {
      idx_r = idx_l + 1;
      node_r = parent->idx.key_ptr[idx_r];
      return true;
    }
    return false;
  }

  void fix_overflow_(node_type *node1, node_type *parent, std::size_t idx1) noexcept {
    std::size_t idx2{};
    node_type *node2{};

    if (pair_left(node2, node1, idx2, idx1, parent)) {
      std::swap(node1, node2);
      std::swap(idx1, idx2);
    } else if (pair_right(node1, node2, idx1, idx2, parent)) {
      // pass
    } else {
      return;
    }

    auto do_2_equal_split_lambda{[=, this]() -> void { this->do_2_equal_split_(node1, node2, parent, idx1); }};
    auto do_2_3_split_lambda{[=, this]() -> void { this->do_2_3_split_(node1, node2, parent, idx1, idx2); }};

    if (!average_2_overflow_(node1, node2) && !average_2_underflow_(node1, node2)) {
      do_2_equal_split_lambda();
      return;
    }

    // <- experiment
    std::size_t idx3{};
    node_type *node3{};
    bool valid3{true};
    if (pair_left(node3, node1, idx3, idx1, parent)) {
      std::swap(node3, node2);
      std::swap(node2, node1);
      std::swap(idx3, idx2);
      std::swap(idx2, idx1);
    } else if (pair_right(node2, node3, idx2, idx3, parent)) {
      // pass
    } else {
      valid3 = false;
    }

    auto do_3_equal_split_lambda{
        [=, this]() -> void { this->do_3_equal_split_(node1, node2, node3, parent, idx1, idx2); }};

    if (valid3 && !average_3_overflow_(node1, node2, node3) && !average_3_underflow_(node1, node2, node3)) {
      do_3_equal_split_lambda();
    }
    // experiment ->

    do_2_3_split_lambda();
  }

  void fix_root_overflow_() {
    node_type *new_root{new node_type{}};
    new_root->key_cnt = 0;
    new_root->is_leaf = false;

    do_1_2_split_(root, new_root, 0);

    root = new_root;
  }

  void fix_underflow_(node_type *node1, node_type *parent, std::size_t idx1) noexcept {

    std::size_t idx2{};
    node_type *node2{};
    if (pair_left(node2, node1, idx2, idx1, parent)) {
      std::swap(node2, node1);
      std::swap(idx2, idx1);
    } else if (pair_right(node1, node2, idx1, idx2, parent)) {
      // pass
    } else {
      return;
    }

    if (!average_2_overflow_(node1, node2) && !average_2_underflow_(node1, node2)) {
      do_2_equal_split_(node1, node2, parent, idx1);
      return;
    }

    // find the 3rd sibling
    std::size_t idx3{};
    node_type *node3{};
    if (pair_left(node3, node1, idx3, idx1, parent)) {
      std::swap(node3, node2);
      std::swap(node2, node1);
      std::swap(idx3, idx2);
      std::swap(idx2, idx1);
    } else if (pair_right(node2, node3, idx2, idx3, parent)) {
      // pass
    } else {
      return;
    }

    if (!average_3_overflow_(node1, node2, node3) && !average_3_underflow_(node1, node2, node3)) {
      do_3_equal_split_(node1, node2, node3, parent, idx1, idx2);
      return;
    }

    do_3_2_merge_(node1, node2, node3, parent, idx1, idx2, idx3);
  }

  void fix_root_underflow_() {
    node_type *node1{root->idx.key_ptr[0]};
    node_type *node2{root->idx.key_ptr[1]};
    if ((node1->is_leaf && node1->key_cnt + node2->key_cnt <= MAX_KEYS) ||
        (!node1->is_leaf && node1->key_cnt + node2->key_cnt < MAX_KEYS)) {
      do_2_1_merge_(node1, node2, root, 0);
      delete root;
      root = node1;
    } else {
      do_2_equal_split_(node1, node2, root, 0);
    }
    return;
  }

  void delete_all_nodes_(bool del_root) {
    if (root) {
      std::vector<node_type *> decon{};
      if (!root->is_leaf) {
        for (std::size_t i = 0; i <= root->key_cnt; i++) {
          decon.emplace_back(root->idx.key_ptr[i]);
        }
      }
      while (!decon.empty()) {
        node_type *cur{decon.back()};
        decon.pop_back();
        if (!cur->is_leaf) {
          for (std::size_t i = 0; i <= cur->key_cnt; i++) {
            decon.emplace_back(cur->idx.key_ptr[i]);
          }
        }
        delete cur;
      }
      if (del_root) {
        delete root; // and root
      }
    }
  }

public:
  b_star_tree() noexcept {
    root = new node_type{};
    root->key_cnt = 0;
    root->is_leaf = true;
  }
  ~b_star_tree() {
    delete_all_nodes_(true);
  }

  b_star_tree(const b_star_tree &obj) = delete;
  b_star_tree(b_star_tree &&obj) noexcept {
    delete_all_nodes_(true);
    root = obj.root;
    obj.root = nullptr;
  }

  b_star_tree &operator=(const b_star_tree &obj) = delete;
  b_star_tree &operator=(b_star_tree &&obj) noexcept {
    delete_all_nodes_(true);
    root = obj.root;
    obj.root = nullptr;
    return *this;
  }

  void clear() {
    delete_all_nodes_(false);
  }

  node_type *insert_down_to_leaf(node_type *root, const key_type &k) noexcept {
    node_type *cur{root}, *next{};
    std::size_t next_from{};
    while (!cur->is_leaf) {
      next_from = cur->find_idx_ptr_index_(k);
      next = cur->idx.key_ptr[next_from];
      if (is_overflow_(next)) {
        fix_overflow_(next, cur, next_from);
        next_from = cur->find_idx_ptr_index_(k);
      }
      cur = cur->idx.key_ptr[next_from];
    }
    return cur;
  }

  bool insert_leaf(node_type *cur, const key_type &k, val_type *v) noexcept {
    std::size_t check{cur->find_data_ptr_index_(k)};
    std::size_t idx{cur->find_idx_ptr_index_(k)};
    // NO DUPLICATED KEY SUPPORTED
    if (check < cur->key_cnt && cur->key[check] == k) {
      return false;
    }
    if (idx != cur->key_cnt) {
      std::memmove(cur->leaf.data_ptr + idx + 1, cur->leaf.data_ptr + idx, (cur->key_cnt - idx) * sizeof(val_type *));
      std::memmove(cur->key + idx + 1, cur->key + idx, (cur->key_cnt - idx) * sizeof(key_type));
    }
    cur->leaf.data_ptr[idx] = v;
    cur->key[idx] = k;
    cur->key_cnt++;
    return true;
  }

  bool insert(const key_type &k, val_type *v) noexcept {
    if (root_overflow_(root)) {
      fix_root_overflow_();
    }
    node_type *cur{insert_down_to_leaf(root, k)};
    return insert_leaf(cur, k, v);
  }

  node_type *erase_down_to_leaf(node_type *root, const key_type &k) noexcept {
    node_type *cur{root}, *next{};
    std::size_t next_from{};
    while (!cur->is_leaf) {
      next_from = cur->find_idx_ptr_index_(k);
      next = cur->idx.key_ptr[next_from];
      if (is_underflow_(next)) {
        fix_underflow_(next, cur, next_from);
        if (cur->is_leaf) break;
        next_from = cur->find_idx_ptr_index_(k);
      }
      cur = cur->idx.key_ptr[next_from];
    }
    return cur;
  }

  bool erase_leaf(node_type *cur, const key_type &k) noexcept {
    std::size_t check{cur->find_data_ptr_index_(k)};
    if (check == cur->key_cnt || cur->key[check] != k) {
      return false;
    }
    if (check != cur->key_cnt - 1) {
      std::memmove(cur->leaf.data_ptr + check, cur->leaf.data_ptr + check + 1,
                   (cur->key_cnt - (check + 1)) * sizeof(val_type *));
      std::memmove(cur->key + check, cur->key + check + 1, (cur->key_cnt - (check + 1)) * sizeof(key_type));
    }
    cur->key_cnt--;
    return true;
  }

  // need test
  bool erase(const key_type &k) noexcept {
    if (root_underflow_()) {
      fix_root_underflow_();
    }
    node_type *cur{erase_down_to_leaf(root, k)};
    return erase_leaf(cur, k);
  }

  node_type *find_down_to_leaf(node_type *root, const key_type &k) const {
    node_type *cur{root};
    while (cur && !cur->is_leaf) {
      cur = cur->idx.key_ptr[cur->find_idx_ptr_index_(k)];
    }
    return cur;
  }

  std::vector<val_type *> find_collect_range(node_type *cur, const key_type &low, const key_type &high) const {
    std::vector<val_type *> vals{};
    while (cur) {
      std::size_t beg{cur->find_data_ptr_index_(low)}, end{cur->find_idx_ptr_index_(high)};
      if (end == 0) break;
      for (std::size_t i = beg; i < end; i++) {
        vals.emplace_back(cur->leaf.data_ptr[i]);
      }
      cur = cur->leaf.sib;
    }
    return vals;
  }

  val_type *find_collect_single(node_type *cur, const key_type &k) const {
    std::size_t beg{cur->find_data_ptr_index_(k)};
    if (cur->key[beg] != k) {
      return nullptr;
    } else {
      return cur->leaf.data_ptr[beg];
    }
  }

  val_type *find_single(const key_type &k) const {
    node_type *cur{find_down_to_leaf(root, k)};
    return find_collect_single(cur, k);
  }

  // [low, high)
  std::vector<val_type *> find_range(const key_type &low, const key_type &high) const {
    node_type *cur{find_down_to_leaf(root, low)};
    return find_collect_range(cur, low, high);
  }
};

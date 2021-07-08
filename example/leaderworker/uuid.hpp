#ifndef __COLZA_UUID_HPP
#define __COLZA_UUID_HPP

#include <cstring>
#include <string>



/**
 * @brief UUID class (Universally Unique IDentifier).
 */
struct UUID {
  unsigned char m_data[16];

 public:
  /**
   * @brief Constructor, produces a zero-ed UUID.
   */
  UUID();

  /**
   * @brief Copy constructor.
   */
  UUID(const UUID& other) = default;

  /**
   * @brief Move constructor.
   */
  UUID(UUID&& other) = default;

  /**
   * @brief Copy-assignment operator.
   */
  UUID& operator=(const UUID& other) = default;

  /**
   * @brief Move-assignment operator.
   */
  UUID& operator=(UUID&& other) = default;

  /**
   * @brief Converts the UUID into a string.
   *
   * @return a readable string representation of the UUID.
   */
  std::string to_string() const;

  /**
   * @brief Converts the UUID into a string and pass it
   * to the provided stream.
   */
  template <typename T>
  friend T& operator<<(T& stream, const UUID& id);

  /**
   * @brief Generates a random UUID.
   *
   * @return a random UUID.
   */
  static UUID generate();

  /**
   * @brief randomize the current UUID.
   */
  void randomize();

  /**
   * @brief Compare the UUID with another UUID.
   */
  bool operator==(const UUID& other) const;

  /**
   * @brief Computes a hash of the UUID.
   *
   * @return a uint64_t hash value.
   */
  // uint64_t hash() const;

  template <typename Archive>
  void save(Archive& a) const {
    a.write(m_data, 16);
  }
  template <typename Archive>
  void load(Archive& a) {
    a.read(m_data, 16);
  }
};

template <typename T>
T& operator<<(T& stream, const UUID& id) {
  stream << id.to_string();
  return stream;
}

struct UUID_as_pair {
  uint64_t a, b;
};

// specialized hash function for unordered_map keys
struct UUID_hash_fn {
  std::size_t operator()(const UUID& uuid) const {
    auto p = reinterpret_cast<const UUID_as_pair*>(uuid.m_data);
    return p->a ^ p->b;
  }
};



#endif
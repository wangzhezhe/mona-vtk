#include "uuid.hpp"

#include <uuid/uuid.h>

#include <cstring>


UUID::UUID() {
  // if data is zero, it is the master
  for (int i = 0; i < 16; i++) {
    m_data[i] = 0;
  }
}

std::string UUID::to_string() const {
  std::string result(32, '\0');
  uuid_unparse(this->m_data, const_cast<char*>(result.data()));
  return result;
}

void UUID::randomize() { uuid_generate_random(this->m_data); }

UUID UUID::generate() {
  UUID result;
  result.randomize();
  return result;
}

bool UUID::operator==(const UUID& other) const {
  int c = memcmp(m_data, other.m_data, sizeof(m_data));
  return c == 0;
}

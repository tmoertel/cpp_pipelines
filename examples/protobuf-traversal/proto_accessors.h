// Uniform interface for protocol buffers. -*- c++ -*-
// Tom Moertel <tom@moertel.com>

#include <iostream>
#include <type_traits>

#include "example.pb.h"

template <typename T>
class ObjectAccessor {
 public:
  explicit ObjectAccessor(T* ptr) : ptr_(ptr) {}
  T& operator*() { return *ptr_; }

 private:
  T* ptr_;
};

template <typename Proto>
ObjectAccessor<std::string> access_name(Proto* proto) {
  return ObjectAccessor<std::string>(proto->mutable_name());
}

template <typename Proto>
ObjectAccessor<const std::string> access_name(const Proto& proto) {
  return ObjectAccessor<const std::string>(&proto.name());
}

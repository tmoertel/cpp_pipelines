// An algebraic library for building pipelines.

#include "consumers_and_producers.hpp"

#include <initializer_list>
#include <functional>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace {

template<typename T>
Producer<T> Produce(std::initializer_list<T> ts) {
  return {
    [=](Consumer<T> c) {
      for (auto& t : ts) {
        c(t);
      }
    }
  };
}

class CPTest : public ::testing::Test {
 protected:
  CPTest() :
    flight_record_{},
    producers_{{
        PZero<Value>(),
        Produce<Value>({"p1"}),
        Produce<Value>({"p2-1", "p2-2"})}},
    consumers_{{
        CZero<Value>(),
        Consumer<Value>{
          [&](Value x) {
            this->flight_record_.push_back({x, "c1"});
          }},
        Consumer<Value>{
          [&](Value x) {
            this->flight_record_.push_back({x, "c2-1"});
            this->flight_record_.push_back({x, "c2-2"});
          }}}} {}

  using Value = const char*;
  using EffectRecord = std::pair<Value, const char*>;
  std::vector<EffectRecord> flight_record_;
  std::vector<Producer<Value>> producers_;
  std::vector<Consumer<Value>> consumers_;

  // Reify the effect produced by fusing a producer and a consumer.
  // This we do by applying the effect to a "flight recorder"; the
  // final record is the effect reified.
  std::vector<EffectRecord> Fusing(Producer<Value> p, Consumer<Value> c) {
    flight_record_.clear();
    p(c);
    return flight_record_;
  }
};

}  // namespace

TEST_F(CPTest, ConsumerMustObeyMonoidLaws) {
  auto zero = CZero<Value>();
  for (const auto& p : producers_) {
    for (const auto& c : consumers_) {
      // CZero must be the left and right identity element under (+).
      EXPECT_EQ(Fusing(p, c), Fusing(p, zero + c));
      EXPECT_EQ(Fusing(p, c), Fusing(p, c + zero));
      for (const auto& c1 : consumers_) {
        for (const auto& c2 : consumers_) {
          // (+) must be associative.
          EXPECT_EQ(Fusing(p, c + (c1 + c2)), Fusing(p, (c + c1) + c2));
        }
      }
    }
  }
}

TEST_F(CPTest, ProducerMustObeyMonoidLaws) {
  auto zero = PZero<Value>();
  for (const auto& c : consumers_) {
    for (const auto& p : producers_) {
      // PZero must be the left and right identity element under (+).
      EXPECT_EQ(Fusing(p, c), Fusing(zero + p, c));
      EXPECT_EQ(Fusing(p, c), Fusing(p + zero, c));
      for (const auto& p1 : producers_) {
        for (const auto& p2 : producers_) {
          // (+) must be associative.
          EXPECT_EQ(Fusing(p + (p1 + p2), c), Fusing((p + p1) + p2, c));
        }
      }
    }
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

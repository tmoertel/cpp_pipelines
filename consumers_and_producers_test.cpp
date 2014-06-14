// Tests for an algebraic library for building pipelines.

#include "consumers_and_producers.hpp"

#include <initializer_list>
#include <functional>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

template<typename T>
Producer<T> Produce(std::vector<T> ts) {
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
          [this](Value x) {
            flight_record_.emplace_back(x, "c1");
          }},
        Consumer<Value>{
          [this](Value x) {
            flight_record_.emplace_back(x, "c2-1");
            flight_record_.emplace_back(x, "c2-2");
          }}}} {}

  using Value = std::string;
  using EffectRecord = std::pair<Value, Value>;
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

// Monad laws.
//   Left identity:   return a >>= f  ≡ f a
//   Right identity:  m >>= return    ≡ m
//   Associativity:   (m >>= f) >>= g ≡ m >>= (\x -> f x >>= g)
TEST_F(CPTest, ProducerMustObeyMonadLaws) {
  Fn<Value, Producer<Value>> MUnitFn{MUnit<Value>};
  for (const auto& c : consumers_) {
    for (const Value& s_f : {"f1", "f2", "f3"}) {
      // Arbitrary function f of type a -> m b.
      Fn<Value, Producer<Value>> f {
        [&](const Value& x) { return MUnit(x + s_f); }
      };

      // Left identity.
      for (const Value& a : {"a1", "a2", "a3"}) {
        EXPECT_EQ(Fusing(MUnit(a) | f, c), Fusing(f(a), c));
      }

      for (const auto& p : producers_) {
        // Right identity.
        EXPECT_EQ(Fusing(p | MUnitFn, c), Fusing(p, c));

        // Associativity.
        for (const Value& s_g : {"g1", "g2", "g3"}) {
          // Arbitrary function g of type a -> m b.
          Fn<Value, Producer<Value>> g {
            [&](const Value& x) {
              return MUnit(x + s_g) + MUnit(s_g + x);
            }
          };
          EXPECT_EQ(Fusing(p | f | g, c), Fusing(p | (f * g), c));
        }
      }
    }
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

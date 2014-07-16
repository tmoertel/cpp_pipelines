// Tests for an algebraic library for building pipelines.

#include "consumers_and_producers.h"

#include <functional>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

using std::vector;

template<typename T>
Producer<T> Produce(vector<T> ts) {
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
  vector<EffectRecord> flight_record_;
  vector<Producer<Value>> producers_;
  vector<Consumer<Value>> consumers_;

  // Reify the effect produced by fusing a producer and a consumer.
  // This we do by applying the effect to a "flight recorder"; the
  // final record is the effect reified.
  vector<EffectRecord> Fusing(Producer<Value> p, Consumer<Value> c) {
    flight_record_.clear();
    Fuse(p, c)();
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
  Filter<Value, Value> PUnitFilter{PUnit<Value>};
  for (const auto& c : consumers_) {
    for (const Value& s_f : {"f1", "f2", "f3"}) {
      // Arbitrary function f of type a -> m b.
      Filter<Value, Value> f {
        [&](const Value& x) { return PUnit(x + s_f); }
      };

      // Left identity.
      for (const Value& a : {"a1", "a2", "a3"}) {
        EXPECT_EQ(Fusing(PUnit(a) | f, c), Fusing(f(a), c));
      }

      for (const auto& p : producers_) {
        // Right identity.
        EXPECT_EQ(Fusing(p | PUnitFilter, c), Fusing(p, c));

        // Associativity.
        for (const Value& s_g : {"g1", "g2", "g3"}) {
          // Arbitrary function g of type a -> m b.
          Filter<Value, Value> g {
            [&](const Value& x) {
              return PUnit(x + s_g) + PUnit(s_g + x);
            }
          };
          EXPECT_EQ(Fusing(p | f | g, c), Fusing(p | (f * g), c));
        }
      }
    }
  }
}

namespace {

using int_string = std::pair<int, std::string>;
int_string IntString(int i, const std::string& s) {
  return std::make_pair(i, s);
}

}  // namespace

// Applicative functor.
TEST_F(CPTest, ProducerMustObeyApplicativeFunctorLaws) {
  Fn<int, int> AddTen = [](int x) { return x + 10; };
  Fn<int, int> Double = [](int x) { return 2 * x; };
  auto produce_fns = Produce<Fn<int, int>>({AddTen, Double});
  auto produce_123 = Produce<int>({1, 2, 3});
  vector<int> recorder;
  Consumer<int> record_int = [&recorder](int x) { recorder.push_back(x); };

  LiftA(AddTen)(PPure(3))(record_int);
  EXPECT_EQ(vector<int>({13}), recorder);

  recorder.clear();
  PApply(produce_fns, produce_123)(record_int);
  EXPECT_EQ(vector<int>({11, 12, 13, 2, 4, 6}), recorder);

  recorder.clear();
  LiftA(AddTen)(produce_123)(record_int);
  EXPECT_EQ(vector<int>({11, 12, 13}), recorder);

  std::function<int(int, int)> Add = [](int x, int y) { return x + y; };
  recorder.clear();
  LiftA(Add)(produce_123, produce_123)(record_int);
  EXPECT_EQ(vector<int>({2, 3, 4, 3, 4, 5, 4, 5, 6}), recorder);

  // Lifted functions must consume left-most producers slowest.
  vector<int_string> int_string_recorder;
  Consumer<int_string> record_int_string =
      [&](const int_string& x) { int_string_recorder.push_back(x); };
  auto produce_abc = Produce<std::string>({"a", "b", "c"});
  LiftA(IntString)(produce_123, produce_abc)(record_int_string);
  EXPECT_EQ(vector<int_string>({{1, "a"}, {1, "b"}, {1, "c"},
                                {2, "a"}, {2, "b"}, {2, "c"},
                                {3, "a"}, {3, "b"}, {3, "c"}}),
            int_string_recorder);
}

TEST_F(CPTest, ProducersAndFiltersMustSupportCrossProducts) {
  auto produce_123 = Produce<int>({1, 2, 3});
  auto produce_abc = Produce<std::string>({"a", "b", "c"});

  using T = std::tuple<int, std::string>;
  vector<T> tuple_recorder;
  Consumer<T> record_tuple =
  	  [&](const T& x) { tuple_recorder.push_back(x); };

  // Cross product for producers.
  PCross(produce_123, produce_abc)(record_tuple);
  EXPECT_EQ(vector<T>({T{1, "a"}, T{1, "b"}, T{1, "c"},
                       T{2, "a"}, T{2, "b"}, T{2, "c"},
                       T{3, "a"}, T{3, "b"}, T{3, "c"}}),
            tuple_recorder);

  // Cross product for filters.
  Filter<int, int> f123 = [&](int /*x*/) { return produce_123; };
  Filter<int, std::string> fabc = [&](int /*x*/) { return produce_abc; };
  auto previous_result = tuple_recorder;
  tuple_recorder.clear();
  FFork(f123, fabc)(1)(record_tuple);
  EXPECT_EQ(previous_result, tuple_recorder);

  FCross(f123, fabc);
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

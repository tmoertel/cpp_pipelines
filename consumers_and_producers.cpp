// An algebraic library for building pipelines.

#include <initializer_list>
#include <iostream>
#include <functional>
#include <vector>

//==============================================================================
// CORE MODEL
//==============================================================================

// A consumer is a value sink. It can be called on values of type T
// to consume them.
template <typename T>
class Consumer {
public:
  typedef T value_type;
  Consumer(std::function<void(T)> f) : f_(f) {}
  void operator()(T t) const { f_(t); }
private:
  std::function<void(T)> f_;
};

// A producer is a value source. It can be called on a corresponding
// consumer to pass its values, one at a time, to the consumer.
// (Interesting fact: a producer is isomorphic to a consumer of
// consumers.)
template <typename T>
class Producer {
public:
  typedef T value_type;
  Producer(std::function<void(Consumer<T>)> f) : f_(f) {}
  void operator()(Consumer<T> c) const { f_(c); }
private:
  std::function<void(Consumer<T>)> f_;
};

//==============================================================================
// COMBINATORS
//==============================================================================

// Fusing a producer to a consumer produces an effect that when
// executed feeds the producer's values to the consumer.
typedef std::function<void()> Effect;
template <typename T>
Effect Fuse(Producer<T> p, Consumer<T> c) {
  return {
    [=] { p(c); }
  };
}

// Producer composition is value serial and forms a monoid:
//   PZero()(c)          === { Empty effect }
//   (PZero() + p)(c)    === p(c)
//   (p + PZero())(c)    === p(c)
//   (p1 + p2)(c)        === p1(c), p2(c)
//   (p1 + (p2 + p3))(c) === ((p1 + p2) + p3)(c)
template <typename T>
Producer<T> PZero() {
  return {
    [](Consumer<T> /*c*/) { /* do nothing */ }
  };
}

template <typename T>
Producer<T> operator+(Producer<T> p1, Producer<T> p2) {
  return {
    [=](Consumer<T> c) { p1(c); p2(c); }
  };
}

// Consumer composition is value parallel and forms a monoid:
//   p(CZero())        === { Empty effect }
//   p(CZero() + c)    === p(c)
//   p(c + CZero())    === p(c)
//   p(c1 + c2)        === p(c1), p(c2)
//   p(c1 + (c2 + c3)) === p((c1 + c2) + c2)
template <typename T>
Consumer<T> CZero() {
  return {
    [](Consumer<T> /*c*/) { /* do nothing */ }
  };
}

template <typename T>
Consumer<T> operator+(Consumer<T> c1, Consumer<T> c2) {
  return {
    [=](T t) { c1(t); c2(t); }
  };
}

// Producers are functors (i.e., value containers supporting a map function).
template <typename A, typename B> using Map = std::function<B(A)>;

template <typename A, typename B>
Producer<B> Fmap(Map<A, B> f, Producer<A> p) {
  return {
    [=](Consumer<B> c_B) {
      Consumer<A> c_A{ [=](A a) { c_B(f(a)); } };
      p(c_A);
    }
  };
}

// Producers are also monads.
template <typename A>
Producer<A> MUnit(A a) {
  return {
    [=](Consumer<A> c) { c(a); }
  };
}

template <typename A>
Producer<A> MJoin(Producer<Producer<A>> p_PA) {
  return {
    [=](Consumer<A> c) {
      Consumer<Producer<A>> c_PA{ [=](Producer<A> p) { p(c); } };
      p_PA(c_PA);
    }
  };
}

template <typename A, typename B>
Producer<B> MBind(Producer<A> p, Map<A, Producer<B>> f) {
  return MJoin(Fmap(f, p));
}

template <typename A, typename B>
Producer<B> operator|(Producer<A> p, Map<A, Producer<B>> f) {
  return MBind(p, f);
}

//==============================================================================
// EXAMPLES
//==============================================================================

template<typename T>
Consumer<T> Print() {
  return {
    [](T t) { std::cout << t << std::endl; }
  };
}

template<typename Container, typename T = typename Container::value_type>
Producer<T> Produce(Container ts) {
  return {
    [=](Consumer<T> c) {
      for (auto& t : ts) {
        c(t);
      }
    }
  };
}

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

Producer<int> TenTwentyThirty(int x) {
  return {
    [=](Consumer<int> c) {
      c(10 + x);
      c(20 + x);
      c(30 + x);
    }
  };
}

int main() {
  Producer<int> p123 {Produce({1, 2, 3})};
  Map<int, Producer<int>> ttt {TenTwentyThirty};
  Producer<int> p123ttt {p123 | ttt};
  Fuse(p123 | ttt, Print<int>())();
}

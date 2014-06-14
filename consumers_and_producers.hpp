// An algebraic library for building pipelines.

#include <functional>

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
    [](T /*x*/) { /* do nothing */ }
  };
}

template <typename T>
Consumer<T> operator+(Consumer<T> c1, Consumer<T> c2) {
  return {
    [=](T t) { c1(t); c2(t); }
  };
}

// Producers are functors (i.e., value containers supporting a map function).
template <typename A, typename B> using Fn = std::function<B(A)>;

template <typename A, typename B>
Producer<B> Fmap(Fn<A, B> f, Producer<A> p) {
  return {
    [=](Consumer<B> c_B) {
      Consumer<A> c_A{ [=](A x) { c_B(f(x)); } };
      p(c_A);
    }
  };
}

// Consumers are cofunctors.
template <typename A, typename B>
Consumer<B> Cofmap(Fn<B, A> f, Consumer<A> c) {
  return {
    [=](B x) { c(f(x)); }
  };
}


// Producers are also monads.
template <typename A>
Producer<A> MUnit(A x) {
  return {
    [=](Consumer<A> c) { c(x); }
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
Producer<B> MBind(Producer<A> p, Fn<A, Producer<B>> f) {
  return MJoin(Fmap(f, p));
}

// Infix version of bind.
template <typename A, typename B>
Producer<B> operator|(Producer<A> p, Fn<A, Producer<B>> f) {
  return MBind(p, f);
}

template <typename A, typename B, typename C>
Fn<A, Producer<C>> KleisliComposition(Fn<A, Producer<B>> f,
                                      Fn<B, Producer<C>> g) {
  return {
    [=](A x) { return f(x) | g; }
  };
}

// Infix version of KleisliComposition.
template <typename A, typename B, typename C>
Fn<A, Producer<C>> operator*(Fn<A, Producer<B>> f, Fn<B, Producer<C>> g) {
  return KleisliComposition(f, g);
}

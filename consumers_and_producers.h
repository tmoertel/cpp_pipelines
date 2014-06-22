// An algebraic library for building pipelines.  -*- c++ -*-

#include <functional>

//==============================================================================
// CORE MODEL
//==============================================================================

// Let's introduce a lightweight notation for function types.
template <typename A, typename B> using Fn = std::function<B(A)>;

// A consumer is a value sink. It can be called on values of type T
// to consume them.
template <typename T>
class Consumer {
public:
  typedef T value_type;
  template <typename F> Consumer(const F& f) : f_(f) {}
  void operator()(T t) const { f_(t); }
private:
  Fn<T, void> f_;
};

// A producer is a value source. It can be called on a corresponding
// consumer to pass its values, one at a time, to the consumer.
// (Interesting fact: a producer is isomorphic to a consumer of
// consumers.)
template <typename T>
class Producer {
public:
  typedef T value_type;
  template <typename F> Producer(const F& f) : f_(f) {}
  void operator()(Consumer<T> c) const { f_(c); }
private:
  Fn<const Consumer<T>&, void> f_;
};

// A filter takes an A and from it produces Bs.
template <typename A, typename B> using Filter = Fn<A, Producer<B>>;

//==============================================================================
// COMBINATORS
//==============================================================================

// Fusing a producer to a consumer produces an effect that when
// executed feeds the producer's values to the consumer.
typedef std::function<void()> Effect;
template <typename T>
Effect Fuse(const Producer<T>& p, const Consumer<T>& c) {
  return [=] { p(c); };
}

// Producer composition is value serial and forms a monoid:
//   PZero()(c)          === { Empty effect }
//   (PZero() + p)(c)    === p(c)
//   (p + PZero())(c)    === p(c)
//   (p1 + p2)(c)        === p1(c), p2(c)
//   (p1 + (p2 + p3))(c) === ((p1 + p2) + p3)(c)
template <typename T>
Producer<T> PZero() {
  return [](Consumer<T> /*c*/) { /* do nothing */ };
}

template <typename T>
Producer<T> operator+(const Producer<T>& p1, const Producer<T>& p2) {
  return [=](Consumer<T> c) { p1(c); p2(c); };
}

// Consumer composition is value parallel and forms a monoid:
//   p(CZero())        === { Empty effect }
//   p(CZero() + c)    === p(c)
//   p(c + CZero())    === p(c)
//   p(c1 + c2)        === p(c1), p(c2)
//   p(c1 + (c2 + c3)) === p((c1 + c2) + c2)
template <typename T>
Consumer<T> CZero() {
  return [](T /*x*/) { /* do nothing */ };
}

template <typename T>
Consumer<T> operator+(const Consumer<T>& c1, const Consumer<T>& c2) {
  return [=](T t) { c1(t); c2(t); };
}

// Producers are functors (i.e., value containers supporting a map function).
template <typename A, typename B>
Producer<B> Fmap(const Fn<A, B>& f, const Producer<A>& p) {
  return [=](Consumer<B> c_B) {
    Consumer<A> c_A{ [=](A x) { c_B(f(x)); } };
    p(c_A);
  };
}

// Consumers are cofunctors.
template <typename A, typename B>
Consumer<B> Cofmap(const Fn<B, A>& f, const Consumer<A>& c) {
  return [=](B x) { c(f(x)); };
}

// Producers are also monads.
template <typename A>
Producer<A> PUnit(const A& x) {
  return [=](Consumer<A> c) { c(x); };
}

template <typename A>
Producer<A> PJoin(Producer<Producer<A>> p_PA) {
  return [=](const Consumer<A>& c) {
    Consumer<Producer<A>> c_PA{ [=](Producer<A> p) { p(c); } };
    p_PA(c_PA);
  };
}

template <typename A, typename B>
Producer<B> PBind(const Producer<A>& p, const Filter<A, B>& f) {
  return PJoin(Fmap(f, p));
}

// Infix version of bind.
template <typename A, typename B>
Producer<B> operator|(const Producer<A>& p, const Filter<A, B>& f) {
  return PBind(p, f);
}

// Filters can be composed (value serial) to form chained filters.
template <typename A, typename B, typename C>
Filter<A, C> KleisliComposition(const Filter<A, B>& f, const Filter<B, C>& g) {
  return [=](A x) { return f(x) | g; };
}

// Infix version of KleisliComposition.
template <typename A, typename B, typename C>
Filter<A, C> operator*(const Filter<A, B>& f, const Filter<B, C>& g) {
  return KleisliComposition(f, g);
}

// Filters can be composed (value parallel) to form Y filters.
// Law: (filter1 + filter2)(x) = filter1(x) + filter2(x)
template <typename A, typename B>
Filter<A, B> operator+(const Filter<A, B>& f, const Filter<A, B>& g) {
  return [=](A x) { return f(x) + g(x); };
}

// An algebraic library for building pipelines.  -*- c++ -*-

#ifndef CONSUMERS_AND_PRODUCERS_H_
#define CONSUMERS_AND_PRODUCERS_H_

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>


//==============================================================================
// First, some tragically necessary template boilerplate.
//==============================================================================

// Helper for elementwise processing of tuples.
struct _TupleHelper {
  // Tuple elems-by-indices idiom: http://stackoverflow.com/questions/7858817/.
  template<int ...>
  struct seq {};

  template<typename ...>
  struct type_pack {};

  template<int N, int ...S>
  struct gens : gens<N-1, N-1, S...> {};

  template<int ...S>
  struct gens<0, S...> {
    typedef seq<S...> type;
  };

  template <typename R, int... Indices, typename... Types, typename F>
  static std::function<R(std::tuple<Types...>)> _UncurryIndexed(
      seq<Indices...>, type_pack<R, Types...>, F f) {
    return [=](std::tuple<Types...> tup) {
      return f(std::get<Indices>(tup)...);
    };
  }

  // Convert a function that accepts a series of arguments into
  // a function that accepts those arguments as a tuple.
  // Case 1: The function already accepts a tuple: return it as is.
  template <typename F, typename... Types,
            typename = decltype(std::declval<F>()(
                std::declval<std::tuple<Types...>>()))>
  static F Uncurry(F f) {
    return f;
  }
  // Case 2: wrap the function within with logic that unpacks
  // an argument tuple.
  template <typename F, typename... Types,
            typename R = decltype(std::declval<F>()(std::declval<Types>()...))>
  static std::function<R(std::tuple<Types...>)> Uncurry(F f) {
    return _UncurryIndexed(typename gens<sizeof...(Types)>::type(),
                           type_pack<R, Types...>(),
			   f);
  }

  template <typename F, typename... Funs, typename... Vals, int... Indices,
	    typename R = decltype(std::declval<F>()(
		std::declval<Funs>()(std::declval<Vals>())...))>
  static R _ApplyElementwiseIndexed(
      F f, seq<Indices...>, std::tuple<Vals...> tup, Funs... funs) {
    return f(funs(std::get<Indices>(tup))...);
  }

  // Apply a series of functions elementwise to a tuple of values,
  // and pass the resulting values as arguments into a function f.
  template <typename F, typename... Funs, typename... Vals,
	    typename R = decltype(std::declval<F>()(
		std::declval<Funs>()(std::declval<Vals>())...))>
  static R ApplyElementwise(F f, std::tuple<Vals...> tup, Funs... funs) {
    return _ApplyElementwiseIndexed(f, typename gens<sizeof...(Vals)>::type(),
				    tup, funs...);
  }
};

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
  void operator()(value_type t) const { f_(t); }
private:
  Fn<value_type, void> f_;
};

// Specialize tuple consumers to let them also be constructed from
// functions that accept the tuple's contents elementwise.
template <typename... Types>
class Consumer<std::tuple<Types...>> {
public:
  typedef std::tuple<Types...> value_type;
  template <typename F> Consumer(const F& f)
      : f_(_TupleHelper::Uncurry<F, Types...>(f)) {}
  void operator()(value_type t) const { f_(t); }
private:
  Fn<value_type, void> f_;
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
//
// Filters are the building blocks of most pipelines. As we will see
// shortly, they combine in three ways:
// (1) chains:         F * G        (read: "F into G")
// (2) tees:           F + G        (read: "F and G")
// (3) cross products: FCross(F, G) (read: "F cross G")
//
// The corresponding semantics (in Pythonesque generator syntax):
//   (F * G)(x)           ==> for r in F(x):
//                              for s in G(r):
//                                yield s
//   (F + G)(x)           ==> for r in F(x): yield r
//                            for r in G(x): yield r
//   FCross(F, G)({x, y}) ==> for r in F(x):
//                              for s in G(y):
//                                yield {r, s}

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

// Consumers are covariant functors.
template <typename A, typename B>
Consumer<B> Cofmap(const Fn<B, A>& f, const Consumer<A>& c) {
  return [=](B x) { c(f(x)); };
}

// Producers are also monads, with the expected unit, join, and bind functions.
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

// Since producers are monads, they are also applicative functors,
// with the expected pure, apply, and lifting functions.
template <typename A>
Producer<A> PPure(A x) {
  return PUnit(x);
}

template <typename A, typename B>
Producer<B> PApply(const Producer<Fn<A, B>>& p_FnAB, const Producer<A>& p_A) {
  return p_FnAB |
      Fn<Fn<A, B>, Producer<B>>([=](Fn<A, B> f) { return Fmap(f, p_A); });
}

template <typename A, typename B>
Fn<const Producer<A>&, Producer<B>> LiftA(const Fn<A, B>& f) {
  return [=](Producer<A> p) { return Fmap(f, p); };
}

template <typename A, typename... Rest, typename B>
std::function<Producer<B>(Producer<A>, Producer<Rest>...)>
LiftA(const std::function<B(A, Rest...)>& f) {
  using std::placeholders::_1;
  return [=](Producer<A> p, Producer<Rest>... ps) {
    std::function<std::function<B(A)>(Rest...)> Bind =
        [=](const Rest&... xs) { return std::bind(f, _1, xs...); };
    Fn<A, Fn<std::function<B(A)>, B>> flip_app = [](const A& x) {
      return [=](std::function<B(A)> g){ return g(x); };
    };
    return PApply(Fmap(flip_app, p), LiftA(Bind)(ps...));
  };
}

template <typename... Args, typename B>
std::function<Producer<B>(Producer<Args>...)>
LiftA(B (*f)(Args...)) {
  return LiftA(std::function<B(Args...)>(f));
}

// The cross product of producers is a producer of the cross product
// of the produced values.
template <typename... Args>
std::tuple<Args...> make_tuple_byval(Args... xs) {
  return std::make_tuple(xs...);
}

template <typename... Args>
Producer<std::tuple<Args...>> PCross(Producer<Args>... ps) {
  return LiftA(make_tuple_byval<Args...>)(ps...);
}

// Filters can be chained. (This is just Klielsi composition for the
// underlying Producer monad.)
template <typename A, typename B, typename C>
Filter<A, C> KleisliComposition(const Filter<A, B>& f, const Filter<B, C>& g) {
  return [=](A x) { return f(x) | g; };
}

// The infix version of KleisliComposition is used to chain filters.
// Read * as "into": filterA * filterB means "FilterA into FilterB".
template <typename A, typename B, typename C>
Filter<A, C> operator*(const Filter<A, B>& f, const Filter<B, C>& g) {
  return KleisliComposition(f, g);
}

// Filters can also be composed to form T filters (value parallel).
// Law: (filter1 + filter2)(x) = filter1(x) + filter2(x)
template <typename A, typename B>
Filter<A, B> operator+(const Filter<A, B>& f, const Filter<A, B>& g) {
  return [=](A x) { return f(x) + g(x); };
}

// Some more template boilerplate, to prepare for filter cross products.
template <typename> struct _filter_input;
template <typename In, typename Out>
struct _filter_input<Filter<In, Out>> {
  using type = In;
};

template <typename... FilterTypes>
using _common_filter_input = typename std::common_type<
  typename _filter_input<FilterTypes>::type...>::type;

template <typename... FilterTypes>
using _filter_outputs_product = std::tuple<
  typename FilterTypes::result_type::value_type...>;

// The general cross product of filters takes a tuple of arguments,
// applies the filters to them elementwise, and then takes the cross
// product of the resulting producers.
// Law:  FCross(f, h) * FCross(g, i) === FCross(f * g, h * i).
template <
  typename... FilterTypes,
  typename In = std::tuple<typename _filter_input<FilterTypes>::type...>,
  typename Out = _filter_outputs_product<FilterTypes...> >
Filter<In, Out> FCross(FilterTypes... filters) {
  return [=](const In& tup) {
    return _TupleHelper::ApplyElementwise(
	PCross<typename FilterTypes::result_type::value_type...>,
	tup,
	filters...);
  };
}

// Filters support "forked" cross products if their input types are compatible.
// Law: FFork(g, h)(x) === PCross(g(x), h(x))
// Law: FFork(f, h) * FCross(g, i) === FFork(f * g, h * i).
template <typename... FilterTypes>
Filter<_common_filter_input<FilterTypes...>,
       _filter_outputs_product<FilterTypes...> >
FFork(FilterTypes... filters) {
  return [=](const _common_filter_input<FilterTypes...>& x) {
    return PCross(filters(x)...);
  };
}

#endif  // CONSUMERS_AND_PRODUCERS_H_

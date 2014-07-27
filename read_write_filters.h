// An algebraic library for building pipelines  -*- c++ -*-
// from filters that provide read/write access
// to underlying values.

#ifndef READ_WRITE_FILTERS_H_
#define READ_WRITE_FILTERS_H_

#include <functional>
#include <tuple>
#include <utility>

#include "consumers_and_producers.h"

// A read-write accessor for some underlying object of type A.
template <typename A> struct RW { const A& ro; A* rw; };

// A read-write filter that visits fields of type F on a probobuf of type P.
template <typename P, typename F> using RWFilter = Filter<RW<P>, RW<F>>;

// Convert a read-write filter into a const-reference filter.
template <typename P, typename F>
Filter<const P&, const F&> ReadOnly(const RWFilter<P, F>& rwfilt) {
  return [=](const P& p) {
    return [=](const Consumer<const F&>& roc) {
      Consumer<const RW<F>&> rwc = [&](const RW<F>& rwval) {
        roc(rwval.ro);
      };
      rwfilt(RW<P>{p, nullptr})(rwc);
    };
  };
};

// Convert a read-write filter into a mutable-pointer filter.
template <typename P, typename F>
Filter<P*, F*> ReadWrite(const RWFilter<P, F>& rwfilt) {
  return [=](P* p) {
    if (!p) {
      return PZero<F*>();
    }
    return Producer<F*> {
      [=](const Consumer<F*>& mpc) {
        Consumer<const RW<F>&> rwc = [&](const RW<F>& rwval) {
          mpc(rwval.rw);
        };
        rwfilt(RW<P>{*p, p})(rwc);
      }
    };
  };
};

// Helper for elementwise processing of tuples of read-write values.
struct _RWTupleHelper : public _TupleHelper {
  template<int... Indices, typename... Types>
  static std::tuple<const Types&...>
  TupleRO_indexed(seq<Indices...>, const std::tuple<RW<Types>...>& tup) {
    return std::tuple<const Types&...>(std::get<Indices>(tup).ro...);
  }

  template <typename... Types>
  static std::tuple<const Types&...>
  TupleRO(const std::tuple<RW<Types>...>& tup) {
    return TupleRO_indexed(typename gens<sizeof...(Types)>::type(), tup);
  }

  template <int... Indices, typename... Types>
  static std::tuple<Types*...>
  TupleRW_indexed(seq<Indices...>, const std::tuple<RW<Types>...>& tup) {
    return std::tuple<Types*...>(std::get<Indices>(tup).rw...);
  }

  template <typename... Types>
  static std::tuple<Types*...>
  TupleRW(const std::tuple<RW<Types>...>& tup) {
    return TupleRW_indexed(typename gens<sizeof...(Types)>::type(), tup);
  }
};

// Convert a filter of read-write tuples into one of const-reference tuples.
template <typename P, typename... Fs>
Filter<const P&, std::tuple<const Fs&...>>
ReadOnly(const Filter<RW<P>, std::tuple<RW<Fs>...>>& rwfilt) {
  return [=](const P& p) {
    return [=](const Consumer<std::tuple<const Fs&...>>& roc) {
      Consumer<const std::tuple<RW<Fs>...>&> rwc =
          [&](const std::tuple<RW<Fs>...>& rwval) {
        roc(_RWTupleHelper::TupleRO(rwval));
      };
      rwfilt(RW<P>{p, nullptr})(rwc);
    };
  };
};

// Convert a filter of read-write tuples into one of mutable-pointer tuples.
template <typename P, typename... Fs>
Filter<P*, std::tuple<Fs*...>>
ReadWrite(const Filter<RW<P>, std::tuple<RW<Fs>...>>& rwfilt) {
  return [=](P* p) {
    return [=](const Consumer<std::tuple<Fs*...>>& mpc) {
      Consumer<const std::tuple<RW<Fs>...>&> rwc =
          [&](const std::tuple<RW<Fs>...>& rwval) {
        mpc(_RWTupleHelper::TupleRW(rwval));
      };
      rwfilt(RW<P>{*p, p})(rwc);
    };
  };
};

#endif  // READ_WRITE_FILTERS_H_

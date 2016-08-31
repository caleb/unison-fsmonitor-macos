/*
 * Copyright (C) 2014 Łukasz Czerwiński
 *
 * GitHub: https://github.com/wo3kie/group_by
 *
 * Distributed under the BSD Software License (see file license)
 */

#pragma once

#include <map>
#include <vector>

namespace details {
  /*
   * return_type
   */

  template<typename T>
  struct return_type {
    typedef typename return_type<typename std::decay<decltype(&T::operator())>::type>::type type;
  };

  template<typename R, typename C, typename... A>
  struct return_type<R(C::*)(A...)> {
    typedef typename std::decay<R>::type type;
  };

  template<typename R, typename C, typename... A>
  struct return_type<R(C::*)(A...) const> {
    typedef typename std::decay<R>::type type;
  };

  template<typename R, typename... A>
  struct return_type<R(*)(A...)> {
    typedef typename std::decay<R>::type type;
  };
}

namespace details {
  /*
   * group_by
   */
  template<typename Arg, typename... Ts>
  struct group_by;

  template<typename Arg, typename T, typename... Ts>
  struct group_by<Arg, T, Ts...> {
    typedef std::map<
      typename details::return_type<typename std::decay<T>::type>::type,
      typename group_by<Arg, Ts...>::return_type
      > return_type;
  };

  template<typename Arg, typename T>
  struct group_by<Arg, T> {
    typedef std::map<
      typename return_type<typename std::decay<T>::type>::type,
      std::vector<Arg>
      > return_type;
  };
}

namespace details {
  /*
   * group_by_impl
   */
  template<typename Map, typename Iterator, typename F>
  void group_by_impl(Map& map, Iterator&& current, F&& f) {
    map[f(*current)].push_back(*current);
  }

  template<typename Map, typename Iterator, typename F, typename... Fs>
  void group_by_impl(Map& map, Iterator&& current, F&& f, Fs&&... fs) {
    group_by_impl(map[f(*current)], current, std::forward<Fs>(fs)...);
  }
}

/*
 * group_by
 */
template<typename Iterator, typename F, typename... Fs>
typename details::group_by<typename std::iterator_traits<Iterator>::value_type, F, Fs...>::return_type
group_by(Iterator begin, Iterator const end, F&& f, Fs&&... fs) {
  typename details::group_by<typename std::iterator_traits<Iterator>::value_type, F, Fs...>::return_type result;
  for (/* empty */; begin != end ; ++begin) {
    details::group_by_impl(result, begin, std::forward<F>(f), std::forward<Fs>(fs)...);
  }

  return result;
}

#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <type_traits>

namespace fm {
  namespace land {
    namespace types {
      template <typename T>
      struct ok {
        ok(const T &val) : val(val) {}

        ok(T &&val) : val(std::move(val)) {}

        T val;
      };

      template <>
      struct ok<void> {
      };

      template <typename E>
      struct err {
        err(const E &val) : val(val) {}

        err(E &&val) : val(std::move(val)) {}

        E val;
      };
    }

    template <typename T, typename CleanT = typename std::decay<T>::type>
    types::ok<CleanT> ok(T &&val) {
      return types::ok<CleanT>(std::forward<T>(val));
    }

    types::ok<void> ok() {
      return types::ok<void>();
    }

    template <typename E, typename CleanE = typename std::decay<E>::type>
    types::err<CleanE> err(E &&val) {
      return types::err<CleanE>(std::forward<E>(val));
    }

    template <typename T, typename E>
    struct result;

    namespace details {
      template <typename...>
      struct void_t {
        typedef void type;
      };

      namespace impl {
        template <typename Func>
        struct result_of;

        template <typename Ret, typename Cls, typename... Args>
        struct result_of<Ret (Cls::*)(Args...)> : public result_of<Ret(Args...)> {
        };

        template <typename Ret, typename... Args>
        struct result_of<Ret(Args...)> {
          typedef Ret type;
        };
      }

      template <typename Func>
      struct result_of : public impl::result_of<decltype(&Func::operator())> {
      };

      template <typename Ret, typename Cls, typename... Args>
      struct result_of<Ret (Cls::*)(Args...) const> {
        typedef Ret type;
      };

      template <typename Ret, typename... Args>
      struct result_of<Ret (*)(Args...)> {
        typedef Ret type;
      };

      template <typename R>
      struct result_ok_t {
        typedef typename std::decay<R>::type type;
      };

      template <typename T, typename E>
      struct result_ok_t<result<T, E>> {
        typedef T type;
      };

      template <typename R>
      struct result_err_t {
        typedef R type;
      };

      template <typename T, typename E>
      struct result_err_t<result<T, E>> {
        typedef typename std::remove_reference<E>::type type;
      };

      template <typename R>
      struct is_result : public std::false_type {
      };
      template <typename T, typename E>
      struct is_result<result<T, E>> : public std::true_type {
      };

      namespace ok {
        namespace impl {
          template <typename T>
          struct map_t;

          template <typename Ret, typename Cls, typename Arg>
          struct map_t<Ret (Cls::*)(Arg) const> : public map_t<Ret(Arg)> {
          };

          template <typename Ret, typename Cls, typename Arg>
          struct map_t<Ret (Cls::*)(Arg)> : public map_t<Ret(Arg)> {
          };

          // General implementation
          template <typename Ret, typename Arg>
          struct map_t<Ret(Arg)> {
            static_assert(!is_result<Ret>::value,
                          "Can not map a callback returning a result, use and_then instead");

            template <typename T, typename E, typename Func>
            static result<Ret, E> map(const result<T, E> &result, Func func) {
              static_assert(
                  std::is_same<T, Arg>::value ||
                      std::is_convertible<T, Arg>::value,
                  "Incompatible types detected");

              if (result.is_ok()) {
                auto res = func(result.storage().template get<T>());
                return types::ok<Ret>(std::move(res));
              }

              return types::err<E>(result.storage().template get<E>());
            }
          };

          // Specialization for callback returning void
          template <typename Arg>
          struct map_t<void(Arg)> {
            template <typename T, typename E, typename Func>
            static result<void, E> map(const result<T, E> &result, Func func) {

              if (result.is_ok()) {
                func(result.storage().template get<T>());
                return types::ok<void>();
              }

              return types::err<E>(result.storage().template get<E>());
            }
          };

          // Specialization for a void result
          template <typename Ret>
          struct map_t<Ret(void)> {
            template <typename T, typename E, typename Func>
            static result<Ret, E> map(const result<T, E> &result, Func func) {
              static_assert(std::is_same<T, void>::value,
                            "Can not map a void callback on a non-void result");

              if (result.is_ok()) {
                auto ret = func();
                return types::ok<Ret>(std::move(ret));
              }

              return types::err<E>(result.storage().template get<E>());
            }
          };

          // Specialization for callback returning void on a void result
          template <>
          struct map_t<void(void)> {
            template <typename T, typename E, typename Func>
            static result<void, E> map(const result<T, E> &result, Func func) {
              static_assert(std::is_same<T, void>::value,
                            "Can not map a void callback on a non-void result");

              if (result.is_ok()) {
                func();
                return types::ok<void>();
              }

              return types::err<E>(result.storage().template get<E>());
            }
          };

          // General specialization for a callback returning a result
          template <typename U, typename E, typename Arg>
          struct map_t<result<U, E>(Arg)> {
            template <typename T, typename Func>
            static result<U, E> map(const result<T, E> &result, Func func) {
              static_assert(
                  std::is_same<T, Arg>::value ||
                      std::is_convertible<T, Arg>::value,
                  "Incompatible types detected");

              if (result.is_ok()) {
                auto res = func(result.storage().template get<T>());
                return res;
              }

              return types::err<E>(result.storage().template get<E>());
            }
          };

          // Specialization for a void callback returning a result
          template <typename U, typename E>
          struct map_t<result<U, E>(void)> {
            template <typename T, typename Func>
            static result<U, E> map(const result<T, E> &result, Func func) {
              static_assert(std::is_same<T, void>::value,
                            "Can not call a void-callback on a non-void result");

              if (result.is_ok()) {
                auto res = func();
                return res;
              }

              return types::err<E>(result.storage().template get<E>());
            }
          };
        } // namespace impl

        template <typename Func>
        struct map_t : public impl::map_t<decltype(&Func::operator())> {
        };

        template <typename Ret, typename... Args>
        struct map_t<Ret (*)(Args...)> : public impl::map_t<Ret(Args...)> {
        };

        template <typename Ret, typename Cls, typename... Args>
        struct map_t<Ret (Cls::*)(Args...)> : public impl::map_t<Ret(Args...)> {
        };

        template <typename Ret, typename Cls, typename... Args>
        struct map_t<Ret (Cls::*)(Args...) const> : public impl::map_t<Ret(Args...)> {
        };

        template <typename Ret, typename... Args>
        struct map_t<std::function<Ret(Args...)>> : public impl::map_t<Ret(Args...)> {
        };
      } // namespace ok

      namespace err {
        namespace impl {
          template <typename T>
          struct map_t;

          template <typename Ret, typename Cls, typename Arg>
          struct map_t<Ret (Cls::*)(Arg) const> {

            static_assert(!is_result<Ret>::value,
                          "Can not map a callback returning a result, use or_else instead");

            template <typename T, typename E, typename Func>
            static result<T, Ret> map(const result<T, E> &result, Func func) {
              if (result.is_err()) {
                auto res = func(result.storage().template get<E>());
                return types::err<Ret>(res);
              }

              return types::ok<T>(result.storage().template get<T>());
            }

            template <typename E, typename Func>
            static result<void, Ret> map(const result<void, E> &result, Func func) {
              if (result.is_err()) {
                auto res = func(result.storage().template get<E>());
                return types::err<Ret>(res);
              }

              return types::ok<void>();
            }
          };
        } // namespace impl

        template <typename Func>
        struct map_t : public impl::map_t<decltype(&Func::operator())> {
        };
      } // namespace err;

      namespace _and {
        namespace impl {
          template <typename Func>
          struct then_t;

          template <typename Ret, typename... Args>
          struct then_t<Ret (*)(Args...)> : public then_t<Ret(Args...)> {
          };

          template <typename Ret, typename Cls, typename... Args>
          struct then_t<Ret (Cls::*)(Args...)> : public then_t<Ret(Args...)> {
          };

          template <typename Ret, typename Cls, typename... Args>
          struct then_t<Ret (Cls::*)(Args...) const> : public then_t<Ret(Args...)> {
          };

          template <typename Ret, typename Arg>
          struct then_t<Ret(Arg)> {
            static_assert(std::is_same<Ret, void>::value,
                          "then() should not return anything, use map() instead");

            template <typename T, typename E, typename Func>
            static result<T, E> then(const result<T, E> &result, Func func) {
              if (result.is_ok()) {
                func(result.storage().template get<T>());
              }
              return result;
            }
          };

          template <typename Ret>
          struct then_t<Ret(void)> {
            static_assert(std::is_same<Ret, void>::value,
                          "then() should not return anything, use map() instead");

            template <typename T, typename E, typename Func>
            static result<T, E> then(const result<T, E> &result, Func func) {
              static_assert(std::is_same<T, void>::value,
                            "Can not call a void-callback on a non-void result");

              if (result.is_ok()) {
                func();
              }

              return result;
            }
          };

        } // namespace impl

        template <typename Func>
        struct then_t : public impl::then_t<decltype(&Func::operator())> {
        };

        template <typename Ret, typename... Args>
        struct then_t<Ret (*)(Args...)> : public impl::then_t<Ret(Args...)> {
        };

        template <typename Ret, typename Cls, typename... Args>
        struct then_t<Ret (Cls::*)(Args...)> : public impl::then_t<Ret(Args...)> {
        };

        template <typename Ret, typename Cls, typename... Args>
        struct then_t<Ret (Cls::*)(Args...) const> : public impl::then_t<Ret(Args...)> {
        };
      } // namespace _and

      namespace _or {
        namespace impl {
          template <typename Func>
          struct _else;

          template <typename Ret, typename... Args>
          struct _else<Ret (*)(Args...)> : public _else<Ret(Args...)> {
          };

          template <typename Ret, typename Cls, typename... Args>
          struct _else<Ret (Cls::*)(Args...)> : public _else<Ret(Args...)> {
          };

          template <typename Ret, typename Cls, typename... Args>
          struct _else<Ret (Cls::*)(Args...) const> : public _else<Ret(Args...)> {
          };

          template <typename T, typename F, typename Arg>
          struct _else<result<T, F>(Arg)> {

            template <typename E, typename Func>
            static result<T, F> or_else(const result<T, E> &result, Func func) {
              static_assert(
                  std::is_same<E, Arg>::value ||
                      std::is_convertible<E, Arg>::value,
                  "Incompatible types detected");

              if (result.is_err()) {
                auto res = func(result.storage().template get<E>());
                return res;
              }

              return types::ok<T>(result.storage().template get<T>());
            }

            template <typename E, typename Func>
            static result<void, F> or_else(const result<void, E> &result, Func func) {
              if (result.is_err()) {
                auto res = func(result.storage().template get<E>());
                return res;
              }

              return types::ok<void>();
            }
          };

          template <typename T, typename F>
          struct _else<result<T, F>(void)> {
            template <typename E, typename Func>
            static result<T, F> or_else(const result<T, E> &result, Func func) {
              static_assert(std::is_same<T, void>::value,
                            "Can not call a void-callback on a non-void result");

              if (result.is_err()) {
                auto res = func();
                return res;
              }

              return types::ok<T>(result.storage().template get<T>());
            }

            template <typename E, typename Func>
            static result<void, F> or_else(const result<void, E> &result, Func func) {
              if (result.is_err()) {
                auto res = func();
                return res;
              }

              return types::ok<void>();
            }
          };
        } // namespace impl

        template <typename Func>
        struct _else : public impl::_else<decltype(&Func::operator())> {
        };

        template <typename Ret, typename... Args>
        struct _else<Ret (*)(Args...)> : public impl::_else<Ret(Args...)> {
        };

        template <typename Ret, typename Cls, typename... Args>
        struct _else<Ret (Cls::*)(Args...)> : public impl::_else<Ret(Args...)> {
        };

        template <typename Ret, typename Cls, typename... Args>
        struct _else<Ret (Cls::*)(Args...) const> : public impl::_else<Ret(Args...)> {
        };

      } // namespace _or

      namespace other {
        namespace impl {
          template <typename Func>
          struct wise;

          template <typename Ret, typename... Args>
          struct wise<Ret (*)(Args...)> : public wise<Ret(Args...)> {
          };

          template <typename Ret, typename Cls, typename... Args>
          struct wise<Ret (Cls::*)(Args...)> : public wise<Ret(Args...)> {
          };

          template <typename Ret, typename Cls, typename... Args>
          struct wise<Ret (Cls::*)(Args...) const> : public wise<Ret(Args...)> {
          };

          template <typename Ret, typename Arg>
          struct wise<Ret(Arg)> {
            template <typename T, typename E, typename Func>
            static result<T, E> otherwise(const result<T, E> &result, Func func) {
              static_assert(
                  std::is_same<E, Arg>::value ||
                      std::is_convertible<E, Arg>::value,
                  "Incompatible types detected");

              static_assert(std::is_same<Ret, void>::value,
                            "callback should not return anything, use map_err() for that");

              if (result.is_err()) {
                func(result.storage().template get<E>());
              }
              return result;
            }
          };
        } // namespace impl

        template <typename Func>
        struct wise : public impl::wise<decltype(&Func::operator())> {
        };

        template <typename Ret, typename... Args>
        struct wise<Ret (*)(Args...)> : public impl::wise<Ret(Args...)> {
        };

        template <typename Ret, typename Cls, typename... Args>
        struct wise<Ret (Cls::*)(Args...)> : public impl::wise<Ret(Args...)> {
        };

        template <typename Ret, typename Cls, typename... Args>
        struct wise<Ret (Cls::*)(Args...) const> : public impl::wise<Ret(Args...)> {
        };
      } // namespace other

      template <typename T, typename E, typename Func,
                typename Ret =
                    result<
                        typename details::result_ok_t<
                            typename details::result_of<Func>::type>::type,
                        E>>
      Ret map(const result<T, E> &result, Func func) {
        return ok::map_t<Func>::map(result, func);
      }

      template <typename T, typename E, typename Func,
                typename Ret =
                    result<T,
                           typename details::result_err_t<
                               typename details::result_of<Func>::type>::type>>
      Ret map_err(const result<T, E> &result, Func func) {
        return err::map_t<Func>::map(result, func);
      }

      template <typename T, typename E, typename Func>
      result<T, E> then(const result<T, E> &result, Func func) {
        return _and::then_t<Func>::then(result, func);
      }

      template <typename T, typename E, typename Func>
      result<T, E> otherwise(const result<T, E> &result, Func func) {
        return other::wise<Func>::otherwise(result, func);
      }

      template <typename T, typename E, typename Func,
                typename Ret =
                    result<T,
                           typename details::result_err_t<
                               typename details::result_of<Func>::type>::type>>
      Ret or_else(const result<T, E> &result, Func func) {
        return _or::_else<Func>::or_else(result, func);
      }

      struct ok_tag {
      };
      struct err_tag {
      };

      template <typename T, typename E>
      struct storage {
        static constexpr size_t Size = sizeof(T) > sizeof(E) ? sizeof(T) : sizeof(E);
        static constexpr size_t Align = sizeof(T) > sizeof(E) ? alignof(T) : alignof(E);

        typedef typename std::aligned_storage<Size, Align>::type type;

        storage() : initialized_(false) {}

        void construct(types::ok<T> &ok) {
          new (&storage_) T(ok.val);
          initialized_ = true;
        }

        void construct(types::ok<T> &&ok) {
          new (&storage_) T(std::move(ok.val));
          initialized_ = true;
        }

        void construct(types::err<E> &err) {
          new (&storage_) E(err.val);
          initialized_ = true;
        }

        void construct(types::err<E> &&err) {
          new (&storage_) E(std::move(err.val));
          initialized_ = true;
        }

        template <typename U>
        void raw_construct(U &&val) {
          typedef typename std::decay<U>::type CleanU;

          new (&storage_) CleanU(std::forward<U>(val));
          initialized_ = true;
        }

        template <typename U>
        const U &get() const {
          return *reinterpret_cast<const U *>(&storage_);
        }

        template <typename U>
        U &get() {
          return *reinterpret_cast<U *>(&storage_);
        }

        void destroy(ok_tag) {
          if (initialized_) {
            get<T>().~T();
            initialized_ = false;
          }
        }

        void destroy(err_tag) {
          if (initialized_) {
            get<E>().~E();
            initialized_ = false;
          }
        }

        type storage_;
        bool initialized_;
      };

      template <typename E>
      struct storage<void, E> {
        typedef typename std::aligned_storage<sizeof(E), alignof(E)>::type type;

        void construct(types::ok<void>) {
          initialized_ = true;
        }

        void construct(types::err<E> &err) {
          new (&storage_) E(err.val);
          initialized_ = true;
        }

        void construct(types::err<E> &&err) {
          new (&storage_) E(std::move(err.val));
          initialized_ = true;
        }

        template <typename U>
        void raw_construct(U &&val) {
          typedef typename std::decay<U>::type CleanU;

          new (&storage_) CleanU(std::forward<U>(val));
          initialized_ = true;
        }

        void destroy(ok_tag) { initialized_ = false; }

        void destroy(err_tag) {
          if (initialized_) {
            get<E>().~E();
            initialized_ = false;
          }
        }

        template <typename U>
        const U &get() const {
          return *reinterpret_cast<const U *>(&storage_);
        }

        template <typename U>
        U &get() {
          return *reinterpret_cast<U *>(&storage_);
        }

        type storage_;
        bool initialized_;
      };

      template <typename T, typename E>
      struct constructor {
        static void move(storage<T, E> &&src, storage<T, E> &dst, ok_tag) {
          dst.raw_construct(std::move(src.template get<T>()));
          src.destroy(ok_tag());
        }

        static void copy(const storage<T, E> &src, storage<T, E> &dst, ok_tag) {
          dst.raw_construct(src.template get<T>());
        }

        static void move(storage<T, E> &&src, storage<T, E> &dst, err_tag) {
          dst.raw_construct(std::move(src.template get<E>()));
          src.destroy(err_tag());
        }

        static void copy(const storage<T, E> &src, storage<T, E> &dst, err_tag) {
          dst.raw_construct(src.template get<E>());
        }
      };

      template <typename E>
      struct constructor<void, E> {
        static void move(storage<void, E> &&src, storage<void, E> &dst, ok_tag) {
        }

        template <typename U>
        static void copy(const storage<void, E> &src, storage<void, E> &dst, ok_tag) {
        }

        static void move(storage<void, E> &&src, storage<void, E> &dst, err_tag) {
          dst.raw_construct(std::move(src.template get<E>()));
          src.destroy(err_tag());
        }

        template <typename U>
        static void copy(const storage<void, E> &src, storage<void, E> &dst, err_tag) {
          dst.raw_construct(src.template get<E>());
        }
      };
    } // namespace details

    namespace concept {
      template <typename T, typename = void>
      struct equality_comparable : std::false_type {
      };

      template <typename T>
      struct equality_comparable<T,
                                typename std::enable_if<
                                    true,
                                    typename details::void_t<decltype(std::declval<T>() == std::declval<T>())>::type>::type>
          : std::true_type {
      };
    } // namespace concept

    template <typename T, typename E = std::string>
    struct result {
      static_assert(!std::is_same<E, void>::value, "void error type is not allowed");

      typedef details::storage<T, E> storage_type;

      result(types::ok<T> &ok) : ok_(true) {
        storage_.construct(ok);
      }

      result(types::ok<T> &&ok) : ok_(true) {
        storage_.construct(std::move(ok));
      }

      result(types::err<E> &err) : ok_(false) {
        storage_.construct(err);
      }

      result(types::err<E> &&err) : ok_(false) {
        storage_.construct(std::move(err));
      }

      result(result &&other) {
        if (other.is_ok()) {
          details::constructor<T, E>::move(std::move(other.storage_), storage_, details::ok_tag());
          ok_ = true;
        } else {
          details::constructor<T, E>::move(std::move(other.storage_), storage_, details::err_tag());
          ok_ = false;
        }
      }

      result(const result &other) {
        if (other.is_ok()) {
          details::constructor<T, E>::copy(other.storage_, storage_, details::ok_tag());
          ok_ = true;
        } else {
          details::constructor<T, E>::copy(other.storage_, storage_, details::err_tag());
          ok_ = false;
        }
      }

      ~result() {
        if (ok_)
          storage_.destroy(details::ok_tag());
        else
          storage_.destroy(details::err_tag());
      }

      bool is_ok() const {
        return ok_;
      }

      bool is_err() const {
        return !ok_;
      }

      explicit operator bool() {
        return this->is_ok();
      }

      T expect(const char *str) const {
        if (!is_ok()) {
          std::fprintf(stderr, "%s\n", str);
          std::terminate();
        }
        return expect_impl(std::is_same<T, void>());
      }

      template <typename Func,
                typename Ret =
                    result<
                        typename details::result_ok_t<
                            typename details::result_of<Func>::type>::type,
                        E>>
      Ret map(Func func) const {
        return details::map(*this, func);
      }

      template <typename Func,
                typename Ret =
                    result<T,
                           typename details::result_err_t<
                               typename details::result_of<Func>::type>::type>>
      Ret map_err(Func func) const {
        return details::map_err(*this, func);
      }

      template <typename Func>
      result<T, E> then(Func func) const {
        return details::then(*this, func);
      }

      template <typename Func>
      result<T, E> otherwise(Func func) const {
        return details::otherwise(*this, func);
      }

      template <typename Func,
                typename Ret =
                    result<T,
                           typename details::result_err_t<
                               typename details::result_of<Func>::type>::type>>
      Ret or_else(Func func) const {
        return details::or_else(*this, func);
      }

      storage_type &storage() {
        return storage_;
      }

      const storage_type &storage() const {
        return storage_;
      }

      template <typename U = T>
      typename std::enable_if<
          !std::is_same<U, void>::value,
          U>::type
      unwrap_or(const U &default_value) const {
        if (is_ok()) {
          return storage().template get<U>();
        }
        return default_value;
      }

      template <typename U = T>
      typename std::enable_if<
          !std::is_same<U, void>::value,
          U>::type
      unwrap() const {
        if (is_ok()) {
          return storage().template get<U>();
        }

        std::fprintf(stderr, "Attempting to unwrap an error result\n");
        std::terminate();
      }

    private:
      T expect_impl(std::true_type) const {}

      T expect_impl(std::false_type) const { return storage_.template get<T>(); }

      bool ok_;
      storage_type storage_;
    };

    template <typename T, typename E>
    bool operator==(const result<T, E> &lhs, const result<T, E> &rhs) {
      static_assert(concept::equality_comparable<T>::value,
                    "T must be equality_comparable for result to be comparable");
      static_assert(concept::equality_comparable<E>::value,
                    "E must be equality_comparable for result to be comparable");

      if (lhs.is_ok() && rhs.is_ok()) {
        return lhs.storage().template get<T>() == rhs.storage().template get<T>();
      }
      if (lhs.is_err() && rhs.is_err()) {
        return lhs.storage().template get<E>() == rhs.storage().template get<E>();
      }
    }

    template <typename T, typename E>
    bool operator==(const result<T, E> &lhs, types::ok<T> ok) {
      static_assert(concept::equality_comparable<T>::value,
                    "T must be equality_comparable for result to be comparable");

      if (!lhs.is_ok())
        return false;

      return lhs.storage().template get<T>() == ok.val;
    }

    template <typename E>
    bool operator==(const result<void, E> &lhs, types::ok<void>) {
      return lhs.is_ok();
    }

    template <typename T, typename E>
    bool operator==(const result<T, E> &lhs, types::err<E> err) {
      static_assert(concept::equality_comparable<E>::value,
                    "E must be equality_comparable for result to be comparable");
      if (!lhs.is_err())
        return false;

      return lhs.storage().template get<E>() == err.val;
    }
  }
}

#define TRY(...)                                             \
  ({                                                         \
    auto res = __VA_ARGS__;                                  \
    if (!res.is_ok()) {                                       \
      typedef details::result_err_type<decltype(res)>::type E; \
      return types::err<E>(res.storage().get<E>());          \
    }                                                        \
    typedef details::result_ok_type<decltype(res)>::type T;    \
    res.storage().get<T>();                                  \
  })

#ifndef _TYPE_PACK_H_
#define _TYPE_PACK_H_

#include <algorithm>
#include <type_traits>
#include <utility>

namespace base {

template <class... Ts>
struct type_pack {};

template <class... Ts>
constexpr auto type_pack_v = type_pack<Ts...>{};

template <class T>
struct just_type {
    using type = T;
};

template <class T>
constexpr auto just_type_v = just_type<T>{};

template <class T>
using subtype = typename T::type;

template <class... Ts, class... Us>
constexpr bool operator==(type_pack<Ts...>, type_pack<Us...>) {
    return false;
}
template <class... Ts>
constexpr bool operator==(type_pack<Ts...>, type_pack<Ts...>) {
    return true;
}

template <class... Ts, class... Us>
constexpr bool operator!=(type_pack<Ts...>, type_pack<Us...>) {
    return true;
}
template <class... Ts>
constexpr bool operator!=(type_pack<Ts...>, type_pack<Ts...>) {
    return false;
}

template <class T, class U>
constexpr bool operator==(just_type<T>, just_type<U>) {
    return false;
}
template <class T>
constexpr bool operator==(just_type<T>, just_type<T>) {
    return true;
}

template <class T, class U>
constexpr bool operator!=(just_type<T>, just_type<U>) {
    return true;
}
template <class T>
constexpr bool operator!=(just_type<T>, just_type<T>) {
    return false;
}

template <class... Ts, class... Us>
constexpr auto operator+(type_pack<Ts...>, type_pack<Us...>) {
    return type_pack_v<Ts..., Us...>;
}

namespace tp {

using empty_pack = type_pack<>;

constexpr auto empty_pack_v = empty_pack{};

template <class... Ts>
constexpr std::size_t size(type_pack<Ts...>) {
    return sizeof...(Ts);
}

template <class... Ts>
constexpr bool empty(type_pack<Ts...> tp) {
    return size(tp) == 0;
}

static_assert(empty(empty_pack_v));
static_assert(!empty(type_pack_v<int, double>));

template <class T, class... Ts>
constexpr auto head(type_pack<T, Ts...>) {
    return just_type_v<T>;
}

template <class T, class... Ts>
constexpr auto tail(type_pack<T, Ts...>) {
    return type_pack_v<Ts...>;
}

template <class... Ts>
constexpr auto make_pack(just_type<Ts>...) {
    return type_pack_v<Ts...>;
}

template <class F, class... Ts>
constexpr decltype(auto) apply(F f, type_pack<Ts...>) {
    return f(just_type_v<Ts>...);
}

template <template <class...> class F>
struct value_fn {
    template <class... Ts>
    constexpr auto operator()(just_type<Ts...>) {
        return F<Ts...>::value;
    }
};

template <template <class...> class F>
constexpr value_fn<F> value_fn_v;

template <template <class...> class F>
struct type_fn {
    template <class... Ts>
    constexpr auto operator()(just_type<Ts...>) {
        return just_type_v<subtype<F<Ts...>>>;
    }
};

template <template <class...> class F>
constexpr type_fn<F> type_fn_v;

// ==================== push front ====================

// type-based
template <class T, class... Ts>
constexpr auto push_front(type_pack<Ts...>) {
    return type_pack_v<T, Ts...>;
}

// value-based
template <class... Ts, class T>
constexpr auto push_front(type_pack<Ts...>, just_type<T>) {
    return type_pack_v<T, Ts...>;
}

static_assert(push_front<int>(type_pack_v<double, char>) ==
              type_pack_v<int, double, char>);

// ==================== pop front ====================

template <class T, class... Ts>
constexpr auto pop_front(type_pack<T, Ts...>) {
    return type_pack_v<Ts...>;
}

static_assert(pop_front(type_pack_v<int, double, char>) ==
              type_pack_v<double, char>);

// ==================== push back ====================

// type-based
template <class T, class... Ts>
constexpr auto push_back(type_pack<Ts...>) {
    return type_pack_v<Ts..., T>;
}

// value-based
template <class... Ts, class T>
constexpr auto push_back(type_pack<Ts...>, just_type<T>) {
    return type_pack_v<Ts..., T>;
}

static_assert(push_back<int>(type_pack_v<double, char>) ==
              type_pack_v<double, char, int>);

// ==================== contains ====================

// type-based
template <class T, class... Ts>
constexpr bool contains(type_pack<Ts...>) {
    return (... || std::is_same_v<T, Ts>);
}

// value-based
template <class... Ts, class T>
constexpr bool contains(type_pack<Ts...> tp, just_type<T>) {
    return contains<T>(tp);
}

static_assert(contains<int>(type_pack_v<int, double, char>));
static_assert(!contains<int*>(type_pack_v<int, double, char>));
static_assert(!contains<int>(empty_pack_v));

// ==================== find ====================

// type-based
template <class T, class... Ts>
constexpr std::size_t find(type_pack<Ts...> tp) {
    bool bs[] = {std::is_same_v<T, Ts>...};
    for (std::size_t i = 0; i < size(tp); ++i) {
        if (bs[i]) {
            return i;
        }
    }
    return size(tp);
}

// value-based
template <class... Ts, class T>
constexpr std::size_t find(type_pack<Ts...> tp, just_type<T>) {
    return find<T>(tp);
}

static_assert(find<double>(type_pack_v<int, double, char>) == 1);

// ==================== find if ====================

// type-based
template <template <class...> class F, class... Ts>
constexpr std::size_t find_if(type_pack<Ts...> tp) {
    bool bs[] = {F<Ts>::value...};
    for (std::size_t i = 0; i < size(tp); ++i) {
        if (bs[i]) {
            return i;
        }
    }
    return size(tp);
}

// value-based
template <class F, class... Ts>
constexpr std::size_t find_if(F f, type_pack<Ts...> tp) {
    bool bs[] = {f(just_type_v<Ts>)...};
    for (std::size_t i = 0; i < size(tp); ++i) {
        if (bs[i]) {
            return i;
        }
    }
    return size(tp);
}

static_assert(find_if<std::is_pointer>(type_pack_v<int, double*, char>) == 1);

// ==================== any, all, none of ====================

template <template <class...> class F, class... Ts>
constexpr bool all_of(type_pack<Ts...>) {
    return (... && F<Ts>::value);
}

template <template <class...> class F, class... Ts>
constexpr bool any_of(type_pack<Ts...>) {
    return (... || F<Ts>::value);
}

template <template <class...> class F, class... Ts>
constexpr bool none_of(type_pack<Ts...> tp) {
    return !any_of<F>(tp);
}

static_assert(all_of<std::is_pointer>(type_pack_v<int*, double*, char*>));
static_assert(all_of<std::is_pointer>(empty_pack_v));
static_assert(any_of<std::is_reference>(type_pack_v<int&, double, char**>));
static_assert(!any_of<std::is_reference>(empty_pack_v));
static_assert(none_of<std::is_void>(type_pack_v<int, double, char>));
static_assert(none_of<std::is_void>(empty_pack_v));

// ==================== transform ====================

// type-based
template <template <class...> class F, class... Ts>
constexpr auto transform(type_pack<Ts...>) {
    return type_pack_v<subtype<F<Ts>>...>;
}

// value-based
template <class F, class... Ts>
constexpr auto transform(F f, type_pack<Ts...>) {
    return make_pack(f(just_type_v<Ts>)...);
}

static_assert(transform<std::add_pointer>(type_pack_v<int, double, char>) ==
              type_pack_v<int*, double*, char*>);

static_assert(transform(type_fn_v<std::add_pointer>,
                        type_pack_v<int, double, char>) ==
              type_pack_v<int*, double*, char*>);

// ==================== reverse ====================

namespace detail {

template <class... Ts>
constexpr auto reverse_impl(empty_pack, type_pack<Ts...>) {
    return type_pack_v<Ts...>;
}

template <class T, class... Ts, class... Us>
constexpr auto reverse_impl(type_pack<T, Ts...>, type_pack<Us...>) {
    return reverse_impl(type_pack_v<Ts...>, type_pack_v<T, Us...>);
}

}  // namespace detail

template <class... Ts>
constexpr auto reverse(type_pack<Ts...> tp) {
    return detail::reverse_impl(tp, {});
}

static_assert(reverse(type_pack_v<int, double, char>) ==
              type_pack_v<char, double, int>);

// ==================== get ====================

namespace detail {

template <class IS>
struct get_impl;

template <std::size_t... Is>
struct get_impl<std::index_sequence<Is...>> {
    template <class T>
    static constexpr T dummy(decltype(Is, (void*)0)..., T*, ...);
};

}  // namespace detail

template <std::size_t I, class... Ts>
constexpr auto get(type_pack<Ts...>) {
    return just_type_v<decltype(
        detail::get_impl<std::make_index_sequence<I>>::dummy((Ts*)(0)...))>;
}

static_assert(get<1>(type_pack_v<double, int, char>) == just_type_v<int>);

// ==================== generate ====================

namespace detail {

template <class... Ts>
constexpr auto generate_helper(Ts*...) {
    return type_pack_v<Ts...>;
}

template <class T, std::size_t... Is>
constexpr auto generate_impl(std::index_sequence<Is...>) {
    return generate_helper(((void)Is, (T*)0)...);
}

}  // namespace detail

template <std::size_t I, class T>
constexpr auto generate() {
    return detail::generate_impl<T>(std::make_index_sequence<I>{});
}

static_assert(generate<3, int>() == type_pack_v<int, int, int>);
static_assert(generate<3, int>() != type_pack_v<int, int, double>);

// ==================== filter ====================

namespace detail {

template <template <class...> class F, class T>
constexpr auto filter_one() {
    if constexpr (F<T>::value) {
        return type_pack_v<T>;
    } else {
        return empty_pack_v;
    }
}

template <class F, class T>
constexpr auto filter_one(F f, just_type<T> jt) {
    if constexpr (f(jt)) {
        return type_pack_v<T>;
    } else {
        return empty_pack_v;
    }
}

}  // namespace detail

// type-based
template <template <class...> class F, class... Ts>
constexpr auto filter(type_pack<Ts...>) {
    return (empty_pack_v + ... + detail::filter_one<F, Ts>());
}

// value-based
template <class F, class... Ts>
constexpr auto filter(F f, type_pack<Ts...>) {
    return (empty_pack_v + ... + detail::filter_one(f, just_type_v<Ts>));
}

static_assert(filter<std::is_pointer>(type_pack_v<char, double*, int*>) ==
              type_pack_v<double*, int*>);
static_assert(filter<std::is_pointer>(empty_pack_v) == empty_pack_v);

}  // namespace tp

}  // namespace base

#endif  // _TYPE_PACK_H_

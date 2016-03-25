// The MIT License (MIT) 

// Copyright (c) 2013-2016 Rapptz, ThePhD and contributors

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SOL_STACK_CHECK_HPP
#define SOL_STACK_CHECK_HPP

#include "stack_core.hpp"
#include "usertype_traits.hpp"
#include "inheritance.hpp"
#include <memory>
#include <functional>
#include <utility>

namespace sol {
namespace stack {
namespace stack_detail {
template <typename T>
inline bool check_metatable(lua_State* L, int index = -2) {
    luaL_getmetatable(L, &usertype_traits<T>::metatable[0]);
    const type expectedmetatabletype = static_cast<type>(lua_type(L, -1));
    if (expectedmetatabletype != type::nil) {
        if (lua_rawequal(L, -1, index) == 1) {
            lua_pop(L, 2);
            return true;
        }
    }
    lua_pop(L, 1);
    return false;
}

template <type expected, int(*check_func)(lua_State*, int)>
struct basic_check {
    template <typename Handler>
    static bool check (lua_State* L, int index, Handler&& handler) {
        bool success = check_func(L, index) == 1;
        if (!success) {
            // expected type, actual type
            handler(L, index, expected, type_of(L, index));
        }
        return success;
    }
};

template <bool b>
struct check_types {
    template <std::size_t I0, std::size_t... I, typename Arg0, typename... Args, typename Handler>
    static bool check(types<Arg0, Args...>, std::index_sequence<I0, I...>, lua_State* L, int firstargument, Handler&& handler) {
        if (!stack::check<Arg0>(L, firstargument + I0, handler))
            return false;
        return check(types<Args...>(), std::index_sequence<I...>(), L, firstargument, std::forward<Handler>(handler));
    }

    template <typename Handler>
    static bool check(types<>, std::index_sequence<>, lua_State*, int, Handler&&) {
        return true;
    }
};

template <>
struct check_types<false> {
    template <std::size_t... I, typename... Args, typename Handler>
    static bool check(types<Args...>, std::index_sequence<I...>, lua_State*, int, Handler&&) {
        return true;
    }
};
} // stack_detail

template <typename T, type expected, typename>
struct checker {
    template <typename Handler>
    static bool check (lua_State* L, int index, Handler&& handler) {
        const type indextype = type_of(L, index);
        bool success = expected == indextype;
        if (!success) {
            // expected type, actual type
            handler(L, index, expected, indextype);
        }
        return success;
    }
};

template <type expected, typename C>
struct checker<type, expected, C> {
    template <typename Handler>
    static bool check (lua_State*, int, Handler&&) {
        return true;
    }
};

template <type expected, typename C>
struct checker<nil_t, expected, C> {
    template <typename Handler>
    static bool check (lua_State* L, int index, Handler&& handler) {
        bool success = lua_isnoneornil(L, index);
        if (!success) {
            // expected type, actual type
            handler(L, index, expected, type_of(L, index));
        }
        return success;
    }
};

template <typename T, typename C>
struct checker<T, type::poly, C> {
    template <typename Handler>
    static bool check (lua_State* L, int index, Handler&& handler) {
        bool success = !lua_isnone(L, index);
        if (!success) {
            // expected type, actual type
            handler(L, index, type::none, type_of(L, index));
        }
        return success;
    }
};

template <typename T, typename C>
struct checker<T, type::lightuserdata, C> {
    template <typename Handler>
    static bool check (lua_State* L, int index, Handler&& handler) {
        type t = type_of(L, index);
	   bool success = t == type::userdata || t == type::lightuserdata;
        if (!success) {
            // expected type, actual type
            handler(L, index, type::lightuserdata, t);
        }
        return success;
    }
};

template <typename T, typename C>
struct checker<non_null<T>, type::userdata, C> : checker<T, lua_type_of<T>::value, C> {};

template <type X, typename C>
struct checker<lua_CFunction, X, C> : stack_detail::basic_check<type::function, lua_iscfunction> {};
template <type X, typename C>
struct checker<std::remove_pointer_t<lua_CFunction>, X, C> : checker<lua_CFunction, X, C> {};
template <type X, typename C>
struct checker<c_closure, X, C> : checker<lua_CFunction, X, C> {};

template <typename T, typename C>
struct checker<T*, type::userdata, C> {
    template <typename Handler>
    static bool check (lua_State* L, int index, Handler&& handler) {
        const type indextype = type_of(L, index);
        // Allow nil to be transformed to nullptr
        if (indextype == type::nil) {
            return true;
        }
        return checker<T, type::userdata, C>{}.check(types<T>(), L, indextype, index, std::forward<Handler>(handler));
    }
};

template <typename T, typename C>
struct checker<T, type::userdata, C> {
    template <typename U, typename Handler>
    static bool check (types<U>, lua_State* L, type indextype, int index, Handler&& handler) {
        if (indextype != type::userdata) {
            handler(L, index, type::userdata, indextype);
            return false;
        }
        if (meta::Or<std::is_same<T, light_userdata_value>, std::is_same<T, userdata_value>>::value)
            return true;
        if (lua_getmetatable(L, index) == 0) {
             handler(L, index, type::userdata, indextype);
             return false;
	   }
	   if (stack_detail::check_metatable<U>(L))
            return true;
	   if (stack_detail::check_metatable<U*>(L))
            return true;
	   if (stack_detail::check_metatable<unique_usertype<U>>(L))
            return true;
#ifndef SOL_NO_EXCEPTIONS
        lua_getfield(L, -1, &detail::base_class_check_key()[0]);
        void* basecastdata = lua_touserdata(L, -1);
        detail::throw_cast basecast = (detail::throw_cast)basecastdata;
        bool success = detail::catch_check<T>(basecast);
#elif !defined(SOL_NO_RTTI)
        lua_getfield(L, -1, &detail::base_class_check_key()[0]);
        if (stack::get<type>(L) == type::nil) {
            lua_pop(L, 2);
            return false;
        }
        void* basecastdata = lua_touserdata(L, -1);
        detail::inheritance_check_function ic = (detail::inheritance_check_function)basecastdata;
        bool success = ic(typeid(T));
#else
        // Topkek
        lua_getfield(L, -1, &detail::base_class_check_key()[0]);
        if (stack::get<type>(L) == type::nil) {
            lua_pop(L, 2);
            return false;
        }
        void* basecastdata = lua_touserdata(L, -1);
        detail::inheritance_check_function ic = (detail::inheritance_check_function)basecastdata;
        bool success = ic(detail::id_for<T>::value);
#endif // No Runtime Type Information || Exceptions
        lua_pop(L, 2);
        if (!success) {
            handler(L, index, type::userdata, indextype);
            return false;
        }
        return true;
    }

    template <typename Handler>
    static bool check (lua_State* L, int index, Handler&& handler) {
        const type indextype = type_of(L, index);
        return check(types<T>(), L, indextype, index, std::forward<Handler>(handler));
    }
};

template<typename T, typename Real, typename C>
struct checker<unique_usertype<T, Real>, type::userdata, C> {
    template <typename Handler>
    static bool check(lua_State* L, int index, Handler&& handler) {
        return checker<T, type::userdata, C>{}.check(L, index, std::forward<Handler>(handler));
    }
};

template<typename T, typename C>
struct checker<std::shared_ptr<T>, type::userdata, C> {
    template <typename Handler>
    static bool check(lua_State* L, int index, Handler&& handler) {
        return checker<unique_usertype<T, std::shared_ptr<T>>, type::userdata, C>{}.check(L, index, std::forward<Handler>(handler));
    }
};

template<typename T, typename D, typename C>
struct checker<std::unique_ptr<T, D>, type::userdata, C> {
    template <typename Handler>
    static bool check(lua_State* L, int index, Handler&& handler) {
        return checker<unique_usertype<T, std::unique_ptr<T, D>>, type::userdata, C>{}.check(L, index, std::forward<Handler>(handler));
    }
};

template<typename T, typename C>
struct checker<std::reference_wrapper<T>, type::userdata, C> {
    template <typename Handler>
    static bool check(lua_State* L, int index, Handler&& handler) {
        return checker<T, type::userdata, C>{}.check(L, index, std::forward<Handler>(handler));
    }
};

template<typename... Args, typename C>
struct checker<std::tuple<Args...>, type::poly, C> {
    template <std::size_t... I, typename Handler>
    static bool apply(std::index_sequence<I...> is, lua_State* L, int index, Handler&& handler) {
        index = index < 0 ? lua_absindex(L, index) - ( sizeof...(I) - 1 ) : index;
        return stack_detail::check_types<true>{}.check(types<Args...>(), is, L, index, handler);
    }

    template <typename Handler>
    static bool check(lua_State* L, int index, Handler&& handler) {
        return apply(std::index_sequence_for<Args...>(), L, index, std::forward<Handler>(handler));
    }
};

template<typename A, typename B, typename C>
struct checker<std::pair<A, B>, type::poly, C> {
    template <typename Handler>
    static bool check(lua_State* L, int index, Handler&& handler) {
        index = index < 0 ? lua_absindex(L, index) - 1 : index;
        return stack::check<A>(L, index, handler) && stack::check<B>(L, index + 1, handler);
    }
};

template<typename T, typename C>
struct checker<optional<T>, type::poly, C> {
    template <typename Handler>
    static bool check(lua_State* L, int index, Handler&& handler) {
        return stack::check<T>(L, index, std::forward<Handler>(handler));
    }
};
} // stack
} // sol

#endif // SOL_STACK_CHECK_HPP

// Honeycomb, Copyright (C) 2015 NewGamePlus Inc.  Distributed under the Boost Software License v1.0.
#pragma once

#include "Honey/Core/Preprocessor.h"

namespace honey
{

/// Meta-programming and compile-time util
/**
  * \defgroup Meta  Meta-programming
  */
/// @{

/// Meta-programming and compile-time util
namespace mt
{

/// Remove the unused parameter warning
#define mt_unused(Param)                                        (void)Param;
/// Returns type T unchanged
template<class T> struct identity                               { typedef T type; };
/// Holds a constant integral value
template<class T, T val> using Value                            = std::integral_constant<T, val>;
/// Always returns true.  Can be used to force a clause to be type dependent.
template<class...> struct True                                  : std::true_type {};
/// Variant of True for integers
template<int64...> struct True_int                              : std::true_type {};
/// Check if type is std::true_type
template<class T> using isTrue                                  = Value<bool, std::is_same<T, std::true_type>::value>;
/// Special void type, use where void is intended but implicit members are required (default ctor, copyable, etc.)
struct Void {};
/// Use to differentiate an overloaded function by type. Accepts dummy parameter default value: `func(tag<0> = 0)`
template<int> struct tag                                        { tag() = default; tag(int) {} };

/// Add reference to type
template<class T> using addRef                                  = std::add_lvalue_reference<T>;
/// Remove reference from type
template<class T> using removeRef                               = std::remove_reference<T>;
/// Add pointer to type
template<class T> using addPtr                                  = std::add_pointer<T>;
/// Remove pointer from type
template<class T> using removePtr                               = std::remove_pointer<T>;
/// Add top-level const qualifier and reference to type.  Use std::decay to remove top-level const/ref.
template<class T> using addConstRef                             = addRef<typename std::add_const<T>::type>;

/// Check if type is an lvalue reference
template<class T> using isLref                                  = Value<bool, std::is_lvalue_reference<T>::value>;
/// Check if type is an rvalue reference
template<class T> using isRref                                  = Value<bool, std::is_rvalue_reference<T>::value>;
/// Check if type is a reference
template<class T> using isRef                                   = Value<bool, std::is_reference<T>::value>;
/// Check if type is a pointer
template<class T> using isPtr                                   = Value<bool, std::is_pointer<T>::value>;
    
/// Opposite of std::enable_if
template<bool b, class T = void> using disable_if               = std::enable_if<!b, T>;

/// Variant of std::conditional for integers, stores result in `value`
template<bool b, int64 t, int64 f> struct conditional_int       : Value<int64, f> {};
template<int64 t, int64 f> struct conditional_int<true, t, f>   : Value<int64, t> {};

/// Version of std::is_base_of that removes reference qualifiers before testing
template<class Base, class Derived> struct is_base_of           : std::is_base_of<typename removeRef<Base>::type, typename removeRef<Derived>::type> {};

/// Check if T is a specialization of Template
template<class T, template<class...> class Template>
struct isSpecializationOf                                       : std::false_type {};
template<template<class...> class Template, class... Param>
struct isSpecializationOf<Template<Param...>, Template>         : std::true_type {};
    
/// Check if type is a tuple or a reference to one
template<class T> using isTuple                                 = isSpecializationOf<typename std::decay<T>::type, tuple>;

/// Check if functor is callable with arguments
template<class Func, class... Args>
class isCallable
{
    template<class F> static auto                               test(void*) -> decltype(declval<F>()(declval<Args>()...), std::true_type());
    template<class F> static std::false_type                    test(...);
public:
    static const bool value = isTrue<decltype(test<Func>(nullptr))>::value;
};

/** \cond */
namespace priv
{
    template<szt cur, szt end, class... Ts> struct typeAt;
    template<szt cur, szt end, class T, class... Ts> struct typeAt<cur, end, T, Ts...>      : typeAt<cur+1, end, Ts...> {};
    template<szt cur, class T, class... Ts> struct typeAt<cur, cur, T, Ts...>               { typedef T type; };
    template<szt cur, szt end> struct typeAt<cur, end> {};
    
    template<szt cur, class Match, class... Ts> struct typeIndex;
    template<szt cur, class Match, class T, class... Ts> struct typeIndex<cur, Match, T, Ts...> : typeIndex<cur+1, Match, Ts...> {};
    template<szt cur, class T, class... Ts> struct typeIndex<cur, T, T, Ts...>              : Value<szt, cur> {};
    template<szt cur, class Match> struct typeIndex<cur, Match>                             : Value<szt, szt(-1)> {};
}
/** \endcond */

/// Get type at index of parameter pack
template<szt I, class... Ts> using typeAt                       = typename priv::typeAt<0, I, Ts...>::type;
/// Get index of first matching type in parameter pack, returns -1 if not found
template<class Match, class... Ts> using typeIndex              = priv::typeIndex<0, Match, Ts...>;

/// Shorthand for std::index_sequence
template<szt... Ints> using idxseq                              = std::index_sequence<Ints...>;
/// Shorthand for std::make_index_sequence
template<szt N> using make_idxseq                               = std::make_index_sequence<N>;

/** \cond */
namespace priv
{
    template<class Func, class Tuple, szt... Seq>
    auto applyTuple(Func&& f, Tuple&& t, idxseq<Seq...>)        { return f(get<Seq>(forward<Tuple>(t))...); }
}
/** \endcond */

/// Call a function with arguments from an unpacked tuple. ie. `f(get<Indices>(t)...)`
template<class Func, class Tuple>
auto applyTuple(Func&& f, Tuple&& t)                            { return priv::applyTuple(forward<Func>(f), forward<Tuple>(t), make_idxseq<tuple_size<typename removeRef<Tuple>::type>::value>()); }

/// Get size (number of elements) of a std::array
template<class Array> using arraySize                           = Value<szt, sizeof(Array) / sizeof(typename Array::value_type)>;

/// Create an array of deduced type initialized with values
template<class T, class... Ts>
auto make_array(T&& t, Ts&&... ts) -> array<T, sizeof...(Ts)+1> { return {forward<T>(t), forward<Ts>(ts)...}; }

/** \cond */
namespace priv { inline void pass(std::initializer_list<int>) {} } //do nothing
/** \endcond */

/// Unpack and evaluate a parameter pack expression in sequence order.  Ex. `void foo(Args... args) { mt_unpackEval(func(args)); }`
#define mt_unpackEval(...)                                      mt::priv::pass({((__VA_ARGS__), 0)...})

inline void exec() {} //dummy to catch empty parameter pack
/// Execute a list of functions. Use to expand parameter packs in arbitrary statements: `exec([&]() { accum += get<Seq>(tuple); }...)`.
template<class Func, class... Funcs>
void exec(Func&& f, Funcs&&... fs)                              { f(); exec(forward<Funcs>(fs)...); }
    
/// Unroll a loop calling f(counter, args...) at each iteration
template<int64 begin, int64 end, int64 step = 1, class Func, class... Args, typename std::enable_if<begin == end, int>::type=0>
void for_(Func&& f, Args&&... args)                             {}
template<int64 begin, int64 end, int64 step = 1, class Func, class... Args, typename std::enable_if<begin != end, int>::type=0>
void for_(Func&& f, Args&&... args)                             { f(begin, forward<Args>(args)...); for_<begin+step, end, step>(forward<Func>(f), forward<Args>(args)...); }

/// Create a method to check if a class has a member with matching name and type
/**
  * `Result` stores the test result. `type` stores the member type if it exists, mt::Void otherwise.
  *
  *     struct A { int Foo;         };  =>  mt_hasMember(Foo);  ->  mt_HasMember_Foo<A, int A::*>           ->  { value = true,    type = int A::*         }
  *     struct A { void Foo(int);   };  =>  mt_hasMember(Foo);  ->  mt_HasMember_Foo<A, void (A::*)(int)>   ->  { value = true,    type = void (A::*)(int) }
  *     struct A { int Bar;         };  =>  mt_hasMember(Foo);  ->  mt_HasMember_Foo<A, int A::*>           ->  { value = false,   type = mt::Void         }
  *
  * mt_hasMember2() can be used to specify the test function name. \n
  * mt_hasMember2() must be used for operator checks because of the special characters in the operator name.
  */
#define mt_hasMember(MemberName)                                mt_hasMember2(MemberName, MemberName)
#define mt_hasMember2(MemberName, TestName)                                                                                     \
    template<class Class, class MemberType>                                                                                     \
    class mt_hasMember_##TestName                                                                                               \
    {                                                                                                                           \
        template<class T, T>                        struct matchType;                                                           \
        template<class T> static auto               memberMatch(void*) -> decltype(declval<matchType<MemberType, &T::MemberName>>(), std::true_type()); \
        template<class T> static std::false_type    memberMatch(...);                                                           \
    public:                                                                                                                     \
        static const bool value = mt::isTrue<decltype(memberMatch<Class>(nullptr))>::value;                                     \
        typedef typename std::conditional<value, MemberType, mt::Void>::type type;                                              \
    };


/// Create a method to check if a class has a nested type/class
/**
  * `value` stores the test result. `type` stores the nested type if it exists, mt::Void otherwise.
  *
  *     struct A { typedef int Foo; };              =>  mt_hasType(Foo);            ->  mt_hasType_Foo<A>   ->  { value = true,     type = int          }
  *     struct A { template<class> struct Foo{}; }; =>  mt_hasType2(Foo<int>, Foo); ->  mt_hasType_Foo<A>   ->  { value = true,     type = A::Foo<int>  }
  *     struct A { typedef int Bar; };              =>  mt_hasType(Foo);            ->  mt_hasType_Foo<A>   ->  { value = false,    type = mt::Void     }
  *
  * mt_hasType2() can be used to specify the test function name. \n
  * mt_hasType2() must be used if type has special characters (ie. <>, ::)
  */
#define mt_hasType(TypeName)                                    mt_hasType2(TypeName, TypeName)
#define mt_hasType2(TypeName, TestName)                                                                                         \
    template<class Class>                                                                                                       \
    class mt_hasType_##TestName                                                                                                 \
    {                                                                                                                           \
        template<class T> static auto               test(void*) -> decltype(declval<typename T::TypeName>(), std::true_type()); \
        template<class T> static std::false_type    test(...);                                                                  \
                                                                                                                                \
        template<bool Res, class Enable = void>                                                                                 \
        struct testType                                             { typedef mt::Void type; };                                 \
        template<bool Res>                                                                                                      \
        struct testType<Res, typename std::enable_if<Res>::type>    { typedef typename Class::TypeName type; };                 \
    public:                                                                                                                     \
        static const bool value = mt::isTrue<decltype(test<Class>(nullptr))>::value;                                            \
        typedef typename testType<value>::type type;                                                                            \
    };


/** \cond */
namespace priv
{
    mt_hasType(iterator_category)
    template<class T> struct isIterator                         : Value<bool, mt_hasType_iterator_category<T>::value || isPtr<T>::value> {};
}
/** \endcond */

/// Check if type is an iterator or a reference to one.  An iterator is a type that has member iterator_category or is a pointer.
template<class T> using isIterator                              = priv::isIterator<typename removeRef<T>::type>;

/// Get function type traits
/**
  * \class funcTraits
  *
  * Valid types for `T`:
  * - a function signature
  * - a function pointer / reference
  * - a member function
  * - a functor (function object) with an operator() that has a unique signature (non-templated and non-overloaded)
  * - a lambda function
  *
  * \retval Sig             function signature
  * \retval Base            base class if this is a non-static member function, `void` otherwise
  * \retval Return          return type
  * \retval arity           number of parameters, includes the hidden base pointer as the first param if there's a base class
  * \retval param<I>        parameter types, from 0 to `arity`-1
  */
template<class T> struct funcTraits;

/// Create a global object that will be initialized upon first access, so it can be accessed safely from a static context
#define mt_global(Class, Func, Ctor)                            inline UNBRACKET(Class)& Func()  { static UNBRACKET(Class) obj Ctor; return obj; }

/// Inherit to declare that class is not copyable
struct NoCopy
{
    NoCopy(const NoCopy&) = delete;
    NoCopy& operator=(const NoCopy&) = delete;
protected:
    NoCopy() = default;
};

/// Get minimum of all arguments
/** \class min */
template<int64... vals> struct min;
template<int64 val, int64... vals> struct min<val, vals...>     : Value<int64, (val < min<vals...>::value ? val : min<vals...>::value)> {};
template<int64 val> struct min<val>                             : Value<int64, val> {};
    
/// Get maximum of all arguments
/** \class max */
template<int64... vals> struct max;
template<int64 val, int64... vals> struct max<val, vals...>     : Value<int64, (val > max<vals...>::value ? val : max<vals...>::value)> {};
template<int64 val> struct max<val>                             : Value<int64, val> {};

/// Get the absolute value of a number
template<int64 val> using abs                                   = Value<int64, (val < 0) ? -val : val>;
/// Get the sign of a number
template<int64 val> using sign                                  = Value<int64, (val < 0) ? -1 : 1>;

/// Calc log base 2 of unsigned integer, rounded down to nearest integer. Returns -1 if x is zero.
template<uint64 x> struct log2Floor                             : Value<int, log2Floor<x/2>::value+1> {};
template<> struct log2Floor<0>                                  : Value<int, -1> {};

/// Calc greatest common divisor of a and b
template<int64 a, int64 b> struct gcd                           : gcd<b, a % b> {};
template<int64 a> struct gcd<a, 0>                              : Value<int64, abs<a>::value> {};
template<int64 b> struct gcd<0, b>                              : Value<int64, abs<b>::value> {};

/** \cond */
namespace priv
{
    template<uint64 N> struct byteCount                         : Value<int, ((max<log2Floor<N>::value, 1>::value + 7) >> 3)> {};

    template<uint64 N> struct byteType                          : identity<uint64> {};
    template<> struct byteType<4>                               : identity<uint32> {};
    template<> struct byteType<3>                               : identity<uint32> {};
    template<> struct byteType<2>                               : identity<uint16> {};
    template<> struct byteType<1>                               : identity<uint8> {};
}
/** \cond */

/// Get smallest unsigned integral type that can hold value N
template<uint64 N> struct uintByValue                           : priv::byteType<priv::byteCount<N>::value> {};
/// Get smallest unsigned integral type that has a size of at least N bytes
template<int N> struct uintBySize                               : priv::byteType<N> {};

//====================================================
// funcTraits
//====================================================
/** \cond */
namespace priv
{
    template<class T> struct functorTraits {};
}
/** \endcond */

template<class T> struct funcTraits                             : priv::functorTraits<decltype(&T::operator())> {};

/** \cond */
#define TRAITS(Ptr)                                                                         \
    template<class R, class... Args>                                                        \
    struct funcTraits<R Ptr (Args...)>                                                      \
    {                                                                                       \
        typedef R Sig(Args...);                                                             \
        typedef void Base;                                                                  \
        typedef R Return;                                                                   \
        static const szt arity = sizeof...(Args);                                           \
        template<szt I> using param = typeAt<I, Args...>;                                   \
    };                                                                                      \

#define M_TRAITS(Const)                                                                     \
    template<class R, class Base_, class... Args>                                           \
    struct funcTraits<R (Base_::*) (Args...) Const>                                         \
    {                                                                                       \
        typedef R (Base_::*Sig) (Args...) Const;                                            \
        typedef Base_ Base;                                                                 \
        typedef R Return;                                                                   \
        static const szt arity = sizeof...(Args)+1;                                         \
    private:                                                                                \
        template<szt I, szt _=0> struct param_  { typedef typeAt<I-1, Args...> type; };     \
        template<szt _> struct param_<0, _>     { typedef Const Base* type; };              \
    public:                                                                                 \
        template<szt I> using param = typename param_<I>::type;                             \
    };                                                                                      \
                                                                                            \
    namespace priv                                                                          \
    {                                                                                       \
    template<class R, class Base_, class... Args>                                           \
    struct functorTraits<R (Base_::*) (Args...) Const>                                      \
    {                                                                                       \
        typedef R Sig(Args...);                                                             \
        typedef void Base;                                                                  \
        typedef R Return;                                                                   \
        static const szt arity = sizeof...(Args);                                           \
        template<szt I> using param = mt::typeAt<I, Args...>;                               \
    };                                                                                      \
    }                                                                                       \

TRAITS()
TRAITS(&)
TRAITS(*)
M_TRAITS()
M_TRAITS(const)
#undef TRAITS
#undef M_TRAITS
/** \endcond */
//====================================================
/// @}

} }

// Honeycomb, Copyright (C) 2015 NewGamePlus Inc.  Distributed under the Boost Software License v1.0.
#pragma once

#include "Honey/Core/Meta.h"

/// Memory-management and allocators
/**
  * \defgroup Memory    Memory Util
  */
/// @{

/// Global `new_`, use in place of `new` keyword to provide allocator with debug info
#ifdef DEBUG
    #define new_                                new (__FILE__, __LINE__)
#else
    #define new_                                new
#endif

inline void* operator new(size_t size, const char* srcFile, int srcLine)    { mt_unused(srcFile); mt_unused(srcLine); return operator new(size); }
inline void* operator new[](size_t size, const char* srcFile, int srcLine)  { mt_unused(srcFile); mt_unused(srcLine); return operator new(size); }
/// @}

namespace honey
{

/// \addtogroup Memory
/// @{

/// Allocate memory for `count` number of T objects.  Objects are not constructed.
template<class T>
T* alloc(size_t count = 1)                      { return static_cast<T*>(operator new(sizeof(T)*count)); }
/// Deallocate memory and set pointer to null. Object is not destroyed.
template<class T>
void free(T*& p)                                { if (!p) return; operator delete(p); p = nullptr; }
template<class T>
void free(T* const& p)                          { if (!p) return; operator delete(p); }

/// Align a pointer to the previous byte boundary `bytes`. Does nothing if p is already on boundary.  Alignment must be a power of two.
template<class T>
T* alignFloor(T* p, int bytes)                  { return reinterpret_cast<T*>(intptr_t(p) & ~(bytes-1)); }
/// Align a pointer to the next byte boundary `bytes`. Does nothing if p is already on boundary.  Alignment must be a power of two.
template<class T>
T* alignCeil(T* p, int bytes)                   { return alignFloor(reinterpret_cast<T*>(intptr_t(p) + bytes-1), bytes); }

/// Allocate memory with alignment.  Alignment must be a power of two.  Allocator element type must be int8.
template<class T, class Alloc>
T* allocAligned(size_t count, int align_, Alloc&& a)
{
    static const int diffSize = sizeof(std::ptrdiff_t);
    int8* base = a.allocate(diffSize + align_-1 + sizeof(T)*count);
    if (!base) return nullptr;
    int8* p = alignCeil(base+diffSize, align_);
    *reinterpret_cast<std::ptrdiff_t*>(p-diffSize) = p - base;
    return reinterpret_cast<T*>(p);
}
/// Allocate memory with alignment using default allocator
template<class T>
T* allocAligned(size_t count, int align)        { return allocAligned<T>(count, align, std::allocator<int8>()); }

/// Deallocate aligned memory.  Allocator element type must be int8.
template<class T, class Alloc>
void freeAligned(T* p, Alloc&& a)
{
    if (!p) return;
    int8* p_ = reinterpret_cast<int8*>(p);
    int8* base = p_ - *reinterpret_cast<std::ptrdiff_t*>(p_ - sizeof(std::ptrdiff_t));
    a.deallocate(base, 1);
}
/// Deallocate aligned memory using default allocator
template<class T>
void freeAligned(T* p)                          { freeAligned(p, std::allocator<int8>()); }


/// Destruct object, free memory and set pointer to null
template<class T>
void delete_(T*& p)                             { delete p; p = nullptr; }
template<class T>
void delete_(T* const& p)                       { delete p; }

/// Destruct object, free memory using allocator and set pointer to null
template<class T, class Alloc>
void delete_(T*& p, Alloc&& a)                  { if (!p) return; a.destroy(p); a.deallocate(p,1); p = nullptr; }
template<class T, class Alloc>
void delete_(T* const& p, Alloc&& a)            { if (!p) return; a.destroy(p); a.deallocate(p,1); }

/// Destruct all array objects, free memory and set pointer to null
template<class T>
void deleteArray(T*& p)                         { delete[] p; p = nullptr; }
template<class T>
void deleteArray(T* const& p)                   { delete[] p; }

/// std::allocator compatible allocator
/**
  * Subclass must define:
  * - default/copy/copy-other ctors
  * - pointer allocate(size_type n, const void* hint = 0)
  * - pointer allocate(size_type n, const char* srcFile, int srcLine, const void* hint = 0)
  * - void deallocate(pointer p, size_type n)
  */
template<template<class> class Subclass, class T>
class Allocator
{
public:
    typedef T           value_type;
    typedef T*          pointer;
    typedef T&          reference;
    typedef const T*    const_pointer;
    typedef const T&    const_reference;
    typedef size_t      size_type;
    typedef ptrdiff_t   difference_type;

    pointer address(reference x) const                      { return &x; }
    const_pointer address(const_reference x) const          { return &x; }
    size_type max_size() const                              { return std::numeric_limits<size_type>::max(); }
    template<class U, class... Args>
    void construct(U* p, Args&&... args)                    { new((void*)p) U(forward<Args>(args)...); }
    template<class U>
    void destroy(U* p)                                      { p->~U(); }
    template<class U> struct rebind                         { typedef Subclass<U> other; };
    bool operator==(const Subclass<T>&) const               { return true; }
    bool operator!=(const Subclass<T>&) const               { return false; }

protected:
    Subclass<T>& subc()                                     { return static_cast<Subclass<T>&>(*this); }
    const Subclass<T>& subc() const                         { return static_cast<const Subclass<T>&>(*this); }
};

/// Objects that inherit from this class will use `Alloc` for new/delete ops
template<template<class> class Alloc>
class AllocatorObject
{
public:
    template<class T> using Allocator = Alloc<T>;

    void* operator new(size_t size)                                         { return _alloc.allocate(size); }
    void* operator new(size_t, void* ptr)                                   { return ptr; }
    void* operator new(size_t size, const char* srcFile, int srcLine)       { return _alloc.allocate(size, srcFile, srcLine); }
    
    void* operator new[](size_t size)                                       { return _alloc.allocate(size); }
    void* operator new[](size_t, void* ptr)                                 { return ptr; }
    void* operator new[](size_t size, const char* srcFile, int srcLine)     { return _alloc.allocate(size, srcFile, srcLine); }
    
    void operator delete(void* p)                                           { _alloc.deallocate(static_cast<int8*>(p), 1); }
    void operator delete[](void* p)                                         { _alloc.deallocate(static_cast<int8*>(p), 1); }

private:
    static Alloc<int8> _alloc;
};
template<template<class> class Alloc> Alloc<int8> AllocatorObject<Alloc>::_alloc;

/// Returns T::Allocator<T> if available, otherwise std::allocator<T>
template<class T, class = std::true_type>
struct DefaultAllocator                                                     { typedef std::allocator<T> type; };
template<class T>
struct DefaultAllocator<T[], std::true_type>                                { typedef std::allocator<T> type; };
template<class T>
struct DefaultAllocator<T, typename mt::True<typename T::template Allocator<T>>::type>
                                                                            { typedef typename T::template Allocator<T> type; };

/// Functor to delete a pointer
template<class T, class Alloc = typename DefaultAllocator<T>::type>
struct finalize
{
    finalize(Alloc a = Alloc())                 : a(move(a)) {}
    void operator()(T*& p)                      { delete_(p,a); }
    void operator()(T* const& p)                { delete_(p,a); }
    Alloc a;
};
/** \cond */
/// Specialization for array
template<class T>
struct finalize<T[], std::allocator<T>>
{
    void operator()(T*& p)                      { deleteArray(p); }
    void operator()(T* const& p)                { deleteArray(p); }
};
/// Specialization for void
template<>
struct finalize<void, std::allocator<void>>
{
    void operator()(void*& p)                   { free(p); }
    void operator()(void* const& p)             { free(p); }
};
/** \endcond */

/// @}

}
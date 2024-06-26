// This is a type-stable implementation of std::vector, for the purpose of ensuring that calls to size() and data()
// will always return the correct value regardless of the underlying type.  Based on the work of Loki Astari.

#pragma once

#include <type_traits>
#include <cstddef>
#include <utility>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <iterator>

namespace pf {

template<typename T> class vector {
public:
   using value_type        = T;
   using reference         = T &;
   using const_reference   = T const &;
   using pointer           = T *;
   using const_pointer     = T const *;
   using iterator          = T *;
   using const_iterator    = T const *;
   using riterator         = std::reverse_iterator<iterator>;
   using const_riterator   = std::reverse_iterator<const_iterator>;
   using difference_type   = std::ptrdiff_t;
   using size_type         = std::size_t;

private:
   static const size_type MIN_CAPACITY = 8;
   size_type capacity;
   size_type length;
   T *elements;

   struct Deleter {
      void operator()(T* elements) const {
         ::operator delete(elements);
      }
   };

public:
   vector(size_type capacity = MIN_CAPACITY) :
      capacity(capacity), length(0), elements(static_cast<T*>(::operator new(sizeof(T) * capacity)))
   { }

   template<typename I> vector(I begin, I end) : capacity(std::distance(begin, end)), length(0) {
      if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;
      elements = static_cast<T*>(::operator new(sizeof(T) * capacity));
      for (auto i = begin; i != end; ++i) {
         pushBackInternal(*i);
      }
   }

   vector(std::initializer_list<T> const& list) : vector(std::begin(list), std::end(list)) { }

   ~vector() {
      std::unique_ptr<T, Deleter> deleter(elements, Deleter());
      clearElements<T>();
   }

   vector(vector const &copy) : capacity(copy.length), length(0) {
      if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;
      elements = static_cast<T*>(::operator new(sizeof(T) * capacity));
      for (size_type i = 0; i < copy.length; ++i) {
         push_back(copy.elements[i]);
      }
   }

   vector& operator=(vector const &copy) {
      //copyAssign<T>(copy);
      vector<T> tmp(copy); // Copy and Swap idiom
      tmp.swap(*this);
      return *this;
   }

   vector(vector &&move) noexcept : capacity(0), length(0), elements(nullptr) {
      move.swap(*this);
   }

   vector& operator=(vector &&move) noexcept {
      move.swap(*this);
      return *this;
   }

   void swap(vector &other) noexcept {
      std::swap(capacity, other.capacity);
      std::swap(length, other.length);
      std::swap(elements, other.elements);
   }

   inline size_type size() const { return length; }
   inline bool      empty() const { return length == 0; }
   inline T* data() { return elements; }
   inline const T* data() const { return elements; }

   inline reference       operator[](size_type index) { return elements[index];}
   inline const_reference operator[](size_type index) const { return elements[index];}
   inline reference       front() { return elements[0];}
   inline const_reference front() const { return elements[0];}
   inline reference       back() { return elements[length - 1];}
   inline const_reference back() const { return elements[length - 1];}

   inline iterator        begin() { return elements;}
   inline riterator       rbegin() { return riterator(end());}
   inline const_iterator  begin() const { return elements;}
   inline const_riterator rbegin() const { return const_riterator(end());}

   inline iterator        end() { return elements + length;}
   inline riterator       rend() { return riterator(begin());}
   inline const_iterator  end() const { return elements + length;}
   inline const_riterator rend() const { return const_riterator(begin());}

   inline const_iterator  cbegin() const { return begin();}
   inline const_riterator crbegin() const { return rbegin();}
   inline const_iterator  cend() const { return end();}
   inline const_riterator crend() const { return rend();}

   inline iterator from(size_t pIndex) { return iterator(&elements[pIndex]); }

   // Erasure

   inline T * erase(iterator Ref) {
      erase(Ref, Ref + 1);
      return Ref;
   }

   inline T * erase(size_t Index) {
      erase(from(Index), from(Index + 1));
      return from(Index);
   }

   T * erase(iterator Start, iterator Stop) {
      if (Stop IS end()) {
         for (auto it = Start; it != Stop; it++) {
            (*it).~T();
            length--;
         }
      }
      else {
         for (auto it=Stop, start=Start; it != end(); it++, start++) {
            *start = std::move(*it);
         }
         auto total_removed = Stop - Start;
         length -= total_removed;
      }

      return Start;
   }

   iterator insert(const_iterator pTarget, T &pValue) {
      if (pTarget == end()) {
         push_back(pValue);
         return iterator(pTarget);
      }

      resize_if_required();

      for (auto it=end(); it != pTarget; it--) {
         if (it == begin()) break;
         *it = std::move(*(it-1));
      }
      *(iterator(pTarget)) = std::move(pValue);
      length++;
      return iterator(pTarget);
   }

   inline void insert(iterator pTarget, iterator pStart, iterator pEnd) {
      auto tgt = pTarget;
      for (auto it = pStart; it != pEnd; it++, tgt++) {
         *tgt = std::move(*it);
      }
   }

   // Comparison

   bool operator!=(vector const &rhs) const {return !(*this == rhs);}

   bool operator==(vector const &rhs) const {
      return (size() == rhs.size()) and std::equal(begin(), end(), rhs.begin());
   }

   void push_back(value_type const &value) {
      resize_if_required();
      pushBackInternal(value);
   }

   void push_back(value_type &&value) {
      resize_if_required();
      moveBackInternal(std::move(value));
   }

   template<typename... Args> T & emplace_back(Args&&... args) {
      resize_if_required();
      return *new (elements + length++) T(std::move(args)...);
   }

   void pop_back() {
      --length;
      elements[length].~T();
   }

   void reserve(size_type capacityUpperBound) {
      if (capacityUpperBound > capacity) {
         reserveCapacity(capacityUpperBound);
      }
   }

   void clear() {
      clearElements<T>();
      length = 0;
   }

   // INTERNAL FUNCTIONALITY

private:
   inline void resize_if_required() {
      if (length == capacity) {
         reserveCapacity(capacity * 2);
      }
   }

   void reserveCapacity(size_type newCapacity) {
      vector<T> tmpBuffer(newCapacity);
      simpleCopy<T>(tmpBuffer);
      tmpBuffer.swap(*this);
   }

   // Add new element to the end using placement new

   inline void pushBackInternal(T const &value) {
      new (elements + length) T(value);
      ++length;
   }

   inline void moveBackInternal(T &&value) {
      new (elements + length) T(std::move(value));
      ++length;
   }

   // Optimizations that use SFINAE to only instantiate one of two versions of a function.
   // simpleCopy()    Moves when no exceptions are guaranteed, otherwise copies.
   // clearElements() When no destructor remove loop.
   // copyAssign()    Avoid resource allocation when no exceptions guaranteed.
   //                 ie. When copying integers reuse the elements if we can
   //                 to avoid expensive resource allocation.

   template<typename X> typename std::enable_if<std::is_nothrow_move_constructible<X>::value == false>::type
      simpleCopy(vector<T>& dst) {
      std::for_each(elements, elements + length, [&dst](T const &v) {
         dst.pushBackInternal(v);
      });
   }

   template<typename X> typename std::enable_if<std::is_nothrow_move_constructible<X>::value == true>::type
      simpleCopy(vector<T>& dst) {
      std::for_each(elements, elements + length, [&dst](T &v){
         dst.moveBackInternal(std::move(v));
      });
   }

   template<typename X> typename std::enable_if<std::is_trivially_destructible<X>::value == false>::type
      clearElements() {
      for (size_type i = 0; i < length; ++i) {
         elements[length - 1 - i].~T();
      }
   }

   // Trivially destructible objects can be reused without using the destructor.

   template<typename X> typename std::enable_if<std::is_trivially_destructible<X>::value == true>::type
      clearElements() {
   }

   template<typename X> typename std::enable_if<(std::is_nothrow_copy_constructible<X>::value
      and std::is_nothrow_destructible<X>::value) == true>::type
      copyAssign(vector<X> &copy) {
      // This function is only used if there is no chance of an exception being
      // thrown during destruction or copy construction of the type T.

      if (this == &copy) return;

      if (capacity <= copy.length) { // Sufficient space available
         clearElements<T>();     // Potentially does nothing but if required will call the destructor of all elements.

         length = 0;
         for (size_type i=0; i < copy.length; ++i) {
            pushBackInternal(copy[i]);
         }
      }
      else {
         vector<T> tmp(copy);
         tmp.swap(*this);
      }
   }

   template<typename X> typename std::enable_if<(std::is_nothrow_copy_constructible<X>::value
      and std::is_nothrow_destructible<X>::value) == false>::type
      copyAssign(vector<X> &copy) {
      vector<T> tmp(copy); // Copy and Swap idiom
      tmp.swap(*this);
   }
};

} // namespace

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
#include <concepts>

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
      capacity(capacity), length(0), elements((T*)(::operator new(sizeof(T) * capacity)))
   { }

   template<std::input_iterator I> vector(I begin, I end) : capacity(std::distance(begin, end)), length(0) {
      if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;
      elements = (T*)(::operator new(sizeof(T) * capacity));
      for (auto i = begin; i != end; ++i) {
         pushBackInternal(*i);
      }
   }

   vector(std::initializer_list<T> const& list) : vector(std::begin(list), std::end(list)) { }

   ~vector() {
      clearElements<T>();
      ::operator delete(elements);
   }

   vector(vector const &copy) : capacity(copy.length), length(0) {
      if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;
      elements = (T*)(::operator new(sizeof(T) * capacity));
      std::uninitialized_copy(copy.elements, copy.elements + copy.length, elements);
      length = copy.length;
   }

   vector& operator=(vector const &copy) {
      if (this IS &copy) return *this;
      vector<T> tmp(copy); // Copy and Swap idiom
      tmp.swap(*this);
      return *this;
   }

   vector(vector &&move) noexcept : capacity(0), length(0), elements(nullptr) {
      move.swap(*this);
   }

   vector& operator=(vector &&move) noexcept {
      if (this IS &move) return *this;
      move.swap(*this);
      return *this;
   }

   void swap(vector &other) noexcept {
      std::swap(capacity, other.capacity);
      std::swap(length, other.length);
      std::swap(elements, other.elements);
   }

   inline size_type size() const { return length; }
   inline bool      empty() const { return length IS 0; }
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

   iterator erase(const_iterator pos) {
      return erase(pos, pos + 1);
   }

   iterator erase(const_iterator first, const_iterator last) {
      if (first > last or first < begin() or last > end()) {
         return const_cast<iterator>(first); // Invalid range, do nothing
      }

      auto start_pos = const_cast<iterator>(first);
      auto num_erased = std::distance(first, last);

      if (num_erased > 0) {
         // Move elements to fill the gap
         auto new_end = std::move(start_pos + num_erased, end(), start_pos);

         // Destruct the now-moved-from objects at the end
         for (iterator it = new_end; it != end(); ++it) {
            it->~T();
         }

         length -= num_erased;
      }

      return start_pos;
   }

   iterator insert(const_iterator pTarget, const T &pValue) {
      size_type index = pTarget - begin();
      if (length IS capacity) {
         // Re-evaluate index after reallocation
         reserveCapacity(capacity * 2);
         pTarget = begin() + index;
      }

      iterator target_iter = const_cast<iterator>(pTarget);

      // Shift elements from the target to the end one position to the right
      if (target_iter < end()) {
         std::move_backward(target_iter, end(), end() + 1);
      }

      // Construct the new element at the target position
      new (target_iter) T(pValue);
      length++;

      return target_iter;
   }

   iterator insert(const_iterator pTarget, T &&pValue) {
      size_type index = pTarget - begin();
      if (length IS capacity) {
         reserveCapacity(capacity * 2);
         pTarget = begin() + index;
      }

      iterator target_iter = const_cast<iterator>(pTarget);

      if (target_iter < end()) {
         std::move_backward(target_iter, end(), end() + 1);
      }

      new (target_iter) T(std::move(pValue));
      length++;

      return target_iter;
   }

   void insert(const_iterator pTarget, const_iterator pStart, const_iterator pEnd) {
      difference_type count = std::distance(pStart, pEnd);
      if (count <= 0) {
         return;
      }

      size_type index = pTarget - begin();

      if (length + count > capacity) {
         size_type new_capacity = capacity;
         while (new_capacity < length + count) {
            new_capacity = new_capacity * 2;
         }
         reserveCapacity(new_capacity);
         pTarget = begin() + index;
      }

      iterator target_iter = const_cast<iterator>(pTarget);

      // Shift existing elements to make space
      if (target_iter < end()) {
         std::move_backward(target_iter, end(), end() + count);
         // Destroy moved-from objects in the insertion range
         for (iterator it = target_iter; it != target_iter + count; ++it) {
            it->~T();
         }
      }

      // Copy new elements into the created space
      std::uninitialized_copy(pStart, pEnd, target_iter);
      length += count;
   }

   // Comparison

   bool operator!=(vector const &rhs) const {return !(*this == rhs);}

   bool operator==(vector const &rhs) const {
      return (size() IS rhs.size()) and std::equal(begin(), end(), rhs.begin());
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
      return *new (elements + length++) T(std::forward<Args>(args)...);
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
      if (length IS capacity) {
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

   template<typename X> requires (!std::is_nothrow_move_constructible_v<X>)
   void simpleCopy(vector<T>& dst) {
      std::for_each(elements, elements + length, [&dst](T const &v) {
         dst.pushBackInternal(v);
      });
   }

   template<typename X> requires std::is_nothrow_move_constructible_v<X>
   void simpleCopy(vector<T>& dst) {
      std::for_each(elements, elements + length, [&dst](T &v){
         dst.moveBackInternal(std::move(v));
      });
   }

   template<typename X> requires (!std::is_trivially_destructible_v<X>)
   void clearElements() {
      for (size_type i = 0; i < length; ++i) {
         elements[length - 1 - i].~T();
      }
   }

   // Trivially destructible objects can be reused without using the destructor.

   template<typename X> requires std::is_trivially_destructible_v<X>
   void clearElements() {
   }

   template<typename X> requires (std::is_nothrow_copy_constructible_v<X> and std::is_nothrow_destructible_v<X>)
   void copyAssign(vector<X> &copy) {
      // This function is only used if there is no chance of an exception being
      // thrown during destruction or copy construction of the type T.

      if (this IS &copy) return;

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

   template<typename X> requires (!(std::is_nothrow_copy_constructible_v<X> and std::is_nothrow_destructible_v<X>))
   void copyAssign(vector<X> &copy) {
      vector<T> tmp(copy); // Copy and Swap idiom
      tmp.swap(*this);
   }
};

} // namespace

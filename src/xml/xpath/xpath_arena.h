//********************************************************************************************************************
// XPath Evaluation Arena
//
// Provides reusable storage for transient XPath data structures to reduce
// allocation pressure during evaluation. The arena supplies pooled
// XPathValue instances as well as generic vector buffers that can be
// recycled across predicate and step processing.
//********************************************************************************************************************

#pragma once

#include "xpath_functions.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

struct XMLAttrib;
struct XMLTag;

// Lightweight representation of an axis match entry shared across the
// evaluator and arena helpers.
struct XPathAxisMatch {
   XMLTag * node = nullptr;
   const XMLAttrib * attribute = nullptr;
};

class XPathArena {
   private:
   struct PoolBase {
      virtual ~PoolBase() = default;
      virtual void reset() = 0;
   };

   template<typename T>
   struct TypedPool final : PoolBase {
      std::vector<std::vector<T>> buffers;
      size_t next = 0;

      void reset() override { next = 0; }

      std::vector<T> &acquire(size_t capacity) {
         if (next >= buffers.size()) {
            buffers.emplace_back();
         }

         auto &buffer = buffers[next++];
         if (buffer.capacity() < capacity) buffer.reserve(capacity);
         buffer.clear();
         return buffer;
      }
   };

   template<typename T>
   static size_t &pool_slot()
   {
      static size_t slot = std::numeric_limits<size_t>::max();
      return slot;
   }

   template<typename T>
   TypedPool<T> &ensure_pool() {
      auto &slot = pool_slot<T>();

      if (slot == std::numeric_limits<size_t>::max()) {
         slot = vector_pools.size();
         vector_pools.emplace_back();
      }

      if (slot >= vector_pools.size()) vector_pools.resize(slot + 1);

      auto &entry = vector_pools[slot];
      if (!entry) entry = std::make_unique<TypedPool<T>>();

      return *static_cast<TypedPool<T> *>(entry.get());
   }

   std::vector<std::unique_ptr<PoolBase>> vector_pools;
   std::vector<XPathValue> value_pool;
   size_t value_pool_index = 0;

   public:
   XPathArena() = default;
   ~XPathArena() = default;

   void reset();

   XPathValue &acquire_value();

   template<typename T>
   std::vector<T> &acquire_vector(size_t capacity = 0) {
      auto &pool = ensure_pool<T>();
      return pool.acquire(capacity);
   }
};

inline void XPathArena::reset()
{
   for (auto &entry : vector_pools) {
      if (entry) entry->reset();
   }

   value_pool_index = 0;
}

inline XPathValue &XPathArena::acquire_value()
{
   if (value_pool_index >= value_pool.size()) {
      value_pool.emplace_back();
   }

   auto &value = value_pool[value_pool_index++];
   value.reset();
   return value;
}


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
#include <memory>
#include <typeindex>
#include <unordered_map>
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
   TypedPool<T> &ensure_pool() {
      auto key = std::type_index(typeid(T));
      auto it = vector_pools.find(key);
      if (it IS vector_pools.end()) {
         auto new_pool = std::make_unique<TypedPool<T>>();
         auto *pool_ptr = new_pool.get();
         vector_pools.emplace(key, std::move(new_pool));
         return *pool_ptr;
      }

      return *static_cast<TypedPool<T> *>(it->second.get());
   }

   std::unordered_map<std::type_index, std::unique_ptr<PoolBase>> vector_pools;
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
      if (entry.second) entry.second->reset();
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


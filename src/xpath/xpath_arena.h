// Provides pooled allocation helpers for XPath evaluation to minimise
// temporary allocations when constructing node, attribute, and string vectors.
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <parasol/strings.hpp>
#include "../xml/xml.h"

class XPathArena {
   private:
   template<typename T>
   struct VectorPool {
      // Supplies cleared std::vector instances for XPath evaluation and reuses
      // them to avoid repeated heap allocations during query processing.
      std::vector<std::unique_ptr<std::vector<T>>> storage;
      std::vector<std::vector<T> *> free_list;

      // Fetches an available vector from the free list, or allocates a new
      // one, ensuring the container is reset before handing it to the caller.
      std::vector<T> & acquire() {
         if (!free_list.empty()) {
            auto *vector = free_list.back();
            free_list.pop_back();
            vector->clear();
            return *vector;
         }

         storage.push_back(std::make_unique<std::vector<T>>());
         auto &vector = *storage.back();
         vector.clear();
         return vector;
      }

      void release(std::vector<T> &vector) {
         vector.clear();
         free_list.push_back(&vector);
      }

      // Clears the pool state while retaining allocated storage, preparing the
      // vectors for reuse without incurring new allocations.
      void reset() {
         free_list.clear();
         for (auto &entry : storage) {
            entry->clear();
            free_list.push_back(entry.get());
         }
      }
   };

   // Specialized pool for XMLTag * that uses pf::vector
   struct NodeVectorPool {
      std::vector<std::unique_ptr<NODES>> storage;
      std::vector<NODES *> free_list;

      NODES & acquire() {
         if (!free_list.empty()) {
            auto *vector = free_list.back();
            free_list.pop_back();
            vector->clear();
            return *vector;
         }

         storage.push_back(std::make_unique<NODES>());
         auto &vector = *storage.back();
         vector.clear();
         return vector;
      }

      void release(NODES &vector) {
         vector.clear();
         free_list.push_back(&vector);
      }

      void reset() {
         free_list.clear();
         for (auto &entry : storage) {
            entry->clear();
            free_list.push_back(entry.get());
         }
      }
   };

   NodeVectorPool node_vectors;
   VectorPool<const XMLAttrib *> attribute_vectors;
   VectorPool<std::string> string_vectors;

   public:
   XPathArena() = default;
   XPathArena(const XPathArena &) = delete;
   XPathArena & operator=(const XPathArena &) = delete;

   NODES & acquire_node_vector() { return node_vectors.acquire(); }
   void release_node_vector(NODES &Vector) { node_vectors.release(Vector); }

   std::vector<const XMLAttrib *> & acquire_attribute_vector() { return attribute_vectors.acquire(); }
   void release_attribute_vector(std::vector<const XMLAttrib *> &Vector) { attribute_vectors.release(Vector); }

   std::vector<std::string> & acquire_string_vector() { return string_vectors.acquire(); }
   void release_string_vector(std::vector<std::string> &Vector) { string_vectors.release(Vector); }

   void reset() {
      node_vectors.reset();
      attribute_vectors.reset();
      string_vectors.reset();
   }
};

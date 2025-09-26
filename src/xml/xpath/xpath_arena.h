#pragma once

#include <memory>
#include <string>
#include <vector>

struct XMLTag;
struct XMLAttrib;

class XPathArena {
   private:
   template<typename T>
   struct VectorPool {
      std::vector<std::unique_ptr<std::vector<T>>> storage;
      std::vector<std::vector<T> *> free_list;

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

      void reset() {
         free_list.clear();
         for (auto &entry : storage) {
            entry->clear();
            free_list.push_back(entry.get());
         }
      }
   };

   VectorPool<XMLTag *> node_vectors;
   VectorPool<const XMLAttrib *> attribute_vectors;
   VectorPool<std::string> string_vectors;

   public:
   XPathArena() = default;
   XPathArena(const XPathArena &) = delete;
   XPathArena & operator=(const XPathArena &) = delete;

   std::vector<XMLTag *> & acquire_node_vector() { return node_vectors.acquire(); }
   void release_node_vector(std::vector<XMLTag *> &Vector) { node_vectors.release(Vector); }

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

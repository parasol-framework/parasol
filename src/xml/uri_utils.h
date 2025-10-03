#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace xml::uri
{
   inline std::string normalise_uri_separators(std::string Value)
   {
      std::replace(Value.begin(), Value.end(), '\\', '/');
      return Value;
   }

   inline bool is_absolute_uri(std::string_view Uri)
   {
      size_t index = 0u;
      while (index < Uri.length()) {
         char ch = Uri[index];
         if (ch IS ':') return index > 0u;
         if ((ch IS '/') or (ch IS '?') or (ch IS '#')) break;
         ++index;
      }
      return false;
   }

   inline std::string strip_query_fragment(std::string_view Uri)
   {
      size_t pos = Uri.find_first_of("?#");
      if (pos != std::string::npos) return std::string(Uri.substr(0u, pos));
      return std::string(Uri);
   }

   inline std::string normalise_path_segments(std::string_view Path)
   {
      std::string path(Path);
      std::vector<std::string> segments;
      bool leading_slash = (!path.empty()) and (path.front() IS '/');
      size_t start = leading_slash ? 1u : 0u;

      while (start <= path.length()) {
         size_t end = path.find('/', start);
         std::string segment;
         if (end IS std::string::npos) {
            segment = path.substr(start);
            start = path.length() + 1u;
         }
         else {
            segment = path.substr(start, end - start);
            start = end + 1u;
         }

         if (segment.empty() or (segment IS ".")) continue;
         if (segment IS "..") {
            if (!segments.empty()) segments.pop_back();
            continue;
         }

         segments.push_back(segment);
      }

      std::string result;
      if (leading_slash) result.push_back('/');

      for (size_t index = 0u; index < segments.size(); ++index) {
         if (index > 0u) result.push_back('/');
         result.append(segments[index]);
      }

      if ((!path.empty()) and (path.back() IS '/') and (!result.empty()) and (result.back() != '/')) {
         result.push_back('/');
      }

      return result;
   }

   inline std::string resolve_relative_uri(std::string_view Relative, std::string_view Base)
   {
      if (Relative.empty()) return std::string(Base);
      if (is_absolute_uri(Relative)) return std::string(Relative);

      std::string base_clean = strip_query_fragment(Base);
      if (base_clean.empty()) return std::string();

      std::string prefix;
      std::string path = base_clean;

      size_t scheme_pos = base_clean.find(':');
      if (scheme_pos != std::string::npos) {
         prefix = base_clean.substr(0u, scheme_pos + 1u);
         path = base_clean.substr(scheme_pos + 1u);

         if (path.rfind("//", 0u) IS 0u) {
            size_t authority_end = path.find('/', 2u);
            if (authority_end IS std::string::npos) {
               std::string combined = prefix + path;
               if ((!Relative.empty()) and (Relative.front() != '/')) combined.push_back('/');
               combined.append(std::string(Relative));
               return combined;
            }

            prefix.append(path.substr(0u, authority_end));
            path = path.substr(authority_end);
         }
      }

      std::string directory = path;
      size_t last_slash = directory.rfind('/');
      if (last_slash != std::string::npos) directory = directory.substr(0u, last_slash + 1u);
      else directory.clear();

      std::string relative_path = std::string(Relative);
      if ((!relative_path.empty()) and (relative_path.front() IS '/')) {
         std::string combined_path = normalise_path_segments(relative_path);
         return prefix + combined_path;
      }

      std::string combined_path = directory;
      combined_path.append(relative_path);
      combined_path = normalise_path_segments(combined_path);

      return prefix + combined_path;
   }
}


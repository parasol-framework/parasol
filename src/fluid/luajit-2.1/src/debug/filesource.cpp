// FileSource tracking for accurate error reporting in imported files.
// Copyright (C) 2025 Paul Manias

#include "filesource.h"
#include <parasol/main.h>

//********************************************************************************************************************
// Register a new file source in the lua_State.
// Returns the file index, or FILESOURCE_OVERFLOW_INDEX (255) if the limit is exceeded.

uint8_t register_file_source(lua_State *L, const std::string &Path, const std::string &Filename,
                             BCLine FirstLine, BCLine SourceLines,
                             uint8_t ParentIndex, BCLine ImportLine)
{
   pf::Log log(__FUNCTION__);

   // Check if we've hit the file limit
   if (L->file_sources.size() >= FILESOURCE_MAX_INDEX) {
      // Initialise the overflow entry if not already done
      if (L->file_sources.size() IS FILESOURCE_MAX_INDEX) {
         log.warning("FileSource limit exceeded (%d files). Additional imports will show as 'unknown'.", FILESOURCE_MAX_INDEX);

         FileSource overflow;
         overflow.path = "unknown";
         overflow.filename = "unknown";
         overflow.declared_namespace = "";
         overflow.first_line = 0;
         overflow.source_lines = 0;
         overflow.path_hash = 0;
         overflow.parent_file_index = 0;
         overflow.import_line = 0;

         L->file_sources.push_back(std::move(overflow));
         // Don't add to file_index_map - overflow is not looked up by hash
      }
      return FILESOURCE_OVERFLOW_INDEX;
   }

   // Calculate path hash for deduplication lookup
   uint32_t path_hash = pf::strihash(Path);

   // Check if this file is already registered
   auto it = L->file_index_map.find(path_hash);
   if (it != L->file_index_map.end()) {
      log.trace("File already registered: %s (index %d)", Filename.c_str(), it->second);
      return it->second;
   }

   // Register the new file
   uint8_t new_index = uint8_t(L->file_sources.size());

   FileSource source;
   source.path = Path;
   source.filename = Filename;
   source.declared_namespace = "";
   source.first_line = FirstLine;
   source.source_lines = SourceLines;
   source.path_hash = path_hash;
   source.parent_file_index = ParentIndex;
   source.import_line = ImportLine;

   L->file_sources.push_back(std::move(source));
   L->file_index_map[path_hash] = new_index;

   log.trace("Registered file source: %s (index %d, parent %d, import line %d)",
             Filename.c_str(), new_index, ParentIndex, ImportLine);

   return new_index;
}

//********************************************************************************************************************
// Find a file source by path hash.

std::optional<uint8_t> find_file_source(lua_State *L, uint32_t PathHash)
{
   auto it = L->file_index_map.find(PathHash);
   if (it != L->file_index_map.end()) {
      return it->second;
   }
   return std::nullopt;
}

//********************************************************************************************************************
// Get a file source by index.

const FileSource* get_file_source(lua_State *L, uint8_t Index)
{
   if (Index < L->file_sources.size()) {
      return &L->file_sources[Index];
   }
   return nullptr;
}

//********************************************************************************************************************
// Initialise file_sources with the main file entry (index 0).

void init_main_file_source(lua_State *L, const std::string &Path, const std::string &Filename, BCLine SourceLines)
{
   // Clear any existing file sources
   L->file_sources.clear();
   L->file_index_map.clear();

   // Register the main file at index 0
   register_file_source(L, Path, Filename, 1, SourceLines, 0, 0);
}

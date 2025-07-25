// A Parasol friendly version of the Lua's 'io' library.  Provided mostly for compatibility purposes, but also makes
// it easier to access the std* file handles.  


#define PRV_SCRIPT
#define PRV_FLUID
#define PRV_FLUID_MODULE
#include <parasol/main.h>
#include <parasol/modules/fluid.h>
#include <parasol/strings.hpp>
#include <inttypes.h>

extern "C" {
 #include "lauxlib.h"
 #include "lj_obj.h"
}

#include "hashes.h"
#include "defs.h"

enum {
   CONST_STDIN = -1,
   CONST_STDOUT = -2,
   CONST_STDERR = -3
};

// File handle userdata structure

struct FileHandle {
   objFile *file;
   bool auto_close;
   
   FileHandle(objFile *f, bool ac = true) : file(f), auto_close(ac) {}
};

// Helper functions

inline FileHandle * check_file_handle(lua_State *Lua, int Index)
{
   return (FileHandle *)luaL_checkudata(Lua, Index, "Fluid.file");
}

inline int push_file_handle(lua_State *Lua, objFile *File, bool AutoClose = true)
{
   auto handle = (FileHandle *)lua_newuserdata(Lua, sizeof(FileHandle));
   new(handle) FileHandle(File, AutoClose);
   luaL_getmetatable(Lua, "Fluid.file");
   lua_setmetatable(Lua, -2);
   return 1;
}

inline int file_gc(lua_State *Lua)
{
   if (auto handle = check_file_handle(Lua, 1)) {
      if (handle->auto_close and handle->file) {
         FreeResource(handle->file);
      }
      handle->~FileHandle();
   }
   return 0;
}

//********************************************************************************************************************
// Forward declarations

static int io_input(lua_State *);
static int io_output(lua_State *);
static int file_read(lua_State *);
static int file_write(lua_State *);
static int file_flush(lua_State *);

//********************************************************************************************************************
// Lua IO functions

static int io_open(lua_State *Lua)
{
   auto path = luaL_checkstring(Lua, 1);
   auto mode = luaL_optstring(Lua, 2, "r");
   
   auto flags = FL::NIL;
   
   for (int i = 0; mode[i]; i++) {
      switch (mode[i]) {
         case 'r': flags |= FL::READ; break;
         case 'w': flags |= FL::WRITE | FL::NEW; break;
         case 'a': flags |= FL::WRITE; break;  // Append mode - will seek to end after open
         case '+': flags |= FL::READ | FL::WRITE; break;
         case 'b': break; // Binary mode - ignored as all files are binary in Parasol
      }
   }
   
   if (auto file = objFile::create::local({ fl::Path(path), fl::Flags(flags) })) {
      if (strchr(mode, 'a')) file->seekEnd(0);
       
      push_file_handle(Lua, file);
      return 1;
   }
   else {
      lua_pushnil(Lua);
      lua_pushstring(Lua, "Failed to open file.");
      return 2;
   }
}

//********************************************************************************************************************

static int io_close(lua_State *Lua)
{
   if (lua_gettop(Lua) IS 0) {
      // TODO: Close default output file with FreeResource() and remove it from the registry
      return 0;
   }
   
   if (auto handle = check_file_handle(Lua, 1)) {
      if (handle->file) {
         FreeResource(handle->file);
         handle->file = nullptr;
      }
   
      lua_pushboolean(Lua, 1);
      return 1;
   }

   return 0;
}

//********************************************************************************************************************

static int io_read(lua_State *Lua)
{
   // Get default input file
   lua_pushstring(Lua, "io.defaultInput");
   lua_gettable(Lua, LUA_REGISTRYINDEX);
   
   if (lua_isnil(Lua, -1)) {
      // Initialize default input by calling io.input()
      lua_pop(Lua, 1);  // Remove nil
      lua_pushcfunction(Lua, io_input);
      lua_call(Lua, 0, 1);  // Call io.input() with no args
   }
   
   if (lua_isnil(Lua, -1)) {
      lua_pushnil(Lua);
      lua_pushstring(Lua, "No default input file");
      return 2;
   }
   
   // Insert the file handle as first argument and call file:read
   lua_insert(Lua, 1);
   return file_read(Lua);
}

//********************************************************************************************************************

static int io_write(lua_State *Lua)
{
   // Get default output file
   lua_pushstring(Lua, "io.defaultOutput");
   lua_gettable(Lua, LUA_REGISTRYINDEX);
   
   if (lua_isnil(Lua, -1)) {
      // Initialize default output by calling io.output()
      lua_pop(Lua, 1);  // Remove nil
      lua_pushcfunction(Lua, io_output);
      lua_call(Lua, 0, 1);  // Call io.output() with no args
   }
   
   if (lua_isnil(Lua, -1)) {
      lua_pushnil(Lua);
      lua_pushstring(Lua, "No default output file");
      return 2;
   }
   
   // Insert the file handle as first argument and call file:write
   lua_insert(Lua, 1);
   return file_write(Lua);
}

//********************************************************************************************************************

static int io_flush(lua_State *Lua)
{
   // Get default output file
   lua_pushstring(Lua, "io.defaultOutput");
   lua_gettable(Lua, LUA_REGISTRYINDEX);
   
   if (lua_isnil(Lua, -1)) {
      // Initialize default output by calling io.output()
      lua_pop(Lua, 1);  // Remove nil
      lua_pushcfunction(Lua, io_output);
      lua_call(Lua, 0, 1);  // Call io.output() with no args
   }
   
   if (lua_isnil(Lua, -1)) {
      lua_pushboolean(Lua, 0);  // Failed
      return 1;
   }
   
   // Call file:flush on the default output
   return file_flush(Lua);
}

//********************************************************************************************************************

static int io_input(lua_State *Lua)
{
   if (lua_gettop(Lua) IS 0) {
      // Return current default input
      lua_pushstring(Lua, "io.defaultInput");
      lua_gettable(Lua, LUA_REGISTRYINDEX);

      if (lua_isnil(Lua, -1)) { // No default set, try to open stdin
         lua_pop(Lua, 1);  // Remove nil
         
         auto file = objFile::create::local({ fl::Path("std:in"), fl::Flags(FL::READ) });

         if (file) {
            push_file_handle(Lua, file, false);  // Don't auto-close stdin
               
            // Store as default
            lua_pushstring(Lua, "io.defaultInput");
            lua_pushvalue(Lua, -2);  // Copy the file handle
            lua_settable(Lua, LUA_REGISTRYINDEX);
         }
         else lua_pushnil(Lua);
      }
      return 1;
   }
   
   // Set new default input
   if ((lua_type(Lua, 1) IS LUA_TSTRING) or (lua_type(Lua, 1) IS LUA_TNUMBER)) {
      std::string path;
      if (lua_type(Lua, 1) IS LUA_TNUMBER) {
         switch(lua_tointeger(Lua, 1)) {
            case CONST_STDIN: path = "std:in"; break;
            default:
               lua_pushnil(Lua);
               lua_pushstring(Lua, "Invalid file descriptor");
               return 2;
         }
      }
      else path = lua_tostring(Lua, 1);

      auto file = objFile::create::local({ fl::Path(path), fl::Flags(FL::READ) });

      if (file) {
         push_file_handle(Lua, file);
            
         // Store as default
         lua_pushstring(Lua, "io.defaultInput");
         lua_pushvalue(Lua, -2);  // Copy the file handle
         lua_settable(Lua, LUA_REGISTRYINDEX);
            
         return 1;
      }
      else {
         lua_pushnil(Lua);
         lua_pushstring(Lua, "Cannot open file for reading");
         return 2;
      }
   }
   else if (check_file_handle(Lua, 1)) { // Use provided file handle as the new default
      lua_pushstring(Lua, "io.defaultInput");
      lua_pushvalue(Lua, 1);  // Copy the file handle
      lua_settable(Lua, LUA_REGISTRYINDEX);     
      lua_pushvalue(Lua, 1);  // Return the file handle
      return 1;
   }
   else {
      lua_pushnil(Lua);
      lua_pushstring(Lua, "Invalid argument, expected string or file handle");
      return 2;
   }
}

//********************************************************************************************************************

static int io_output(lua_State *Lua)
{
   if (lua_gettop(Lua) IS 0) {
      // Return current default output
      lua_pushstring(Lua, "io.defaultOutput");
      lua_gettable(Lua, LUA_REGISTRYINDEX);
      if (lua_isnil(Lua, -1)) {
         // No default set, try to open stdout
         lua_pop(Lua, 1);  // Remove nil
         
         if (auto file = objFile::create::local({ fl::Path("std:out"), fl::Flags(FL::WRITE) })) {
            push_file_handle(Lua, file, false);  // Don't auto-close stdout
               
            // Store as default
            lua_pushstring(Lua, "io.defaultOutput");
            lua_pushvalue(Lua, -2);  // Copy the file handle
            lua_settable(Lua, LUA_REGISTRYINDEX);
         }
         else lua_pushnil(Lua);
      }
      return 1;
   }
   
   // Set new default output

   if (lua_type(Lua, 1) IS LUA_TSTRING) { // Open file for writing
      auto path = lua_tostring(Lua, 1);

      if (auto file = objFile::create::local({ fl::Path(path), fl::Flags(FL::NEW|FL::WRITE) })) {
         push_file_handle(Lua, file);
            
         // Store as default
         lua_pushstring(Lua, "io.defaultOutput");
         lua_pushvalue(Lua, -2);  // Copy the file handle
         lua_settable(Lua, LUA_REGISTRYINDEX);
            
         return 1;
      }
      else {
         lua_pushnil(Lua);
         lua_pushstring(Lua, "Cannot open file for writing");
         return 2;
      }
   }
   else if (check_file_handle(Lua, 1)) { // Use provided file handle      
      // Store as default
      lua_pushstring(Lua, "io.defaultOutput");
      lua_pushvalue(Lua, 1);  // Copy the file handle
      lua_settable(Lua, LUA_REGISTRYINDEX);
      lua_pushvalue(Lua, 1);  // Return the file handle
      return 1;
   }
   else {
      lua_pushnil(Lua);
      lua_pushstring(Lua, "Invalid argument, expected string or file handle");
      return 2;
   }
}

//********************************************************************************************************************

// Iterator state for io.lines
struct LinesIterator {
   FileHandle *file_handle;
   bool close_on_finish;
   
   LinesIterator(FileHandle *fh, bool close) : file_handle(fh), close_on_finish(close) {}
};

static int lines_iterator(lua_State *Lua)
{
   auto iter = (LinesIterator *)lua_touserdata(Lua, lua_upvalueindex(1));
   
   if (!iter->file_handle or !iter->file_handle->file) {
      return 0; // End iteration
   }
   
   struct fl::ReadLine args;
   if (Action(fl::ReadLine::id, iter->file_handle->file, &args) IS ERR::Okay) {
      lua_pushstring(Lua, args.Result);
      return 1;
   }
   else {
      // End of file or error - close if needed
      if (iter->close_on_finish and iter->file_handle->file) {
         FreeResource(iter->file_handle->file);
         iter->file_handle->file = nullptr;
      }
      return 0; // End iteration
   }
}

static int lines_iterator_gc(lua_State *Lua)
{
   auto iter = (LinesIterator *)lua_touserdata(Lua, 1);
   if (iter->close_on_finish and iter->file_handle and iter->file_handle->file) {
      FreeResource(iter->file_handle->file);
      iter->file_handle->file = nullptr;
   }
   iter->~LinesIterator();
   return 0;
}

static int io_lines(lua_State *Lua)
{
   FileHandle *file_handle = nullptr;
   bool close_on_finish = false;
   
   if (lua_gettop(Lua) IS 0) {
      // No arguments - use default input
      lua_pushstring(Lua, "io.defaultInput");
      lua_gettable(Lua, LUA_REGISTRYINDEX);
      
      if (lua_isnil(Lua, -1)) {
         // Initialize default input
         lua_pop(Lua, 1);
         lua_pushcfunction(Lua, io_input);
         lua_call(Lua, 0, 1);
      }
      
      if (lua_isnil(Lua, -1)) {
         luaL_error(Lua, "No default input file available");
         return 0;
      }
      
      file_handle = check_file_handle(Lua, -1);
      close_on_finish = false; // Don't close default input
   }
   else if (lua_type(Lua, 1) IS LUA_TSTRING) { // Filename provided - open file
      auto path = lua_tostring(Lua, 1);

      if (auto file = objFile::create::local({ fl::Path(path), fl::Flags(FL::READ) })) {
         push_file_handle(Lua, file);
         file_handle = check_file_handle(Lua, -1);
         close_on_finish = true; // Close when iteration ends
      }
      else {
         luaL_error(Lua, "Cannot open file: %s", path);
         return 0;
      }
   }
   else {
      // File handle provided
      file_handle = check_file_handle(Lua, 1);
      close_on_finish = false; // Don't close provided handle
   }
   
   // Create iterator state
   auto iter = (LinesIterator *)lua_newuserdata(Lua, sizeof(LinesIterator));
   new(iter) LinesIterator(file_handle, close_on_finish);
   
   // Set up GC metamethod for iterator state
   lua_newtable(Lua);
   lua_pushcfunction(Lua, lines_iterator_gc);
   lua_setfield(Lua, -2, "__gc");
   lua_setmetatable(Lua, -2);
   
   // Return the iterator function with the state as upvalue
   lua_pushcclosure(Lua, lines_iterator, 1);
   return 1;
}

//********************************************************************************************************************
// TODO: Open pipe to process - requires Task integration and using callbacks to receive data from stdout.

static int io_popen(lua_State *Lua)
{
   luaL_error(Lua, "io.popen not yet implemented");
   return 0;
}

//********************************************************************************************************************
// Create a temporary buffer file in memory.  In theory this is the best and most performant option if you 
// also consider that the OS can use swap space for large memory files.

static int io_tmpfile(lua_State *Lua)
{
   if (auto file = objFile::create::local({ fl::Size(4096), fl::Flags(FL::BUFFER|FL::READ|FL::WRITE) })) {
      push_file_handle(Lua, file);
      return 1;
   }
   else {
      lua_pushnil(Lua);
      return 1;
   }
}

//********************************************************************************************************************

static int io_type(lua_State *Lua)
{
   if (lua_type(Lua, 1) IS LUA_TUSERDATA) {
      auto handle = (FileHandle *)luaL_testudata(Lua, 1, "Fluid.file");
      if (handle) {
         if (handle->file) lua_pushstring(Lua, "file");
         else lua_pushstring(Lua, "closed file");
         return 1;
      }
   }
   
   lua_pushnil(Lua);
   return 1;
}

//********************************************************************************************************************
// File handle methods

static int file_read(lua_State *Lua)
{
   if (auto handle = check_file_handle(Lua, 1)) {
      if (!handle->file) {
         lua_pushnil(Lua);
         lua_pushstring(Lua, "Attempted to use a closed file");
         return 2;
      }

      int nargs = lua_gettop(Lua);
      
      // Default to reading a line if no arguments
      if (nargs IS 1) {
         struct fl::ReadLine args;
         if (Action(fl::ReadLine::id, handle->file, &args) IS ERR::Okay) {
            lua_pushstring(Lua, args.Result);
            return 1;
         }
         else {
            lua_pushnil(Lua);
            return 1;
         }
      }
      
      // Process read format arguments
      for (int i = 2; i <= nargs; i++) {
         if (lua_type(Lua, i) IS LUA_TSTRING) {
            auto format = lua_tostring(Lua, i);
            
            if (format[0] IS '*') {
               switch (format[1]) {
                  case 'n': { // Read a number
                     struct fl::ReadLine args;
                     if (Action(fl::ReadLine::id, handle->file, &args) IS ERR::Okay) {
                        lua_pushnumber(Lua, std::strtod(args.Result, nullptr));
                     }
                     else lua_pushnil(Lua);
                     break;
                  }
                  
                  case 'a': { // Read entire file
                     auto current_pos = handle->file->Position;
                     handle->file->seekEnd(0);
                     auto file_size = handle->file->Position;
                     handle->file->seek(current_pos, SEEK::START);
                     
                     auto remaining = file_size - current_pos;
                     if (remaining > 0) {
                        std::string buffer(remaining, '\0');
                        int bytes_read;
                        if (acRead(handle->file, buffer.data(), remaining, &bytes_read) IS ERR::Okay) {
                           buffer.resize(bytes_read);
                           lua_pushlstring(Lua, buffer.data(), bytes_read);
                        }
                        else lua_pushnil(Lua);
                     }
                     else lua_pushstring(Lua, "");
                     break;
                  }
                  
                  case 'l': { // Read a line (default behavior)
                     struct fl::ReadLine args;
                     if (Action(fl::ReadLine::id, handle->file, &args) IS ERR::Okay) {
                        lua_pushstring(Lua, args.Result);
                     }
                     else lua_pushnil(Lua);
                     break;
                  }
                  
                  default:
                     lua_pushnil(Lua);
                     break;
               }
            }
            else {
               lua_pushnil(Lua);
            }
         }
         else if (lua_type(Lua, i) IS LUA_TNUMBER) {
            // Read specified number of bytes
            auto bytes_to_read = lua_tointeger(Lua, i);
            if (bytes_to_read > 0) {
               std::string buffer(bytes_to_read, '\0');
               int bytes_read;
               if (acRead(handle->file, buffer.data(), bytes_to_read, &bytes_read) IS ERR::Okay and bytes_read > 0) {
                  buffer.resize(bytes_read);
                  lua_pushlstring(Lua, buffer.data(), bytes_read);
               }
               else lua_pushnil(Lua);
            }
            else {
               lua_pushstring(Lua, "");
            }
         }
         else {
            lua_pushnil(Lua);
         }
      }
      
      return nargs - 1; // Return number of results (excluding file handle)
   }
   
   return 0;
}

//********************************************************************************************************************

static int file_write(lua_State *Lua)
{
   auto handle = check_file_handle(Lua, 1);
   if (!handle->file) {
      lua_pushnil(Lua);
      lua_pushstring(Lua, "Attempted to use a closed file");
      return 2;
   }
   
   int nargs = lua_gettop(Lua);
   for (int i = 2; i <= nargs; i++) {
      size_t len;
      auto str = luaL_checklstring(Lua, i, &len);
      
      int result;
      if (acWrite(handle->file, str, len, &result) != ERR::Okay) {
         lua_pushnil(Lua);
         lua_pushstring(Lua, "Write failed");
         return 2;
      }
   }
   
   lua_pushvalue(Lua, 1); // Return file handle
   return 1;
}

//********************************************************************************************************************

static int file_close(lua_State *Lua)
{
   return io_close(Lua);
}

//********************************************************************************************************************

static int file_flush(lua_State *Lua)
{
   if (auto handle = check_file_handle(Lua, 1)) {
      if (!handle->file) {
         lua_pushnil(Lua);
         lua_pushstring(Lua, "Attempted to use a closed file");
         return 2;
      }

      acFlush(handle->file);

      lua_pushboolean(Lua, 1);
      return 1;
   }
   else { // Should never happen
      lua_pushnil(Lua);
      return 1;
   }
}

//********************************************************************************************************************

static int file_seek(lua_State *Lua)
{
   auto handle = check_file_handle(Lua, 1);
   if (!handle->file) {
      lua_pushnil(Lua);
      lua_pushstring(Lua, "Attempted to use a closed file");
      return 2;
   }
   
   auto whence_str = luaL_optstring(Lua, 2, "cur");
   auto offset = luaL_optnumber(Lua, 3, 0);
   
   SEEK whence = SEEK::CURRENT;
   if (iequals("set", whence_str)) whence = SEEK::START;
   else if (iequals("cur", whence_str)) whence = SEEK::CURRENT;
   else if (iequals("end", whence_str)) whence = SEEK::END;
   
   if (acSeek(handle->file, offset, whence) IS ERR::Okay) {
      lua_pushnumber(Lua, handle->file->Position);
      return 1;
   }
   else {
      lua_pushnil(Lua);
      lua_pushstring(Lua, "Seek failed");
      return 2;
   }
}

//********************************************************************************************************************

static int file_lines(lua_State *Lua)
{
   auto handle = check_file_handle(Lua, 1);
   if (!handle->file) {
      luaL_error(Lua, "Attempted to use a closed file");
      return 0;
   }
   
   // Create iterator state - don't close the file when iteration ends since it's a file method
   auto iter = (LinesIterator *)lua_newuserdata(Lua, sizeof(LinesIterator));
   new(iter) LinesIterator(handle, false);
   
   // Set up GC metamethod for iterator state
   lua_newtable(Lua);
   lua_pushcfunction(Lua, lines_iterator_gc);
   lua_setfield(Lua, -2, "__gc");
   lua_setmetatable(Lua, -2);
   
   // Return the iterator function with the state as upvalue
   lua_pushcclosure(Lua, lines_iterator, 1);
   return 1;
}

//********************************************************************************************************************

void register_io_class(lua_State *Lua)
{
   static const struct luaL_Reg iolib_functions[] = {
      { "close",       io_close },   
      { "flush",       io_flush },   
      { "input",       io_input },   
      { "lines",       io_lines },   
      { "open",        io_open },    
      { "output",      io_output },  
      { "popen",       io_popen },   
      { "read",        io_read },    
      { "tmpfile",     io_tmpfile }, 
      { "type",        io_type },    
      { "write",       io_write },   
      { nullptr, nullptr }
   };

   static const struct luaL_Reg file_methods[] = {
      { "read",        file_read },
      { "write",       file_write },
      { "close",       file_close },
      { "flush",       file_flush },
      { "seek",        file_seek },
      { "lines",       file_lines },
      { "__gc",        file_gc },
      { nullptr, nullptr }
   };

   pf::Log log(__FUNCTION__);
   log.trace("Registering io interface.");

   // Create file handle metatable
   luaL_newmetatable(Lua, "Fluid.file");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable
   lua_settable(Lua, -3);   // metatable.__index = metatable
   luaL_openlib(Lua, nullptr, file_methods, 0);

   // Create io metatable  
   luaL_newmetatable(Lua, "Fluid.io");
   lua_pushstring(Lua, "__index");
   lua_pushvalue(Lua, -2);  // pushes the metatable created earlier
   lua_settable(Lua, -3);   // metatable.__index = metatable

   luaL_openlib(Lua, "io", iolib_functions, 0);
   
   // Add stdin, stdout, stderr constants
   lua_pushnumber(Lua, CONST_STDIN);
   lua_setfield(Lua, -2, "stdin");
   
   lua_pushnumber(Lua, CONST_STDOUT);
   lua_setfield(Lua, -2, "stdout");
   
   lua_pushnumber(Lua, CONST_STDERR);
   lua_setfield(Lua, -2, "stderr");
}

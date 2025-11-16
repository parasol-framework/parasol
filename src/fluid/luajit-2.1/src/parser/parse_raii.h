// RAII Helper Classes for LuaJIT Parser
// Copyright (C) 2025 Paul Manias.

#pragma once

static void fscope_end(FuncState* fs);
static void bcreg_reserve(FuncState* fs, BCReg n);

// ScopeGuard: RAII wrapper for automatic scope cleanup
//
// Ensures fscope_end() is called when the guard goes out of scope,
// preventing resource leaks even in the presence of early returns.
//
// Usage:
//    FuncScope bl;
//    ScopeGuard scope_guard(fs, &bl, flags);
//    // ... parse statements ...
//    // Automatic cleanup on scope exit

class ScopeGuard {
   FuncState* fs_;

public:
   ScopeGuard(FuncState* fs, FuncScope* bl, int flags) : fs_(fs) {
      fscope_begin(fs, bl, flags);
   }

   ~ScopeGuard() {
      fscope_end(fs_);
   }

   // Prevent copying
   ScopeGuard(const ScopeGuard&) = delete;
   ScopeGuard& operator=(const ScopeGuard&) = delete;

   // Allow moving
   ScopeGuard(ScopeGuard&& other) noexcept : fs_(other.fs_) {
      other.fs_ = nullptr;
   }

   ScopeGuard& operator=(ScopeGuard&& other) noexcept {
      if (this != &other) {
         if (fs_) fscope_end(fs_);
         fs_ = other.fs_;
         other.fs_ = nullptr;
      }
      return *this;
   }
};

// RegisterGuard: RAII wrapper for automatic register restoration
//
// Saves and restores fs->freereg to ensure register state is properly
// managed across function calls and expression evaluation.
//
// Usage:
//    RegisterGuard reg_guard(fs);
//    // ... use registers ...
//    // Automatic restoration on scope exit

class RegisterGuard {
   FuncState* fs_;
   BCReg saved_freereg_;

public:
   explicit RegisterGuard(FuncState* fs)
      : fs_(fs), saved_freereg_(fs->freereg) {}

   explicit RegisterGuard(FuncState* fs, BCReg reserve_count)
      : fs_(fs), saved_freereg_(fs->freereg) {
      if (reserve_count > 0) bcreg_reserve(fs, reserve_count);
   }

   ~RegisterGuard() {
      if (fs_) fs_->freereg = saved_freereg_;
   }

   // Manually release to a specific register level
   void release_to(BCReg reg) {
      fs_->freereg = reg;
   }

   // Get saved register level
   [[nodiscard]] BCReg saved() const { return saved_freereg_; }

   // Prevent copying
   RegisterGuard(const RegisterGuard&) = delete;
   RegisterGuard& operator=(const RegisterGuard&) = delete;

   // Allow moving
   RegisterGuard(RegisterGuard&& other) noexcept
      : fs_(other.fs_), saved_freereg_(other.saved_freereg_) {
      other.fs_ = nullptr;
   }

   RegisterGuard& operator=(RegisterGuard&& other) noexcept {
      if (this != &other) {
         if (fs_) fs_->freereg = saved_freereg_;
         fs_ = other.fs_;
         saved_freereg_ = other.saved_freereg_;
         other.fs_ = nullptr;
      }
      return *this;
   }
};

// VStackGuard: RAII wrapper for automatic variable stack restoration
//
// Saves and restores ls->vtop to manage temporary variable stack entries
// like goto/label resolution markers.
//
// Usage:
//    VStackGuard vstack_guard(ls);
//    // ... manipulate vstack ...
//    // Automatic restoration on scope exit

class VStackGuard {
   LexState* ls_;
   MSize saved_vtop_;

public:
   explicit VStackGuard(LexState* ls)
      : ls_(ls), saved_vtop_(ls->vtop) {}

   ~VStackGuard() {
      if (ls_) ls_->vtop = saved_vtop_;
   }

   // Manually update saved position
   void update_saved() {
      saved_vtop_ = ls_->vtop;
   }

   // Get saved vtop
   [[nodiscard]] MSize saved() const { return saved_vtop_; }

   // Prevent copying
   VStackGuard(const VStackGuard&) = delete;
   VStackGuard& operator=(const VStackGuard&) = delete;

   // Allow moving
   VStackGuard(VStackGuard&& other) noexcept
      : ls_(other.ls_), saved_vtop_(other.saved_vtop_) {
      other.ls_ = nullptr;
   }

   VStackGuard& operator=(VStackGuard&& other) noexcept {
      if (this != &other) {
         if (ls_) ls_->vtop = saved_vtop_;
         ls_ = other.ls_;
         saved_vtop_ = other.saved_vtop_;
         other.ls_ = nullptr;
      }
      return *this;
   }
};

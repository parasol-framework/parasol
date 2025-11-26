// RAII Helper Classes for LuaJIT Parser
// Copyright (C) 2025 Paul Manias.

#pragma once

static void fscope_end(FuncState *);
static void bcreg_reserve(FuncState *, BCREG);

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
   FuncState *fs_;

public:
   ScopeGuard(FuncState *fs, FuncScope* bl, FuncScopeFlag flags) : fs_(fs) {
      fscope_begin(fs, bl, flags);
   }

   ~ScopeGuard() {
      if (fs_) fscope_end(fs_);
   }

   constexpr void disarm() noexcept {
      fs_ = nullptr;
   }

   // Prevent copying
   ScopeGuard(const ScopeGuard &) = delete;
   ScopeGuard& operator=(const ScopeGuard&) = delete;

   // Allow moving
   ScopeGuard(ScopeGuard && other) noexcept : fs_(other.fs_) {
      other.fs_ = nullptr;
   }

   ScopeGuard & operator=(ScopeGuard &&other) noexcept {
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
   FuncState *fs_;
   BCReg saved_freereg_;

public:
   explicit RegisterGuard(FuncState *fs)
      : fs_(fs), saved_freereg_(BCReg(fs->freereg)) {}

   explicit RegisterGuard(FuncState *fs, BCReg reserve_count)
      : fs_(fs), saved_freereg_(BCReg(fs->freereg)) {
      if (reserve_count.raw() > 0) bcreg_reserve(fs, reserve_count.raw());
   }

   ~RegisterGuard() {
      if (fs_) fs_->freereg = saved_freereg_.raw();
   }

   // Manually release to a specific register level

   constexpr void release_to(BCReg Reg) noexcept { fs_->freereg = Reg.raw(); }
   constexpr void adopt_saved(BCReg Reg) noexcept { saved_freereg_ = Reg; }
   constexpr void disarm() noexcept { fs_ = nullptr; }

   // Get saved register level
   [[nodiscard]] constexpr BCReg saved() const noexcept { return saved_freereg_; }

   // Prevent copying
   RegisterGuard(const RegisterGuard &) = delete;
   RegisterGuard & operator=(const RegisterGuard &) = delete;

   // Allow moving
   RegisterGuard(RegisterGuard &&other) noexcept
      : fs_(other.fs_), saved_freereg_(other.saved_freereg_) {
      other.fs_ = nullptr;
   }

   RegisterGuard & operator=(RegisterGuard &&other) noexcept {
      if (this != &other) {
         if (fs_) fs_->freereg = saved_freereg_.raw();
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
   constexpr void update_saved() noexcept {
      this->saved_vtop_ = ls_->vtop;
   }

   // Get saved vtop
   [[nodiscard]] constexpr MSize saved() const noexcept { return saved_vtop_; }

   // Prevent copying
   VStackGuard(const VStackGuard &) = delete;
   VStackGuard & operator=(const VStackGuard &) = delete;

   // Allow moving
   VStackGuard(VStackGuard &&other) noexcept
      : ls_(other.ls_), saved_vtop_(other.saved_vtop_) {
      other.ls_ = nullptr;
   }

   VStackGuard & operator=(VStackGuard &&other) noexcept {
      if (this != &other) {
         if (ls_) ls_->vtop = saved_vtop_;
         ls_ = other.ls_;
         saved_vtop_ = other.saved_vtop_;
         other.ls_ = nullptr;
      }
      return *this;
   }
};

#pragma once

#include <concepts>
#include <memory>

#include "guard.h"
#include "imports.h"

extern "C" {
#include <utils/memutils.h>
}

namespace cppgres {

struct abstract_memory_context {

  template <typename T = void> T *alloc(size_t size) {
    return static_cast<T *>(ffi_guarded(::MemoryContextAlloc)(_memory_context(), size));
  }
  template <typename T = void> void free(T *ptr) { ffi_guarded(::pfree)(ptr); }

  void reset() { ffi_guarded(::MemoryContextReset)(_memory_context()); }

  bool operator==(abstract_memory_context &c) { return _memory_context() == c._memory_context(); }
  bool operator!=(abstract_memory_context &c) { return _memory_context() != c._memory_context(); }

  operator ::MemoryContext() { return _memory_context(); }

  ::MemoryContextCallback *register_reset_callback(::MemoryContextCallbackFunction func,
                                                   void *arg) {
    auto cb = alloc<::MemoryContextCallback>(sizeof(::MemoryContextCallback));
    cb->func = func;
    cb->arg = arg;
    ffi_guarded(::MemoryContextRegisterResetCallback)(_memory_context(), cb);
    return cb;
  }

protected:
  virtual ::MemoryContext _memory_context() = 0;
};

struct owned_memory_context : public abstract_memory_context {
  friend struct memory_context;

protected:
  owned_memory_context(::MemoryContext context) : context(context), moved(false) {}

  ~owned_memory_context() {
    if (!moved) {
      ffi_guarded(::MemoryContextDelete)(context);
    }
  }

  ::MemoryContext context;
  bool moved;

  ::MemoryContext _memory_context() override { return context; }
};

struct memory_context : public abstract_memory_context {

  friend struct owned_memory_context;

  explicit memory_context() : context(::CurrentMemoryContext) {}
  explicit memory_context(::MemoryContext context) : context(context) {}
  explicit memory_context(abstract_memory_context &&context) : context(context) {}

  explicit memory_context(owned_memory_context &&ctx) : context(ctx) { ctx.moved = true; }

  static memory_context for_pointer(void *ptr) {
    if (ptr == nullptr || ptr != (void *)MAXALIGN(ptr)) {
      throw std::runtime_error("invalid pointer");
    }
    return memory_context(ffi_guarded(::GetMemoryChunkContext)(ptr));
  }

  template <typename C> requires std::derived_from<C, abstract_memory_context>
  friend struct tracking_memory_context;

protected:
  ::MemoryContext context;

  ::MemoryContext _memory_context() override { return context; }
};

struct always_current_memory_context : public abstract_memory_context {
  always_current_memory_context() = default;

protected:
  ::MemoryContext _memory_context() override { return ::CurrentMemoryContext; }
};

struct alloc_set_memory_context : public owned_memory_context {
  using owned_memory_context::owned_memory_context;
  alloc_set_memory_context()
      : owned_memory_context(ffi_guarded(::AllocSetContextCreateInternal)(
            ::CurrentMemoryContext, nullptr, ALLOCSET_DEFAULT_SIZES)) {}
};

memory_context top_memory_context = memory_context(TopMemoryContext);

template <typename C> requires std::derived_from<C, abstract_memory_context>
struct tracking_memory_context : public abstract_memory_context {
  explicit tracking_memory_context(tracking_memory_context<C> const &ctx)
      : ctx(ctx.ctx), counter(ctx.counter), cb(ctx.cb) {
    cb->arg = this;
  }

  explicit tracking_memory_context(C ctx) : ctx(ctx), counter(0) {
    cb = this->register_reset_callback(
        [](void *i) { static_cast<struct tracking_memory_context<C> *>(i)->counter++; }, this);
  }

  tracking_memory_context(tracking_memory_context &&other) noexcept
      : ctx(std::move(other.ctx)), counter(std::move(other.counter)), cb(std::move(other.cb)) {
    other.cb = nullptr;
    cb->arg = this;
  }

  tracking_memory_context(tracking_memory_context &other) noexcept
      : cb(std::move(other.cb)), ctx(std::move(other.ctx)), counter(std::move(other.counter)) {
    cb->arg = this;
  }

  tracking_memory_context &operator=(tracking_memory_context &&other) noexcept {
    ctx = other.ctx;
    cb = other.cb;
    counter = other.counter;
    cb->arg = this;
    return *this;
  }

  ~tracking_memory_context() {
    if (cb != nullptr) {
      cb->func = [](void *) {};
    }
  }

  uint64_t resets() const { return counter; }

private:
  template <typename T> requires std::integral<T>
  struct shared_counter {
    T value;
    constexpr explicit shared_counter(T init = 0) noexcept : value(init) {}

    shared_counter &operator=(T v) noexcept {
      value = v;
      return *this;
    }

    shared_counter &operator++() noexcept {
      ++value;
      return *this;
    }

    T operator++(int) noexcept {
      T old = value;
      ++value;
      return old;
    }

    constexpr operator T() const noexcept { return value; }
  };
  C ctx;
  shared_counter<uint64_t> counter;
  ::MemoryContextCallback *cb;

protected:
  ::MemoryContext _memory_context() override { return ctx._memory_context(); }
};

template <typename T>
concept a_memory_context =
    std::derived_from<T, abstract_memory_context> && std::default_initializable<T>;

template <a_memory_context Context> struct memory_context_scope {
  explicit memory_context_scope(Context &ctx)
      : previous(::CurrentMemoryContext), ctx(ctx.operator ::MemoryContext()) {
    ::CurrentMemoryContext = ctx;
  }
  explicit memory_context_scope(Context &&ctx)
      : previous(::CurrentMemoryContext), ctx(ctx.operator ::MemoryContext()) {
    ::CurrentMemoryContext = ctx;
  }

  ~memory_context_scope() { ::CurrentMemoryContext = previous; }

private:
  ::MemoryContext previous;
  ::MemoryContext ctx;
};

template <class T, a_memory_context Context = memory_context> struct memory_context_allocator {
  using value_type = T;
  memory_context_allocator() noexcept : context(Context()), explicit_deallocation(false) {}
  memory_context_allocator(Context &&ctx, bool explicit_deallocation) noexcept
      : context(std::move(ctx)), explicit_deallocation(explicit_deallocation) {}

  constexpr memory_context_allocator(const memory_context_allocator<T> &c) noexcept
      : context(c.context) {}

  [[nodiscard]] T *allocate(std::size_t n) {
    try {
      return static_cast<T *>(context.alloc(n * sizeof(T)));
    } catch (pg_exception &e) {
      throw std::bad_alloc();
    }
  }

  void deallocate(T *p, std::size_t n) noexcept {
    if (explicit_deallocation || context == top_memory_context) {
      context.free(p);
    }
  }

  bool operator==(const memory_context_allocator &c) { return context == c.context; }
  bool operator!=(const memory_context_allocator &c) { return context != c.context; }

  Context &memory_context() { return context; }

private:
  Context context;
  bool explicit_deallocation;
};

struct pointer_gone_exception : public std::exception {
  const char *what() const noexcept override {
    return "pointer belongs to a MemoryContext that has been reset or deleted";
  }
};

} // namespace cppgres

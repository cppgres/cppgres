/**
 * \file
 */
#pragma once

namespace cppgres {
class pg_exception : public std::exception {
  ::MemoryContext mcxt;
  ::MemoryContext error_cxt;
  ::ErrorData *error;

  pg_exception(::MemoryContext mcxt);

  const char *what() const noexcept override { return error->message; }

  template <typename Func> friend struct ffi_guard;

public:
  const char *message() const noexcept { return error->message; }

  /**
   * @brief The complete error record captured from Postgres.
   *
   * Valid for the lifetime of the exception object.
   */
  const ::ErrorData *error_data() const noexcept { return error; }

  /**
   * @brief Re-throws the captured error as a Postgres error, with every
   *        field (SQLSTATE, detail, hint, context, ...) intact.
   *
   * Intended to be called from a `catch` block once cleanup is done and the
   * error should continue propagating to Postgres.  Never returns.
   *
   * The longjmp does not run this object's destructor, so the exception's
   * backing storage is handed to Postgres to be released when ErrorContext
   * is reset — that is, once the error has been fully handled.
   */
  [[noreturn]] void rethrow();

  ~pg_exception();
};
} // namespace cppgres

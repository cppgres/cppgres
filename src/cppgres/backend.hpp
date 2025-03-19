#pragma once

#include "imports.h"

namespace cppgres {

enum q { backend };

namespace backend_type {
enum type {
  invalid = B_INVALID,
  backend = B_BACKEND,
  autovac_launcher = B_AUTOVAC_LAUNCHER,
  autovac_worker = B_AUTOVAC_WORKER,
  bg_worker = B_BG_WORKER,
  wal_sender = B_WAL_SENDER,
  slotsync_worker = B_SLOTSYNC_WORKER,
  standalone_backend = B_STANDALONE_BACKEND,
  archiver = B_ARCHIVER,
  bg_writer = B_BG_WRITER,
  checkpointer = B_CHECKPOINTER,
  startup = B_STARTUP,
  wal_receiver = B_WAL_RECEIVER,
  wal_summarizer = B_WAL_SUMMARIZER,
  wal_writer = B_WAL_WRITER,
  logger = B_LOGGER
};
}

struct backend {
  static backend_type::type type() { return static_cast<backend_type::type>(::MyBackendType); };
};

} // namespace cppgres

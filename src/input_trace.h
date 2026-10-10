#pragma once
#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DECLARE(bool, pvz_input_trace);

// Diagnostic input log; arguments are evaluated only while tracing.
#define PVZ_TRACE(...) \
  do { if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO(__VA_ARGS__); } while (0)

// Host shim: the analysis layer only needs the recursive-mutex type names.
// The replay harness is single threaded, so the lock is a no-op.
#pragma once
#define portMAX_DELAY 0xFFFFFFFFu

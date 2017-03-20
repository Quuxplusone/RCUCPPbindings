#pragma once

extern "C" struct rcu_head;

#if USE_URCU_BP
#include "urcu-bp.hpp"
#elif USE_URCU_MB
#include "urcu-mb.hpp"
#elif USE_URCU_QSBR
#include "urcu-qsbr.hpp"
#elif USE_URCU_RV
#include "urcu-rv.hpp"
#elif USE_URCU_SIGNAL
#include "urcu-signal.hpp"
#else
#include "urcu-signal.hpp"  // default to urcu-signal
#endif

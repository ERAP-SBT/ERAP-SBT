#pragma once

inline bool ENABLE_DEBUG
#ifdef DEBUG
    = true;
#else
    = false;
#endif

#define DEBUG_LOG(string) \
    do { \
        if (ENABLE_DEBUG) { \
            std::cout << __FILE__ << " => L." << __LINE__ << ": " << (string) << "\n"; \
        } \
    } while (0)

inline bool FULL_BACKTRACKING = false;

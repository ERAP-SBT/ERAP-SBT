#pragma once

#ifdef DEBUG
#define DEBUG_LOG(string) std::cout << __FILE__ << " => L." << __LINE__ << ": " << (string) << "\n";
#else
#define DEBUG_LOG(string)
#endif

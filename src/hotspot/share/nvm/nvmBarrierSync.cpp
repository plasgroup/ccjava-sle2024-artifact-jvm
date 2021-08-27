#ifdef OUR_PERSIST

#include "nvm/nvmBarrierSync.inline.hpp"

pthread_mutex_t NVMBarrierSync::_mtx = PTHREAD_MUTEX_INITIALIZER;

#endif // OUR_PERSIST

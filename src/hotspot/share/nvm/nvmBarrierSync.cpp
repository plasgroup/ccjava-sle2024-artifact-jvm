#ifdef OUR_PERSIST

#include "nvm/nvmBarrierSync.inline.hpp"

pthread_mutex_t NVMBarrierSync::_mtx = PTHREAD_MUTEX_INITIALIZER;
#ifdef ASSERT
Thread* NVMBarrierSync::_locked_thread = NULL;
#endif // ASSERT

#endif // OUR_PERSIST

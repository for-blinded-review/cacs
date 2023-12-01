/*
Copyright 2022 The Photon Authors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#include <cinttypes>
#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#include <cassert>
#include "../callback.h"
// #include <photon/common/identity-pool.h>

// Defines allocator and deallocator for I/O memory,
// which deal with a ranged size.
// Users are free to implement their cusomized allocators.
struct IOAlloc
{
    struct RangeSize { int min, max;};  // must be non-negative

    // allocate a memory buffer of size within [size.min, size.max],
    // the more the better, returning the size actually allocated
    // or <0 indicating failrue
    typedef Callback<RangeSize, void**> Allocator;
    Allocator allocate;

    void* alloc(size_t size)
    {
        if (size > INT32_MAX)
            return nullptr;

        void* ptr = nullptr;
        int ret = allocate({(int)size, (int)size}, &ptr);
        return ret > 0 ?  ptr : nullptr;
    }

    // de-allocate a memory buffer
    typedef Callback<void*> Deallocator;
    Deallocator deallocate;

    int dealloc(void* ptr)
    {
        return deallocate(ptr);
    }

    IOAlloc() :
        allocate(nullptr, &default_allocator),
        deallocate(nullptr, &default_deallocator) { }

    IOAlloc(Allocator alloc, Deallocator dealloc) :
        allocate(alloc), deallocate(dealloc) { }

    void reset()
    {
        allocate.bind(nullptr, &default_allocator);
        deallocate.bind(nullptr, &default_deallocator);
    }

// protected:
    static int default_allocator(void*, RangeSize size, void** ptr)
    {
        assert(size.min > 0 && size.max >= size.min);
        *ptr = ::malloc((size_t)size.max);
        return size.max;
    }
    static int default_deallocator(void*, void* ptr)
    {
        ::free(ptr);
        return 0;
    }
};

struct AlignedAlloc : public IOAlloc
{
    AlignedAlloc(size_t alignment) : IOAlloc(
        Allocator{(void*)alignment, &aligned_allocator},
        Deallocator{(void*)alignment, &default_deallocator}) { }

    void reset()
    {
        auto alignment = allocate._obj;
        allocate.bind(alignment, &aligned_allocator);
        deallocate.bind(alignment, &default_deallocator);
    }

protected:
    static int aligned_allocator(void* alignment, RangeSize size, void** ptr)
    {
        assert(size.min > 0 && size.max >= size.min);
        int err = ::posix_memalign(ptr, (size_t)alignment, (size_t)size.max);
        if (err) {
            errno = err;
            return -1;
        }
        return size.max;
    }
};


/*
 * RawStorage.cpp
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 */


#include "vislib/RawStorage.h"

#include <stdexcept>

#include "the/assert.h"
#include "the/memory.h"
#include "the/trace.h"


/*
 * vislib::RawStorage::RawStorage
 */
vislib::RawStorage::RawStorage(const size_t size) : data(NULL), size(size) {
    this->EnforceSize(this->size);
}


/*
 * vislib::RawStorage::RawStorage
 */
vislib::RawStorage::RawStorage(const RawStorage& rhs) 
        : data(NULL), size(rhs.size) {
    if (this->size > 0) {
        this->EnforceSize(this->size);
        ::memcpy(this->data, rhs.data, this->size);
    }
}


/*
 * vislib::RawStorage::~RawStorage
 */
vislib::RawStorage::~RawStorage(void) {
    the::safe_free(this->data);
}


/*
 * vislib::RawStorage::Append
 */
void *vislib::RawStorage::Append(const void *data, const size_t cntData) {
    size_t offset = this->size;
    void *retval = NULL;

    this->EnforceSize(this->size + cntData, true);
    retval = static_cast<uint8_t *>(this->data) + offset;

    if (data != NULL) {
        ::memcpy(retval, data, cntData);
    }

    return retval;
}


/*
 * vislib::RawStorage::AssertSize
 */
bool vislib::RawStorage::AssertSize(const size_t size, const bool keepContent) {
	if (!this->TestSize(size)) {
        this->EnforceSize(size, keepContent);
        return true;

    } else {
        return false;
    }
}


/*
 * vislib::RawStorage::EnforceSize
 */
void vislib::RawStorage::EnforceSize(const size_t size, 
                                     const bool keepContent) {
    
    if ((this->size = size) > 0) {
                                         
        if (keepContent) {
            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO,
                "RawStorage::AssertSize reallocates %u bytes.\n", this->size);
            this->data = ::realloc(this->data, this->size);

        } else {
            //THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, 
            //    "RawStorage::AssertSize allocates %u bytes.\n", this->size);
            the::safe_free(this->data);
            this->data = ::malloc(this->size);
        }

        if (this->data == NULL) {
            this->size = 0;
            throw std::bad_alloc();
        }

    } /* end if ((this->size = size) > 0) */
}


/*
 * vislib::RawStorage::ZeroAll
 */
void vislib::RawStorage::ZeroAll(void) {
    if (this->data != NULL) {
        the::zero_memory(this->data, this->size);
    }
}


/*
 * vislib::RawStorage::operator =
 */
vislib::RawStorage& vislib::RawStorage::operator =(const RawStorage& rhs) {
    if (this != &rhs) {
        this->EnforceSize(rhs.size);
        ::memcpy(this->data, rhs.data, rhs.size);
        
        THE_ASSERT(this->size == rhs.size);
    }

    return *this;
}


/*
 * vislib::RawStorage::operator ==
 */
bool vislib::RawStorage::operator ==(const RawStorage& rhs) const {
    if (this != &rhs) {
        return (::memcmp(this->data, rhs.data, this->size) == 0);
    } else {
        return true;
    }
}

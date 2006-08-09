/*
 * OutOfRangeException.h
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 * Copyright (C) 2005 by Christoph Mueller. All rights reserved.
 */

#ifndef OUTOFRANGEEXCEPTION_H_INCLUDED
#define OUTOFRANGEEXCEPTION_H_INCLUDED
#if (_MSC_VER > 1000)
#pragma once
#endif /* (_MSC_VER > 1000) */


#include "Exception.h"


namespace vislib {

    /**
     * This exception indicates an illegal parameter value.
     *
     * @author Christoph Mueller
     */
    class OutOfRangeException : public Exception {

    public:

        /**
         * Ctor.
         *
         * @param val    The actual value.
         * @param minVal The allowed minimum value.
         * @param maxVal The allowed maximum value.
         * @param file      The file the exception was thrown in.
         * @param line      The line the exception was thrown in.
         */
        OutOfRangeException(const int val, const int minVal, const int maxVal,
            const char *file, const int line);

        /**
         * Create a clone of 'rhs'.
         *
         * @param rhs The object to be cloned.
         */
        OutOfRangeException(const OutOfRangeException& rhs);

        /** Dtor. */
        virtual ~OutOfRangeException(void);

        /**
         * Assignment operator.
         *
         * @param rhs The right hand side operand.
         *
         * @return *this.
         */
        virtual OutOfRangeException& operator =(const OutOfRangeException& rhs);
    };
}

#endif /* OUTOFRANGEEXCEPTION_H_INCLUDED */

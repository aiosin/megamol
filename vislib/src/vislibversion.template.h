/*
 * vislibversion.h
 *
 * Copyright (C) 2008 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 */

#ifndef VISLIB_VISLIBVERSION_H_INCLUDED
#define VISLIB_VISLIBVERSION_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */
#if defined(_WIN32) && defined(_MANAGED)
#pragma managed(push, off)
#endif /* defined(_WIN32) && defined(_MANAGED) */

/*
 * This header contains defines holding the version number constants of the
 * vislib (all projects, excluding extension).
 *
 * THIS FILE IS GENERATED FROM THE FILE vislibversion.template.h
 * DO NOT EDIT THIS FILE DIRECTLY.
 */


/**
 * Major version number of the vislib
 *
 * Increment on major changes, i.e. breaking changes
 */
#define VISLIB_VERSION_MAJOR 2


/**
 * Minor version number of the vislib
 *
 * Increment on addition of new functions
 *
 * Exception: During the v2.x redesign we increase the minor revision on
 *            function porting
 *            (from old classes to newly added external libraries)
 */
#define VISLIB_VERSION_MINOR 2


/**
 * Revision version number of the vislib
 *
 * Increment on bug fixes
 */
#define VISLIB_VERSION_REVISION 0


/**
 * Build version number of the vislib
 *
 * Autoincrements based on the repository revision
 */
#define VISLIB_VERSION_BUILD $VISLIB_VERSION$



/**
 * Dirty Flag
 *
 * Defined if Vislib working copy has local modifications
 */
#define VISLIB_DIRTY_BUILD $VISLIB_HAS_MODIFICATIONS$



/*
 * VISLIB_STRINGIZING_HELPERS for macro argument expansion
 */
#ifndef VISLIB_STRINGIZING_HELPERS
#define VISLIB_STRINGIZING_HELPERS

#define VISLIB_STRINGIZING_HELPER_INTERNAL(arg) #arg
#define VISLIB_STRINGIZING_HELPER(arg) VISLIB_STRINGIZING_HELPER_INTERNAL(arg)
#define VISLIB_STRINGIZING_W_HELPER_INTERNAL(arg) L ## #arg
#define VISLIB_STRINGIZING_W_HELPER(arg) VISLIB_STRINGIZING_W_HELPER_INTERNAL(arg)

#endif /* VISLIB_STRINGIZING_HELPERS */


/**
 * The vislib version number ANSI string.
 * Implementation note: Add no spaces after new line!!!
 */
#define VISLIB_VERSION_STR VISLIB_STRINGIZING_HELPER(VISLIB_VERSION_MAJOR.\
VISLIB_VERSION_MINOR.VISLIB_VERSION_REVISION.VISLIB_VERSION_BUILD)


/**
 * The vislib version number ANSI string.
 */
#define VISLIB_VERSION_STRA VISLIB_VERSION_STR


/**
 * The vislib version number unicode string.
 * Implementation note: Add no spaces after new line!!!
 */
#define VISLIB_VERSION_STRW VISLIB_STRINGIZING_W_HELPER(VISLIB_VERSION_MAJOR.\
VISLIB_VERSION_MINOR.VISLIB_VERSION_REVISION.VISLIB_VERSION_BUILD)


#if defined(_WIN32) && defined(_MANAGED)
#pragma managed(pop)
#endif /* defined(_WIN32) && defined(_MANAGED) */
#endif /* VISLIB_VISLIBVERSION_H_INCLUDED */

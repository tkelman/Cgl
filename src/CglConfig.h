/* Copyright (C) 2011
 * All Rights Reserved.
 * This code is published under the Eclipse Public License.
 *
 * $Id$
 *
 * Include file for the configuration of Cgl.
 *
 * On systems where the code is configured with the configure script
 * (i.e., compilation is always done with HAVE_CONFIG_H defined), this
 * header file includes the automatically generated header file, and
 * undefines macros that might configure with other Config.h files.
 *
 * On systems that are compiled in other ways (e.g., with the
 * Developer Studio), a header files is included to define those
 * macros that depend on the operating system and the compiler.  The
 * macros that define the configuration of the particular user setting
 * (e.g., presence of other COIN-OR packages or third party code) are set
 * by the files config_*default.h. The project maintainer needs to remember
 * to update these file and choose reasonable defines.
 * A user can modify the default setting by editing the config_*default.h files.
 *
 */

#ifndef __CGLCONFIG_H__
#define __CGLCONFIG_H__

#ifdef HAVE_CONFIG_H
#ifdef CGL_BUILD
#include "config.h"
#else
#include "config_cgl.h"
#endif

/*
  Used to come from CoinUtils, but no more. Seems pointless that individual
  Cgl's should need to repeat this logic.
*/
#ifdef HAVE_CMATH 
# include <cmath> 
#else
# ifdef HAVE_MATH.H
#   include <math.h>
# else
#   error "No header file for math!"
# endif
#endif

#else /* HAVE_CONFIG_H */

#ifdef CGL_BUILD
#include "config_default.h"
#else
#include "config_cgl_default.h"
#endif

#endif /* HAVE_CONFIG_H */

#endif /*__CGLCONFIG_H__*/

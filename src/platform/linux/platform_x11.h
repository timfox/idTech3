#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#if defined(__sun)
#include <X11/Sunkeysym.h>
#endif

#if defined(__has_include)
#  if __has_include(<X11/extensions/Xxf86dga.h>)
#    include <X11/extensions/Xxf86dga.h>
#    define PLATFORM_HAVE_XF86DGA 1
#  else
#    define PLATFORM_HAVE_XF86DGA 0
#  endif
#else
#  include <X11/extensions/Xxf86dga.h>
#  define PLATFORM_HAVE_XF86DGA 1
#endif

#if PLATFORM_HAVE_XF86DGA
#ifndef HAVE_XF86DGA
#  define HAVE_XF86DGA
#endif
#else
#ifndef XF86DGADirectMouse
#  define XF86DGADirectMouse 0
#endif
#ifndef XF86DGADirectVideo
#  define XF86DGADirectVideo 0
#endif
#endif

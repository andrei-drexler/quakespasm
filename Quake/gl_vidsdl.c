/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gl_vidsdl.c -- SDL GL vid component

#include "quakedef.h"
#include "cfgfile.h"
#include "bgmusic.h"
#include "resource.h"
#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

//ericw -- for putting the driver into multithreaded mode
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#endif

#define MAX_MODE_LIST	600 //johnfitz -- was 30
#define MAX_BPPS_LIST	5
#define MAX_RATES_LIST	20
#define MAXWIDTH		10000
#define MAXHEIGHT		10000

#define DEFAULT_SDL_FLAGS	SDL_OPENGL

#define DEFAULT_REFRESHRATE	60

typedef struct {
	int			width;
	int			height;
	int			refreshrate;
	int			bpp;
} vmode_t;

#define MAKE_GL_VERSION(major, minor)		(((major) << 16) | (minor))
#define MIN_GL_VERSION_MAJOR				4
#define MIN_GL_VERSION_MINOR				3
#define MIN_GL_VERSION						MAKE_GL_VERSION(MIN_GL_VERSION_MAJOR, MIN_GL_VERSION_MINOR)
#define MIN_GL_VERSION_STR					QS_STRINGIFY(MIN_GL_VERSION_MAJOR)"."QS_STRINGIFY(MIN_GL_VERSION_MINOR)

static const char *gl_vendor;
static const char *gl_renderer;
static const char *gl_version;
static int gl_version_major;
static int gl_version_minor;
static int gl_version_number;
static int gl_num_extensions;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;

static qboolean	vid_initialized = false;

static SDL_Window	*draw_context;
static SDL_GLContext	gl_context;

static qboolean	vid_locked = false; //johnfitz
static qboolean vid_changed = false;

static void VID_Menu_Init (void); //johnfitz
static void VID_Menu_f (void); //johnfitz
static void VID_MenuDraw (void);
static void VID_MenuKey (int key);
static void VID_MenuMouse (int cx, int cy);

static void ClearAllStates (void);
static void GL_Init (void);
static void GL_SetupState (void); //johnfitz

viddef_t	vid;				// global video state
modestate_t	modestate = MS_UNINIT;
qboolean	scr_skipupdate;

qboolean gl_anisotropy_able = false; //johnfitz
qboolean gl_buffer_storage_able = false;
qboolean gl_multi_bind_able = false;
qboolean gl_bindless_able = false;
qboolean gl_clipcontrol_able = false;
float gl_max_anisotropy; //johnfitz
int gl_stencilbits;

unsigned glstate;
GLint ssbo_align;
GLint ubo_align;
static GLuint globalvao;

#define QGL_DEFINE_FUNC(ret, name, args) ret (APIENTRYP GL_##name##Func) args = NULL;
QGL_ALL_FUNCTIONS(QGL_DEFINE_FUNC)
#undef QGL_DEFINE_FUNC

typedef struct glfunc_t {
	void**			ptr;
	const char*		name;
} glfunc_t;

#define QGL_REGISTER_NAMED_FUNC(ret, name, args) { (void**)&GL_##name##Func, "gl" #name },
static const glfunc_t gl_core_functions[] =
{
	QGL_CORE_FUNCTIONS(QGL_REGISTER_NAMED_FUNC)
	{NULL, NULL}
};

static const glfunc_t gl_arb_buffer_storage_functions[] =
{
	QGL_ARB_buffer_storage_FUNCTIONS(QGL_REGISTER_NAMED_FUNC)
	{NULL, NULL}
};

static const glfunc_t gl_arb_multi_bind_functions[] =
{
	QGL_ARB_multi_bind_FUNCTIONS(QGL_REGISTER_NAMED_FUNC)
	{NULL, NULL}
};

static const glfunc_t gl_arb_bindless_texture_functions[] =
{
	QGL_ARB_bindless_texture_FUNCTIONS(QGL_REGISTER_NAMED_FUNC)
	{NULL, NULL}
};

static const glfunc_t gl_arb_clip_control_functions[] =
{
	QGL_ARB_clip_control_FUNCTIONS(QGL_REGISTER_NAMED_FUNC)
	{NULL, NULL}
};
#undef QGL_REGISTER_NAMED_FUNC

//====================================

//johnfitz -- new cvars
cvar_t		vid_fullscreen = {"vid_fullscreen", "0", CVAR_ARCHIVE};	// QuakeSpasm, was "1"
cvar_t		vid_width = {"vid_width", "800", CVAR_ARCHIVE};		// QuakeSpasm, was 640
cvar_t		vid_height = {"vid_height", "600", CVAR_ARCHIVE};	// QuakeSpasm, was 480
cvar_t		vid_bpp = {"vid_bpp", "16", CVAR_ARCHIVE};
cvar_t		vid_refreshrate = {"vid_refreshrate", "60", CVAR_ARCHIVE};
cvar_t		vid_vsync = {"vid_vsync", "0", CVAR_ARCHIVE};
cvar_t		vid_fsaa = {"vid_fsaa", "0", CVAR_ARCHIVE}; // QuakeSpasm
cvar_t		vid_fsaamode = {"vid_fsaamode", "0", CVAR_ARCHIVE};
cvar_t		vid_desktopfullscreen = {"vid_desktopfullscreen", "0", CVAR_ARCHIVE}; // QuakeSpasm
cvar_t		vid_borderless = {"vid_borderless", "0", CVAR_ARCHIVE}; // QuakeSpasm
//johnfitz
cvar_t		vid_saveresize = {"vid_saveresize", "1", CVAR_ARCHIVE};

cvar_t		vid_gamma = {"gamma", "1", CVAR_ARCHIVE}; //johnfitz -- moved here from view.c
cvar_t		vid_contrast = {"contrast", "1", CVAR_ARCHIVE}; //QuakeSpasm, MarkV

void TexMgr_Anisotropy_f (cvar_t *var);
void TexMgr_CompressTextures_f (cvar_t *var);

extern cvar_t gl_texture_anisotropy;
extern cvar_t gl_texturemode;
extern cvar_t gl_compress_textures;
extern cvar_t gl_lodbias;
extern cvar_t r_particles;
extern cvar_t r_dynamic;
extern cvar_t host_maxfps;
extern cvar_t scr_showfps;

//==========================================================================
//
//  HARDWARE GAMMA -- johnfitz
//
//==========================================================================

static int fsaa;

/*
================
VID_Gamma_Init -- call on init
================
*/
static void VID_Gamma_Init (void)
{
	Cvar_RegisterVariable (&vid_gamma);
	Cvar_RegisterVariable (&vid_contrast);
}

/*
======================
VID_GetCurrentWidth
======================
*/
static int VID_GetCurrentWidth (void)
{
	int w = 0, h = 0;
	SDL_GetWindowSize(draw_context, &w, &h);
	return w;
}

/*
=======================
VID_GetCurrentHeight
=======================
*/
static int VID_GetCurrentHeight (void)
{
	int w = 0, h = 0;
	SDL_GetWindowSize(draw_context, &w, &h);
	return h;
}

/*
====================
VID_GetCurrentRefreshRate
====================
*/
static int VID_GetCurrentRefreshRate (void)
{
	SDL_DisplayMode mode;
	int current_display;

	current_display = SDL_GetWindowDisplayIndex(draw_context);

	if (0 != SDL_GetCurrentDisplayMode(current_display, &mode))
		return DEFAULT_REFRESHRATE;

	return mode.refresh_rate;
}


/*
====================
VID_GetCurrentBPP
====================
*/
static int VID_GetCurrentBPP (void)
{
	const Uint32 pixelFormat = SDL_GetWindowPixelFormat(draw_context);
	return SDL_BITSPERPIXEL(pixelFormat);
}

/*
====================
VID_GetFullscreen
 
returns true if we are in regular fullscreen or "desktop fullscren"
====================
*/
static qboolean VID_GetFullscreen (void)
{
	return (SDL_GetWindowFlags(draw_context) & SDL_WINDOW_FULLSCREEN) != 0;
}

/*
====================
VID_GetDesktopFullscreen
 
returns true if we are specifically in "desktop fullscreen" mode
====================
*/
static qboolean VID_GetDesktopFullscreen (void)
{
	return (SDL_GetWindowFlags(draw_context) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP;
}

/*
====================
VID_GetWindow

used by pl_win.c
====================
*/
void *VID_GetWindow (void)
{
	return draw_context;
}

/*
====================
VID_SetWindowTitle
====================
*/
void VID_SetWindowTitle (const char *title)
{
	SDL_SetWindowTitle (draw_context, title);
}

/*
====================
VID_HasMouseOrInputFocus
====================
*/
qboolean VID_HasMouseOrInputFocus (void)
{
	return (SDL_GetWindowFlags(draw_context) & (SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_INPUT_FOCUS)) != 0;
}

/*
====================
VID_IsMinimized
====================
*/
qboolean VID_IsMinimized (void)
{
	return !(SDL_GetWindowFlags(draw_context) & SDL_WINDOW_SHOWN);
}

/*
================
VID_SDL2_GetDisplayMode

Returns a pointer to a statically allocated SDL_DisplayMode structure
if there is one with the requested params on the default display.
Otherwise returns NULL.

This is passed to SDL_SetWindowDisplayMode to specify a pixel format
with the requested bpp. If we didn't care about bpp we could just pass NULL.
================
*/
static SDL_DisplayMode *VID_SDL2_GetDisplayMode(int width, int height, int refreshrate, int bpp)
{
	static SDL_DisplayMode mode;
	const int sdlmodes = SDL_GetNumDisplayModes(0);
	int i;

	for (i = 0; i < sdlmodes; i++)
	{
		if (SDL_GetDisplayMode(0, i, &mode) != 0)
			continue;

		if (mode.w == width && mode.h == height
			&& SDL_BITSPERPIXEL(mode.format) == bpp
			&& mode.refresh_rate == refreshrate)
		{
			return &mode;
		}
	}
	return NULL;
}

/*
================
VID_ValidMode
================
*/
static qboolean VID_ValidMode (int width, int height, int refreshrate, int bpp, qboolean fullscreen)
{
// ignore width / height / bpp if vid_desktopfullscreen is enabled
	if (fullscreen && vid_desktopfullscreen.value)
		return true;

	if (width < 320)
		return false;

	if (height < 200)
		return false;

	if (fullscreen && VID_SDL2_GetDisplayMode(width, height, refreshrate, bpp) == NULL)
		bpp = 0;

	switch (bpp)
	{
	case 16:
	case 24:
	case 32:
		break;
	default:
		return false;
	}

	return true;
}

/*
================
VID_SetMode
================
*/
static qboolean VID_SetMode (int width, int height, int refreshrate, int bpp, qboolean fullscreen)
{
	int		temp;
	Uint32	flags;
	char		caption[50];
	int		depthbits, stencilbits;
	int		previous_display;

	// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();
	BGM_Pause ();

	/* z-buffer depth */
	if (bpp == 16)
		depthbits = 16;
	else
		depthbits = 24;
	stencilbits = 8;
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depthbits);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, stencilbits);

	q_snprintf(caption, sizeof(caption), WINDOW_TITLE_STRING);

	/* Create the window if needed, hidden */
	if (!draw_context)
	{
		flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;

		if (vid_borderless.value)
			flags |= SDL_WINDOW_BORDERLESS;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, MIN_GL_VERSION_MAJOR);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, MIN_GL_VERSION_MINOR);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifndef NDEBUG
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
		draw_context = SDL_CreateWindow (caption, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
		if (!draw_context) { // scale back SDL_GL_DEPTH_SIZE
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
			draw_context = SDL_CreateWindow (caption, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
		}
		if (!draw_context)
			Sys_Error ("Couldn't create window");

		SDL_SetWindowMinimumSize (draw_context, 320, 240);

		previous_display = -1;
	}
	else
	{
		previous_display = SDL_GetWindowDisplayIndex(draw_context);
	}

	/* Ensure the window is not fullscreen */
	if (VID_GetFullscreen ())
	{
		if (SDL_SetWindowFullscreen (draw_context, 0) != 0)
			Sys_Error("Couldn't set fullscreen state mode");
	}

	/* Set window size and display mode */
	SDL_SetWindowSize (draw_context, width, height);
	if (previous_display >= 0)
		SDL_SetWindowPosition (draw_context, SDL_WINDOWPOS_CENTERED_DISPLAY(previous_display), SDL_WINDOWPOS_CENTERED_DISPLAY(previous_display));
	else
		SDL_SetWindowPosition(draw_context, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_SetWindowDisplayMode (draw_context, VID_SDL2_GetDisplayMode(width, height, refreshrate, bpp));
	SDL_SetWindowBordered (draw_context, vid_borderless.value ? SDL_FALSE : SDL_TRUE);

	/* Make window fullscreen if needed, and show the window */

	if (fullscreen) {
		const Uint32 flag = vid_desktopfullscreen.value ?
				SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
		if (SDL_SetWindowFullscreen (draw_context, flag) != 0)
			Sys_Error ("Couldn't set fullscreen state mode");
	}

	SDL_ShowWindow (draw_context);
	SDL_RaiseWindow (draw_context);

	/* Create GL context if needed */
	if (!gl_context) {
		gl_context = SDL_GL_CreateContext(draw_context);
		if (!gl_context)
		{
			// Couldn't create an OpenGL context with our minimum requirements.
			// Try again with the default attributes and see what we get,
			// so we have more meaningful information for the error message.
			int major, minor;
			const char *version;

			SDL_GL_ResetAttributes();
			gl_context = SDL_GL_CreateContext(draw_context);
			version = gl_context ? (const char *) glGetString(GL_VERSION) : NULL;
			if (!version || sscanf(version, "%d.%d", &major, &minor) != 2)
				major = minor = 0;

			if (major && MAKE_GL_VERSION(major, minor) < MIN_GL_VERSION)
				Sys_Error(
					"This engine requires OpenGL %d.%d, but only version %d.%d was found.\n"
					"Please make sure that your GPU (%s) meets the minimum requirements and that the graphics drivers are up to date.",
					MIN_GL_VERSION_MAJOR, MIN_GL_VERSION_MINOR, major, minor, (const char *) glGetString(GL_RENDERER)
				);
			else if (gl_context)
				Sys_Error(
					"Could not create OpenGL %d.%d context.\n"
					"Please make sure that your GPU (%s) meets the minimum requirements and that the graphics drivers are up to date.",
					MIN_GL_VERSION_MAJOR, MIN_GL_VERSION_MINOR, (const char *) glGetString(GL_RENDERER)
				);
			else
				Sys_Error(
					"Could not create OpenGL %d.%d context. " // no newline
					"Please make sure that your GPU meets the minimum requirements and that the graphics drivers are up to date.",
					MIN_GL_VERSION_MAJOR, MIN_GL_VERSION_MINOR
				);
		}
	}

	vid.width = VID_GetCurrentWidth();
	vid.height = VID_GetCurrentHeight();
	vid.maxscale = q_max (4, vid.height / 240);
	vid.refreshrate = VID_GetCurrentRefreshRate();
	vid.conwidth = vid.width & 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
	vid.numpages = 2;

// read the obtained z-buffer depth
	if (SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depthbits) == -1)
		depthbits = 0;

// read stencil bits
	if (SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &gl_stencilbits) == -1)
		gl_stencilbits = 0;

	modestate = VID_GetFullscreen() ? MS_FULLSCREEN : MS_WINDOWED;

	CDAudio_Resume ();
	BGM_Resume ();
	scr_disabled_for_loading = temp;

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	Con_SafePrintf ("Video mode: %dx%dx%d Z%d S%d %dHz\n",
				VID_GetCurrentWidth(),
				VID_GetCurrentHeight(),
				VID_GetCurrentBPP(),
				depthbits,
				gl_stencilbits,
				VID_GetCurrentRefreshRate());

	vid.recalc_refdef = 1;

// no pending changes
	vid_changed = false;

	return true;
}

/*
=================
VID_ApplyVSync
=================
*/
static void VID_ApplyVSync (void)
{
	const int MAX_INTERVAL = 4;
	int interval = (int) vid_vsync.value;
	if (abs(interval) > MAX_INTERVAL)
	{
		Con_SafePrintf ("VSync interval %d too high, clamping to %d\n", abs(interval), MAX_INTERVAL);
		interval = interval < 0 ? -MAX_INTERVAL : MAX_INTERVAL;
	}
	if (SDL_GL_SetSwapInterval (interval) != 0)
	{
		if (interval == 0)
			Con_SafePrintf ("Could not disable vsync\n");
		else
			Con_SafePrintf ("Could not set vsync interval to %d\n", interval);
	}
}

/*
=================
VID_VSync_f

Called when vid_vsync changes
=================
*/
static void VID_VSync_f (cvar_t *cvar)
{
	VID_ApplyVSync ();
}

/*
=================
VID_FSAA_f

Called when vid_fsaa changes
=================
*/
static void VID_FSAA_f (cvar_t *cvar)
{
	if (!host_initialized)
		return;
	GL_DeleteFrameBuffers ();
	GL_CreateFrameBuffers ();
	gl_lodbias.callback (&gl_lodbias);
}

/*
=================
VID_FSAAMode_f

Called when vid_fsaamode changes
=================
*/
static void VID_FSAAMode_f (cvar_t *cvar)
{
	if (!host_initialized)
		return;
	GL_MinSampleShadingFunc (cvar->value);
	gl_lodbias.callback (&gl_lodbias);
}

/*
===================
VID_Changed_f -- kristian -- notify us that a value has changed that requires a vid_restart
===================
*/
void VID_Changed_f (cvar_t *var)
{
	if (vid_initialized && !vid_locked)
		Con_SafePrintf ("%s %s will be applied after a vid_restart\n", var->name, var->string);
	vid_changed = true;
}

void VID_RecalcConsoleSize (void)
{
	vid.conwidth = (scr_conwidth.value > 0) ? (int)scr_conwidth.value : (scr_conscale.value > 0) ? (int)(vid.width/scr_conscale.value) : vid.width;
	vid.conwidth = CLAMP (320, vid.conwidth, vid.width);
	vid.conwidth &= 0xFFFFFFF8;
	vid.conheight = vid.conwidth * vid.height / vid.width;
}

/*
===================
VID_Restart -- johnfitz -- change video modes on the fly
===================
*/
static void VID_Restart (void)
{
	int width, height, refreshrate, bpp;
	qboolean fullscreen;

	if (vid_locked || !vid_changed)
		return;

	width = (int)vid_width.value;
	height = (int)vid_height.value;
	refreshrate = (int)vid_refreshrate.value;
	bpp = (int)vid_bpp.value;
	fullscreen = vid_fullscreen.value ? true : false;

//
// validate new mode
//
	if (!VID_ValidMode (width, height, refreshrate, bpp, fullscreen))
	{
		Con_Printf ("%dx%dx%d %dHz %s is not a valid mode\n",
				width, height, bpp, refreshrate, fullscreen? "fullscreen" : "windowed");
		return;
	}

	GL_DeleteFrameBuffers ();

//
// set new mode
//
	VID_SetMode (width, height, refreshrate, bpp, fullscreen);

	//conwidth and conheight need to be recalculated
	VID_RecalcConsoleSize ();

	GL_CreateFrameBuffers ();
//
// keep cvars in line with actual mode
//
	VID_SyncCvars();
//
// update mouse grab
//
	if (key_dest == key_console)
		IN_DeactivateForConsole();
	else if (key_dest == key_menu)
		IN_DeactivateForMenu();
}

/*
================
VID_Test -- johnfitz -- like vid_restart, but asks for confirmation after switching modes
================
*/
static void VID_Test (void)
{
	int old_width, old_height, old_refreshrate, old_bpp, old_fullscreen;

	if (vid_locked || !vid_changed)
		return;
//
// now try the switch
//
	old_width = VID_GetCurrentWidth();
	old_height = VID_GetCurrentHeight();
	old_refreshrate = VID_GetCurrentRefreshRate();
	old_bpp = VID_GetCurrentBPP();
	old_fullscreen = VID_GetFullscreen() ? true : false;

	VID_Restart ();

	//pop up confirmation dialoge
	if (!SCR_ModalMessage("Would you like to keep this\nvideo mode? (y/n)\n", 5.0f))
	{
		//revert cvars and mode
		Cvar_SetValueQuick (&vid_width, old_width);
		Cvar_SetValueQuick (&vid_height, old_height);
		Cvar_SetValueQuick (&vid_refreshrate, old_refreshrate);
		Cvar_SetValueQuick (&vid_bpp, old_bpp);
		Cvar_SetQuick (&vid_fullscreen, old_fullscreen ? "1" : "0");
		VID_Restart ();
	}
}

/*
================
VID_Unlock -- johnfitz
================
*/
static void VID_Unlock (void)
{
	VID_SyncCvars();
	vid_locked = false;
}

/*
================
VID_Lock -- ericw

Subsequent changes to vid_* mode settings, and vid_restart commands, will
be ignored until the "vid_unlock" command is run.

Used when changing gamedirs so the current settings override what was saved
in the config.cfg.
================
*/
void VID_Lock (void)
{
	vid_locked = true;
}

//==============================================================================
//
//	OPENGL STUFF
//
//==============================================================================

/*
===============
GL_Info_f -- johnfitz
===============
*/
static void GL_Info_f (void)
{
	int i;
	Con_Printf ("GL_VENDOR:     %s\n", gl_vendor);
	Con_Printf ("GL_RENDERER:   %s\n", gl_renderer);
	Con_Printf ("GL_VERSION:    %s\n", gl_version);

	Con_Printf ("GL_EXTENSIONS: %d\n", gl_num_extensions);

	if (Cmd_Argc () >= 2)
	{
		const char *filter = Cmd_Argv (1);
		int filterlen = strlen (filter);
		int count = 0;
		for (i = 0; i < gl_num_extensions; i++)
		{
			const char *ext = (const char *) GL_GetStringiFunc (GL_EXTENSIONS, i);
			const char *match = q_strcasestr (ext, filter);
			if (match)
			{
				Con_Printf ("%3d. %.*s", i + 1, (int)(match - ext), ext);
				Con_Printf ("\x02%.*s", filterlen, match);
				Con_Printf ("%s\n", match + filterlen);
				count++;
			}
		}
		Con_Printf ("%3d extensions containing \"%s\"\n", count, filter);
	}
	else
	{
		for (i = 0; i < gl_num_extensions; i++)
			Con_Printf("%3d. %s\n", i + 1, GL_GetStringiFunc (GL_EXTENSIONS, i));
	}
}

/*
===============
GL_FindExtension
===============
*/
static qboolean GL_FindExtension (const char *name)
{
	int i;
	for (i = 0; i < gl_num_extensions; i++)
	{
		if (0 == strcmp(name, (const char*) GL_GetStringiFunc (GL_EXTENSIONS, i)))
		{
			if (Q_strncmp (name, "GL_", 3) == 0)
				name += 3;
			Con_SafePrintf ("FOUND: %s\n", name);
			return true;
		}
	}
	return false;
}

/*
=============
GL_BeginGroup
=============
*/
static qboolean gldebug = false;
void GL_BeginGroup (const char *name)
{
	if (gldebug)
		GL_PushDebugGroupFunc (GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
}

/*
=============
GL_EndGroup
=============
*/
void GL_EndGroup (void)
{
	if (gldebug)
		GL_PopDebugGroupFunc ();
}

/*
===============
GL_DebugCallback
===============
*/
static void APIENTRY GL_DebugCallback (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
	const char *str_source = "";
	const char *str_type = "";
	const char *str_severity = "";

	switch (source)
	{
		case GL_DEBUG_SOURCE_API:				str_source = "api"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:		str_source = "window system"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER:	str_source = "shader compiler"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:		str_source = "third party"; break;
		case GL_DEBUG_SOURCE_APPLICATION:		str_source = "application"; break;
		case GL_DEBUG_SOURCE_OTHER:				str_source = "other"; break;
		default:
			break;
	}

	switch (type)
	{
		case GL_DEBUG_TYPE_PUSH_GROUP:
		case GL_DEBUG_TYPE_POP_GROUP:
			return;
		case GL_DEBUG_TYPE_ERROR:				str_type = "error "; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:	str_type = "deprecated "; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:	str_type = "undefined "; break;
		case GL_DEBUG_TYPE_PORTABILITY:			str_type = "portability "; break;
		case GL_DEBUG_TYPE_PERFORMANCE:			str_type = "performance "; break;
		case GL_DEBUG_TYPE_MARKER:				str_type = "marker "; break;
		case GL_DEBUG_TYPE_OTHER:				str_type = ""; break;
		default:
			break;
	}

	switch (severity)
	{
		case GL_DEBUG_SEVERITY_NOTIFICATION:	str_severity = "info"; break;
		case GL_DEBUG_SEVERITY_LOW:				str_severity = "low"; break;
		case GL_DEBUG_SEVERITY_MEDIUM:			str_severity = "med"; break;
		case GL_DEBUG_SEVERITY_HIGH:			str_severity = "high"; break;
		default:
			break;
	}

	if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
	{
		Sys_Printf ("GL %s %s[#%u/%s]: %s\n", str_source, str_type, id, str_severity, message);
	}
	else
	{
		Con_SafePrintf ("\x02GL %s %s[#%u/%s]: ", str_source, str_type, id, str_severity);
		Con_SafePrintf ("%s\n", message);
	}
}

/*
===============
GL_InitFunctions
===============
*/
qboolean GL_InitFunctions (const glfunc_t *funcs, qboolean required)
{
	qboolean ret = true;

	while (funcs->name)
	{
		if ((*funcs->ptr = SDL_GL_GetProcAddress (funcs->name)) == NULL)
		{
			if (required)
			{
				Sys_Error ("OpenGL function %s not found\n", funcs->name);
			}
			else
			{
				Con_Warning ("OpenGL function %s not found\n", funcs->name);
				ret = false;
			}
		}
		funcs++;
	}

	return ret;
}

/*
===============
GL_CheckExtensions
===============
*/
static void GL_CheckExtensions (void)
{
	GL_InitFunctions (gl_core_functions, true);

#ifdef NDEBUG
	if (COM_CheckParm("-gldebug"))
#endif
	{
		gldebug = true;
		GL_DebugMessageCallbackFunc (&GL_DebugCallback, NULL);
		glEnable (GL_DEBUG_OUTPUT);
		glEnable (GL_DEBUG_OUTPUT_SYNCHRONOUS);
	}

	// anisotropic filtering
	//
	if (GL_FindExtension ("GL_EXT_texture_filter_anisotropic"))
	{
		float test1,test2;
		GLuint tex;

		// test to make sure we really have control over it
		// 1.0 and 2.0 should always be legal values
		glGenTextures(1, &tex);
		glBindTexture (GL_TEXTURE_2D, tex);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
		glGetTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &test1);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
		glGetTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, &test2);
		glDeleteTextures(1, &tex);

		if (test1 == 1 && test2 == 2)
		{
			gl_anisotropy_able = true;
		}
		else
		{
			Con_Warning ("anisotropic filtering locked by driver. Current driver setting is %f\n", test1);
		}

		//get max value either way, so the menu and stuff know it
		glGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_max_anisotropy);
		if (gl_max_anisotropy < 2)
		{
			gl_anisotropy_able = false;
			gl_max_anisotropy = 1;
			Con_Warning ("anisotropic filtering broken: disabled\n");
		}
	}
	else
	{
		gl_max_anisotropy = 1;
		Con_Warning ("texture_filter_anisotropic not supported\n");
	}
	Cvar_SetValueQuick (&gl_texture_anisotropy, CLAMP (1.f, gl_texture_anisotropy.value, gl_max_anisotropy));

	gl_buffer_storage_able =
		!COM_CheckParm ("-nobufferstorage") &&
		GL_FindExtension ("GL_ARB_buffer_storage") &&
		GL_InitFunctions (gl_arb_buffer_storage_functions, false)
	;

	gl_multi_bind_able =
		!COM_CheckParm ("-nomultibind") &&
		GL_FindExtension ("GL_ARB_multi_bind") &&
		GL_InitFunctions (gl_arb_multi_bind_functions, false)
	;

	gl_bindless_able =
		!COM_CheckParm ("-nobindless") &&
		GL_FindExtension ("GL_ARB_bindless_texture") &&
		GL_FindExtension ("GL_ARB_shader_draw_parameters") &&
		GL_InitFunctions (gl_arb_bindless_texture_functions, false)
	;

	gl_clipcontrol_able =
		!COM_CheckParm ("-noclipcontrol") &&
		GL_FindExtension ("GL_ARB_clip_control") &&
		GL_InitFunctions (gl_arb_clip_control_functions, false)
	;
}

/*
=============
GL_SetStateEx
=============
*/
static void GL_SetStateEx (unsigned mask, unsigned force)
{
	unsigned diff = (mask ^ glstate) | force;

	if (diff & GLS_MASK_BLEND)
	{
		switch (mask & GLS_MASK_BLEND)
		{
			default:
			case GLS_BLEND_OPAQUE:
				glBlendFunc(GL_ONE, GL_ZERO);
				break;
			case GLS_BLEND_ALPHA:
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				break;
			case GLS_BLEND_ADD:
				glBlendFunc(GL_ONE, GL_ONE);
				break;
			case GLS_BLEND_MULTIPLY:
				glBlendFunc(GL_ZERO, GL_SRC_COLOR);
				break;
		}
	}

	if (diff & GLS_MASK_CULL)
	{
		unsigned cull = mask & GLS_MASK_CULL;
		if (cull == GLS_CULL_NONE)
		{
			glDisable(GL_CULL_FACE);
		}
		else
		{
			if ((glstate & GLS_MASK_CULL) == GLS_CULL_NONE || (force & GLS_MASK_CULL) != 0)
				glEnable(GL_CULL_FACE);
			if (cull == GLS_CULL_FRONT)
				glCullFace(GL_FRONT);
			else
				glCullFace(GL_BACK);
		}
	}

	if (diff & GLS_NO_ZTEST)
	{
		if (mask & GLS_NO_ZTEST)
			glDisable(GL_DEPTH_TEST);
		else
			glEnable(GL_DEPTH_TEST);
	}

	if (diff & GLS_NO_ZWRITE)
		glDepthMask((mask & GLS_NO_ZWRITE) == 0);

	if (diff & GLS_MASK_ATTRIBS)
	{
		int oldattribs = (glstate & GLS_MASK_ATTRIBS) >> GLS_ATTRIBS_SHIFT;
		int newattribs = (mask    & GLS_MASK_ATTRIBS) >> GLS_ATTRIBS_SHIFT;
		int i;

		if (force & GLS_MASK_ATTRIBS)
		{
			for (i = 0; i < GLS_ATTRIBS_MAXCOUNT; i++)
				if (i < newattribs)
					GL_EnableVertexAttribArrayFunc(i);
				else
					GL_DisableVertexAttribArrayFunc(i);
		}
		else
		{
			for (i = oldattribs; i < newattribs; i++)
				GL_EnableVertexAttribArrayFunc(i);
			for (i = newattribs; i < oldattribs; i++)
				GL_DisableVertexAttribArrayFunc(i);
		}
	}

	if (diff & GLS_MASK_INSTANCED_ATTRIBS)
	{
		int oldattribs = (glstate & GLS_MASK_INSTANCED_ATTRIBS) >> GLS_INSTANCED_ATTRIBS_SHIFT;
		int newattribs = (mask    & GLS_MASK_INSTANCED_ATTRIBS) >> GLS_INSTANCED_ATTRIBS_SHIFT;
		int i;

		if (force & GLS_MASK_INSTANCED_ATTRIBS)
		{
			for (i = 0; i < GLS_ATTRIBS_MAXCOUNT; i++)
				GL_VertexAttribDivisorFunc(i, i < newattribs);
		}
		else
		{
			for (i = oldattribs; i < newattribs; i++)
				GL_VertexAttribDivisorFunc(i, 1);
			for (i = newattribs; i < oldattribs; i++)
				GL_VertexAttribDivisorFunc(i, 0);
		}
	}

	glstate = mask;
}

/*
=============
GL_SetState
=============
*/
void GL_SetState (unsigned mask)
{
	GL_SetStateEx (mask, 0);
}

/*
=============
GL_ResetState
=============
*/
void GL_ResetState (void)
{
	GL_SetStateEx (GLS_DEFAULT_STATE, ~0u);
}

/*
===============
GL_SetupState -- johnfitz

does all the stuff from GL_Init that needs to be done every time a new GL render context is created
===============
*/
static void GL_SetupState (void)
{
	glClearColor (0.f, 0.f, 0.f, 0.f);
	if (gl_clipcontrol_able)
	{
		GL_ClipControlFunc (GL_LOWER_LEFT, GL_ZERO_TO_ONE);
		glClearDepth (0.f);
		glDepthFunc (GL_GEQUAL);
	}
	else
	{
		glClearDepth (1.f);
		glDepthFunc (GL_LEQUAL);
	}
	glFrontFace (GL_CW); //johnfitz -- glquake used CCW with backwards culling -- let's do it right
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	GL_DepthRange (ZRANGE_FULL); //johnfitz -- moved here becuase gl_ztrick is gone.
	glEnable (GL_BLEND);
	glEnable (GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glEnable (GL_SAMPLE_SHADING);

	GL_ResetState ();
}

/*
===============
GL_Init
===============
*/
static void GL_Init (void)
{
	gl_vendor = (const char *) glGetString (GL_VENDOR);
	gl_renderer = (const char *) glGetString (GL_RENDERER);
	gl_version = (const char *) glGetString (GL_VERSION);
	glGetIntegerv (GL_NUM_EXTENSIONS, &gl_num_extensions);

	Con_SafePrintf ("GL_VENDOR: %s\n", gl_vendor);
	Con_SafePrintf ("GL_RENDERER: %s\n", gl_renderer);
	Con_SafePrintf ("GL_VERSION: %s\n", gl_version);
	
	if (gl_version == NULL || sscanf(gl_version, "%d.%d", &gl_version_major, &gl_version_minor) < 2)
	{
		gl_version_major = 0;
		gl_version_minor = 0;
	}
	gl_version_number = MAKE_GL_VERSION(gl_version_major, gl_version_minor);
	if (gl_version_number < MIN_GL_VERSION)
		Sys_Error("OpenGL " MIN_GL_VERSION_STR " required, found %d.%d\n", gl_version_major, gl_version_minor);

	GL_CheckExtensions ();

	GL_GenVertexArraysFunc (1, &globalvao);
	GL_BindVertexArrayFunc (globalvao);

	glGetIntegerv (GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &ssbo_align);
	ssbo_align = q_max (ssbo_align, 16);
	--ssbo_align;

	glGetIntegerv (GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &ubo_align);
	ubo_align = q_max (ubo_align, 16);
	--ubo_align;

#ifdef __APPLE__
	// ericw -- enable multi-threaded OpenGL, gives a decent FPS boost.
	// https://developer.apple.com/library/mac/technotes/tn2085/
	if (host_parms->numcpus > 1 &&
	    kCGLNoError != CGLEnable(CGLGetCurrentContext(), kCGLCEMPEngine))
	{
		Con_Warning ("Couldn't enable multi-threaded OpenGL");
	}
#endif

	//johnfitz -- intel video workarounds from Baker
	if (!strcmp(gl_vendor, "Intel"))
	{
		Con_SafePrintf ("Intel Display Adapter detected, enabling gl_clear\n");
		Cbuf_AddText ("gl_clear 1");
	}
	//johnfitz

	GL_CreateShaders ();
	GL_CreateFrameBuffers ();
	GLLight_CreateResources ();
	GLPalette_CreateResources ();

	GL_ClearBufferBindings ();
	GL_CreateFrameResources ();
}

/*
=================
GL_BeginRendering -- sets values of glx, gly, glwidth, glheight
=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	qboolean postprocess = vid_gamma.value != 1.f || vid_contrast.value != 1.f || softemu;

	if (vid.resized)
	{
		vid.resized = false;
		vid.recalc_refdef = true;
		if (vid_saveresize.value)
		{
			qboolean was_locked = vid_locked;
			vid_locked = true; // avoid "vid_width will be applied after a vid_restart" spam
			Cvar_SetValueQuick (&vid_width, vid.width);
			Cvar_SetValueQuick (&vid_height, vid.height);
			vid_locked = was_locked;
		}
		VID_RecalcConsoleSize ();
		GL_DeleteFrameBuffers ();
		GL_CreateFrameBuffers ();
	}

	*x = *y = 0;
	*width = vid.width;
	*height = vid.height;

	GL_AcquireFrameResources ();

	GL_BindFramebufferFunc (GL_FRAMEBUFFER, postprocess ? framebufs.composite.fbo : 0);
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering (void)
{
	GL_PostProcess ();
	GL_ReleaseFrameResources ();

	if (!scr_skipupdate)
	{
		SDL_GL_SwapWindow(draw_context);
	}
}


void	VID_Shutdown (void)
{
	if (vid_initialized)
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		draw_context = NULL;
		gl_context = NULL;
		PL_VID_Shutdown();
	}
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
static void ClearAllStates (void)
{
	Key_ClearStates ();
	IN_ClearStates ();
}


//==========================================================================
//
//  COMMANDS
//
//==========================================================================

/*
=================
VID_DescribeCurrentMode_f
=================
*/
static void VID_DescribeCurrentMode_f (void)
{
	if (draw_context)
		Con_Printf("%dx%dx%d %dHz %s\n",
			VID_GetCurrentWidth(),
			VID_GetCurrentHeight(),
			VID_GetCurrentBPP(),
			VID_GetCurrentRefreshRate(),
			VID_GetFullscreen() ? "fullscreen" : "windowed");
}

/*
=================
VID_DescribeModes_f -- johnfitz -- changed formatting, and added refresh rates after each mode.
=================
*/
static void VID_DescribeModes_f (void)
{
	int	i;
	int	lastwidth, lastheight, lastbpp, count;

	lastwidth = lastheight = lastbpp = count = 0;

	for (i = 0; i < nummodes; i++)
	{
		if (lastwidth != modelist[i].width || lastheight != modelist[i].height || lastbpp != modelist[i].bpp)
		{
			if (count > 0)
				Con_SafePrintf ("\n");
			Con_SafePrintf ("   %4i x %4i x %i : %i", modelist[i].width, modelist[i].height, modelist[i].bpp, modelist[i].refreshrate);
			lastwidth = modelist[i].width;
			lastheight = modelist[i].height;
			lastbpp = modelist[i].bpp;
			count++;
		}
	}
	Con_Printf ("\n%i modes\n", count);
}

//==========================================================================
//
//  INIT
//
//==========================================================================

/*
=================
VID_InitModelist
=================
*/
static void VID_InitModelist (void)
{
	const int sdlmodes = SDL_GetNumDisplayModes(0);
	int i;

	nummodes = 0;
	for (i = 0; i < sdlmodes; i++)
	{
		SDL_DisplayMode mode;

		if (nummodes >= MAX_MODE_LIST)
			break;
		if (SDL_GetDisplayMode(0, i, &mode) == 0)
		{
			modelist[nummodes].width = mode.w;
			modelist[nummodes].height = mode.h;
			modelist[nummodes].bpp = SDL_BITSPERPIXEL(mode.format);
			modelist[nummodes].refreshrate = mode.refresh_rate;
			nummodes++;
		}
	}
}

/*
===================
VID_Init
===================
*/
void	VID_Init (void)
{
	static char vid_center[] = "SDL_VIDEO_CENTERED=center";
	int		p, width, height, refreshrate, bpp;
	int		display_width, display_height, display_refreshrate, display_bpp;
	qboolean	fullscreen;
	const char	*read_vars[] =
	{
		"vid_fullscreen",
		"vid_width",
		"vid_height",
		"vid_refreshrate",
		"vid_bpp",
		"vid_vsync",
		"vid_fsaa",
		"vid_desktopfullscreen",
		"vid_borderless",
		"gl_texture_anisotropy",
		"gl_compress_textures",
	};
#define num_readvars	( sizeof(read_vars)/sizeof(read_vars[0]) )

	Cvar_RegisterVariable (&vid_fullscreen); //johnfitz
	Cvar_RegisterVariable (&vid_width); //johnfitz
	Cvar_RegisterVariable (&vid_height); //johnfitz
	Cvar_RegisterVariable (&vid_refreshrate); //johnfitz
	Cvar_RegisterVariable (&vid_bpp); //johnfitz
	Cvar_RegisterVariable (&vid_vsync); //johnfitz
	Cvar_RegisterVariable (&vid_fsaa); //QuakeSpasm
	Cvar_RegisterVariable (&vid_fsaamode);
	Cvar_RegisterVariable (&vid_desktopfullscreen); //QuakeSpasm
	Cvar_RegisterVariable (&vid_borderless); //QuakeSpasm
	Cvar_RegisterVariable (&vid_saveresize);
	Cvar_SetCallback (&vid_fullscreen, VID_Changed_f);
	Cvar_SetCallback (&vid_width, VID_Changed_f);
	Cvar_SetCallback (&vid_height, VID_Changed_f);
	Cvar_SetCallback (&vid_refreshrate, VID_Changed_f);
	Cvar_SetCallback (&vid_bpp, VID_Changed_f);
	Cvar_SetCallback (&vid_vsync, VID_VSync_f);
	Cvar_SetCallback (&vid_fsaa, VID_FSAA_f);
	Cvar_SetCallback (&vid_fsaamode, VID_FSAAMode_f);
	Cvar_SetCallback (&vid_desktopfullscreen, VID_Changed_f);
	Cvar_SetCallback (&vid_borderless, VID_Changed_f);

	Cvar_RegisterVariable (&gl_texture_anisotropy);
	Cvar_SetCallback (&gl_texture_anisotropy, &TexMgr_Anisotropy_f);

	Cvar_RegisterVariable (&gl_compress_textures);
	Cvar_SetCallback (&gl_compress_textures, TexMgr_CompressTextures_f);

	Cmd_AddCommand ("vid_unlock", VID_Unlock); //johnfitz
	Cmd_AddCommand ("vid_restart", VID_Restart); //johnfitz
	Cmd_AddCommand ("vid_test", VID_Test); //johnfitz
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);

	putenv (vid_center);	/* SDL_putenv is problematic in versions <= 1.2.9 */

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		Sys_Error("Couldn't init SDL video: %s", SDL_GetError());

	{
		SDL_DisplayMode mode;
		if (SDL_GetDesktopDisplayMode(0, &mode) != 0)
			Sys_Error("Could not get desktop display mode");

		display_width = mode.w;
		display_height = mode.h;
		display_refreshrate = mode.refresh_rate;
		display_bpp = SDL_BITSPERPIXEL(mode.format);
	}

	Cvar_SetValueQuick (&vid_width, (float)display_width);
	Cvar_SetValueQuick (&vid_height, (float)display_height);
	Cvar_SetValueQuick (&vid_bpp, (float)display_bpp);
	Cvar_SetValueQuick (&vid_refreshrate, (float)display_refreshrate);

	if (CFG_OpenConfig(CONFIG_NAME) == 0 || CFG_OpenConfig("config.cfg") == 0)
	{
		CFG_ReadCvars(read_vars, num_readvars);
		CFG_CloseConfig();
	}
	CFG_ReadCvarOverrides(read_vars, num_readvars);

	VID_InitModelist();

	width = (int)vid_width.value;
	height = (int)vid_height.value;
	refreshrate = (int)vid_refreshrate.value;
	bpp = (int)vid_bpp.value;
	fullscreen = (int)vid_fullscreen.value;
	fsaa = (int)vid_fsaa.value;

	if (COM_CheckParm("-current"))
	{
		width = display_width;
		height = display_height;
		refreshrate = display_refreshrate;
		bpp = display_bpp;
		fullscreen = true;
	}
	else
	{
		p = COM_CheckParm("-width");
		if (p && p < com_argc-1)
		{
			width = Q_atoi(com_argv[p+1]);

			if(!COM_CheckParm("-height"))
				height = width * 3 / 4;
		}

		p = COM_CheckParm("-height");
		if (p && p < com_argc-1)
		{
			height = Q_atoi(com_argv[p+1]);

			if(!COM_CheckParm("-width"))
				width = height * 4 / 3;
		}

		p = COM_CheckParm("-refreshrate");
		if (p && p < com_argc-1)
			refreshrate = Q_atoi(com_argv[p+1]);

		p = COM_CheckParm("-bpp");
		if (p && p < com_argc-1)
			bpp = Q_atoi(com_argv[p+1]);

		if (COM_CheckParm("-window") || COM_CheckParm("-w"))
			fullscreen = false;
		else if (COM_CheckParm("-fullscreen") || COM_CheckParm("-f"))
			fullscreen = true;
	}

	p = COM_CheckParm ("-fsaa");
	if (p && p < com_argc-1)
		fsaa = atoi(com_argv[p+1]);

	if (!VID_ValidMode(width, height, refreshrate, bpp, fullscreen))
	{
		width = (int)vid_width.value;
		height = (int)vid_height.value;
		refreshrate = (int)vid_refreshrate.value;
		bpp = (int)vid_bpp.value;
		fullscreen = (int)vid_fullscreen.value;
	}

	if (!VID_ValidMode(width, height, refreshrate, bpp, fullscreen))
	{
		width = 640;
		height = 480;
		refreshrate = display_refreshrate;
		bpp = display_bpp;
		fullscreen = false;
	}

	vid_initialized = true;

	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	VID_SetMode (width, height, refreshrate, bpp, fullscreen);
	VID_ApplyVSync ();

	PL_SetWindowIcon();

	GL_Init ();
	GL_SetupState ();
	Cmd_AddCommand ("gl_info", GL_Info_f); //johnfitz

	//johnfitz -- removed code creating "glquake" subdirectory

	vid_menucmdfn = VID_Menu_f; //johnfitz
	vid_menudrawfn = VID_MenuDraw;
	vid_menukeyfn = VID_MenuKey;
	vid_menumousefn = VID_MenuMouse;

	VID_Gamma_Init(); //johnfitz
	VID_Menu_Init(); //johnfitz

	//QuakeSpasm: current vid settings should override config file settings.
	//so we have to lock the vid mode from now until after all config files are read.
	vid_locked = true;
}

// new proc by S.A., called by alt-return key binding.
void	VID_Toggle (void)
{
	// disabling the fast path completely because SDL_SetWindowFullscreen was changing
	// the window size on SDL2/WinXP and we weren't set up to handle it. --ericw
	//
	// TODO: Clear out the dead code, reinstate the fast path using SDL_SetWindowFullscreen
	// inside VID_SetMode, check window size to fix WinXP issue. This will
	// keep all the mode changing code in one place.
	static qboolean vid_toggle_works = false;
	qboolean toggleWorked;
	Uint32 flags = 0;

	S_ClearBuffer ();

	if (!vid_toggle_works)
		goto vrestart;

	if (!VID_GetFullscreen()) {
		flags = vid_desktopfullscreen.value ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
	}

	toggleWorked = SDL_SetWindowFullscreen(draw_context, flags) == 0;

	if (toggleWorked)
	{
		Sbar_Changed ();	// Sbar seems to need refreshing

		modestate = VID_GetFullscreen() ? MS_FULLSCREEN : MS_WINDOWED;

		VID_SyncCvars();

		// update mouse grab
		if (key_dest == key_console)
			IN_DeactivateForConsole();
		else if (key_dest == key_menu)
			IN_DeactivateForMenu();
	}
	else
	{
		vid_toggle_works = false;
		Con_DPrintf ("SDL_WM_ToggleFullScreen failed, attempting VID_Restart\n");
	vrestart:
		Cvar_SetQuick (&vid_fullscreen, VID_GetFullscreen() ? "0" : "1");
		Cbuf_AddText ("vid_restart\n");
	}
}

/*
================
VID_SyncCvars -- johnfitz -- set vid cvars to match current video mode
================
*/
void VID_SyncCvars (void)
{
	if (draw_context)
	{
		if (!VID_GetDesktopFullscreen())
		{
			Cvar_SetValueQuick (&vid_width, VID_GetCurrentWidth());
			Cvar_SetValueQuick (&vid_height, VID_GetCurrentHeight());
		}
		Cvar_SetValueQuick (&vid_refreshrate, VID_GetCurrentRefreshRate());
		Cvar_SetValueQuick (&vid_bpp, VID_GetCurrentBPP());
		Cvar_SetQuick (&vid_fullscreen, VID_GetFullscreen() ? "1" : "0");
		// don't sync vid_desktopfullscreen, it's a user preference that
		// should persist even if we are in windowed mode.
	}

	vid_changed = false;
}

//==========================================================================
//
//  NEW VIDEO MENU -- johnfitz
//
//==========================================================================

enum {
	VID_OPT_MODE,
	VID_OPT_BPP,
	VID_OPT_REFRESHRATE,
	VID_OPT_FULLSCREEN,
	VID_OPT_VSYNC,
	VID_OPT_FSAA,
	VID_OPT_FSAA_MODE,
	VID_OPT_SCALE,
	VID_OPT_ANISO,
	VID_OPT_TEXFILTER,
	VID_OPT_PARTICLES,
	VID_OPT_WATERWARP,
	VID_OPT_DLIGHTS,
	VID_OPT_SOFTEMU,
	VID_OPT_FPSLIMIT,
	VID_OPT_SHOWFPS,
	VID_OPT_TEST,
	VID_OPT_APPLY,
	VIDEO_OPTIONS_ITEMS
};

static int	video_options_cursor = 0;

typedef struct {
	int width,height;
} vid_menu_mode;

//TODO: replace these fixed-length arrays with hunk_allocated buffers
static vid_menu_mode vid_menu_modes[MAX_MODE_LIST];
static int vid_menu_nummodes = 0;

static int vid_menu_bpps[MAX_BPPS_LIST];
static int vid_menu_numbpps = 0;

static int vid_menu_rates[MAX_RATES_LIST];
static int vid_menu_numrates=0;

/*
================
VID_Menu_Init
================
*/
static void VID_Menu_Init (void)
{
	int i, j, h, w;

	for (i = 0; i < nummodes; i++)
	{
		w = modelist[i].width;
		h = modelist[i].height;

		for (j = 0; j < vid_menu_nummodes; j++)
		{
			if (vid_menu_modes[j].width == w &&
				vid_menu_modes[j].height == h)
				break;
		}

		if (j == vid_menu_nummodes)
		{
			vid_menu_modes[j].width = w;
			vid_menu_modes[j].height = h;
			vid_menu_nummodes++;
		}
	}
}

/*
================
VID_Menu_RebuildBppList

regenerates bpp list based on current vid_width and vid_height
================
*/
static void VID_Menu_RebuildBppList (void)
{
	int i, j, b;

	vid_menu_numbpps = 0;

	for (i = 0; i < nummodes; i++)
	{
		if (vid_menu_numbpps >= MAX_BPPS_LIST)
			break;

		//bpp list is limited to bpps available with current width/height
		if (modelist[i].width != vid_width.value ||
			modelist[i].height != vid_height.value)
			continue;

		b = modelist[i].bpp;

		for (j = 0; j < vid_menu_numbpps; j++)
		{
			if (vid_menu_bpps[j] == b)
				break;
		}

		if (j == vid_menu_numbpps)
		{
			vid_menu_bpps[j] = b;
			vid_menu_numbpps++;
		}
	}

	//if there are no valid fullscreen bpps for this width/height, just pick one
	if (vid_menu_numbpps == 0)
	{
		Cvar_SetValueQuick (&vid_bpp, (float)modelist[0].bpp);
		return;
	}

	//if vid_bpp is not in the new list, change vid_bpp
	for (i = 0; i < vid_menu_numbpps; i++)
		if (vid_menu_bpps[i] == (int)(vid_bpp.value))
			break;

	if (i == vid_menu_numbpps)
		Cvar_SetValueQuick (&vid_bpp, (float)vid_menu_bpps[0]);
}

/*
================
VID_Menu_RebuildRateList

regenerates rate list based on current vid_width, vid_height and vid_bpp
================
*/
static void VID_Menu_RebuildRateList (void)
{
	int i, j, r;

	vid_menu_numrates = 0;

	for (i = 0; i < nummodes; i++)
	{
		//rate list is limited to rates available with current width/height/bpp
		if (modelist[i].width != vid_width.value ||
		    modelist[i].height != vid_height.value ||
		    modelist[i].bpp != vid_bpp.value)
			continue;

		r = modelist[i].refreshrate;

		for (j = 0; j < vid_menu_numrates; j++)
		{
			if (vid_menu_rates[j] == r)
				break;
		}

		if (j == vid_menu_numrates)
		{
			vid_menu_rates[j] = r;
			vid_menu_numrates++;
		}
	}

	//if there are no valid fullscreen refreshrates for this width/height, just pick one
	if (vid_menu_numrates == 0)
	{
		Cvar_SetValue ("vid_refreshrate",(float)modelist[0].refreshrate);
		return;
	}

	//if vid_refreshrate is not in the new list, change vid_refreshrate
	for (i = 0; i < vid_menu_numrates; i++)
		if (vid_menu_rates[i] == (int)(vid_refreshrate.value))
			break;

	if (i == vid_menu_numrates)
		Cvar_SetValue ("vid_refreshrate",(float)vid_menu_rates[0]);
}

/*
================
VID_Menu_ChooseNextMode

chooses next resolution in order, then updates vid_width and
vid_height cvars, then updates bpp and refreshrate lists
================
*/
static void VID_Menu_ChooseNextMode (int dir)
{
	int i;

	if (vid_menu_nummodes)
	{
		for (i = 0; i < vid_menu_nummodes; i++)
		{
			if (vid_menu_modes[i].width == vid_width.value &&
				vid_menu_modes[i].height == vid_height.value)
				break;
		}

		if (i == vid_menu_nummodes) //can't find it in list, so it must be a custom windowed res
		{
			i = 0;
		}
		else
		{
			i += dir;
			if (i >= vid_menu_nummodes)
				i = 0;
			else if (i < 0)
				i = vid_menu_nummodes-1;
		}

		Cvar_SetValueQuick (&vid_width, (float)vid_menu_modes[i].width);
		Cvar_SetValueQuick (&vid_height, (float)vid_menu_modes[i].height);
		VID_Menu_RebuildBppList ();
		VID_Menu_RebuildRateList ();
	}
}

/*
================
VID_Menu_ChooseNextBpp

chooses next bpp in order, then updates vid_bpp cvar
================
*/
static void VID_Menu_ChooseNextBpp (int dir)
{
	int i;

	if (vid_menu_numbpps)
	{
		for (i = 0; i < vid_menu_numbpps; i++)
		{
			if (vid_menu_bpps[i] == vid_bpp.value)
				break;
		}

		if (i == vid_menu_numbpps) //can't find it in list
		{
			i = 0;
		}
		else
		{
			i += dir;
			if (i >= vid_menu_numbpps)
				i = 0;
			else if (i < 0)
				i = vid_menu_numbpps-1;
		}

		Cvar_SetValueQuick (&vid_bpp, (float)vid_menu_bpps[i]);
	}
}

/*
================
VID_Menu_ChooseNextRate

chooses next refresh rate in order, then updates vid_refreshrate cvar
================
*/
static void VID_Menu_ChooseNextRate (int dir)
{
	int i;

	for (i = 0; i < vid_menu_numrates; i++)
	{
		if (vid_menu_rates[i] == vid_refreshrate.value)
			break;
	}

	if (i == vid_menu_numrates) //can't find it in list
	{
		i = 0;
	}
	else
	{
		i += dir;
		if (i >= vid_menu_numrates)
			i = 0;
		else if (i < 0)
			i = vid_menu_numrates-1;
	}

	Cvar_SetValue ("vid_refreshrate",(float)vid_menu_rates[i]);
}

/*
================
VID_Menu_ChooseNextAA

chooses next AA level in order, then updates vid_fsaa cvar
================
*/
static void VID_Menu_ChooseNextAA (int dir)
{
	int samples = Q_nextPow2 (framebufs.scene.samples);

	if (dir < 0)
	{
		samples <<= 1;
		if (samples > framebufs.max_samples)
			samples = 1;
	}
	else
	{
		samples >>= 1;
		if (samples < 1)
			samples = framebufs.max_samples;
	}

	Cvar_SetValueQuick (&vid_fsaa, CLAMP (1, samples, framebufs.max_samples));
}

/*
================
VID_Menu_ChooseNextAnisotropy

chooses next anisotropy level in order, then updates gl_texture_anisotropy cvar
================
*/
static void VID_Menu_ChooseNextAnisotropy (int dir)
{
	int aniso = Q_nextPow2 (q_max (1, (int)gl_texture_anisotropy.value));

	if (dir < 0)
	{
		aniso <<= 1;
		if (aniso > gl_max_anisotropy)
			aniso = 1;
	}
	else
	{
		aniso >>= 1;
		if (aniso < 1)
			aniso = gl_max_anisotropy;
	}

	Cvar_SetValueQuick (&gl_texture_anisotropy, CLAMP (1, aniso, gl_max_anisotropy));
}

/*
================
VID_Menu_ChooseNextScale

chooses next scale in order, then updates r_scale cvar
================
*/
static void VID_Menu_ChooseNextScale (int dir)
{
	// cycle [1..vid_maxscale]
	int scale = 1 + (r_refdef.scale - 1 + vid.maxscale - dir) % vid.maxscale;
	Cvar_SetValueQuick (&r_scale, scale);
}

static const char *const texfilters[][2] =
{
	{"gl_nearest_mipmap_linear", "Classic"},
	{"gl_linear_mipmap_linear", "Smooth"},
};

/*
================
VID_Menu_ChooseNextTexFilter

chooses next texture filter, then updates gl_texturemode cvar
================
*/
static void VID_Menu_ChooseNextTexFilter (void)
{
	const char *filter = gl_texturemode.string;
	int i;

	for (i = 0; i < countof (texfilters); i++)
	{
		if (!q_strcasecmp (filter, texfilters[i][0]))
		{
			filter = texfilters[(i + 1) % countof (texfilters)][0];
			break;
		}
	}
	if (i == countof (texfilters))
		filter = texfilters[0][0];

	Cvar_SetQuick (&gl_texturemode, filter);
}

/*
================
VID_Menu_GetTexFilterDesc
================
*/
static const char *VID_Menu_GetTexFilterDesc (void)
{
	const char *current = Cvar_VariableString ("gl_texturemode");
	int i;
	for (i = 0; i < countof (texfilters); i++)
		if (!q_strcasecmp (current, texfilters[i][0]))
			return texfilters[i][1];
	return "";
}

/*
================
VID_Menu_ChooseNextFPSLimit

chooses next fps limit in order, then updates host_maxfps cvar
================
*/
static void VID_Menu_ChooseNextFPSLimit (int dir)
{
	static const int values[] = {0, 60, 72, 100, 120, 144, 165, 180, 200, 240, 300, 360, 500};
	int i, current = (int)host_maxfps.value;

	if (dir < 0)
		for (i = 0; i < countof (values) && values[i] <= current; i++)
			;
	else
		for (i = countof (values) - 1; i >= 0 && values[i] >= current; i--)
			;

	if (i < 0)
		i = countof (values) - 1;
	else if (i == countof (values))
		i = 0;

	Cvar_SetValueQuick (&host_maxfps, values[i]);
}

/*
================
VID_Menu_GetSoftEmuDesc
================
*/
static const char *VID_Menu_GetSoftEmuDesc (void)
{
	switch (softemu)
	{
	case SOFTEMU_BANDED: return "Raw";
	case SOFTEMU_COARSE: return "Balanced";
	case SOFTEMU_FINE: return "Subtle";
	case SOFTEMU_OFF: return "Off";
	default: return "";
	}
}

/*
================
VID_Menu_GetWaterWarpDesc
================
*/
static const char *VID_Menu_GetWaterWarpDesc (void)
{
	switch ((int)r_waterwarp.value)
	{
	case 0: return "Off";
	case 1: return "Classic";
	case 2: return "glQuake";
	default: return "";
	}
}

/*
================
VID_Menu_GetParticlesDesc
================
*/
static const char *VID_Menu_GetParticlesDesc (void)
{
	switch ((int)r_particles.value)
	{
	case 0: return "Off";
	case 1: return "glQuake";
	case 2: return "Classic";
	default: return "";
	}
}

/*
================
VID_MenuKey
================
*/
static void VID_MenuKey (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		VID_SyncCvars (); //sync cvars before leaving menu. FIXME: there are other ways to leave menu
		S_LocalSound ("misc/menu1.wav");
		M_Menu_Options_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor--;
		if (video_options_cursor < 0)
			video_options_cursor = VIDEO_OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		video_options_cursor++;
		if (video_options_cursor >= VIDEO_OPTIONS_ITEMS)
			video_options_cursor = 0;
		break;

	case K_LEFTARROW:
	//case K_MOUSE2:
	case K_MWHEELDOWN:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case VID_OPT_MODE:
			VID_Menu_ChooseNextMode (1);
			break;
		case VID_OPT_BPP:
			VID_Menu_ChooseNextBpp (1);
			break;
		case VID_OPT_REFRESHRATE:
			VID_Menu_ChooseNextRate (1);
			break;
		case VID_OPT_FULLSCREEN:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case VID_OPT_VSYNC:
			Cbuf_AddText ("toggle vid_vsync\n"); // kristian
			break;
		case VID_OPT_FSAA:
			VID_Menu_ChooseNextAA (1);
			break;
		case VID_OPT_FSAA_MODE:
			Cbuf_AddText ("toggle vid_fsaamode\n");
			break;
		case VID_OPT_SCALE:
			VID_Menu_ChooseNextScale (1);
			break;
		case VID_OPT_ANISO:
			VID_Menu_ChooseNextAnisotropy (1);
			break;
		case VID_OPT_TEXFILTER:
			VID_Menu_ChooseNextTexFilter ();
			break;
		case VID_OPT_PARTICLES:
			Cbuf_AddText ("cycle r_particles 1 2 0\n");
			break;
		case VID_OPT_WATERWARP:
			Cbuf_AddText ("cycle r_waterwarp 2 1 0\n");
			break;
		case VID_OPT_DLIGHTS:
			Cbuf_AddText ("toggle r_dynamic\n");
			break;
		case VID_OPT_SOFTEMU:
			Cbuf_AddText ("cycle r_softemu 3 2 1 0\n");
			break;
		case VID_OPT_FPSLIMIT:
			VID_Menu_ChooseNextFPSLimit (1);
			break;
		case VID_OPT_SHOWFPS:
			Cbuf_AddText ("toggle scr_showfps\n");
			break;
		default:
			break;
		}
		break;

	case K_RIGHTARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu3.wav");
		switch (video_options_cursor)
		{
		case VID_OPT_MODE:
			VID_Menu_ChooseNextMode (-1);
			break;
		case VID_OPT_BPP:
			VID_Menu_ChooseNextBpp (-1);
			break;
		case VID_OPT_REFRESHRATE:
			VID_Menu_ChooseNextRate (-1);
			break;
		case VID_OPT_FULLSCREEN:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case VID_OPT_VSYNC:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		case VID_OPT_FSAA:
			VID_Menu_ChooseNextAA (-1);
			break;
		case VID_OPT_FSAA_MODE:
			Cbuf_AddText ("toggle vid_fsaamode\n");
			break;
		case VID_OPT_SCALE:
			VID_Menu_ChooseNextScale (-1);
			break;
		case VID_OPT_ANISO:
			VID_Menu_ChooseNextAnisotropy (-1);
			break;
		case VID_OPT_TEXFILTER:
			VID_Menu_ChooseNextTexFilter ();
			break;
		case VID_OPT_PARTICLES:
			Cbuf_AddText ("cycle r_particles 0 2 1\n");
			break;
		case VID_OPT_WATERWARP:
			Cbuf_AddText ("cycle r_waterwarp 0 1 2\n");
			break;
		case VID_OPT_DLIGHTS:
			Cbuf_AddText ("toggle r_dynamic\n");
			break;
		case VID_OPT_SOFTEMU:
			Cbuf_AddText ("cycle r_softemu 0 1 2 3\n");
			break;
		case VID_OPT_FPSLIMIT:
			VID_Menu_ChooseNextFPSLimit (-1);
			break;
		case VID_OPT_SHOWFPS:
			Cbuf_AddText ("toggle scr_showfps\n");
			break;
		default:
			break;
		}
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		m_entersound = true;
		switch (video_options_cursor)
		{
		case VID_OPT_MODE:
			VID_Menu_ChooseNextMode (1);
			break;
		case VID_OPT_BPP:
			VID_Menu_ChooseNextBpp (1);
			break;
		case VID_OPT_REFRESHRATE:
			VID_Menu_ChooseNextRate (1);
			break;
		case VID_OPT_FULLSCREEN:
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case VID_OPT_VSYNC:
			Cbuf_AddText ("toggle vid_vsync\n");
			break;
		case VID_OPT_FSAA:
			VID_Menu_ChooseNextAA (-1);
			break;
		case VID_OPT_FSAA_MODE:
			Cbuf_AddText ("toggle vid_fsaamode\n");
			break;
		case VID_OPT_SCALE:
			VID_Menu_ChooseNextScale (-1);
			break;
		case VID_OPT_ANISO:
			VID_Menu_ChooseNextAnisotropy (-1);
			break;
		case VID_OPT_TEXFILTER:
			VID_Menu_ChooseNextTexFilter ();
			break;
		case VID_OPT_PARTICLES:
			Cbuf_AddText ("cycle r_particles 0 2 1\n");
			break;
		case VID_OPT_WATERWARP:
			Cbuf_AddText ("cycle r_waterwarp 0 1 2\n");
			break;
		case VID_OPT_DLIGHTS:
			Cbuf_AddText ("toggle r_dynamic\n");
			break;
		case VID_OPT_SOFTEMU:
			Cbuf_AddText ("cycle r_softemu 0 1 2 3\n");
			break;
		case VID_OPT_FPSLIMIT:
			VID_Menu_ChooseNextFPSLimit (-1);
			break;
		case VID_OPT_SHOWFPS:
			Cbuf_AddText ("toggle scr_showfps\n");
			break;
		case VID_OPT_TEST:
			Cbuf_AddText ("vid_test\n");
			break;
		case VID_OPT_APPLY:
			Cbuf_AddText ("vid_restart\n");
			key_dest = key_game;
			m_state = m_none;
			IN_Activate();
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

/*
================
VID_MenuMouse
================
*/
static void VID_MenuMouse (int cx, int cy)
{
	int cursor = (cy - 48) / 8;
	// Handle the visual gap between the last option and "Test changes"
	if (cursor > VID_OPT_TEST)
		--cursor; // past the gap, correct the index
	else if (cursor == VID_OPT_TEST)
		return; // inside the gap, do nothing
	cursor = CLAMP (0, cursor, VIDEO_OPTIONS_ITEMS - 1);
	video_options_cursor = cursor;
}

/*
================
VID_MenuDraw
================
*/
static void VID_MenuDraw (void)
{
	int i, x0, x1, y;
	qpic_t *p;
	const char *title;

	y = 4;

	// plaque
	p = Draw_CachePic ("gfx/qplaque.lmp");
	M_DrawTransPic (16, y, p);

	//p = Draw_CachePic ("gfx/vidmodes.lmp");
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, y, p);

	y += 28;

	// title
	title = "Video Options";
	M_PrintWhite ((320-8*strlen(title))/2, y, title);

	y += 16;

	x0 = 32;
	x1 = x0 + 168;

	// options
	for (i = 0; i < VIDEO_OPTIONS_ITEMS; i++)
	{
		switch (i)
		{
		case VID_OPT_MODE:
			M_Print (x0, y, "        Video mode");
			M_Print (x1, y, va("%ix%i", (int)vid_width.value, (int)vid_height.value));
			break;
		case VID_OPT_BPP:
			M_Print (x0, y, "       Color depth");
			M_Print (x1, y, va("%i", (int)vid_bpp.value));
			break;
		case VID_OPT_REFRESHRATE:
			M_Print (x0, y, "      Refresh rate");
			M_Print (x1, y, va("%i", (int)vid_refreshrate.value));
			break;
		case VID_OPT_FULLSCREEN:
			M_Print (x0, y, "        Fullscreen");
			M_DrawCheckbox (x1, y, (int)vid_fullscreen.value);
			break;
		case VID_OPT_VSYNC:
			M_Print (x0, y, "     Vertical sync");
			M_DrawCheckbox (x1, y, (int)vid_vsync.value);
			break;
		case VID_OPT_FSAA:
			M_Print (x0, y, "      Antialiasing");
			M_Print (x1, y, framebufs.scene.samples >= 2 ? va("%ix", framebufs.scene.samples) : "Off");
			break;
		case VID_OPT_FSAA_MODE:
			M_Print (x0, y, "           AA mode");
			M_Print (x1, y, vid_fsaamode.value ? "Full" : "Edges only");
			break;
		case VID_OPT_SCALE:
			M_Print (x0, y, "      Render scale");
			M_Print (x1, y, r_refdef.scale >= 2 ? va("1/%i", r_refdef.scale) : "Off");
			break;
		case VID_OPT_ANISO:
			M_Print (x0, y, "       Anisotropic");
			M_Print (x1, y, gl_texture_anisotropy.value >= 2.f ?
				va("%ix", q_min ((int)gl_texture_anisotropy.value, (int)gl_max_anisotropy)) :
				"Off"
			);
			break;
		case VID_OPT_TEXFILTER:
			M_Print (x0, y, "          Textures");
			M_Print (x1, y, VID_Menu_GetTexFilterDesc ());
			break;
		case VID_OPT_PARTICLES:
			M_Print (x0, y, "         Particles");
			M_Print (x1, y, VID_Menu_GetParticlesDesc ());
			break;
		case VID_OPT_WATERWARP:
			M_Print (x0, y, "     Underwater FX");
			M_Print (x1, y, VID_Menu_GetWaterWarpDesc ());
			break;
		case VID_OPT_DLIGHTS:
			M_Print (x0, y, "    Dynamic lights");
			M_Print (x1, y, r_dynamic.value ? "On" : "Off");
			break;
		case VID_OPT_SOFTEMU:
			M_Print (x0, y, "        8-bit mode");
			M_Print (x1, y, VID_Menu_GetSoftEmuDesc ());
			break;
		case VID_OPT_FPSLIMIT:
			M_Print (x0, y, "         FPS Limit");
			M_Print (x1, y, host_maxfps.value ? va("%i", (int)host_maxfps.value): "Off");
			break;
		case VID_OPT_SHOWFPS:
			M_Print (x0, y, "          Show FPS");
			M_Print (x1, y, scr_showfps.value ? "On" : "Off");
			break;
		case VID_OPT_TEST:
			y += 8; //separate the test and apply items
			M_Print (x0, y, "      Test changes");
			break;
		case VID_OPT_APPLY:
			M_Print (x0, y, "     Apply changes");
			break;
		}

		if (video_options_cursor == i)
			M_DrawCharacter (x1 - 16, y, 12+((int)(realtime*4)&1));

		y += 8;
	}
}

/*
================
VID_Menu_f
================
*/
static void VID_Menu_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;

	//set all the cvars to match the current mode when entering the menu
	VID_SyncCvars ();

	//set up bpp and rate lists based on current cvars
	VID_Menu_RebuildBppList ();
	VID_Menu_RebuildRateList ();
}


/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
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

#include "quakedef.h"
#include "bgmusic.h"
#include "q_ctype.h"

void (*vid_menucmdfn)(void); //johnfitz
void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);
void (*vid_menumousefn)(int cx, int cy);

extern cvar_t cl_mousemenu;
extern qboolean quake64;

enum m_state_e m_state;
extern qboolean	keydown[256];
int m_mousex, m_mousey;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
		void M_Menu_Skill_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_Setup_f (void);
		void M_Menu_Net_f (void);
		void M_Menu_LanConfig_f (void);
		void M_Menu_GameOptions_f (void);
		void M_Menu_Search_f (void);
		void M_Menu_ServerList_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_Mods_f (void);
	void M_Menu_Help_f (void);
	void M_Menu_Quit_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
		void M_Skill_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_Setup_Draw (void);
		void M_Net_Draw (void);
		void M_LanConfig_Draw (void);
		void M_GameOptions_Draw (void);
		void M_Search_Draw (void);
		void M_ServerList_Draw (void);
	void M_Options_Draw (void);
		void M_Keys_Draw (void);
		void M_Video_Draw (void);
	void M_Mods_Draw (void);
	void M_Help_Draw (void);
	void M_Quit_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
		void M_Skill_Key (int key);
	void M_MultiPlayer_Key (int key);
		void M_Setup_Key (int key);
		void M_Net_Key (int key);
		void M_LanConfig_Key (int key);
		void M_GameOptions_Key (int key);
		void M_Search_Key (int key);
		void M_ServerList_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
		void M_Video_Key (int key);
	void M_Mods_Key (int key);
	void M_Help_Key (int key);
	void M_Quit_Key (int key);

void M_Main_Mousemove (int cx, int cy);
	void M_SinglePlayer_Mousemove (int cx, int cy);
		void M_Load_Mousemove (int cx, int cy);
		void M_Save_Mousemove (int cx, int cy);
		void M_Skill_Mousemove (int cx, int cy);
	void M_MultiPlayer_Mousemove (int cx, int cy);
		void M_Setup_Mousemove (int cx, int cy);
		void M_Net_Mousemove (int cx, int cy);
		void M_LanConfig_Mousemove (int cx, int cy);
		void M_GameOptions_Mousemove (int cx, int cy);
		//void M_Search_Mousemove (int cx, int cy);
		void M_ServerList_Mousemove (int cx, int cy);
	void M_Options_Mousemove (int cx, int cy);
		void M_Keys_Mousemove (int cx, int cy);
		void M_Video_Mousemove (int cx, int cy);
	void M_Mods_Mousemove (int cx, int cy);
	//void M_Help_Mousemove (int cx, int cy);
	//void M_Quit_Mousemove (int cx, int cy);

qboolean M_Menu_Skill_IsSupported (void);

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound
qboolean	m_recursiveDraw;

enum m_state_e	m_return_state;
qboolean	m_return_onerror;
char		m_return_reason [32];

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)
#define	IPXConfig		(m_net_cursor == 0)
#define	TCPIPConfig		(m_net_cursor == 1)

void M_ConfigureNetSubsystem(void);

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character (cx, line, num);
}

void M_PrintEx (int cx, int cy, int dim, const char *str)
{
	while (*str)
	{
		Draw_CharacterEx (cx, cy, dim, dim, (*str)+128);
		str++;
		cx += dim;
	}
}

void M_Print (int cx, int cy, const char *str)
{
	M_PrintEx (cx, cy, 8, str);
}

void M_PrintWhiteEx (int cx, int cy, int dim, const char *str)
{
	while (*str)
	{
		Draw_CharacterEx (cx, cy, dim, dim, *str);
		str++;
		cx += dim;
	}
}

void M_PrintWhite (int cx, int cy, const char *str)
{
	M_PrintWhiteEx (cx, cy, 8, str);
}

void M_DrawTransPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x, y, pic); //johnfitz -- simplified becuase centering is handled elsewhere
}

void M_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x, y, pic); //johnfitz -- simplified becuase centering is handled elsewhere
}

void M_DrawSubpic (int x, int y, qpic_t *pic, int left, int top, int width, int height)
{
	float s1 = left   / (float)pic->width;
	float t1 = top    / (float)pic->height;
	float s2 = width  / (float)pic->width;
	float t2 = height / (float)pic->height;
	Draw_SubPic (x, y, width, height, pic, s1, t1, s2, t2, NULL, 1.f);
}

void M_DrawTransPicTranslate (int x, int y, qpic_t *pic, int top, int bottom) //johnfitz -- more parameters
{
	Draw_TransPicTranslate (x, y, pic, top, bottom); //johnfitz -- simplified becuase centering is handled elsewhere
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	qpic_t	*p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	M_DrawTransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	M_DrawTransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		M_DrawTransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			M_DrawTransPic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		M_DrawTransPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	M_DrawTransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	M_DrawTransPic (cx, cy+8, p);
}

void M_DrawQuakeBar (int x, int y, int cols)
{
	M_DrawCharacter (x, y, '\35');
	x += 8;
	cols -= 2;
	while (cols-- > 0)
	{
		M_DrawCharacter (x, y, '\36');
		x += 8;
	}
	M_DrawCharacter (x, y, '\37');
}

void M_DrawEllipsisBar (int x, int y, int cols)
{
	while (cols > 0)
	{
		M_DrawCharacter (x, y, '.' | 128);
		cols -= 2;
		x += 16;
	}
}

//=============================================================================
/* Mouse helpers */

qboolean M_IsMouseKey (int key)
{
	switch (key)
	{
	case K_MOUSE1:
	case K_MOUSE2:
	case K_MOUSE3:
	case K_MOUSE4:
	case K_MOUSE5:
	case K_MWHEELUP:
	case K_MWHEELDOWN:
		return true;
	default:
		return false;
	}
}

void M_ForceMousemove (void)
{
	int x, y;
	SDL_GetMouseState (&x, &y);
	M_Mousemove (x, y);
}

void M_UpdateCursor (int mousey, int starty, int itemheight, int numitems, int *cursor)
{
	int pos = (mousey - starty) / itemheight;
	if (pos > numitems - 1)
		pos = numitems - 1;
	if (pos < 0)
		pos = 0;
	*cursor = pos;
}

void M_UpdateCursorWithTable (int mousey, const int *table, int numitems, int *cursor)
{
	int i, dy;
	for (i = 0; i < numitems; i++)
	{
		dy = mousey - table[i];
		if (dy >= 0 && dy < 8)
		{
			*cursor = i;
			break;
		}
	}
}

//=============================================================================
/* Listbox */

typedef struct
{
	int			cursor;
	int			numitems;
	int			viewsize;
	int			scroll;
} menulist_t;

void M_List_CheckIntegrity (const menulist_t *list)
{
	SDL_assert (list->numitems >= 0);
	SDL_assert (list->cursor >= 0);
	SDL_assert (list->cursor < list->numitems);
	SDL_assert (list->scroll >= 0);
	SDL_assert (list->scroll < list->numitems);
	SDL_assert (list->viewsize > 0);
}

void M_List_AutoScroll (menulist_t *list)
{
	if (list->numitems <= list->viewsize)
		return;
	if (list->cursor < list->scroll + 1)
		list->scroll = list->cursor;
	else if (list->cursor >= list->scroll + list->viewsize)
		list->scroll = list->cursor - list->viewsize + 1;
}

void M_List_CenterCursor (menulist_t *list)
{
	if (list->cursor >= list->viewsize)
	{
		if (list->cursor + list->viewsize >= list->numitems)
			list->scroll = list->numitems - list->viewsize; // last page, scroll to the end
		else
			list->scroll = list->cursor - list->viewsize / 2; // keep centered
		list->scroll = CLAMP (0, list->scroll, list->numitems - list->viewsize);
	}
	else
		list->scroll = 0;
}

int M_List_GetOverflow (const menulist_t *list)
{
	return list->numitems - list->viewsize;
}

// Note: y is in pixels, height is in chars!
qboolean M_List_GetScrollbar (const menulist_t *list, int *y, int *height)
{
	if (list->numitems <= list->viewsize)
	{
		*y = *height = 0;
		return false;
	}

	*height = (int)(list->viewsize * list->viewsize / (float)list->numitems + 0.5f);
	*height = q_max (*height, 2);
	*y = (int)(list->scroll * 8 / (float)(list->numitems - list->viewsize) * (list->viewsize - *height) + 0.5f);

	return true;
}

void M_List_DrawScrollbar (const menulist_t *list, int cx, int cy)
{
	int y, h;
	if (!M_List_GetScrollbar (list, &y, &h))
		return;
	M_DrawTextBox (cx - 4, cy + y - 4, 0, h - 1);
}

qboolean M_List_UseScrollbar (menulist_t *list, int yrel)
{
	int scrolly, scrollh, range;
	if (!M_List_GetScrollbar (list, &scrolly, &scrollh))
		return false;

	yrel -= scrollh * 4; // half the thumb height, in pixels
	range = (list->viewsize - scrollh) * 8;
	list->scroll = (int)(yrel * (float)(list->numitems - list->viewsize) / range + 0.5f);

	if (list->scroll > list->numitems - list->viewsize)
		list->scroll = list->numitems - list->viewsize;
	if (list->scroll < 0)
		list->scroll = 0;

	return true;
}

void M_List_GetVisibleRange (const menulist_t *list, int *first, int *count)
{
	*first = list->scroll;
	*count = q_min (list->scroll + list->viewsize, list->numitems) - list->scroll;
}

qboolean M_List_IsItemVisible (const menulist_t *list, int i)
{
	int first, count;
	M_List_GetVisibleRange (list, &first, &count);
	return (unsigned)(i - first) < (unsigned)count;
}

qboolean M_List_Key (menulist_t *list, int key)
{
	switch (key)
	{
	case K_HOME:
	case K_KP_HOME:
		S_LocalSound ("misc/menu1.wav");
		list->cursor = 0;
		M_List_AutoScroll (list);
		return true;

	case K_END:
	case K_KP_END:
		S_LocalSound ("misc/menu1.wav");
		list->cursor = list->numitems - 1;
		M_List_AutoScroll (list);
		return true;

	case K_PGDN:
	case K_KP_PGDN:
		S_LocalSound ("misc/menu1.wav");
		if (list->cursor - list->scroll < list->viewsize - 1)
			list->cursor = list->scroll + list->viewsize - 1;
		else
			list->cursor += list->viewsize - 1;
		list->cursor = q_min (list->cursor, list->numitems - 1);
		M_List_AutoScroll (list);
		return true;

	case K_PGUP:
	case K_KP_PGUP:
		S_LocalSound ("misc/menu1.wav");
		if (list->cursor > list->scroll)
			list->cursor = list->scroll;
		else
			list->cursor -= list->viewsize - 1;
		list->cursor = q_max (list->cursor, 0);
		M_List_AutoScroll (list);
		return true;

	case K_UPARROW:
	case K_KP_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--list->cursor < 0)
			list->cursor = list->numitems - 1;
		M_List_AutoScroll (list);
		return true;

	case K_MWHEELUP:
		list->scroll -= 3;
		if (list->scroll < 0)
			list->scroll = 0;
		M_ForceMousemove ();
		return true;

	case K_MWHEELDOWN:
		list->scroll += 3;
		if (list->scroll > list->numitems - list->viewsize)
			list->scroll = list->numitems - list->viewsize;
		if (list->scroll < 0)
			list->scroll = 0;
		M_ForceMousemove ();
		return true;

	case K_DOWNARROW:
	case K_KP_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++list->cursor >= list->numitems)
			list->cursor = 0;
		M_List_AutoScroll (list);
		return true;

	default:
		return false;
	}
}

qboolean M_List_CycleMatch (menulist_t *list, int key, qboolean (*match_fn) (int idx, char c))
{
	int i, j, dir;

	if (!(key >= 'a' && key <= 'z') &&
		!(key >= 'A' && key <= 'Z') &&
		!(key >= '0' && key <= '9'))
		return false;

	if (list->numitems <= 0)
		return false;

	S_LocalSound ("misc/menu1.wav");

	key = q_tolower (key);
	dir = keydown[K_SHIFT] ? -1 : 1;

	for (i = 1, j = list->cursor + dir; i < list->numitems; i++, j += dir)
	{
		j = (j + list->numitems) % list->numitems; // avoid negative mod
		if (match_fn (j, (char)key))
		{
			list->cursor = j;
			M_List_AutoScroll (list);
			break;
		}
	}

	return true;
}

void M_List_Mousemove (menulist_t *list, int yrel)
{
	int firstvis, numvis;
	M_List_GetVisibleRange (list, &firstvis, &numvis);
	if (!numvis)
		return;
	yrel /= 8;
	if (yrel < 0)
		return;
	if (yrel >= numvis)
		return;
	yrel += firstvis;
	if (list->cursor == yrel)
		return;
	list->cursor = yrel;
}


//=============================================================================

int m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}

		IN_Activate();
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	if (key_dest == key_console)
	{
		Con_ToggleConsole_f ();
	}
	else
	{
		M_Menu_Main_f ();
	}
}


//=============================================================================
/* MAIN MENU */

int	m_main_cursor;
int m_main_mods;

enum
{
	MAIN_SINGLEPLAYER,
	MAIN_MULTIPLAYER,
	MAIN_OPTIONS,
	MAIN_MODS,
	MAIN_HELP,
	MAIN_QUIT,

	MAIN_ITEMS,
};

void M_Menu_Main_f (void)
{
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;

	// When switching to a mod with a custom UI the 'Mods' option
	// is no longer available in the main menu, so we move the cursor
	// to 'Options' to nudge the player toward the secondary location.
	// TODO (maybe): inform the user about the missing option
	// and its alternative location?
	if (!m_main_mods && m_main_cursor == MAIN_MODS)
	{
		extern int options_cursor;
		m_main_cursor = MAIN_OPTIONS;
		options_cursor = 1; // OPT_MODS
	}
}


void M_Main_Draw (void)
{
	int		cursor, f;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	p = Draw_CachePic ("gfx/mainmenu.lmp");
	if (m_main_mods)
	{
		int split = 60;
		M_DrawSubpic (72, 32, p, 0, 0, p->width, split);
		if (m_main_mods > 0)
			M_DrawTransPic (72, 32 + split, Draw_CachePic ("gfx/menumods.lmp"));
		else
			M_PrintEx (74, 32 + split + 1, 16, "MODS");
		M_DrawSubpic (72, 32 + split + 20, p, 0, split, p->width, p->height - split);
	}
	else
		M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mainmenu.lmp"));

	f = (int)(realtime * 10)%6;
	cursor = m_main_cursor;
	if (!m_main_mods && cursor > MAIN_MODS)
		--cursor;
	M_DrawTransPic (54, 32 + cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}


void M_Main_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		IN_Activate();
		key_dest = key_game;
		m_state = m_none;
		cls.demonum = m_save_demonum;
		if (!fitzmode && !cl_startdemos.value)	/* QuakeSpasm customization: */
			break;
		if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
			CL_NextDemo ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		else if (!m_main_mods && m_main_cursor == MAIN_MODS)
			++m_main_cursor;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		else if (!m_main_mods && m_main_cursor == MAIN_MODS)
			--m_main_cursor;
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case MAIN_SINGLEPLAYER:
			M_Menu_SinglePlayer_f ();
			break;

		case MAIN_MULTIPLAYER:
			M_Menu_MultiPlayer_f ();
			break;

		case MAIN_OPTIONS:
			M_Menu_Options_f ();
			break;

		case MAIN_HELP:
			M_Menu_Help_f ();
			break;

		case MAIN_MODS:
			M_Menu_Mods_f ();
			break;

		case MAIN_QUIT:
			M_Menu_Quit_f ();
			break;
		}
	}
}

void M_Main_Mousemove (int cx, int cy)
{
	M_UpdateCursor (cy, 32, 20, MAIN_ITEMS - !m_main_mods, &m_main_cursor);
	if (m_main_cursor >= MAIN_MODS && !m_main_mods)
		++m_main_cursor;
}

//=============================================================================

void M_CheckMods (void)
{
	unsigned int id_mods, id_main;
	int h, length;

	m_main_mods = 0;
	if (!COM_FileExists ("gfx/menumods.lmp", &id_mods))
		return;

	length = COM_OpenFile ("gfx/mainmenu.lmp", &h, &id_main);
	if (length == 26888)
	{
		int mark = Hunk_LowMark ();
		byte* data = (byte*) Hunk_Alloc (length);
		if (length == Sys_FileRead (h, data, length))
		{
			unsigned int hash = COM_HashBlock (data, length);
			if (hash == 0x136bc7fd || hash == 0x90555cb4)
				m_main_mods = 1;
		}
		Hunk_FreeToLowMark (mark);
	}

	if (id_mods >= id_main)
		m_main_mods = 1;

	COM_CloseFile (h);
}

//=============================================================================
/* SINGLE PLAYER MENU */

int	m_singleplayer_cursor;
#define	SINGLEPLAYER_ITEMS	3


void M_Menu_SinglePlayer_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
}


void M_SinglePlayer_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp") );

	f = (int)(realtime * 10)%6;

	M_DrawTransPic (54, 32 + m_singleplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}


void M_SinglePlayer_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		m_entersound = true;

		switch (m_singleplayer_cursor)
		{
		case 0:
			if (sv.active)
				if (!SCR_ModalMessage("Are you sure you want to\nstart a new game?\n", 0.0f))
					break;
			if (quake64 && M_Menu_Skill_IsSupported ())
			{
				M_Menu_Skill_f ();
				break;
			}
			IN_Activate();
			key_dest = key_game;
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("maxplayers 1\n");
			Cbuf_AddText ("deathmatch 0\n"); //johnfitz
			Cbuf_AddText ("coop 0\n"); //johnfitz
			Cbuf_AddText ("map start\n");
			break;

		case 1:
			M_Menu_Load_f ();
			break;

		case 2:
			M_Menu_Save_f ();
			break;
		}
	}
}


void M_SinglePlayer_Mousemove (int cx, int cy)
{
	M_UpdateCursor (cy, 32, 20, SINGLEPLAYER_ITEMS, &m_singleplayer_cursor);
}

//=============================================================================
/* LOAD/SAVE MENU */

int		load_cursor;		// 0 < load_cursor < MAX_SAVEGAMES

#define	MAX_SAVEGAMES		20	/* johnfitz -- increased from 12 */
char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH+1];
int		loadable[MAX_SAVEGAMES];

void M_ScanSaves (void)
{
	int	i, j;
	char	name[MAX_OSPATH];
	FILE	*f;
	int	version;

	for (i = 0; i < MAX_SAVEGAMES; i++)
	{
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		q_snprintf (name, sizeof(name), "%s/s%i.sav", com_gamedir, i);
		f = Sys_fopen (name, "r");
		if (!f)
			continue;
		fscanf (f, "%i\n", &version);
		fscanf (f, "%79s\n", name);
		q_strlcpy (m_filenames[i], name, SAVEGAME_COMMENT_LENGTH+1);

	// change _ back to space
		for (j = 0; j < SAVEGAME_COMMENT_LENGTH; j++)
		{
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		}
		loadable[i] = true;
		fclose (f);
	}
}

void M_Menu_Load_f (void)
{
	m_entersound = true;
	m_state = m_load;

	IN_DeactivateForMenu();
	key_dest = key_menu;
	M_ScanSaves ();
}


void M_Menu_Save_f (void)
{
	if (!sv.active)
		return;
	if (cl.intermission)
		return;
	if (svs.maxclients != 1)
		return;
	m_entersound = true;
	m_state = m_save;

	IN_DeactivateForMenu();
	key_dest = key_menu;
	M_ScanSaves ();
}


void M_Load_Draw (void)
{
	int		i;
	qpic_t	*p;

	p = Draw_CachePic ("gfx/p_load.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Save_Draw (void)
{
	int		i;
	qpic_t	*p;

	p = Draw_CachePic ("gfx/p_save.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Load_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		IN_Activate();
		key_dest = key_game;

	// Host_Loadgame_f can't bring up the loading plaque because too much
	// stack space has been used, so do it now
		SCR_BeginLoadingPlaque ();

	// issue the load command
		Cbuf_AddText (va ("load s%i\n", load_cursor) );
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}


void M_Save_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_SinglePlayer_f ();
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		m_state = m_none;
		IN_Activate();
		key_dest = key_game;
		Cbuf_AddText (va("save s%i\n", load_cursor));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

void M_Load_Mousemove (int cx, int cy)
{
	M_UpdateCursor (cy, 32, 8, MAX_SAVEGAMES, &load_cursor);
}

void M_Save_Mousemove (int cx, int cy)
{
	M_UpdateCursor (cy, 32, 8, MAX_SAVEGAMES, &load_cursor);
}

//=============================================================================
/* SKILL MENU */

int m_skill_cursor;

void M_Menu_Skill_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_skill;
	m_entersound = true;
	m_skill_cursor = (int)skill.value;
	m_skill_cursor = CLAMP (0, m_skill_cursor, 3);
}

qboolean M_Menu_Skill_IsSupported (void)
{
	return COM_FileExists ("gfx/skillmenu.lmp", NULL);
}

void M_Skill_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/skillmenu.lmp") );

	f = (int)(realtime * 10)%6;

	M_DrawTransPic (54, 32 + m_skill_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}

void M_Skill_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_SinglePlayer_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_skill_cursor > 3)
			m_skill_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_skill_cursor < 0)
			m_skill_cursor = 3;
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		IN_Activate();
		key_dest = key_game;
		if (sv.active)
			Cbuf_AddText ("disconnect\n");
		Cbuf_AddText (va ("skill %d\n", m_skill_cursor));
		Cbuf_AddText ("maxplayers 1\n");
		Cbuf_AddText ("deathmatch 0\n"); //johnfitz
		Cbuf_AddText ("coop 0\n"); //johnfitz
		Cbuf_AddText ("map start\n");
		break;
	}
}

void M_Skill_Mousemove (int cx, int cy)
{
	M_UpdateCursor (cy, 32, 20, 4, &m_skill_cursor);
}

//=============================================================================
/* MULTIPLAYER MENU */

int	m_multiplayer_cursor;
#define	MULTIPLAYER_ITEMS	3


void M_Menu_MultiPlayer_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_multiplayer;
	m_entersound = true;
}


void M_MultiPlayer_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mp_menu.lmp") );

	f = (int)(realtime * 10)%6;

	M_DrawTransPic (54, 32 + m_multiplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );

	if (ipxAvailable || tcpipAvailable)
		return;
	M_PrintWhite ((320/2) - ((27*8)/2), 148, "No Communications Available");
}


void M_MultiPlayer_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
			if (ipxAvailable || tcpipAvailable)
				M_Menu_Net_f ();
			break;

		case 1:
			if (ipxAvailable || tcpipAvailable)
				M_Menu_Net_f ();
			break;

		case 2:
			M_Menu_Setup_f ();
			break;
		}
	}
}


void M_MultiPlayer_Mousemove (int cx, int cy)
{
	M_UpdateCursor (cy, 32, 20, MULTIPLAYER_ITEMS, &m_multiplayer_cursor);
}

//=============================================================================
/* SETUP MENU */

int		setup_cursor = 4;
int		setup_cursor_table[] = {40, 56, 80, 104, 140};

char	setup_hostname[16];
char	setup_myname[16];
int		setup_oldtop;
int		setup_oldbottom;
int		setup_top;
int		setup_bottom;

#define	NUM_SETUP_CMDS	5

void M_Menu_Setup_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;
	Q_strcpy(setup_myname, cl_name.string);
	Q_strcpy(setup_hostname, hostname.string);
	setup_top = setup_oldtop = ((int)cl_color.value) >> 4;
	setup_bottom = setup_oldbottom = ((int)cl_color.value) & 15;
}


void M_Setup_Draw (void)
{
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (64, 40, "Hostname");
	M_DrawTextBox (160, 32, 16, 1);
	M_Print (168, 40, setup_hostname);

	M_Print (64, 56, "Your name");
	M_DrawTextBox (160, 48, 16, 1);
	M_Print (168, 56, setup_myname);

	M_Print (64, 80, "Shirt color");
	M_Print (64, 104, "Pants color");

	M_DrawTextBox (64, 140-8, 14, 1);
	M_Print (72, 140, "Accept Changes");

	p = Draw_CachePic ("gfx/bigbox.lmp");
	M_DrawTransPic (160, 64, p);
	p = Draw_CachePic ("gfx/menuplyr.lmp");
	M_DrawTransPicTranslate (172, 72, p, setup_top, setup_bottom);

	M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1));

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_hostname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8*strlen(setup_myname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));
}


void M_Setup_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_LEFTARROW:
	//case K_MOUSE2:
	case K_MWHEELDOWN:
		if (setup_cursor < 2)
			return;
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top - 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom - 1;
		break;
	case K_RIGHTARROW:
	case K_MWHEELUP:
		if (setup_cursor < 2)
			return;
forward:
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top + 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom + 1;
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		if (setup_cursor == 0 || setup_cursor == 1)
			return;

		if (setup_cursor == 2 || setup_cursor == 3)
			goto forward;

		// setup_cursor == 4 (OK)
		if (Q_strcmp(cl_name.string, setup_myname) != 0)
			Cbuf_AddText ( va ("name \"%s\"\n", setup_myname) );
		if (Q_strcmp(hostname.string, setup_hostname) != 0)
			Cvar_Set("hostname", setup_hostname);
		if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
			Cbuf_AddText( va ("color %i %i\n", setup_top, setup_bottom) );
		m_entersound = true;
		M_Menu_MultiPlayer_f ();
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_hostname))
				setup_hostname[strlen(setup_hostname)-1] = 0;
		}

		if (setup_cursor == 1)
		{
			if (strlen(setup_myname))
				setup_myname[strlen(setup_myname)-1] = 0;
		}
		break;
	}

	if (setup_top > 13)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 13;
	if (setup_bottom > 13)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 13;
}


void M_Setup_Char (int k)
{
	int l;

	switch (setup_cursor)
	{
	case 0:
		l = strlen(setup_hostname);
		if (l < 15)
		{
			setup_hostname[l+1] = 0;
			setup_hostname[l] = k;
		}
		break;
	case 1:
		l = strlen(setup_myname);
		if (l < 15)
		{
			setup_myname[l+1] = 0;
			setup_myname[l] = k;
		}
		break;
	}
}


qboolean M_Setup_TextEntry (void)
{
	return (setup_cursor == 0 || setup_cursor == 1);
}


void M_Setup_Mousemove (int cx, int cy)
{
	M_UpdateCursorWithTable (cy, setup_cursor_table, NUM_SETUP_CMDS, &setup_cursor);
}

//=============================================================================
/* NET MENU */

int	m_net_cursor;
int m_net_items;

const char *net_helpMessage [] =
{
/* .........1.........2.... */
  " Novell network LANs    ",
  " or Windows 95 DOS-box. ",
  "                        ",
  "(LAN=Local Area Network)",

  " Commonly used to play  ",
  " over the Internet, but ",
  " also used on a Local   ",
  " Area Network.          "
};

void M_Menu_Net_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_net;
	m_entersound = true;
	m_net_items = 2;

	if (m_net_cursor >= m_net_items)
		m_net_cursor = 0;
	m_net_cursor--;
	M_Net_Key (K_DOWNARROW);
}


void M_Net_Draw (void)
{
	int		f;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	f = 32;

	if (ipxAvailable)
		p = Draw_CachePic ("gfx/netmen3.lmp");
	else
		p = Draw_CachePic ("gfx/dim_ipx.lmp");
	M_DrawTransPic (72, f, p);

	f += 19;
	if (tcpipAvailable)
		p = Draw_CachePic ("gfx/netmen4.lmp");
	else
		p = Draw_CachePic ("gfx/dim_tcp.lmp");
	M_DrawTransPic (72, f, p);

	f = (320-26*8)/2;
	M_DrawTextBox (f, 96, 24, 4);
	f += 8;
	M_Print (f, 104, net_helpMessage[m_net_cursor*4+0]);
	M_Print (f, 112, net_helpMessage[m_net_cursor*4+1]);
	M_Print (f, 120, net_helpMessage[m_net_cursor*4+2]);
	M_Print (f, 128, net_helpMessage[m_net_cursor*4+3]);

	f = (int)(realtime * 10)%6;
	M_DrawTransPic (54, 32 + m_net_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}


void M_Net_Key (int k)
{
again:
	switch (k)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_MultiPlayer_f ();
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_net_cursor >= m_net_items)
			m_net_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_net_cursor < 0)
			m_net_cursor = m_net_items - 1;
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		m_entersound = true;
		M_Menu_LanConfig_f ();
		break;
	}

	if (m_net_cursor == 0 && !ipxAvailable)
		goto again;
	if (m_net_cursor == 1 && !tcpipAvailable)
		goto again;
}


void M_Net_Mousemove (int cx, int cy)
{
	M_UpdateCursor (cy, 32, 20, m_net_items, &m_net_cursor);
	if (m_net_cursor == 0 && !ipxAvailable)
		m_net_cursor = 1;
	if (m_net_cursor == 1 && !tcpipAvailable)
		m_net_cursor = 0;
}

//=============================================================================
/* OPTIONS MENU */

enum
{
	OPT_CUSTOMIZE,
	OPT_MODS,
	OPT_CONSOLE,
	OPT_DEFAULTS,
	OPT_HUDSTYLE,
	OPT_SBALPHA,
	OPT_SCALE,
	OPT_SCRSIZE,
	OPT_GAMMA,
	OPT_CONTRAST,
	OPT_MOUSESPEED,
	OPT_SNDVOL,
	OPT_MUSICVOL,
	OPT_MUSICEXT,
	OPT_ALWAYRUN,
	OPT_INVMOUSE,
	OPT_ALWAYSMLOOK,
	OPT_LOOKSPRING,
	OPT_LOOKSTRAFE,
//#ifdef _WIN32
//	OPT_USEMOUSE,
//#endif
	OPT_VIDEO,	// This is the last before OPTIONS_ITEMS
	OPTIONS_ITEMS
};

enum
{
	ALWAYSRUN_OFF = 0,
	ALWAYSRUN_VANILLA,
	ALWAYSRUN_QUAKESPASM,
	ALWAYSRUN_ITEMS
};

#define	SLIDER_RANGE	10

int options_cursor;
qboolean slider_grab;
float target_scale_frac;

void M_Menu_Options_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_options;
	m_entersound = true;
	slider_grab = false;
}


void M_AdjustSliders (int dir)
{
	int	curr_alwaysrun, target_alwaysrun;
	float	f, l;

	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor)
	{
	case OPT_SCALE:	// console and menu scale
		l = ((vid.width + 31) / 32) / 10.0;
		f = scr_conscale.value + dir * .1;
		if (f < 1)	f = 1;
		else if(f > l)	f = l;
		Cvar_SetValue ("scr_conscale", f);
		Cvar_SetValue ("scr_menuscale", f);
		Cvar_SetValue ("scr_sbarscale", f);
		Cvar_SetValue ("scr_crosshairscale", f);
		break;
	case OPT_SCRSIZE:	// screen size
		f = scr_viewsize.value + dir * 10;
		if (f > 120)	f = 120;
		else if(f < 30)	f = 30;
		Cvar_SetValue ("viewsize", f);
		break;
	case OPT_GAMMA:	// gamma
		f = vid_gamma.value - dir * 0.05;
		if (f < 0.5)	f = 0.5;
		else if (f > 1)	f = 1;
		Cvar_SetValue ("gamma", f);
		break;
	case OPT_CONTRAST:	// contrast
		f = vid_contrast.value + dir * 0.1;
		if (f < 1)	f = 1;
		else if (f > 2)	f = 2;
		Cvar_SetValue ("contrast", f);
		break;
	case OPT_MOUSESPEED:	// mouse speed
		f = sensitivity.value + dir * 0.5;
		if (f > 11)	f = 11;
		else if (f < 1)	f = 1;
		Cvar_SetValue ("sensitivity", f);
		break;
	case OPT_SBALPHA:	// statusbar alpha
		f = scr_sbaralpha.value - dir * 0.05;
		if (f < 0)	f = 0;
		else if (f > 1)	f = 1;
		Cvar_SetValue ("scr_sbaralpha", f);
		break;
	case OPT_MUSICVOL:	// music volume
		f = bgmvolume.value + dir * 0.1;
		if (f < 0)	f = 0;
		else if (f > 1)	f = 1;
		Cvar_SetValue ("bgmvolume", f);
		break;
	case OPT_MUSICEXT:	// enable external music vs cdaudio
		Cvar_Set ("bgm_extmusic", bgm_extmusic.value ? "0" : "1");
		break;
	case OPT_SNDVOL:	// sfx volume
		f = sfxvolume.value + dir * 0.1;
		if (f < 0)	f = 0;
		else if (f > 1)	f = 1;
		Cvar_SetValue ("volume", f);
		break;

	case OPT_HUDSTYLE:	// hud style
		Cvar_SetValueQuick (&scr_hudstyle, ((int) q_max (scr_hudstyle.value, 0.f) + 3 + dir) % 3);
		break;

	case OPT_ALWAYRUN:	// always run
		if (cl_alwaysrun.value)
			curr_alwaysrun = ALWAYSRUN_QUAKESPASM;
		else if (cl_forwardspeed.value > 200)
			curr_alwaysrun = ALWAYSRUN_VANILLA;
		else
			curr_alwaysrun = ALWAYSRUN_OFF;
			
		target_alwaysrun = (ALWAYSRUN_ITEMS + curr_alwaysrun + dir) % ALWAYSRUN_ITEMS;
			
		if (target_alwaysrun == ALWAYSRUN_VANILLA)
		{
			Cvar_SetValue ("cl_alwaysrun", 0);
			Cvar_SetValue ("cl_forwardspeed", 400);
			Cvar_SetValue ("cl_backspeed", 400);
		}
		else if (target_alwaysrun == ALWAYSRUN_QUAKESPASM)
		{
			Cvar_SetValue ("cl_alwaysrun", 1);
			Cvar_SetValue ("cl_forwardspeed", 200);
			Cvar_SetValue ("cl_backspeed", 200);
		}
		else // ALWAYSRUN_OFF
		{
			Cvar_SetValue ("cl_alwaysrun", 0);
			Cvar_SetValue ("cl_forwardspeed", 200);
			Cvar_SetValue ("cl_backspeed", 200);
		}
		break;

	case OPT_INVMOUSE:	// invert mouse
		Cvar_SetValue ("m_pitch", -m_pitch.value);
		break;

	case OPT_ALWAYSMLOOK:
		if ((in_mlook.state & 1) || freelook.value)
		{
			Cvar_SetValueQuick (&freelook, 0.f);
			Cbuf_AddText("-mlook");
		}
		else
		{
			Cvar_SetValueQuick (&freelook, 1.f);
		}
		break;

	case OPT_LOOKSPRING:	// lookspring
		Cvar_Set ("lookspring", lookspring.value ? "0" : "1");
		break;

	case OPT_LOOKSTRAFE:	// lookstrafe
		Cvar_Set ("lookstrafe", lookstrafe.value ? "0" : "1");
		break;
	}
}


void M_DrawSlider (int x, int y, float range)
{
	int	i;

	if (range < 0)
		range = 0;
	if (range > 1)
		range = 1;
	M_DrawCharacter (x-8, y, 128);
	for (i = 0; i < SLIDER_RANGE; i++)
		M_DrawCharacter (x + i*8, y, 129);
	M_DrawCharacter (x+i*8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox (int x, int y, int on)
{
#if 0
	if (on)
		M_DrawCharacter (x, y, 131);
	else
		M_DrawCharacter (x, y, 129);
#endif
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}

qboolean M_SetSliderValue (int option, float f)
{
	float l;
	f = CLAMP (0.f, f, 1.f);

	switch (option)
	{
	case OPT_SCALE:	// console and menu scale
		target_scale_frac = f;
		// Delay the actual update until we release the mouse button
		// to keep the UI layout stable while adjusting the scale
		if (!slider_grab)
		{
			l = (vid.width / 320.0) - 1;
			f = l > 0 ? f * l + 1 : 0;
			Cvar_SetValue ("scr_conscale", f);
			Cvar_SetValue ("scr_menuscale", f);
			Cvar_SetValue ("scr_sbarscale", f);
			Cvar_SetValue ("scr_crosshairscale", f);
		}
		return true;
	case OPT_SCRSIZE:	// screen size
		f = f * (120 - 30) + 30;
		if (f >= 100)
			f = floor (f / 10 + 0.5) * 10;
		Cvar_SetValue ("viewsize", f);
		return true;
	case OPT_GAMMA:	// gamma
		f = 1.f - f * 0.5f;
		Cvar_SetValue ("gamma", f);
		return true;
	case OPT_CONTRAST:	// contrast
		f += 1.f;
		Cvar_SetValue ("contrast", f);
		return true;
	case OPT_MOUSESPEED:	// mouse speed
		f = f * 10.f + 1.f;
		Cvar_SetValue ("sensitivity", f);
		return true;
	case OPT_SBALPHA:	// statusbar alpha
		f = 1.f - f;
		Cvar_SetValue ("scr_sbaralpha", f);
		return true;
	case OPT_MUSICVOL:	// music volume
		Cvar_SetValue ("bgmvolume", f);
		return true;
	case OPT_SNDVOL:	// sfx volume
		Cvar_SetValue ("volume", f);
		return true;
	default:
		return false;
	}
}

float M_MouseToSliderFraction (int cx)
{
	float f;
	f = (cx - 4) / (float)((SLIDER_RANGE - 1) * 8);
	return CLAMP (0.f, f, 1.f);
}

void M_ReleaseSliderGrab (void)
{
	if (!slider_grab)
		return;
	slider_grab = false;
	S_LocalSound ("misc/menu1.wav");
	if (options_cursor == OPT_SCALE)
		M_SetSliderValue (OPT_SCALE, target_scale_frac);
}

qboolean M_SliderClick (int cx, int cy)
{
	cx -= 220;
	if (cx < -12 || cx > SLIDER_RANGE*8+4)
		return false;
	// HACK: we set the flag to true before updating the slider
	// to avoid changing the UI scale and implicitly the layout
	if (options_cursor == OPT_SCALE)
		slider_grab = true;
	if (!M_SetSliderValue (options_cursor, M_MouseToSliderFraction (cx)))
		return false;
	slider_grab = true;
	S_LocalSound ("misc/menu3.wav");
	return true;
}

void M_Options_Draw (void)
{
	float		r, l;
	qpic_t	*p;

	if (slider_grab && !keydown[K_MOUSE1])
		M_ReleaseSliderGrab ();

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	// Draw the items in the order of the enum defined above:
	// OPT_CUSTOMIZE:
	M_Print (16, 32 + 8*OPT_CUSTOMIZE,	"              Controls");
	// OPT_CONSOLE:
	M_Print (16, 32 + 8*OPT_CONSOLE,	"          Goto console");
	// OPT_DEFAULTS:
	M_Print (16, 32 + 8*OPT_DEFAULTS,	"          Reset config");

	// OPT_MODS
	M_Print (16, 32 + 8*OPT_MODS,		"                  Mods");

	// OPT_SCALE:
	M_Print (16, 32 + 8*OPT_SCALE,		"                 Scale");
	l = (vid.width / 320.0) - 1;
	r = l > 0 ? (scr_conscale.value - 1) / l : 0;
	if (slider_grab && options_cursor == OPT_SCALE)
		r = target_scale_frac;
	M_DrawSlider (220, 32 + 8*OPT_SCALE, r);

	// OPT_SCRSIZE:
	M_Print (16, 32 + 8*OPT_SCRSIZE,	"           Screen size");
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_DrawSlider (220, 32 + 8*OPT_SCRSIZE, r);

	// OPT_GAMMA:
	M_Print (16, 32 + 8*OPT_GAMMA,		"            Brightness");
	r = (1.0 - vid_gamma.value) / 0.5;
	M_DrawSlider (220, 32 + 8*OPT_GAMMA, r);

	// OPT_CONTRAST:
	M_Print (16, 32 + 8*OPT_CONTRAST,	"              Contrast");
	r = vid_contrast.value - 1.0;
	M_DrawSlider (220, 32 + 8*OPT_CONTRAST, r);
	
	// OPT_MOUSESPEED:
	M_Print (16, 32 + 8*OPT_MOUSESPEED,	"           Mouse Speed");
	r = (sensitivity.value - 1)/10;
	M_DrawSlider (220, 32 + 8*OPT_MOUSESPEED, r);

	// OPT_SBALPHA:
	M_Print (16, 32 + 8*OPT_SBALPHA,	"             HUD alpha");
	r = (1.0 - scr_sbaralpha.value) ; // scr_sbaralpha range is 1.0 to 0.0
	M_DrawSlider (220, 32 + 8*OPT_SBALPHA, r);

	// OPT_HUDSTYLE
	M_Print (16, 32 + 8*OPT_HUDSTYLE,	"                   HUD");
	if (scr_hudstyle.value < 1)
		M_Print (220, 32 + 8*OPT_HUDSTYLE, "Classic");
	else if (scr_hudstyle.value < 2)
		M_Print (220, 32 + 8*OPT_HUDSTYLE, "Modern 1");
	else
		M_Print (220, 32 + 8*OPT_HUDSTYLE, "Modern 2");

	// OPT_SNDVOL:
	M_Print (16, 32 + 8*OPT_SNDVOL,		"          Sound Volume");
	r = sfxvolume.value;
	M_DrawSlider (220, 32 + 8*OPT_SNDVOL, r);

	// OPT_MUSICVOL:
	M_Print (16, 32 + 8*OPT_MUSICVOL,	"          Music Volume");
	r = bgmvolume.value;
	M_DrawSlider (220, 32 + 8*OPT_MUSICVOL, r);

	// OPT_MUSICEXT:
	M_Print (16, 32 + 8*OPT_MUSICEXT,	"        External Music");
	M_DrawCheckbox (220, 32 + 8*OPT_MUSICEXT, bgm_extmusic.value);

	// OPT_ALWAYRUN:
	M_Print (16, 32 + 8*OPT_ALWAYRUN,	"            Always Run");
	if (cl_alwaysrun.value)
		M_Print (220, 32 + 8*OPT_ALWAYRUN, "quakespasm");
	else if (cl_forwardspeed.value > 200.0)
		M_Print (220, 32 + 8*OPT_ALWAYRUN, "vanilla");
	else
		M_Print (220, 32 + 8*OPT_ALWAYRUN, "off");

	// OPT_INVMOUSE:
	M_Print (16, 32 + 8*OPT_INVMOUSE,	"          Invert Mouse");
	M_DrawCheckbox (220, 32 + 8*OPT_INVMOUSE, m_pitch.value < 0);

	// OPT_ALWAYSMLOOK:
	M_Print (16, 32 + 8*OPT_ALWAYSMLOOK,	"            Mouse Look");
	M_DrawCheckbox (220, 32 + 8*OPT_ALWAYSMLOOK, (in_mlook.state & 1) || freelook.value);

	// OPT_LOOKSPRING:
	M_Print (16, 32 + 8*OPT_LOOKSPRING,	"            Lookspring");
	M_DrawCheckbox (220, 32 + 8*OPT_LOOKSPRING, lookspring.value);

	// OPT_LOOKSTRAFE:
	M_Print (16, 32 + 8*OPT_LOOKSTRAFE,	"            Lookstrafe");
	M_DrawCheckbox (220, 32 + 8*OPT_LOOKSTRAFE, lookstrafe.value);

	// OPT_VIDEO:
	if (vid_menudrawfn)
		M_Print (16, 32 + 8*OPT_VIDEO,	"         Video Options");

// cursor
	M_DrawCharacter (200, 32 + options_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Options_Key (int k)
{
	if (!keydown[K_MOUSE1])
		M_ReleaseSliderGrab ();

	if (slider_grab)
	{
		switch (k)
		{
		case K_ESCAPE:
		case K_BBUTTON:
		case K_MOUSE4:
		case K_MOUSE2:
			M_ReleaseSliderGrab ();
			break;
		}
		return;
	}

	switch (k)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_Main_f ();
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	enter:
		m_entersound = true;
		switch (options_cursor)
		{
		case OPT_CUSTOMIZE:
			M_Menu_Keys_f ();
			break;
		case OPT_CONSOLE:
			m_state = m_none;
			Con_ToggleConsole_f ();
			break;
		case OPT_DEFAULTS:
			if (SCR_ModalMessage("This will reset all controls\n"
					"and stored cvars. Continue? (y/n)\n", 15.0f))
			{
				Cbuf_AddText ("resetcfg\n");
				Cbuf_AddText ("exec default.cfg\n");
			}
			break;
		case OPT_MODS:
			M_Menu_Mods_f ();
			break;
		case OPT_VIDEO:
			M_Menu_Video_f ();
			break;
		default:
			M_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_LEFTARROW:
	case K_MWHEELDOWN:
	//case K_MOUSE2:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
	case K_MWHEELUP:
		M_AdjustSliders (1);
		break;

	case K_MOUSE1:
		if (!M_SliderClick (m_mousex, m_mousey))
			goto enter;
		break;
	}

	if (options_cursor == OPTIONS_ITEMS - 1 && vid_menudrawfn == NULL)
	{
		if (k == K_UPARROW)
			options_cursor = OPTIONS_ITEMS - 2;
		else
			options_cursor = 0;
	}
}

void M_Options_Mousemove (int cx, int cy)
{
	if (slider_grab)
	{
		if (!keydown[K_MOUSE1])
		{
			M_ReleaseSliderGrab ();
			return;
		}
		M_SetSliderValue (options_cursor, M_MouseToSliderFraction (cx - 220));
		return;
	}

	M_UpdateCursor (cy, 32, 8, OPTIONS_ITEMS, &options_cursor);
}

//=============================================================================
/* KEYS MENU */

static const char* const bindnames[][2] =
{
	{"+forward",		"Move forward"},
	{"+back",			"Move backward"},
	{"+moveleft",		"Move left"},
	{"+moveright",		"Move right"},
	{"+left",			"Turn left"},
	{"+right",			"Turn right"},
	{"+jump",			"Jump / swim up"},
	{"+moveup",			"Swim up"},
	{"+movedown",		"Swim down"},
	{"+speed",			"Run"},
	{"+strafe",			"Sidestep"},
	{"+lookup",			"Look up"},
	{"+lookdown",		"Look down"},
	{"centerview",		"Center view"},
	{"+mlook",			"Mouse look"},
	{"+klook",			"Keyboard look"},
	{"+attack",			"Attack"},
	{"impulse 10",		"Next weapon"},
	{"impulse 12",		"Previous weapon"},
	{"impulse 1",		"Axe"},
	{"impulse 2",		"Shotgun"},
	{"impulse 3",		"Super Shotgun"},
	{"impulse 4",		"Nailgun"},
	{"impulse 5",		"Super Nailgun"},
	{"impulse 6",		"Grenade Launcher"},
	{"impulse 7",		"Rocket Launcher"},
	{"impulse 8",		"Thunderbolt"},
	{"impulse 225",		"Laser Cannon"},
	{"impulse 226",		"Mjolnir"},
};

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

static struct
{
	menulist_t list;
} keysmenu;

static qboolean	bind_grab;

void M_Menu_Keys_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_keys;
	m_entersound = true;
	keysmenu.list.cursor = 0;
	keysmenu.list.scroll = 0;
	keysmenu.list.numitems = hipnotic ? NUMCOMMANDS : NUMCOMMANDS - 2;
	keysmenu.list.viewsize = 17;
}


void M_FindKeysForCommand (const char *command, int *threekeys)
{
	Key_GetKeysForCommand (command, threekeys, 3);
}

void M_UnbindCommand (const char *command)
{
	int		j;
	char	*b;

	for (j = 0; j < MAX_KEYS; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strcmp (b, command) )
			Key_SetBinding (j, NULL);
	}
}

extern qpic_t	*pic_up, *pic_down;

void M_Keys_Draw (void)
{
	int		firstvis, numvis;
	int		i, x, y, cols;
	int		keys[3];
	const char	*name;
	qpic_t	*p;

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, backspace to clear");

	x = 0;
	y = 56;
	cols = 40;

	if (M_List_GetOverflow (&keysmenu.list) > 0)
	{
		if (keysmenu.list.scroll > 0)
			M_DrawEllipsisBar (x, y - 8, cols);
		if (keysmenu.list.scroll + keysmenu.list.viewsize < keysmenu.list.numitems)
			M_DrawEllipsisBar (x, y + keysmenu.list.viewsize*8, cols);
	}

	// search for known bindings
	M_List_GetVisibleRange (&keysmenu.list, &firstvis, &numvis);
	while (numvis-- > 0)
	{
		void (*print_fn) (int cx, int cy, const char *text);

		i = firstvis++;
		print_fn = (i == keysmenu.list.cursor && bind_grab) ? M_PrintWhite : M_Print;

		print_fn (0, y, bindnames[i][1]);

		M_FindKeysForCommand (bindnames[i][0], keys);
		// If we already have 3 keys bound to this action
		// they will all be unbound when a new one is assigned.
		// We show this outcome to the user before it actually
		// occurs so they have a chance to abort the process
		// and keep the existing key bindings.
		if (i == keysmenu.list.cursor && bind_grab && keys[2] != -1)
			keys[0] = -1;

		if (keys[0] == -1)
		{
			print_fn (136, y, "???");
		}
		else
		{
			name = Key_KeynumToString (keys[0]);
			print_fn (136, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				name = Key_KeynumToString (keys[1]);
				print_fn (136 + x + 8, y, "or");
				print_fn (136 + x + 32, y, name);
				x = x + 32 + strlen(name) * 8;
				if (keys[2] != -1)
				{
					print_fn (136 + x + 8, y, "or");
					print_fn (136 + x + 32, y, Key_KeynumToString (keys[2]));
				}
			}
		}

		if (i == keysmenu.list.cursor)
		{
			if (bind_grab)
				M_DrawCharacter (128, y, '=');
			else
				M_DrawCharacter (128, y, 12+((int)(realtime*4)&1));
		}

		y += 8;
	}
}


void M_Keys_Key (int k)
{
	char	cmd[80];
	int		keys[3];

	if (bind_grab)
	{	// defining a key
		S_LocalSound ("misc/menu1.wav");
		if ((k != K_ESCAPE) && (k != '`'))
		{
			M_FindKeysForCommand (bindnames[keysmenu.list.cursor][0], keys);
			if (keys[2] != -1)
				M_UnbindCommand (bindnames[keysmenu.list.cursor][0]);
			sprintf (cmd, "bind \"%s\" \"%s\"\n", Key_KeynumToString (k), bindnames[keysmenu.list.cursor][0]);
			Cbuf_InsertText (cmd);
		}

		bind_grab = false;
		IN_DeactivateForMenu(); // deactivate because we're returning to the menu
		return;
	}

	if (M_List_Key (&keysmenu.list, k))
		return;

	switch (k)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_Options_f ();
		break;

	case K_ENTER:		// go into bind mode
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		bind_grab = true;
		M_List_AutoScroll (&keysmenu.list);
		IN_Activate(); // activate to allow mouse key binding
		break;

	case K_BACKSPACE:	// delete bindings
	case K_DEL:
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keysmenu.list.cursor][0]);
		break;
	}
}


void M_Keys_Mousemove (int cx, int cy)
{
	M_List_Mousemove (&keysmenu.list, cy - 56);
}

//=============================================================================
/* VIDEO MENU */

void M_Menu_Video_f (void)
{
	(*vid_menucmdfn) (); //johnfitz
}


void M_Video_Draw (void)
{
	(*vid_menudrawfn) ();
}


void M_Video_Key (int key)
{
	(*vid_menukeyfn) (key);
}

void M_Video_Mousemove (int cx, int cy)
{
	(*vid_menumousefn) (cx, cy);
}

//=============================================================================
/* HELP MENU */

int		help_page;
#define	NUM_HELP_PAGES	6


void M_Menu_Help_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}



void M_Help_Draw (void)
{
	M_DrawPic (0, 0, Draw_CachePic ( va("gfx/help%i.lmp", help_page)) );
}


void M_Help_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_Main_f ();
		break;

	case K_UPARROW:
	case K_RIGHTARROW:
	case K_MWHEELDOWN:
	case K_MOUSE1:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
	case K_MWHEELUP:
	//case K_MOUSE2:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES-1;
		break;
	}

}

//=============================================================================
/* QUIT MENU */

int		msgNumber;
enum m_state_e	m_quit_prevstate;
qboolean	wasInMenus;

const char*const quitMessage [] = 
{
/* .........1.........2.... */
  "  Are you gonna quit    ",
  "  this game just like   ",
  "   everything else?     ",
  "                        ",
 
  " Milord, methinks that  ",
  "   thou art a lowly     ",
  " quitter. Is this true? ",
  "                        ",

  " Do I need to bust your ",
  "  face open for trying  ",
  "        to quit?        ",
  "                        ",

  " Man, I oughta smack you",
  "   for trying to quit!  ",
  "     Press Y to get     ",
  "      smacked out.      ",
 
  " Press Y to quit like a ",
  "   big loser in life.   ",
  "  Press N to stay proud ",
  "    and successful!     ",
 
  "   If you press Y to    ",
  "  quit, I will summon   ",
  "  Satan all over your   ",
  "      hard drive!       ",
 
  "  Um, Asmodeus dislikes ",
  " his children trying to ",
  " quit. Press Y to return",
  "   to your Tinkertoys.  ",
 
  "  If you quit now, I'll ",
  "  throw a blanket-party ",
  "   for you next time!   ",
  "                        ",

  "  Leave QUAKE?  ",
  "",
  "",
  "\xD9\x65s   \xCE\x6F",
};

void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = (cl_confirmquit.value >= 2.f) ? rand()&7 : 8;
}


void M_Quit_Key (int key)
{
	if (key == K_ESCAPE ||
		key == K_MOUSE2 ||
		key == K_MOUSE4)
	{
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			IN_Activate();
			key_dest = key_game;
			m_state = m_none;
		}
	}
}


void M_Quit_Char (int key)
{
	switch (key)
	{
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			IN_Activate();
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case 'y':
	case 'Y':
		IN_DeactivateForConsole();
		key_dest = key_console;
		Host_Quit_f ();
		break;

	default:
		break;
	}

}


qboolean M_Quit_TextEntry (void)
{
	return true;
}


void M_Quit_Draw (void) //johnfitz -- modified for new quit message
{
	const char*const *msg = quitMessage + msgNumber*4;
	int i, boxlen = 0;

	if (wasInMenus)
	{
		m_state = m_quit_prevstate;
		m_recursiveDraw = true;
		M_Draw ();
		m_state = m_quit;
	}

	//okay, this is kind of fucked up.  M_DrawTextBox will always act as if
	//width is even. Also, the width and lines values are for the interior of the box,
	//but the x and y values include the border.
	for (i = 0; i < 4; i++)
	{
		int msglen = (int)strlen (msg[i]);
		boxlen = q_max (boxlen, msglen);
	}
	boxlen = (boxlen + 1) & ~1;
	M_DrawTextBox (160 - 4*(boxlen+2), 76, boxlen, 5);

	//now do the text
	for (i = 0; i < 4; i++)
		M_Print (160 - 8*((strlen(msg[i])+1)>>1), 88 + i*8, msg[i]);
}

//=============================================================================
/* LAN CONFIG MENU */

int		lanConfig_cursor = -1;
int		lanConfig_cursor_table [] = {72, 92, 124};
#define NUM_LANCONFIG_CMDS	3

int 	lanConfig_port;
char	lanConfig_portname[6];
char	lanConfig_joinname[22];

void M_Menu_LanConfig_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_lanconfig;
	m_entersound = true;
	if (lanConfig_cursor == -1)
	{
		if (JoiningGame && TCPIPConfig)
			lanConfig_cursor = 2;
		else
			lanConfig_cursor = 1;
	}
	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = 1;
	lanConfig_port = DEFAULTnet_hostport;
	sprintf(lanConfig_portname, "%u", lanConfig_port);

	m_return_onerror = false;
	m_return_reason[0] = 0;
}


void M_LanConfig_Draw (void)
{
	qpic_t	*p;
	int		basex;
	const char	*startJoin;
	const char	*protocol;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, p);

	basex = 72; /* Arcane Dimensions has an oversized gfx/p_multi.lmp */

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	if (IPXConfig)
		protocol = "IPX";
	else
		protocol = "TCP/IP";
	M_Print (basex, 32, va ("%s - %s", startJoin, protocol));
	basex += 8;

	M_Print (basex, 52, "Address:");
	if (IPXConfig)
		M_Print (basex+9*8, 52, my_ipx_address);
	else
		M_Print (basex+9*8, 52, my_tcpip_address);

	M_Print (basex, lanConfig_cursor_table[0], "Port");
	M_DrawTextBox (basex+8*8, lanConfig_cursor_table[0]-8, 6, 1);
	M_Print (basex+9*8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame)
	{
		M_Print (basex, lanConfig_cursor_table[1], "Search for local games...");
		M_Print (basex, 108, "Join game at:");
		M_DrawTextBox (basex+8, lanConfig_cursor_table[2]-8, 22, 1);
		M_Print (basex+16, lanConfig_cursor_table[2], lanConfig_joinname);
	}
	else
	{
		M_DrawTextBox (basex, lanConfig_cursor_table[1]-8, 2, 1);
		M_Print (basex+8, lanConfig_cursor_table[1], "OK");
	}

	M_DrawCharacter (basex-8, lanConfig_cursor_table [lanConfig_cursor], 12+((int)(realtime*4)&1));

	if (lanConfig_cursor == 0)
		M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), lanConfig_cursor_table [0], 10+((int)(realtime*4)&1));

	if (lanConfig_cursor == 2)
		M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), lanConfig_cursor_table [2], 10+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (basex, 148, m_return_reason);
}


void M_LanConfig_Key (int key)
{
	int		l;

	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor--;
		if (lanConfig_cursor < 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor++;
		if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
			lanConfig_cursor = 0;
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = true;

		M_ConfigureNetSubsystem ();

		if (lanConfig_cursor == 1)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f ();
				break;
			}
			M_Menu_Search_f();
			break;
		}

		if (lanConfig_cursor == 2)
		{
			m_return_state = m_state;
			m_return_onerror = true;
			IN_Activate();
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText ( va ("connect \"%s\"\n", lanConfig_joinname) );
			break;
		}

		break;

	case K_BACKSPACE:
		if (lanConfig_cursor == 0)
		{
			if (strlen(lanConfig_portname))
				lanConfig_portname[strlen(lanConfig_portname)-1] = 0;
		}

		if (lanConfig_cursor == 2)
		{
			if (strlen(lanConfig_joinname))
				lanConfig_joinname[strlen(lanConfig_joinname)-1] = 0;
		}
		break;
	}

	if (StartingGame && lanConfig_cursor == 2)
	{
		if (key == K_UPARROW)
			lanConfig_cursor = 1;
		else
			lanConfig_cursor = 0;
	}

	l =  Q_atoi(lanConfig_portname);
	if (l > 65535)
		l = lanConfig_port;
	else
		lanConfig_port = l;
	sprintf(lanConfig_portname, "%u", lanConfig_port);
}


void M_LanConfig_Char (int key)
{
	int l;

	switch (lanConfig_cursor)
	{
	case 0:
		if (key < '0' || key > '9')
			return;
		l = strlen(lanConfig_portname);
		if (l < 5)
		{
			lanConfig_portname[l+1] = 0;
			lanConfig_portname[l] = key;
		}
		break;
	case 2:
		l = strlen(lanConfig_joinname);
		if (l < 21)
		{
			lanConfig_joinname[l+1] = 0;
			lanConfig_joinname[l] = key;
		}
		break;
	}
}


qboolean M_LanConfig_TextEntry (void)
{
	return (lanConfig_cursor == 0 || lanConfig_cursor == 2);
}


void M_LanConfig_Mousemove (int cx, int cy)
{
	M_UpdateCursorWithTable (cy, lanConfig_cursor_table, NUM_LANCONFIG_CMDS - StartingGame, &lanConfig_cursor);
}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct
{
	const char	*name;
	const char	*description;
} level_t;

level_t		levels[] =
{
	{"start", "Entrance"},	// 0

	{"e1m1", "Slipgate Complex"},				// 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},				// 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},			// 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},				// 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},			// 31

	{"dm1", "Place of Two Deaths"},				// 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

//MED 01/06/97 added hipnotic levels
level_t     hipnoticlevels[] =
{
	{"start", "Command HQ"},	// 0

	{"hip1m1", "The Pumping Station"},			// 1
	{"hip1m2", "Storage Facility"},
	{"hip1m3", "The Lost Mine"},
	{"hip1m4", "Research Facility"},
	{"hip1m5", "Military Complex"},

	{"hip2m1", "Ancient Realms"},				// 6
	{"hip2m2", "The Black Cathedral"},
	{"hip2m3", "The Catacombs"},
	{"hip2m4", "The Crypt"},
	{"hip2m5", "Mortum's Keep"},
	{"hip2m6", "The Gremlin's Domain"},

	{"hip3m1", "Tur Torment"},				// 12
	{"hip3m2", "Pandemonium"},
	{"hip3m3", "Limbo"},
	{"hip3m4", "The Gauntlet"},

	{"hipend", "Armagon's Lair"},				// 16

	{"hipdm1", "The Edge of Oblivion"}			// 17
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
level_t		roguelevels[] =
{
	{"start",	"Split Decision"},
	{"r1m1",	"Deviant's Domain"},
	{"r1m2",	"Dread Portal"},
	{"r1m3",	"Judgement Call"},
	{"r1m4",	"Cave of Death"},
	{"r1m5",	"Towers of Wrath"},
	{"r1m6",	"Temple of Pain"},
	{"r1m7",	"Tomb of the Overlord"},
	{"r2m1",	"Tempus Fugit"},
	{"r2m2",	"Elemental Fury I"},
	{"r2m3",	"Elemental Fury II"},
	{"r2m4",	"Curse of Osiris"},
	{"r2m5",	"Wizard's Keep"},
	{"r2m6",	"Blood Sacrifice"},
	{"r2m7",	"Last Bastion"},
	{"r2m8",	"Source of Evil"},
	{"ctf1",    "Division of Change"}
};

typedef struct
{
	const char	*description;
	int		firstLevel;
	int		levels;
} episode_t;

episode_t	episodes[] =
{
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

//MED 01/06/97  added hipnotic episodes
episode_t   hipnoticepisodes[] =
{
	{"Scourge of Armagon", 0, 1},
	{"Fortress of the Dead", 1, 5},
	{"Dominion of Darkness", 6, 6},
	{"The Rift", 12, 4},
	{"Final Level", 16, 1},
	{"Deathmatch Arena", 17, 1}
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
episode_t	rogueepisodes[] =
{
	{"Introduction", 0, 1},
	{"Hell's Fortress", 1, 7},
	{"Corridors of Time", 8, 8},
	{"Deathmatch Arena", 16, 1}
};

int	startepisode;
int	startlevel;
int maxplayers;
qboolean m_serverInfoMessage = false;
double m_serverInfoMessageTime;

void M_Menu_GameOptions_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = svs.maxclientslimit;
}


int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 112, 120};
#define	NUM_GAMEOPTIONS	9
int		gameoptions_cursor;

void M_GameOptions_Draw (void)
{
	qpic_t	*p;
	int		x;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_DrawTextBox (152, 32, 10, 1);
	M_Print (160, 40, "begin game");

	M_Print (0, 56, "      Max players");
	M_Print (160, 56, va("%i", maxplayers) );

	M_Print (0, 64, "        Game Type");
	if (coop.value)
		M_Print (160, 64, "Cooperative");
	else
		M_Print (160, 64, "Deathmatch");

	M_Print (0, 72, "        Teamplay");
	if (rogue)
	{
		const char *msg;

		switch((int)teamplay.value)
		{
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
			case 3: msg = "Tag"; break;
			case 4: msg = "Capture the Flag"; break;
			case 5: msg = "One Flag CTF"; break;
			case 6: msg = "Three Team CTF"; break;
			default: msg = "Off"; break;
		}
		M_Print (160, 72, msg);
	}
	else
	{
		const char *msg;

		switch((int)teamplay.value)
		{
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
			default: msg = "Off"; break;
		}
		M_Print (160, 72, msg);
	}

	M_Print (0, 80, "            Skill");
	if (skill.value == 0)
		M_Print (160, 80, "Easy difficulty");
	else if (skill.value == 1)
		M_Print (160, 80, "Normal difficulty");
	else if (skill.value == 2)
		M_Print (160, 80, "Hard difficulty");
	else
		M_Print (160, 80, "Nightmare difficulty");

	M_Print (0, 88, "       Frag Limit");
	if (fraglimit.value == 0)
		M_Print (160, 88, "none");
	else
		M_Print (160, 88, va("%i frags", (int)fraglimit.value));

	M_Print (0, 96, "       Time Limit");
	if (timelimit.value == 0)
		M_Print (160, 96, "none");
	else
		M_Print (160, 96, va("%i minutes", (int)timelimit.value));

	M_Print (0, 112, "         Episode");
	// MED 01/06/97 added hipnotic episodes
	if (hipnotic)
		M_Print (160, 112, hipnoticepisodes[startepisode].description);
	// PGM 01/07/97 added rogue episodes
	else if (rogue)
		M_Print (160, 112, rogueepisodes[startepisode].description);
	else
		M_Print (160, 112, episodes[startepisode].description);

	M_Print (0, 120, "           Level");
	// MED 01/06/97 added hipnotic episodes
	if (hipnotic)
	{
		M_Print (160, 120, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].description);
		M_Print (160, 128, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name);
	}
	// PGM 01/07/97 added rogue episodes
	else if (rogue)
	{
		M_Print (160, 120, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].description);
		M_Print (160, 128, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name);
	}
	else
	{
		M_Print (160, 120, levels[episodes[startepisode].firstLevel + startlevel].description);
		M_Print (160, 128, levels[episodes[startepisode].firstLevel + startlevel].name);
	}

// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(realtime*4)&1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320-26*8)/2;
			M_DrawTextBox (x, 138, 24, 4);
			x += 8;
			M_Print (x, 146, "  More than 4 players   ");
			M_Print (x, 154, " requires using command ");
			M_Print (x, 162, "line parameters; please ");
			M_Print (x, 170, "   see techinfo.txt.    ");
		}
		else
		{
			m_serverInfoMessage = false;
		}
	}
}


void M_NetStart_Change (int dir)
{
	int count;
	float	f;

	switch (gameoptions_cursor)
	{
	case 1:
		maxplayers += dir;
		if (maxplayers > svs.maxclientslimit)
		{
			maxplayers = svs.maxclientslimit;
			m_serverInfoMessage = true;
			m_serverInfoMessageTime = realtime;
		}
		if (maxplayers < 2)
			maxplayers = 2;
		break;

	case 2:
		Cvar_Set ("coop", coop.value ? "0" : "1");
		break;

	case 3:
		count = (rogue) ? 6 : 2;
		f = teamplay.value + dir;
		if (f > count)	f = 0;
		else if (f < 0)	f = count;
		Cvar_SetValue ("teamplay", f);
		break;

	case 4:
		f = skill.value + dir;
		if (f > 3)	f = 0;
		else if (f < 0)	f = 3;
		Cvar_SetValue ("skill", f);
		break;

	case 5:
		f = fraglimit.value + dir * 10;
		if (f > 100)	f = 0;
		else if (f < 0)	f = 100;
		Cvar_SetValue ("fraglimit", f);
		break;

	case 6:
		f = timelimit.value + dir * 5;
		if (f > 60)	f = 0;
		else if (f < 0)	f = 60;
		Cvar_SetValue ("timelimit", f);
		break;

	case 7:
		startepisode += dir;
	//MED 01/06/97 added hipnotic count
		if (hipnotic)
			count = 6;
	//PGM 01/07/97 added rogue count
	//PGM 03/02/97 added 1 for dmatch episode
		else if (rogue)
			count = 4;
		else if (registered.value)
			count = 7;
		else
			count = 2;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
	//MED 01/06/97 added hipnotic episodes
		if (hipnotic)
			count = hipnoticepisodes[startepisode].levels;
	//PGM 01/06/97 added hipnotic episodes
		else if (rogue)
			count = rogueepisodes[startepisode].levels;
		else
			count = episodes[startepisode].levels;

		if (startlevel < 0)
			startlevel = count - 1;

		if (startlevel >= count)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_LEFTARROW:
	case K_MWHEELDOWN:
	//case K_MOUSE2:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
	case K_MWHEELUP:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("listen 0\n");	// so host_netport will be re-examined
			Cbuf_AddText ( va ("maxplayers %u\n", maxplayers) );
			SCR_BeginLoadingPlaque ();

			if (hipnotic)
				Cbuf_AddText ( va ("map %s\n", hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name) );
			else if (rogue)
				Cbuf_AddText ( va ("map %s\n", roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name) );
			else
				Cbuf_AddText ( va ("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name) );

			return;
		}

		M_NetStart_Change (1);
		break;
	}
}


void M_GameOptions_Mousemove (int cx, int cy)
{
	M_UpdateCursorWithTable (cy, gameoptions_cursor_table, NUM_GAMEOPTIONS, &gameoptions_cursor);
}

//=============================================================================
/* SEARCH MENU */

qboolean	searchComplete = false;
double		searchCompleteTime;

void M_Menu_Search_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	NET_Slist_f();

}


void M_Search_Draw (void)
{
	qpic_t	*p;
	int x;

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	x = (320/2) - ((12*8)/2) + 4;
	M_DrawTextBox (x-8, 32, 12, 1);
	M_Print (x, 40, "Searching...");

	if(slistInProgress)
	{
		NET_Poll();
		return;
	}

	if (! searchComplete)
	{
		searchComplete = true;
		searchCompleteTime = realtime;
	}

	if (hostCacheCount)
	{
		M_Menu_ServerList_f ();
		return;
	}

	M_PrintWhite ((320/2) - ((22*8)/2), 64, "No Quake servers found");
	if ((realtime - searchCompleteTime) < 3.0)
		return;

	M_Menu_LanConfig_f ();
}


void M_Search_Key (int key)
{
}

//=============================================================================
/* SLIST MENU */

int		slist_cursor;
qboolean slist_sorted;

void M_Menu_ServerList_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
	slist_cursor = 0;
	m_return_onerror = false;
	m_return_reason[0] = 0;
	slist_sorted = false;
}


void M_ServerList_Draw (void)
{
	int	n;
	qpic_t	*p;

	if (!slist_sorted)
	{
		slist_sorted = true;
		NET_SlistSort ();
	}

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	for (n = 0; n < hostCacheCount; n++)
		M_Print (16, 32 + 8*n, NET_SlistPrintServer (n));
	M_DrawCharacter (0, 32 + slist_cursor*8, 12+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (16, 148, m_return_reason);
}


void M_ServerList_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		M_Menu_LanConfig_f ();
		break;

	case K_SPACE:
		M_Menu_Search_f ();
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor--;
		if (slist_cursor < 0)
			slist_cursor = hostCacheCount - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		slist_cursor++;
		if (slist_cursor >= hostCacheCount)
			slist_cursor = 0;
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		S_LocalSound ("misc/menu2.wav");
		m_return_state = m_state;
		m_return_onerror = true;
		slist_sorted = false;
		IN_Activate();
		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText ( va ("connect \"%s\"\n", NET_SlistPrintServerName(slist_cursor)) );
		break;

	default:
		break;
	}

}


void M_ServerList_Mousemove (int cx, int cy)
{
	M_UpdateCursor (cy, 32, 8, hostCacheCount, &slist_cursor);
}

//=============================================================================
/* Mods menu */

#define MAX_MODS		4096
#define MAX_VIS_MODS	18

typedef struct
{
	const char	*name;
	qboolean	active;
} moditem_t;

static struct
{
	menulist_t			list;
	int					x, y, cols;
	enum m_state_e		prev;
	qboolean			scrollbar_grab;
	moditem_t			items[MAX_MODS];
} modsmenu;

static qboolean M_Mods_IsActive (const char *game)
{
	extern char com_gamenames[];
	const char *list, *end, *p;

	if (!q_strcasecmp (game, GAMENAME))
		return !*com_gamenames;

	list = com_gamenames;
	while (*list)
	{
		end = list;
		while (*end && *end != ';')
			end++;

		p = game;
		while (*p && list != end)
			if (q_tolower (*p) == q_tolower (*list))
				p++, list++;
			else
				break;

		if (!*p && list == end)
			return true;

		list = end;
		if (*list)
			list++;
	}

	return false;
}

static void M_Mods_Add (const char *name)
{
	moditem_t *mod = &modsmenu.items[modsmenu.list.numitems];
	mod->name = name;
	mod->active = M_Mods_IsActive (name);
	if (mod->active && modsmenu.list.cursor == -1)
		modsmenu.list.cursor = modsmenu.list.numitems;
	modsmenu.list.numitems++;
}

static void M_Mods_Init (void)
{
	filelist_item_t *item;

	modsmenu.x = 64;
	modsmenu.y = 32;
	modsmenu.cols = 28;
	modsmenu.scrollbar_grab = false;
	modsmenu.list.viewsize = MAX_VIS_MODS;
	modsmenu.list.cursor = -1;
	modsmenu.list.scroll = 0;
	modsmenu.list.numitems = 0;

	for (item = modlist; item && modsmenu.list.numitems < MAX_MODS; item = item->next)
		M_Mods_Add (item->name);

	if (modsmenu.list.cursor == -1)
		modsmenu.list.cursor = 0;

	M_List_CenterCursor (&modsmenu.list);
}

void M_Menu_Mods_f (void)
{
	IN_DeactivateForMenu();
	key_dest = key_menu;
	modsmenu.prev = m_state;
	m_state = m_mods;
	m_entersound = true;
	M_Mods_Init ();
}

void M_Mods_Draw (void)
{
	const char *str;
	int x, y, i, j, cols;
	int firstvis, numvis;

	if (!keydown[K_MOUSE1])
		modsmenu.scrollbar_grab = false;

	x = modsmenu.x;
	y = modsmenu.y;
	cols = modsmenu.cols;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	Draw_StringEx (x, y - 28, 12, "Mods");
	M_DrawQuakeBar (x - 8, y - 16, cols + 2);

	M_List_GetVisibleRange (&modsmenu.list, &firstvis, &numvis);
	for (i = 0; i < numvis; i++)
	{
		int idx = i + firstvis;
		int mask = modsmenu.items[idx].active ? 0 : 128;
		for (j = 0; j < cols - 1 && modsmenu.items[idx].name[j]; j++)
			M_DrawCharacter (x + j*8, y + i*8, modsmenu.items[idx].name[j] | mask);

		if (idx == modsmenu.list.cursor)
			M_DrawCharacter (x - 8, y + i*8, 12+((int)(realtime*4)&1));
	}

	if (M_List_GetOverflow (&modsmenu.list) > 0)
	{
		M_List_DrawScrollbar (&modsmenu.list, x + cols*8 - 8, y);

		str = va("%d-%d of %d", firstvis + 1, firstvis + numvis, modsmenu.list.numitems);
		M_Print (x + (cols - strlen (str))*8, y - 24, str);

		if (modsmenu.list.scroll > 0)
			M_DrawEllipsisBar (x, y - 8, cols);
		if (modsmenu.list.scroll + modsmenu.list.viewsize < modsmenu.list.numitems)
			M_DrawEllipsisBar (x, y + modsmenu.list.viewsize*8, cols);
	}
}

qboolean M_Mods_Match (int index, char initial)
{
	return q_tolower (modsmenu.items[index].name[0]) == initial;
}

void M_Mods_Char (int key)
{
	M_List_CycleMatch (&modsmenu.list, key, M_Mods_Match);
}

qboolean M_Mods_TextEntry (void)
{
	return !modsmenu.scrollbar_grab;
}

void M_Mods_Key (int key)
{
	int x, y;

	if (modsmenu.scrollbar_grab)
	{
		switch (key)
		{
		case K_ESCAPE:
		case K_BBUTTON:
		case K_MOUSE4:
		case K_MOUSE2:
			modsmenu.scrollbar_grab = false;
			break;
		}
		return;
	}

	if (M_List_Key (&modsmenu.list, key))
		return;

	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		if (modsmenu.prev == m_options)
			M_Menu_Options_f ();
		else
			M_Menu_Main_f ();
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	enter:
		Cbuf_AddText (va ("game %s\n", modsmenu.items[modsmenu.list.cursor].name));
		M_Menu_Main_f ();
		break;

	case K_MOUSE1:
		x = m_mousex - modsmenu.x - (modsmenu.cols - 1) * 8;
		y = m_mousey - modsmenu.y;
		if (x < -8 || !M_List_UseScrollbar (&modsmenu.list, y))
			goto enter;
		modsmenu.scrollbar_grab = true;
		M_Mods_Mousemove (m_mousex, m_mousey);
		break;

	default:
		break;
	}
}


void M_Mods_Mousemove (int cx, int cy)
{
	cy -= modsmenu.y;

	if (modsmenu.scrollbar_grab)
	{
		if (!keydown[K_MOUSE1])
		{
			modsmenu.scrollbar_grab = false;
			return;
		}
		M_List_UseScrollbar (&modsmenu.list, cy);
		// Note: no return, we also update the cursor
	}

	M_List_Mousemove (&modsmenu.list, cy);
}

//=============================================================================
/* Credits menu -- used by the 2021 re-release */

void M_Menu_Credits_f (void)
{
}

//=============================================================================
/* Menu Subsystem */


void M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
	Cmd_AddCommand ("menu_credits", M_Menu_Credits_f); // needed by the 2021 re-release
	Cmd_AddCommand ("menu_mods", M_Menu_Mods_f);
}


void M_Draw (void)
{
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw)
	{
		if (scr_con_current)
		{
			Draw_ConsoleBackground ();
			S_ExtraUpdate ();
		}

		Draw_FadeScreen (); //johnfitz -- fade even if console fills screen
	}
	else
	{
		m_recursiveDraw = false;
	}

	GL_SetCanvas (CANVAS_MENU); //johnfitz

	switch (m_state)
	{
	case m_none:
		break;

	case m_main:
		M_Main_Draw ();
		break;

	case m_singleplayer:
		M_SinglePlayer_Draw ();
		break;

	case m_load:
		M_Load_Draw ();
		break;

	case m_save:
		M_Save_Draw ();
		break;

	case m_skill:
		M_Skill_Draw ();
		break;

	case m_multiplayer:
		M_MultiPlayer_Draw ();
		break;

	case m_setup:
		M_Setup_Draw ();
		break;

	case m_net:
		M_Net_Draw ();
		break;

	case m_options:
		M_Options_Draw ();
		break;

	case m_keys:
		M_Keys_Draw ();
		break;

	case m_video:
		M_Video_Draw ();
		break;

	case m_mods:
		M_Mods_Draw ();
		break;

	case m_help:
		M_Help_Draw ();
		break;

	case m_quit:
		if (!fitzmode && !cl_confirmquit.value)
		{ /* QuakeSpasm customization: */
			/* Quit now! S.A. */
			key_dest = key_console;
			Host_Quit_f ();
		}
		M_Quit_Draw ();
		break;

	case m_lanconfig:
		M_LanConfig_Draw ();
		break;

	case m_gameoptions:
		M_GameOptions_Draw ();
		break;

	case m_search:
		M_Search_Draw ();
		break;

	case m_slist:
		M_ServerList_Draw ();
		break;
	}

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	S_ExtraUpdate ();
}


void M_Keydown (int key)
{
	if (!cl_mousemenu.value && M_IsMouseKey (key))
		return;

	switch (m_state)
	{
	case m_none:
		return;

	case m_main:
		M_Main_Key (key);
		return;

	case m_singleplayer:
		M_SinglePlayer_Key (key);
		return;

	case m_load:
		M_Load_Key (key);
		return;

	case m_save:
		M_Save_Key (key);
		return;

	case m_skill:
		M_Skill_Key (key);
		return;

	case m_multiplayer:
		M_MultiPlayer_Key (key);
		return;

	case m_setup:
		M_Setup_Key (key);
		return;

	case m_net:
		M_Net_Key (key);
		return;

	case m_options:
		M_Options_Key (key);
		return;

	case m_keys:
		M_Keys_Key (key);
		return;

	case m_video:
		M_Video_Key (key);
		return;

	case m_mods:
		M_Mods_Key (key);
		return;

	case m_help:
		M_Help_Key (key);
		return;

	case m_quit:
		M_Quit_Key (key);
		return;

	case m_lanconfig:
		M_LanConfig_Key (key);
		return;

	case m_gameoptions:
		M_GameOptions_Key (key);
		return;

	case m_search:
		M_Search_Key (key);
		break;

	case m_slist:
		M_ServerList_Key (key);
		return;
	}
}


void M_Mousemove (int x, int y)
{
	vrect_t bounds, viewport;

	if (!cl_mousemenu.value)
		return;

	Draw_GetMenuTransform (&bounds, &viewport);
	m_mousex = x = bounds.x + (int)((x - viewport.x) * bounds.width / (float)viewport.width + 0.5f);
	m_mousey = y = bounds.y + (int)((y - viewport.y) * bounds.height / (float)viewport.height + 0.5f);

	switch (m_state)
	{
	default:
		return;

	case m_main:
		M_Main_Mousemove (x, y);
		return;

	case m_singleplayer:
		M_SinglePlayer_Mousemove (x, y);
		return;

	case m_load:
		M_Load_Mousemove (x, y);
		return;

	case m_save:
		M_Save_Mousemove (x, y);
		return;

	case m_skill:
		M_Skill_Mousemove (x, y);
		return;

	case m_multiplayer:
		M_MultiPlayer_Mousemove (x, y);
		return;

	case m_setup:
		M_Setup_Mousemove (x, y);
		return;

	case m_net:
		M_Net_Mousemove (x, y);
		return;

	case m_options:
		M_Options_Mousemove (x, y);
		return;

	case m_keys:
		M_Keys_Mousemove (x, y);
		return;

	case m_video:
		M_Video_Mousemove (x, y);
		return;

	case m_mods:
		M_Mods_Mousemove (x, y);
		return;

	//case m_help:
	//	M_Help_Mousemove (x, y);
	//	return;

	//case m_quit:
	//	M_Quit_Mousemove (x, y);
	//	return;

	case m_lanconfig:
		M_LanConfig_Mousemove (x, y);
		return;

	case m_gameoptions:
		M_GameOptions_Mousemove (x, y);
		return;

	//case m_search:
	//	M_Search_Mousemove (x, y);
	//	break;

	case m_slist:
		M_ServerList_Mousemove (x, y);
		return;
	}
}


void M_Charinput (int key)
{
	switch (m_state)
	{
	case m_setup:
		M_Setup_Char (key);
		return;
	case m_quit:
		M_Quit_Char (key);
		return;
	case m_lanconfig:
		M_LanConfig_Char (key);
		return;
	case m_mods:
		M_Mods_Char (key);
		return;
	default:
		return;
	}
}


qboolean M_TextEntry (void)
{
	switch (m_state)
	{
	case m_setup:
		return M_Setup_TextEntry ();
	case m_quit:
		return M_Quit_TextEntry ();
	case m_lanconfig:
		return M_LanConfig_TextEntry ();
	case m_mods:
		return M_Mods_TextEntry ();
	default:
		return false;
	}
}


qboolean M_KeyBinding (void)
{
	return key_dest == key_menu && m_state == m_keys && bind_grab;
}


void M_ConfigureNetSubsystem(void)
{
// enable/disable net systems to match desired config
	Cbuf_AddText ("stopdemo\n");

	if (IPXConfig || TCPIPConfig)
		net_hostport = lanConfig_port;
}


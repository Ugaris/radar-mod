/*
 * Mod options exposed in the client's Options screen ("Gameplay" tab).
 * Shared between the client (src/modder, src/gui) and mods (via amod_structs.h).
 * Keep this header self-contained: no other includes.
 */
#ifndef AMOD_OPTIONS_H
#define AMOD_OPTIONS_H

#define AMOD_OPT_HEADER 0 /* section header row (label only) */
#define AMOD_OPT_TOGGLE 1 /* checkbox: value 0/1 */
#define AMOD_OPT_SLIDER 2 /* slider: value in [min_val, max_val] */

/* Options tab a mod option row is shown in. Reported per index by the mod's
 * optional export `int amod_option_tab(int index)`; a mod without that export
 * (or one returning an unknown value) gets AMOD_TAB_GAMEPLAY, the historical
 * behavior. Kept out of struct amod_option so the struct layout stays
 * identical across client/mod version mixes. */
#define AMOD_TAB_GAMEPLAY 0
#define AMOD_TAB_AUDIO    1
#define AMOD_TAB_UI       2

struct amod_option {
	int type; /* AMOD_OPT_* */
	int value; /* current value */
	int min_val; /* slider range (ignored for toggle/header) */
	int max_val;
	char label[48]; /* user-visible label */
};

#endif

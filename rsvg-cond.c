/* vim: set sw=4: -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
   rsvg.c: SAX-based renderer for SVG files into a GdkPixbuf.

   Copyright (C) 2004 Dom Lachowicz <cinamod@hotmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Dom Lachowicz
*/

#include "config.h"

#include "rsvg.h"
#include "rsvg-private.h"
#include "rsvg-css.h"
#include "rsvg-styles.h"

#include <string.h>
#include <stdlib.h>
#include <locale.h>

gboolean rsvg_eval_switch_attributes (RsvgPropertyBag *atts);

static const char * implemented_features [] = 
	{
		"http://www.w3.org/TRr/SVG11/feature#BasicText",
		"http://www.w3.org/TR/SVG11/feature#BasicFilter",
		"http://www.w3.org/TR/SVG11/feature#BasicGraphicsAttribute",
		"http://www.w3.org/TR/SVG11/feature#BasicPaintAttribute",
		"http://www.w3.org/TR/SVG11/feature#BasicStructure",
		"http://www.w3.org/TR/SVG11/feature#ConditionalProcessing",
		"http://www.w3.org/TR/SVG11/feature#ContainerAttribute",
		"http://www.w3.org/TR/SVG11/feature#Filter",
		"http://www.w3.org/TR/SVG11/feature#Gradient",
		"http://www.w3.org/TR/SVG11/feature#Image",
		"http://www.w3.org/TR/SVG11/feature#Marker",
		"http://www.w3.org/TR/SVG11/feature#Mask",
		"http://www.w3.org/TR/SVG11/feature#OpacityAttribute",
		"http://www.w3.org/TR/SVG11/feature#Pattern",
		"http://www.w3.org/TR/SVG11/feature#Shape",
		"http://www.w3.org/TR/SVG11/feature#Structure",
		"http://www.w3.org/TR/SVG11/feature#Style",
		"http://www.w3.org/TR/SVG11/feature#SVG",
		"http://www.w3.org/TR/SVG11/feature#SVG-static",
		"http://www.w3.org/TR/SVG11/feature#View",
		"org.w3c.svg.static" /* deprecated SVG 1.0 feature string */
	};
static const guint nb_implemented_features = G_N_ELEMENTS(implemented_features);

static int 
rsvg_feature_compare(const void *a, const void *b)
{
	return strcmp((const char *)a, (const char *)b);
}

/* http://www.w3.org/TR/SVG/struct.html#RequiredFeaturesAttribute */
static gboolean 
rsvg_cond_parse_required_features (const char * value)
{
	guint nb_elems = 0;
	char ** elems;
	gboolean whatever = TRUE;

	elems = rsvg_css_parse_list(value, &nb_elems);

	if(elems && nb_elems) {
		guint i;

		for(i = 0; (i < nb_elems) && whatever; i++)
			if(!bsearch (elems[i], implemented_features, 
						 nb_implemented_features, sizeof(char *),
						 rsvg_feature_compare))
				whatever = FALSE;

		g_strfreev(elems);
	}

	return whatever;
}

static const char * implemented_extensions [] = 
	{
	};
static const guint nb_implemented_extensions = G_N_ELEMENTS(implemented_features);

/* http://www.w3.org/TR/SVG/struct.html#SystemLanguageAttribute */
static gboolean 
rsvg_cond_parse_required_extensions (const char * value)
{
	guint nb_elems = 0;
	char ** elems;
	gboolean whatever = TRUE;

	elems = rsvg_css_parse_list(value, &nb_elems);

	if(elems && nb_elems) {
		guint i;

		for(i = 0; (i < nb_elems) && whatever; i++)
			if(!bsearch (elems[i], implemented_extensions, 
						 nb_implemented_extensions, sizeof(char *),
						 rsvg_feature_compare))
				whatever = FALSE;

		g_strfreev(elems);
	}

	return whatever;
}

static gboolean
rsvg_locale_compare (const char * a, const char * b)
{
	/* TODO: we're required to do a lot more, but i'm lazy...
	   http://www.w3.org/TR/SVG/struct.html#SystemLanguageAttribute */
	return (!g_ascii_strcasecmp (a, b));
}

/* http://www.w3.org/TR/SVG/struct.html#SystemLanguageAttribute */
static gboolean 
rsvg_cond_parse_system_language (const char * value)
{
	guint nb_elems = 0;
	char ** elems;
	gboolean whatever = TRUE;

	elems = rsvg_css_parse_list(value, &nb_elems);

	if(elems && nb_elems) {
		guint i;
		gchar * locale = NULL;
		
		/* we're required to be pessimistic until we hit a language we recognize */
		whatever = FALSE;

#if defined(G_OS_WIN32)
		locale = g_win32_getlocale ();
#elif defined(HAVE_LC_MESSAGES)
		locale = g_strdup (setlocale (LC_MESSAGES, NULL));
#else
		locale = g_strdup (setlocale (LC_ALL, NULL));
#endif

		if (locale)
			{				
				for(i = 0; (i < nb_elems) && !whatever; i++) {
					if (rsvg_locale_compare (locale, elems[i]))
						whatever = TRUE;
				}
				
				g_free (locale);
			}

		g_strfreev(elems);
	}

	return whatever;
}

/* returns TRUE if this element should be processed according to <switch> semantics
   http://www.w3.org/TR/SVG/struct.html#SwitchElement */
gboolean 
rsvg_eval_switch_attributes (RsvgPropertyBag *atts)
{
	gboolean whatever = TRUE;

	if (atts && rsvg_property_bag_size (atts))
		{
			const char * value;

			if ((value = rsvg_property_bag_lookup (atts, "requiredFeatures")))
				whatever = rsvg_cond_parse_required_features (value);

			if (whatever && (value = rsvg_property_bag_lookup (atts, "requiredExtensions")))
				whatever = rsvg_cond_parse_required_extensions (value);

			if (whatever && (value = rsvg_property_bag_lookup (atts, "systemLanguage")))
				whatever = rsvg_cond_parse_system_language (value);
		}

	return whatever;
}
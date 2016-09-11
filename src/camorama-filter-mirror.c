/* This file is part of camorama
 *
 * AUTHORS
 *     Sven Herzberg  <herzi@gnome-de.org>
 *
 * Copyright (C) 2006  Sven Herzberg <herzi@gnome-de.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "filter.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib/gi18n.h>

#include <stdio.h>
#include <stdlib.h>

/* GType stuff for CamoramaFilterMirror */
typedef struct _CamoramaFilter      CamoramaFilterMirror;
typedef struct _CamoramaFilterClass CamoramaFilterMirrorClass;

G_DEFINE_TYPE(CamoramaFilterMirror, camorama_filter_mirror, CAMORAMA_TYPE_FILTER);

static void
camorama_filter_mirror_init(CamoramaFilterMirror* self) {}

static gint oldWidth = -1;
static gint oldHeight = -1;
static gint oldDepth = -1;

static long *lastSignal = 0;
static long *lastHigh = 0;
static long *lastHighThenLow = 0;
static long *output = 0;

static int lowPassTC = 1;
static int highPassTC = 10;

static int count = 0;

char debug = 1;

static void MaybeNewMemory(gint width, gint height, gint depth)
{
	long needed, i, memory;

	if(width == oldWidth && height == oldHeight && depth == oldDepth)
		return;

	if(lastSignal)
		free(lastSignal);
	if(lastHigh)
		free(lastHigh);
	if(lastHighThenLow)
		free(lastHighThenLow);
	if(output)
		free(output);

	if(debug)
		printf("\n\nwidth: %d, height: %d, depth: %d\n\n", width, height, depth);

	needed = 5 + (width+1)*(height+1);
	memory = needed*sizeof(long);
	lastSignal = (long *)malloc(memory);
	if (!lastSignal)
	{
		printf("ERROR: Cannot malloc image memory for lastSignal\n");
		return;
	}
	lastHigh = (long *)malloc(memory);
	if (!lastHigh)
	{
		printf("ERROR: Cannot malloc image memory for lastHigh\n");
		return;
	}
	lastHighThenLow = (long *)malloc(memory);
	if (!lastHighThenLow)
	{
		printf("ERROR: Cannot malloc image memory for lastHighThenLow\n");
		return;
	}
	output = (long *)malloc(memory);
	if (!output)
	{
		printf("ERROR: Cannot malloc image memory for output\n");
		return;
	}
	for(i=0; i < needed; i++)
	{
		lastSignal[i] = 127;
		lastHigh[i] = 127;
		lastHighThenLow[i] = 127;
	}

	oldWidth = width;
	oldHeight = height;
	oldDepth = depth;
}


static void
camorama_filter_mirror_filter(CamoramaFilter* filter, guchar *image, gint width, gint height, gint depth) 
{
	gint x, y, z, row_length, row, column, thisPixel, thisXY, thatXY;

	long signal, thisHigh, thisHighThenLow, thatHigh, thatHighThenLow, thatSignal, newValue, max, min, scale;

	MaybeNewMemory(width, height, depth);

	max = LONG_MIN;
	min = LONG_MAX;
	thatSignal = 0;

	row_length   = width * depth;
	for(y = 0; y < height; y++) 
	{
		row = y*row_length;
		for (x = 1; x < width; x++) 
		{
			column = x*depth;
			thisPixel = row + column;
			thisXY = y*width+x;
			thatXY = thisXY - 1; 

			// Go to grey

			signal = 0;
			for (z = 0; z < depth; z++) 
			{
				signal += image[thisPixel + z];
			}
			signal = signal/depth;

			// Apply Reichardt
			
			thisHigh = (highPassTC*(lastHigh[thisXY] + signal - lastSignal[thisXY]))/(highPassTC + 1);
			thisHighThenLow = (thisHigh + lastHighThenLow[thisXY]*lowPassTC)/(lowPassTC + 1);
			thatHigh = (highPassTC*(lastHigh[thatXY] + thatSignal - lastSignal[thatXY]))/(highPassTC + 1);
			thatHighThenLow = (thatHigh + lastHighThenLow[thatXY]*lowPassTC)/(lowPassTC + 1);

			newValue = thisHighThenLow*thatHigh - thatHighThenLow*thisHigh;
			output[thisXY] = newValue;

			if(newValue > max)
				max = newValue;
			if(newValue < min)
				min = newValue;			

			// Remember for next time

			thatSignal = signal;
			lastHigh[thisXY] = thisHigh;
			lastHighThenLow[thisXY] = thisHighThenLow;
			lastSignal[thisXY] = signal;
		}
	}
	count++;
	if(debug && !(count%50))
	{
		printf("\nmax: %ld, min: %ld\n", max, min);
	}

	scale = max - min;
	if(scale == 0)
		scale = 1;

	for(y = 0; y < height; y++) 
	{
		row = y*row_length;
		for (x = 1; x < width; x++) 
		{
			column = x*depth;
			thisPixel = row + column;
			thisXY = y*width+x;
			signal = ((output[thisXY] - min)*255)/scale;
			if(signal > 255)
				signal = 255;
			if(signal < 0)
				signal = 0;
			for (z = 0; z < depth; z++) 
			{
				image[thisPixel + z] = (guchar)signal;
			}
		}
	}
}

static void
camorama_filter_mirror_class_init(CamoramaFilterMirrorClass* self_class) {
	self_class->filter = camorama_filter_mirror_filter;
	// TRANSLATORS: This is a noun
	self_class->name   = _("Mirror");
}



/*  Copyright (c) 2003-2007 Xfce Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "weather-parsers.h"
#include <libxfce4panel/libxfce4panel.h>
#define _XOPEN_SOURCE
#include <time.h>
#include <stdlib.h>

static time_t my_timegm(struct tm *tm)
{
	time_t ret;
	char *tz;

	tz = getenv("TZ");
	setenv("TZ", "", 1);
	tzset();
	ret = mktime(tm);
	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
	tzset();
	return ret;
}

xml_weather *
parse_weather (xmlNode *cur_node)
{
  xml_weather *ret;
  xmlNode     *child_node;
  guint        i = 0;

  if (!NODE_IS_TYPE (cur_node, "weatherdata"))
    {
      return NULL;
    }

  if ((ret = g_slice_new0 (xml_weather)) == NULL)
    return NULL;

  ret->num_timeslices = 0;
  for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
    {
      if (cur_node->type != XML_ELEMENT_NODE)
        continue;

      if (NODE_IS_TYPE (cur_node, "product"))
        {
	  gchar *class = xmlGetProp (cur_node, (const xmlChar *) "class");
	  if (xmlStrcasecmp(class, "pointData")) {
		xmlFree(class);
		continue;
	  }
	  g_free(class);
          for (child_node = cur_node->children; child_node;
               child_node = child_node->next)
            {
              if (NODE_IS_TYPE (child_node, "time"))
                {
                  parse_time(child_node, ret);
                }
            }
        }
    }

  return ret;
}

void parse_time (xmlNode * cur_node, xml_weather * data) {
	gchar *datatype = xmlGetProp (cur_node, (const xmlChar *) "datatype");
	gchar *start = xmlGetProp (cur_node, (const xmlChar *) "from");
	gchar *end = xmlGetProp (cur_node, (const xmlChar *) "to");
	struct tm start_t, end_t;
	time_t start_ts, end_ts;
	time_t cur_ts;

	if (xmlStrcasecmp(datatype, "forecast")) {
		xmlFree(datatype);
		xmlFree(start);
		xmlFree(end);
		return;
	}

	xmlFree(datatype);

	if (strptime(start, "%Y-%m-%dT%H:%M:%SZ", &start_t) == NULL) {
		xmlFree(start);
		xmlFree(end);
		return;
	}

	if (strptime(end, "%Y-%m-%dT%H:%M:%SZ", &end_t) == NULL) {
		xmlFree(start);
		xmlFree(end);
		return;
	}

	xmlFree(start);
	xmlFree(end);

	start_ts = my_timegm(&start_t);
	end_ts = my_timegm(&end_t);
	
	/* split per-hour */
	for (cur_ts = start_ts; cur_ts < end_ts; cur_ts += 3600) {
		xml_time *timeslice = get_timeslice(data, cur_ts, cur_ts + 3600);
		xmlNode *child_node;

		if (!timeslice)
			return;
		for (child_node = cur_node->children; child_node;
		     child_node = child_node->next) {
			if (NODE_IS_TYPE (child_node, "location")) {
				if (timeslice->location == NULL)
					timeslice->location =
						g_slice_new0(xml_location);
				parse_location(child_node, timeslice->location);
			}
		}
	}
}

xml_time *get_timeslice(xml_weather *data, time_t start, time_t end)
{
	int i;
	for (i = 0; i < data->num_timeslices; i++) {
		if (data->timeslice[i]->start == start
		 && data->timeslice[i]->end == end)
			return data->timeslice[i];
	}
	if (data->num_timeslices == MAX_TIMESLICE -1)
		return NULL;

	data->timeslice[data->num_timeslices] = g_slice_new0(xml_time);
	data->num_timeslices++;

	return data->timeslice[data->num_timeslices - 1];
}

void parse_location (xmlNode * cur_node, xml_location *loc)
{
	xmlNode *child_node;

	for (child_node = cur_node->children; child_node;
	     child_node = child_node->next) {
		if (NODE_IS_TYPE (child_node, "temperature")) {
			g_free(loc->temperature_unit);
			g_free(loc->temperature_value);
			loc->temperature_unit = PROP(child_node, "unit");
			loc->temperature_value = PROP(child_node, "value");
		}
		if (NODE_IS_TYPE (child_node, "windDirection")) {
			g_free(loc->wind_dir_deg);
			g_free(loc->wind_dir_name);
			loc->wind_dir_deg = PROP(child_node, "deg");
			loc->wind_dir_name = PROP(child_node, "name");
		}
		if (NODE_IS_TYPE (child_node, "windSpeed")) {
			g_free(loc->wind_speed_mps);
			g_free(loc->wind_speed_beaufort);
			loc->wind_speed_mps = PROP(child_node, "mps");
			loc->wind_speed_beaufort = PROP(child_node, "beaufort");
		}
		if (NODE_IS_TYPE (child_node, "humidity")) {
			g_free(loc->humidity_unit);
			g_free(loc->humidity_value);
			loc->humidity_unit = PROP(child_node, "unit");
			loc->humidity_value = PROP(child_node, "value");
		}
		if (NODE_IS_TYPE (child_node, "pressure")) {
			g_free(loc->pressure_unit);
			g_free(loc->pressure_value);
			loc->pressure_unit = PROP(child_node, "unit");
			loc->pressure_value = PROP(child_node, "value");
		}
		if (NODE_IS_TYPE (child_node, "fog")) {
			g_free(loc->fog_percent);
			loc->fog_percent = PROP(child_node, "percent");
		}
		if (NODE_IS_TYPE (child_node, "lowClouds")) {
			g_free(loc->cloudiness_percent[CLOUDINESS_LOW]);
			loc->cloudiness_percent[CLOUDINESS_LOW] = PROP(child_node, "percent");
		}
		if (NODE_IS_TYPE (child_node, "mediumClouds")) {
			g_free(loc->cloudiness_percent[CLOUDINESS_MED]);
			loc->cloudiness_percent[CLOUDINESS_MED] = PROP(child_node, "percent");
		}
		if (NODE_IS_TYPE (child_node, "highClouds")) {
			g_free(loc->cloudiness_percent[CLOUDINESS_HIGH]);
			loc->cloudiness_percent[CLOUDINESS_HIGH] = PROP(child_node, "percent");
		}
		if (NODE_IS_TYPE (child_node, "precipitation")) {
			g_free(loc->precipitation_unit);
			g_free(loc->precipitation_value);
			loc->precipitation_unit = PROP(child_node, "unit");
			loc->precipitation_value = PROP(child_node, "value");
		}
		if (NODE_IS_TYPE (child_node, "symbol")) {
			g_free(loc->symbol);
			loc->symbol = PROP(child_node, "id");
		}
	}
}

void
xml_weather_free (xml_weather *data)
{
  guint i;

  g_slice_free (xml_weather, data);
}

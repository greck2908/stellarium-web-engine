/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <float.h>
#include <zlib.h> // For crc32.

typedef struct city {
    obj_t       obj;
    double      longitude;
    double      latitude;
    double      elevation;
    char        country_code[3];
    char        *timezone;
} city_t;

static obj_klass_t city_klass = {
    .id = "city",
    .size = sizeof(city_t),
    .attributes = (attribute_t[]) {
        PROPERTY("name"),
        PROPERTY("longitude", "f", MEMBER(city_t, longitude)),
        PROPERTY("latitude", "f", MEMBER(city_t, latitude)),
        PROPERTY("elevation", "f", MEMBER(city_t, elevation)),
        PROPERTY("timezone", "s", MEMBER(city_t, timezone)),
        {}
    },
};
OBJ_REGISTER(city_klass)

typedef struct cities {
    obj_t       obj;
} cities_t;

static int cities_init(obj_t *obj, json_value *args);
static obj_t *cities_get(const obj_t *obj, const char *id, int flags);
static obj_t *cities_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint);
static obj_klass_t cities_klass = {
    .id = "cities",
    .size = sizeof(cities_t),
    .flags = OBJ_MODULE,
    .init = cities_init,
    .get = cities_get,
    .get_by_oid = cities_get_by_oid,
};

OBJ_REGISTER(cities_klass)

static void add_cities(cities_t *cities);

static int cities_init(obj_t *obj, json_value *args)
{
    cities_t *cities = (void*)obj;
    add_cities(cities);
    return 0;
}

static obj_t *cities_get(const obj_t *obj, const char *id, int flags)
{
    obj_t *city;
    if (!str_startswith(id, "CITY ")) return NULL;
    DL_FOREACH(obj->children, city) {
        if (strcmp(city->id, id) == 0) {
            city->ref++; // XXX: make it singleton.
            return city;
        }
    }
    return NULL;
}

static obj_t *cities_get_by_oid(const obj_t *obj, uint64_t oid, uint64_t hint)
{
    obj_t *city;
    if (!oid_is_catalog(oid, "CITY")) return NULL;
    DL_FOREACH(obj->children, city) {
        if (city->oid == oid) {
            city->ref++;
            return city;
        }
    }
    return NULL;
}

static void add_cities(cities_t *cities)
{
    char *data, *pos;
    char *name, *asciiname, *country_code, *timezone;
    char asciiname_upper[256], id[256];
    double lat, lon, el;
    city_t *city;

    pos = data = strdup(asset_get_data("asset://cities.txt", NULL, NULL));
    assert(pos);
    while (pos && *pos) {
#define TOK(d, s) ({char *r = d; d = strchr(d, s); *d++ = '\0'; r;})
        name = TOK(pos, '\t');
        asciiname = TOK(pos, '\t');
        str_to_upper(asciiname, asciiname_upper);
        lat = atof(TOK(pos, '\t'));
        lon = atof(TOK(pos, '\t'));
        el = atof(TOK(pos, '\t'));
        country_code = TOK(pos, '\t');
        timezone = TOK(pos, '\n');

        sprintf(id, "CITY %s %s", country_code, asciiname_upper);
        city = (city_t*)obj_create("city", id, (obj_t*)cities, NULL);
        city->obj.oid = oid_create("CITY", crc32(0, (void*)id, strlen(id)));

        strcpy(city->country_code, country_code);
        city->timezone = strdup(timezone);
        city->longitude = lon * DD2R;
        city->latitude = lat * DD2R;
        city->elevation = el;
        sprintf(id, "%s (%s)", name, country_code);
        identifiers_add("NAME", id, city->obj.oid, 0, "CITY", 0,
                        asciiname_upper, id);
#undef TOK
    }
    free(data);
}

EMSCRIPTEN_KEEPALIVE
obj_t *city_create(const char *name, const char *country_code,
                   const char *timezone,
                   double latitude,
                   double longitude,
                   double elevation,
                   double nearby)
{
    obj_t *cities = obj_get(&core->obj, "CITIES", 0);
    char id[256], namebuf[256], asciiname_upper[256];
    double dist, best_dist = FLT_MAX;
    const double EARTH_RADIUS_KM = 6371;
    city_t *city, *best = NULL;
    if (isnan(elevation)) elevation = 0;

    sprintf(id, "CITY %s %s", country_code, name);
    str_to_upper(id, id);
    // First search for a nearby city.
    if (!isnan(nearby)) {
        OBJ_ITER(cities, city, "city") {
            dist = EARTH_RADIUS_KM *
                        eraSeps(longitude, latitude,
                                city->longitude, city->latitude);
            if (dist > nearby) continue;
            if (strcmp(city->obj.id, id) == 0) {
                return &city->obj;
            }
            if (dist < best_dist) {
                best_dist = dist;
                best = city;
            }
        }
    }
    if (best) return &best->obj;

    city = (city_t*)obj_create("city", id, (obj_t*)cities, NULL);
    if (country_code) strcpy(city->country_code, country_code);
    if (timezone) city->timezone = strdup(timezone);
    city->latitude = latitude;
    city->longitude = longitude;
    city->elevation = elevation;
    str_to_upper(name, asciiname_upper);
    city->obj.oid = oid_create("CITY", crc32(0, (void*)id, strlen(id)));
    sprintf(namebuf, "%s (%s)", name, country_code);
    identifiers_add("NAME", namebuf, city->obj.oid, 0, "CITY", 0,
                    asciiname_upper, id);
    return &city->obj;
}

#if COMPILE_TESTS

static void test_cities(void)
{
    obj_t *city;
    const char *name;
    const char tz[64];
    double lat;

    core_init(100, 100, 1.0);
    city = obj_get(NULL, "CITY GB London", 0);
    assert(city);
    name = identifiers_get(city->oid, "NAME");
    test_str(name, "London (GB)");
    obj_get_attr(city, "timezone", "S", tz);
    test_str(tz, "Europe/London");
    obj_get_attr(city, "latitude", "f", &lat);
    assert(fabs(lat * DR2D - 51.50853) < 0.01);
    city = city_create("taipei", "TW", NULL,
                       25.09319 * DD2R, 121.558442 * DD2R, 0, 100);
    assert(city);
}

TEST_REGISTER(NULL, test_cities, TEST_AUTO);

#endif

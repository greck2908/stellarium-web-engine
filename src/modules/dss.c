/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

// HIPS survey support.

typedef struct dss {
    obj_t       obj;
    fader_t     visible;
    hips_t      *hips;
} dss_t;

static int dss_init(obj_t *obj, json_value *args)
{
    dss_t *dss = (void*)obj;
    fader_init(&dss->visible, true); // Visible by default.
    return 0;
}

static int dss_render(const obj_t *obj, const painter_t *painter)
{
    PROFILE(dss_render, 0);
    double visibility;
    dss_t *dss = (dss_t*)obj;
    painter_t painter2 = *painter;
    double lum, c;

    if (dss->visible.value == 0.0) return 0;
    if (!dss->hips) return 0;
    // Fade the survey between 20° and 10° fov.
    visibility = smoothstep(20 * DD2R, 10 * DD2R, core->fov);
    painter2.color[3] *= dss->visible.value * visibility;

    // Adjust for eye adaptation.
    lum = 0.075;
    c = tonemapper_map(&core->tonemapper, lum);
    c = clamp(c, 0, 1);
    painter2.color[3] *= c;

    if (painter2.color[3] == 0.0) return 0;
    return hips_render(dss->hips, &painter2, 2 * M_PI);
}

static int dss_update(obj_t *obj, const observer_t *obs, double dt)
{
    dss_t *dss = (dss_t*)obj;
    return fader_update(&dss->visible, dt);
}

static int dss_add_data_source(
        obj_t *obj, const char *url, const char *type, json_value *args)
{
    dss_t *dss = (dss_t*)obj;
    const char *title, *release_date_str;
    double release_date = 0;

    if (dss->hips) return 1;
    if (!type || !args || strcmp(type, "hips")) return 1;
    title = json_get_attr_s(args, "obs_title");
    if (!title || strcasecmp(title, "DSS colored") != 0) return 1;
    release_date_str = json_get_attr_s(args, "hips_release_date");
    if (release_date_str)
        release_date = hips_parse_date(release_date_str);
    dss->hips = hips_create(url, release_date, NULL);
    return 0;
}

/*
 * Meta class declarations.
 */

static obj_klass_t dss_klass = {
    .id = "dss",
    .size = sizeof(dss_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = dss_init,
    .update = dss_update,
    .render = dss_render,
    .render_order = 6,
    .add_data_source = dss_add_data_source,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b", MEMBER(dss_t, visible.target)),
        {}
    },
};
OBJ_REGISTER(dss_klass)

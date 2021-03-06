/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

// Extra flags (on top of anchors flags).
enum {
    SKIPPED = 1 << 16,
};

typedef struct label label_t;
struct label
{
    label_t *next, *prev;
    char    *text; // Original passed text.
    char    *render_text; // Processed text (can point to text).
    double  pos[2];
    double  radius;     // Radius of the object (pixel).
    double  size;
    double  color[4];
    double  angle;
    int     flags;
    fader_t fader;

    double  priority;
    double  box[4];
};

typedef struct labels {
    obj_t obj;
    bool skip_selection; // If set, do no render the core selection label.
    label_t *labels;
} labels_t;

static labels_t *g_labels = NULL;

void labels_reset(void)
{
    label_t *label, *tmp;
    DL_FOREACH_SAFE(g_labels->labels, label, tmp) {
        if (label->fader.target == false && label->fader.value == 0) {
            DL_DELETE(g_labels->labels, label);
            if (label->render_text != label->text) free(label->render_text);
            free(label->text);
            free(label);
        } else {
            label->fader.target = false;
        }
    }
}

static label_t *label_get(label_t *list, const char *txt, double size,
                          const double pos[2])
{
    label_t *label;
    DL_FOREACH(list, label) {
        if (label->size == size && strcmp(txt, label->text) == 0)
            return label;
    }
    return NULL;
}

static void label_get_box(const painter_t *painter, const label_t *label,
                          int anchor, double box[4])
{
    double borders[2];
    double pos[2];
    int size[2];
    double border = 4;

    vec2_copy(label->pos, pos);
    paint_text_size(painter, label->render_text, label->size, size);

    borders[0] = label->radius;
    borders[1] = label->radius;

    if (anchor & ANCHOR_LEFT) pos[0] += size[0] / 2 + border + borders[0];
    if (anchor & ANCHOR_RIGHT) pos[0] -= size[0] / 2 + border + borders[0];
    if (anchor & ANCHOR_BOTTOM) pos[1] -= size[1] / 2 + border + borders[1];
    if (anchor & ANCHOR_TOP) pos[1] += size[1] / 2 + border + borders[1];

    box[0] = pos[0] - size[0] / 2;
    box[1] = pos[1] - size[1] / 2;
    box[2] = pos[0] + size[0] / 2;
    box[3] = pos[1] + size[1] / 2;
}

static bool label_get_boxes(const painter_t *painter, const label_t *label,
                            int i, double box[4])
{
    const int anchors_around[] = {ANCHOR_BOTTOM_LEFT, ANCHOR_BOTTOM_RIGHT,
                                  ANCHOR_TOP_LEFT, ANCHOR_TOP_RIGHT};
    const int anchors_over[] = {ANCHOR_CENTER, ANCHOR_TOP,
                                ANCHOR_BOTTOM, ANCHOR_LEFT, ANCHOR_RIGHT};
    if (label->flags & ANCHOR_FIXED) {
        if (i != 0) return false;
        label_get_box(painter, label, label->flags, box);
        return true;
    }
    if (label->flags & ANCHOR_AROUND) {
        if (i >= 4) return false;
        label_get_box(painter, label, anchors_around[i], box);
    }
    if (label->flags & ANCHOR_CENTER) {
        if (i >= 5) return false;
        label_get_box(painter, label, anchors_over[i], box);
    }
    return true;
}

static bool box_overlap(const double a[4], const double b[4])
{
    return a[2] >  b[0] &&
           a[0] <= b[2] &&
           a[3] >  b[1] &&
           a[1] <= b[3];
}

static bool test_label_overlaps(const label_t *label)
{
    label_t *other;
    if (label->flags & ANCHOR_FIXED) return false;
    DL_FOREACH(g_labels->labels, other) {
        if (other == label) break;
        if (other->flags & SKIPPED) continue;
        if (box_overlap(other->box, label->box)) return true;
    }
    return false;
}

static int label_cmp(void *a, void *b)
{
    return cmp(((label_t*)b)->priority, ((label_t*)a)->priority);
}

static int labels_init(obj_t *obj, json_value *args)
{
    g_labels = (void*)obj;
    return 0;
}

static int labels_render(const obj_t *obj, const painter_t *painter)
{
    label_t *label;
    int i;
    double pos[2], color[4];
    DL_SORT(g_labels->labels, label_cmp);
    DL_FOREACH(g_labels->labels, label) {
        // We fade in the label slowly, but fade out very fast, otherwise
        // we don't get updated positions for fading out labels.
        fader_update(&label->fader, label->fader.target ? 0.01 : 1);
        for (i = 0; ; i++) {
            if (!label_get_boxes(painter, label, i, label->box)) {
                label->flags |= SKIPPED;
                goto skip;
            }
            if (!test_label_overlaps(label)) break;
        }
        pos[0] = (label->box[0] + label->box[2]) / 2;
        pos[1] = (label->box[1] + label->box[3]) / 2;
        vec4_copy(label->color, color);
        color[3] *= label->fader.value;
        paint_text(painter, label->render_text, pos, label->size,
                   color, label->angle);
        label->flags &= ~SKIPPED;
skip:;
    }
    return 0;
}

/*
 * Function: labels_add
 * Render a label on screen.
 *
 * Parameters:
 *   text       - The text to render.
 *   win_pow    - Position of the text in windows coordinates.
 *   radius     - Radius of the point the label is linked to.  Zero for
 *                independent label.
 *   size       - Height of the text in pixel.
 *   color      - Color of the text.
 *   angle      - Rotation angle (rad).
 *   flags      - Union of <LABEL_FLAGS>.  Used to specify anchor position
 *                and text effects.
 *   oid        - Optional unique id for the label.
 */
void labels_add(const char *text, const double pos[2],
                double radius, double size, const double color[4],
                double angle, int flags, double priority, uint64_t oid)
{
    if (flags & ANCHOR_FIXED) priority = 1024.0; // Use FLT_MAX ?
    assert(priority <= 1024.0);
    assert(color);
    label_t *label;

    if (!text || !*text) return;
    if (    g_labels->skip_selection && oid && core->selection &&
            oid == core->selection->oid) {
        return;
    }
    label = label_get(g_labels->labels, text, size, pos);
    if (!label) {
        label = calloc(1, sizeof(*label));
        fader_init(&label->fader, false);
        label->render_text = label->text = strdup(text);
        if (flags & LABEL_UPPERCASE) {
            label->render_text = malloc(strlen(text) + 64);
            u8_upper(label->render_text, text, strlen(text) + 64);
        }
        DL_APPEND(g_labels->labels, label);
    }

    vec2_set(label->pos, pos[0], pos[1]);
    label->radius = radius;
    label->size = size;
    vec4_set(label->color, color[0], color[1], color[2], color[3]);
    label->angle = angle;
    label->flags = flags;
    label->priority = priority;
    label->fader.target = true;
}


/*
 * Meta class declarations.
 */

static obj_klass_t labels_klass = {
    .id = "labels",
    .size = sizeof(labels_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = labels_init,
    .render = labels_render,
    .render_order = 100,
    .attributes = (attribute_t[]) {
        PROPERTY("skip_selection", "b", MEMBER(labels_t, skip_selection)),
        {},
    },
};

OBJ_REGISTER(labels_klass)

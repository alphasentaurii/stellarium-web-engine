/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <zlib.h> // For crc32.

typedef struct
{
    int     n;              // Number of step in the full circle.
    int     level;          // Split level at which to render.
    uint8_t splits[16];     // Split decomposition to reach the step.
} step_t;

static const step_t STEPS_DEG[] = {
    {      1,  0, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 360°
    {      2,  1, {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 180°
    {      4,  2, {2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  90°
    {      6,  2, {2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  60°

    {     12,  3, {2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  30°
    {     18,  3, {2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  20°
    {     24,  4, {2, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  15°
    {     36,  4, {2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  10°
    {     72,  5, {2, 2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //   5°
    {    180,  5, {2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //   2°
    {    360,  6, {2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //   1°

    {    720,  7, {2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  30'
    {   1080,  7, {2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  20'
    {   1440,  8, {2, 2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1}}, //  15'
    {   2160,  8, {2, 2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1}}, //  10'
    {   4320,  9, {2, 2, 2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1}}, //   5'
    {  10800,  9, {2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1, 1, 1, 1}}, //   2'
    {  21600, 10, {2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1, 1, 1}}, //   1'

    {  43200, 11, {2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1, 1}}, //  30"
    {  64800, 11, {2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 1, 1, 1, 1, 1}}, //  20"
    {  86400, 12, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1}}, //  15"
    { 129600, 12, {2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 1, 1, 1, 1}}, //  10"
    { 259200, 13, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 1, 1, 1}}, //   5"
    { 648000, 13, {2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 5, 1, 1, 1}}, //   2"
    {1296000, 14, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 5, 5, 5, 1, 1}}, //   1"
};

static const step_t STEPS_HOUR[] = {
    {      1,  0, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 24h
    {      2,  1, {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 12h
    {      3,  1, {3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  8h
    {      6,  2, {2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  4h
    {     12,  3, {2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  2h
    {     24,  4, {2, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  1h

    {     48,  5, {2, 2, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 30m
    {     72,  5, {2, 2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 20m
    {     96,  6, {2, 2, 2, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 15m
    {    144,  6, {2, 2, 2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, // 10m
    {    288,  7, {2, 2, 2, 2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  5m
    {    720,  7, {2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1}}, //  2m
    {   1440,  8, {2, 2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1}}, //  1m

    {   2880,  9, {2, 2, 2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1}}, // 30s
    {   4320,  9, {2, 2, 2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1, 1, 1}}, // 20s
    {   5760, 10, {2, 2, 2, 2, 2, 2, 2, 3, 3, 5, 1, 1, 1, 1, 1, 1}}, // 15s
    {   8640, 10, {2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1, 1}}, // 10s
    {  17280, 11, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 1, 1, 1, 1, 1}}, //  5s
    {  43200, 11, {2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1, 1}}, //  2s
    {  86400, 12, {2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 5, 5, 1, 1, 1, 1}}, //  1s
};

static struct {
    const char  *name;
    const char  *id;
    uint32_t    color;
    int         frame;      // One of the FRAME_ value.
    char        format;     // 'h' for hour, 'd' for degree. 0 for no labels.
    bool        grid;       // If true render the whole grid.
} LINES[] = {
    {
        .name       = "Azimuthal",
        .id         = "azimuthal",
        .color      = 0x4c3319ff,
        .frame      = FRAME_OBSERVED,
        .format     = 'd',
        .grid       = true,
    },
    {
        .name       = "Equatorial",
        .id         = "equatorial",
        .color      = 0x2a81ad80,
        .frame      = FRAME_ICRF,
        .format     = 'h',
        .grid       = true,
    },
    {
        .name       = "Meridian",
        .id         = "meridian",
        .color      = 0x339933ff,
        .frame      = FRAME_OBSERVED,
        .grid       = false,
    },
    {
        .name       = "Ecliptic",
        .id         = "ecliptic",
        .color      = 0xb33333ff,
        .frame      = FRAME_OBSERVED, // XXX: probably need to change that.
        .grid       = false,
    },
};

typedef struct lines lines_t;
struct lines {
    obj_t       obj;
};

typedef struct line line_t;
struct line {
    obj_t           obj;
    fader_t         visible;
    int             frame;      // One of FRAME_ value.
    char            format;     // 'd', 'h', or 0
    bool            grid;       // If true render the whole grid.
    double          color[4];
};

static int lines_init(obj_t *obj, json_value *args)
{
    obj_t *lines = obj;
    int i;
    line_t *line;

    for (i = 0; i < ARRAY_SIZE(LINES); i++) {
        line = (line_t*)obj_create("line", LINES[i].id, lines, NULL);
        obj_add_sub(&line->obj, "lines");
        line->frame = LINES[i].frame;
        line->grid = LINES[i].grid;
        hex_to_rgba(LINES[i].color, line->color);
        line->format = LINES[i].format;
        fader_init(&line->visible, false);
    }
    return 0;
}

static int lines_update(obj_t *obj, const observer_t *obs, double dt)
{
    obj_t *line;
    int ret = 0;
    DL_FOREACH(obj->children, line)
        ret |= line->klass->update(line, obs, dt);
    return ret;
}

static int lines_render(const obj_t *obj, const painter_t *painter)
{
    PROFILE(lines_render, 0);
    obj_t *line;
    OBJ_ITER(obj, line, "line")
        line->klass->render(line, painter);
    return 0;
}

static void lines_gui(obj_t *obj, int location)
{
    int i;
    if (!DEFINED(SWE_GUI)) return;
    if (location == 0 && gui_tab("Grids")) {
        for (i = 0; i < ARRAY_SIZE(LINES); i++) {
            gui_item(&(gui_item_t){
                    .label = LINES[i].name,
                    .obj = obj_get(obj, LINES[i].id, 0),
                    .attr = "visible"});
        }
        gui_tab_end();
    }
}

static int line_update(obj_t *obj, const observer_t *obs, double dt)
{
    bool changed = false;
    line_t *line = (line_t*)obj;
    changed |= fader_update(&line->visible, dt);
    return changed ? 1 : 0;
}

static void spherical_project(
        const projection_t *proj, int flags, const double *v, double *out)
{
    double az, al;
    az = v[0] * 360 * DD2R;
    al = (v[1] - 0.5) * 180 * DD2R;
    eraS2c(az, al, out);
    out[3] = 0; // Project to infinity.
}

static bool check_borders(const double a[3], const double b[3],
                          const projection_t *proj,
                          double p[2], double u[2],
                          double v[2]);

/*
 * Function: render_label
 * Render the border label
 *
 * Parameters:
 *   p      - Position of the border intersection.
 *   u      - Direction of the line.
 *   v      - Normal of the window border inward.
 *   dir    - 0: alt, 1: az
 */
static void render_label(const double p[2], const double u[2],
                         const double v[2], const double uv[2],
                         int dir, line_t *line, int step,
                         const painter_t *painter)
{
    char buff[32];
    double pos[2];
    double a, color[4], label_angle;
    char s;
    int h[4];
    double n[2];
    double bounds[4], size[2];

    vec2_normalize(u, n);

    // Give up if angle with screen is too acute.
    if (fabs(vec2_dot(n, v)) < 0.25) return;

    if (vec2_dot(n, v) < 0) {
        vec2_mul(-1, u, u);
        vec2_mul(-1, n, n);
    }

    label_angle = atan2(u[1], u[0]);
    if (fabs(label_angle) > M_PI / 2) label_angle += M_PI;

    if (dir == 0) a = mix(-90, +90 , uv[1]) * DD2R;
    else          a = mix(  0, +360, uv[0]) * DD2R;
    if (dir == 0 || line->format == 'd') {
        eraA2af(1, a, &s, h);
        if (step <= 360)
            sprintf(buff, "%c%d°", s, h[0]);
        else if (step <= 21600)
            sprintf(buff, "%c%d°%2d'", s, h[0], h[1]);
        else
            sprintf(buff, "%c%d°%2d'%2d\"", s, h[0], h[1], h[2]);
    } else {
        eraA2tf(1, a, &s, h);
        if (step <= 24)
            sprintf(buff, "%c%dh", s, h[0]);
        else  if (step <= 1440)
            sprintf(buff, "%c%dh%2d", s, h[0], h[1]);
        else
            sprintf(buff, "%c%dh%2dm%2ds", s, h[0], h[1], h[2]);
    }

    paint_text_bounds(painter, buff, p, ALIGN_CENTER | ALIGN_MIDDLE,
                      13, bounds);
    size[0] = bounds[2] - bounds[0];
    size[1] = bounds[3] - bounds[1];

    vec2_normalize(u, n);
    pos[0] = p[0] + n[0] * size[0] / 2 + v[0] * 4;
    pos[1] = p[1] + n[1] * size[0] / 2 + v[1] * 4;
    pos[0] += fabs(v[1]) * size[1] * 0.7;
    pos[1] += fabs(v[0]) * size[1] * 0.7;

    vec4_copy(painter->color, color);

    color[3] = 1.0;
    paint_text(painter, buff, pos, ALIGN_CENTER | ALIGN_MIDDLE, 13,
               color, label_angle);
}

int on_quad(int step, qtree_node_t *node,
            const double uv[4][2], const double pos[4][4],
            const painter_t *painter_,
            void *user, int s[2])
{
    double lines[4][4] = {};
    double p[2], u[2], v[2]; // For the border labels.
    bool visible;
    projection_t *proj_spherical = USER_GET(user, 0);
    line_t *line = USER_GET(user, 1);
    step_t **steps = USER_GET(user, 2);
    int dir;
    painter_t painter = *painter_;

    // Compute the next split.
    if (step == 0) {
        s[0] = steps[0]->splits[node->level];
        s[1] = steps[1]->splits[node->level + 1];
        if (s[0] == 1 && s[1] == 1) {
            s[0] = s[1] = 2;
        }
        if (node->level == 0) return 1;
        return 2;
    }

    if (step == 1) { // After visibility check.
        if (node->level < min(steps[0]->level, steps[1]->level)) return 1;
        if (node->level < 2) return 1;
        return 2;
    }
    vec2_copy(uv[0], lines[0]);
    vec2_copy(uv[2], lines[1]);
    vec2_copy(uv[0], lines[2]);
    vec2_copy(uv[1], lines[3]);
    assert(node->c < 3);

    for (dir = 0; dir < 2; dir++) {
        if (!line->grid && dir == 1) break;
        if (node->c & (1 << dir)) continue; // Do we need that?
        visible = (node->level >= steps[dir]->level) &&
            node->xy[dir] % (1 << (node->level - steps[dir]->level)) == 0;
        if (!visible) continue;
        node->c |= (1 << dir);
        paint_lines(&painter, line->frame, 2, lines + dir * 2,
                    proj_spherical, 8, 0);
        if (!line->format) continue;
        if (check_borders(pos[0], pos[2 - dir], painter.proj, p, u, v)) {
            render_label(p, u, v, uv[0], 1 - dir, line,
                         node->s[dir] * (dir + 1), &painter);
        }
    }
    return (node->level >= max(steps[0]->level, steps[1]->level)) ? 0 : 1;
}

// Compute an estimation of the visible range of azimuthal angle.  If we look
// at the pole it can go up to 360°.
static double get_theta_range(const painter_t *painter, int frame)
{
    double p[4] = {0, 0, 0, 0};
    double theta, phi;
    double theta_max = -DBL_MAX, theta_min = DBL_MAX;
    int i;

    /*
     * This works by unprojection the four screen corners into the grid
     * frame and testing the maximum and minimum distance to the meridian
     * for each of them.
     */
    for (i = 0; i < 4; i++) {
        p[0] = 2 * ((i % 2) - 0.5);
        p[1] = 2 * ((i / 2) - 0.5);
        project(painter->proj, PROJ_BACKWARD, 4, p, p);
        convert_frame(painter->obs, FRAME_VIEW, frame, true, p, p);
        eraC2s(p, &theta, &phi);
        theta_max = max(theta_max, theta);
        theta_min = min(theta_min, theta);
    }
    return theta_max - theta_min;
}


// XXX: make it better.
static void get_steps(double fov, char type, int frame,
                      const painter_t *painter,
                      const step_t *steps[2])
{
    double a = fov / 8;
    int i;
    double theta_range = get_theta_range(painter, frame);
    if (type == 'd') {
        i = (int)round(1.7 * log(2 * M_PI / a));
        i = min(i, ARRAY_SIZE(STEPS_DEG) - 1);
        if (STEPS_DEG[i].n % 4) i++;
        steps[0] = &STEPS_DEG[i];
        steps[1] = &STEPS_DEG[i];
    } else {
        i = (int)round(1.7 * log(2 * M_PI / a));
        i = min(i, ARRAY_SIZE(STEPS_DEG) - 1);
        if (STEPS_DEG[i].n % 4) i++;
        steps[1] = &STEPS_DEG[i];
        i = (int)round(1.5 * log(2 * M_PI / a));
        i = min(i, ARRAY_SIZE(STEPS_HOUR) - 1);
        steps[0] = &STEPS_HOUR[i];
    }
    // Make sure that we don't render more than 15 azimuthal lines.
    while (steps[0]->n > 24 && theta_range / (M_PI * 2 / steps[0]->n) > 15)
        steps[0]--;
}

static int line_render(const obj_t *obj, const painter_t *painter)
{
    line_t *line = (line_t*)obj;
    double UV[4][2] = {{0.0, 1.0}, {1.0, 1.0},
                       {0.0, 0.0}, {1.0, 0.0}};
    double transform[4][4];
    const step_t *steps[2];
    mat4_set_identity(transform);

    if (strcmp(line->obj.id, "ecliptic") == 0) {
        mat3_to_mat4(core->observer->re2h, transform);
        mat4_rx(M_PI / 2, transform, transform);
    }

    qtree_node_t nodes[128]; // Too little?
    projection_t proj_spherical = {
        .name       = "spherical",
        .backward   = spherical_project,
    };
    painter_t painter2 = *painter;

    if (line->visible.value == 0.0) return 0;

    vec4_copy(line->color, painter2.color);
    painter2.color[3] *= line->visible.value;
    painter2.transform = &transform;
    // Compute the number of divisions of the grid.
    if (line->format)
        get_steps(core->fov, line->format, line->frame, &painter2, steps);
    else {
        steps[0] = &STEPS_DEG[1];
        steps[1] = &STEPS_DEG[0];
    }
    traverse_surface(nodes, ARRAY_SIZE(nodes), UV, &proj_spherical,
                     &painter2, line->frame, 1,
                     USER_PASS(&proj_spherical, line, steps), on_quad);
    return 0;
}


// Check if a line interect the normalized viewport.
// If there is an intersection, `border` will be set with the border index
// from 0 to 3.
static double seg_intersect(const double a[2], const double b[2], int *border)
{
    // We use the common 'slab' algo for AABB/seg intersection.
    // With some care to make sure we never use infinite values.
    double idx, idy,
           tx1 = -DBL_MAX, tx2 = +DBL_MAX,
           ty1 = -DBL_MAX, ty2 = +DBL_MAX,
           txmin = -DBL_MAX, txmax = +DBL_MAX,
           tymin = -DBL_MAX, tymax = +DBL_MAX,
           ret = DBL_MAX, vmin, vmax;
    if (a[0] != b[0]) {
        idx = 1.0 / (b[0] - a[0]);
        tx1 = (-1 == a[0] ? -DBL_MAX : (-1 - a[0]) * idx);
        tx2 = (+1 == a[0] ?  DBL_MAX : (+1 - a[0]) * idx);
        txmin = min(tx1, tx2);
        txmax = max(tx1, tx2);
    }
    if (a[1] != b[1]) {
        idy = 1.0 / (b[1] - a[1]);
        ty1 = (-1 == a[1] ? -DBL_MAX : (-1 - a[1]) * idy);
        ty2 = (+1 == a[1] ?  DBL_MAX : (+1 - a[1]) * idy);
        tymin = min(ty1, ty2);
        tymax = max(ty1, ty2);
    }
    ret = DBL_MAX;
    if (tymin <= txmax && txmin <= tymax) {
        vmin = max(txmin, tymin);
        vmax = min(txmax, tymax);
        if (0.0 <= vmax && vmin <= 1.0) {
            ret = vmin >= 0 ? vmin : vmax;
        }
    }
    *border = (ret == tx1) ? 0 :
              (ret == tx2) ? 1 :
              (ret == ty1) ? 2 :
                             3;

    return ret;
}

static void ndc_to_win(const projection_t *proj, const double ndc[2],
                       double win[2])
{
    win[0] = (+ndc[0] + 1) / 2 * proj->window_size[0];
    win[1] = (-ndc[1] + 1) / 2 * proj->window_size[1];
}

static bool check_borders(const double a[3], const double b[3],
                          const projection_t *proj,
                          double p[2], // Window pos on the border.
                          double u[2], // Window direction of the line.
                          double v[2]) // Window Norm of the border.
{
    double pos[2][3], q;
    bool visible[2];
    int border;
    const double VS[4][2] = {{+1, 0}, {-1, 0}, {0, -1}, {0, +1}};
    visible[0] = project(proj, PROJ_TO_NDC_SPACE, 3, a, pos[0]);
    visible[1] = project(proj, PROJ_TO_NDC_SPACE, 3, b, pos[1]);
    if (visible[0] != visible[1]) {
        q = seg_intersect(pos[0], pos[1], &border);
        if (q == DBL_MAX) return false;
        vec2_mix(pos[0], pos[1], q, p);
        ndc_to_win(proj, p, p);
        ndc_to_win(proj, pos[0], pos[0]);
        ndc_to_win(proj, pos[1], pos[1]);
        vec2_sub(pos[1], pos[0], u);
        vec2_copy(VS[border], v);
        return true;
    }
    return false;
}

/*
 * Meta class declarations.
 */

static obj_klass_t line_klass = {
    .id = "line",
    .size = sizeof(line_t),
    .flags = OBJ_IN_JSON_TREE,
    .update = line_update,
    .render = line_render,
    .attributes = (attribute_t[]) {
        PROPERTY("visible", "b", MEMBER(line_t, visible.target)),
        {}
    },
};
OBJ_REGISTER(line_klass)


static obj_klass_t lines_klass = {
    .id = "lines",
    .size = sizeof(lines_t),
    .flags = OBJ_IN_JSON_TREE | OBJ_MODULE,
    .init = lines_init,
    .update = lines_update,
    .render = lines_render,
    .gui = lines_gui,
    .render_order = 40,
};
OBJ_REGISTER(lines_klass)

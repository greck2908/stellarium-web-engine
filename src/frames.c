/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

static void correct_speed_of_light(double pv[2][3]) {
    double ldt = vec3_norm(pv[0]) * DAU / LIGHT_YEAR_IN_METER * DJY;
    vec3_addk(pv[0], pv[1], -ldt, pv[0]);
}

void position_to_apparent(const observer_t *obs, int origin, bool at_inf,
                          const double in[2][3], double out[2][3])
{
    eraCpv(in, out);

    if (!at_inf) {
        // Take into account relative position of observer/object
        // This is a classical formula, we should use the relativistic
        // velocity addition formula instead see
        // (https://en.wikipedia.org/wiki/Velocity-addition_formula)
        switch (origin) {
        case ORIGIN_BARYCENTRIC:
            eraPvmpv(out, obs->obs_pvb, out);
            break;
        case ORIGIN_HELIOCENTRIC:
            eraPvppv(out, obs->sun_pvb, out);
            eraPvmpv(out, obs->obs_pvb, out);
            break;
        case ORIGIN_GEOCENTRIC:
            eraPvmpv(out, obs->obs_pvg, out);
            break;
        default:
            assert(0);
        }
        // Correct in one shot space motion, annual & diurnal abberrations
        correct_speed_of_light(out);
    } else {
        // Light deflection by the Sun
        // TODO: adapt this formula to also work for solar system bodies
        // Currently this works only for distant stars.
        assert(vec3_norm(out[0]) == 1.0);
        eraLdsun(out[0], obs->astrom.eh, obs->astrom.em, out[0]);
        // Annual aberration is already taken into account for solar system
        // objects.
        eraAb(out[0], obs->astrom.v, obs->astrom.em, obs->astrom.bm1, out[0]);
    }
}

static void convert_frame_forward(const observer_t *obs,
                        int origin, int dest, bool at_inf, double p[3])
{
    const eraASTROM *astrom = &obs->astrom;

    if (origin == FRAME_ASTROM) {
        astrometric_to_apparent(obs, p, at_inf, p);
    }
    // ICRS to CIRS
    if (origin < FRAME_CIRS && dest >= FRAME_CIRS) {
        // Bias-precession-nutation, giving CIRS proper direction.
        eraRxp(astrom->bpn, p, p);
    }

    if (dest == FRAME_JNOW) {
        // The bridge between the classical and CIRS systems is the equation of
        // the origins, which is ERA−GST or equivalently αCIRS − αapparent; its
        // value is returned by several of the SOFA astrometry functions in
        // case it is needed.
        double mat[3][3];
        mat3_set_identity(mat);
        mat3_rz(-obs->eo, mat, mat);
        mat3_mul_vec3(mat, p, p);
        return;
    }

    // CIRS to OBSERVED.
    if (origin < FRAME_OBSERVED && dest >= FRAME_OBSERVED) {
        // Precomputed earth rotation and polar motion.
        // Ignores Diurnal aberration for the moment
        mat3_mul_vec3(obs->ri2h, p, p);

        if (at_inf) {
            refraction(p, astrom->refa, astrom->refb, p);
            vec3_normalize(p, p);
        } else {
            // Special case for null's vectors
            double dist = vec3_norm(p);
            if (dist == 0.0) {
                vec3_set(p, 0, 0, 0);
                return;
            }
            vec3_mul(1.0 / dist, p, p);
            refraction(p, astrom->refa, astrom->refb, p);
            vec3_normalize(p, p);
            vec3_mul(dist, p, p);
        }
    }

    // OBSERVED to VIEW.
    if (origin < FRAME_VIEW && dest >= FRAME_VIEW)
        mat3_mul_vec3(obs->ro2v, p, p);
}

static void convert_frame_backward(const observer_t *obs,
                        int origin, int dest, double p[3])
{
    assert(0);
}

EMSCRIPTEN_KEEPALIVE
int convert_frame(const observer_t *obs,
                        int origin, int dest, bool at_inf,
                        const double in[3], double out[3])
{
    PROFILE(convert_frame, PROFILE_AGGREGATE);
    obs = obs ?: (observer_t*)core->observer;

    // Can't convert to NDC space.
    assert(dest < FRAME_NDC && origin < FRAME_NDC);
    vec3_copy(in, out);
    assert(!isnan(out[0] + out[1] + out[2]));

    if (dest > origin) {
        convert_frame_forward(obs, origin, dest, at_inf, out);
    } else if (dest < origin) {
        convert_frame_backward(obs, origin, dest, out);
    }

    assert(!isnan(out[0] + out[1] + out[2]));
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int convert_framev4(const observer_t *obs,
                        int origin, int dest,
                        const double in[4], double out[3])
{
    if (in[3] == 1.0) {
        return convert_frame(obs, origin, dest, false, in, out);
    } else {
        assert(fabs(vec3_norm2(in) - 1.0) <= 0.0000000001);
        return convert_frame(obs, origin, dest, true, in, out);
    }
}

void position_to_astrometric(const observer_t *obs, int origin,
                                const double in[2][3], double out[2][3])
{
    eraCpv(in, out);

    // Take into account relative position of earth/object
    switch (origin) {
    case ORIGIN_BARYCENTRIC:
        eraPvmpv(out, obs->earth_pvb, out);
        break;
    case ORIGIN_HELIOCENTRIC:
        eraPvppv(out, obs->sun_pvb, out);
        eraPvmpv(out, obs->earth_pvb, out);
        break;
    case ORIGIN_GEOCENTRIC:
        break;
    case ORIGIN_OBSERVERCENTRIC:
        eraPvppv(out, obs->obs_pvb, out);
        eraPvmpv(out, obs->earth_pvb, out);
    default:
        assert(0);
    }

    // We exclude observer's speed in this computation, otherwise it would also
    // add annual aberration at the same time, which we don't want here.
    // It will be added later in astrometric_to_apparent function.
    double tmp[3];
    eraCp(out[1], tmp);
    eraPpp(out[1], obs->earth_pvb[1], out[1]);
    correct_speed_of_light(out);
    eraCp(tmp, out[1]);
}

void astrometric_to_apparent(const observer_t *obs, const double in[3],
                             bool inf, double out[3])
{
    eraCp(in, out);

    if (inf) {
        assert(fabs(vec3_norm2(out) - 1.0) <= 0.0000000001);
        // Light deflection by the Sun, giving BCRS natural direction.
        // TODO: adapt this formula for solar system bodies, this works only for
        // distant stars.
        eraLdsun(out, obs->astrom.eh, obs->astrom.em, out);
        // Aberration, giving GCRS proper direction.
        eraAb(out, obs->astrom.v, obs->astrom.em, obs->astrom.bm1, out);
        assert(fabs(vec3_norm2(out) - 1.0) <= 0.0000000001);
    } else {
        eraPpp(out, obs->obs_pvb[0], out);
        eraPmp(out, obs->earth_pvb[0], out);
        double dist = vec3_norm(out);
        vec3_mul(1.0 / dist, out, out);
        eraAb(out, obs->astrom.v, obs->astrom.em, obs->astrom.bm1, out);
        vec3_mul(dist, out, out);
    }
}

/******** TESTS ***********************************************************/

#if COMPILE_TESTS

struct planet_test_pvs {
    const char *name;
    double pv_bary[2][3]; // Barycentric position
    double pv_geo[2][3];  // Geocentric astrometric position
    double pv_obs[2][3];  // Observer-centric astrometric position
    double altazd[3];     // Observer-centric apparent position (az, alt, dist)
};
typedef struct planet_test_pvs planet_test_pvs_t;

// Data generated using the script in tools/compute-ephemeris2.py
// based on the Skyfield python library.
static const planet_test_pvs_t test_pvs[] = {
{
 "sun",
 {{-0.000491427976, 0.006775501407, 0.002867701470},
  {-0.000007705635, 0.000001971237, 0.000001065545}},
 {{-0.410211024005, -0.823278548145, -0.356888629682},
  {0.015921847952, -0.006507692735, -0.002820412155}},
 {{-0.410244857939, -0.823267709476, -0.356912120337},
  {0.015853564323, -0.006720592014, -0.002820294706}},
 {-18.556857983642, 256.377350996914, 0.986639157227},
},
{
 "venus",
 {{0.063294323484, 0.662254045539, 0.293764888122},
  {-0.020223193303, 0.001077901385, 0.001764302905}},
 {{-0.346379706149, -0.167802425151, -0.065995416671},
  {-0.004293525628, -0.005430590389, -0.001056654582}},
 {{-0.346413536665, -0.167791586664, -0.066018907624},
  {-0.004361809249, -0.005643489581, -0.001056537093}},
 {-43.455390536200, 290.511717679150, 0.390531498223},
},
{
 "earth",
 {{0.409719639938, 0.830054038320, 0.359756325081},
  {-0.015929553568, 0.006509664008, 0.002821477715}},
 {{0.000000000000, 0.000000000000, 0.000000000000},
  {0.000000000000, 0.000000000000, 0.000000000000}},
 {{-0.000033830018, 0.000010837069, -0.000023491348},
  {-0.000068283598, -0.000212899218, 0.000000117476}},
 {-89.822452578568, 0.023937985194, 0.000042588210},
},
{
 "moon",
 {{0.408174810701, 0.831790531159, 0.360555757719},
  {-0.016411687408, 0.006140127347, 0.002720378706}},
 {{-0.001544596293, 0.001736405687, 0.000799394026},
  {-0.000482133372, -0.000369531590, -0.000101096770}},
 {{-0.001578428179, 0.001747243590, 0.000775903032},
  {-0.000550416997, -0.000582430825, -0.000100979301}},
 {-30.684622899649, 33.643192832833, 0.002479177524},
},
{
 "pluto barycenter",
 {{11.779431371110, -28.939671112777, -12.580297736726},
  {0.003009902686, 0.000764428169, -0.000668327032}},
 {{11.369113833582, -29.769876994895, -12.939921301465},
  {0.018939474401, -0.005745280416, -0.003489824125}},
 {{11.369079999851, -29.769866156175, -12.939944792164},
  {0.018871190772, -0.005958179695, -0.003489706675}},
 {15.862703582649, 228.739267626277, 34.393939614761},
}};

// Barycentric position of atlanta from SkyField
static const planet_test_pvs_t atlanta_pos = {
    "atlanta",
    {{0.409753473872, 0.830043199650, 0.359779815735},
     {-0.015861269939, 0.006722563288, 0.002821360265}},
    {{0.000033837834, -0.000010840322, 0.000023489961},
     {0.000068283991, 0.000212899236, -0.000000117423}},
    {{0.0, 0.0, 0.0},
     {0.0, 0.0, 0.0}},
    {0.0, 0.0, 0.0}
};

static void test_convert_origin(void)
{
    observer_t *obs;
    double p[3], pref[3];

    static const double precision = 5.0 / 3600;  // 5 arcsec
    int i;
    static const double date = 58450; // 2018-Nov-28 00:00 (UT)
    static const double lon = -84.3880; // Atlanta
    static const double lat = 33.7490;  // Atlanta

    core_init(100, 100, 1.0);
    obs = core->observer;
    obj_set_attr((obj_t*)obs, "utc", "f", date);
    obj_set_attr((obj_t*)obs, "longitude", "f", lon * DD2R);
    obj_set_attr((obj_t*)obs, "latitude", "f", lat * DD2R);
    obs->refraction = false;
    observer_update(core->observer, false);

    const planet_test_pvs_t *sun = &test_pvs[0];
    const planet_test_pvs_t *earth = &test_pvs[2];

    // Compare time scales with Skyfield to rule out issues in observer's code
    // TAI = 58450.000428240746
    // TT  = 58450.000800740905
    // UT1 = 58449.999997198116
    assert(tests_compare_time(obs->tt, 58450.000800740905, 0.1));
    assert(tests_compare_time(obs->ut1, 58449.999997198116, 200));

    // Compare BCRS basic ephemerides for sun, earth, observer
    assert(tests_compare_pv(obs->sun_pvb, sun->pv_bary, 5, 10));
    assert(tests_compare_pv(obs->earth_pvb, earth->pv_bary, 5, 10));
    assert(tests_compare_pv(obs->obs_pvb, atlanta_pos.pv_bary, 5, 10));

    for (i = 0; i < ARRAY_SIZE(test_pvs); i++) {
        const planet_test_pvs_t* planet = &test_pvs[i];
        double out[2][3];
        position_to_astrometric(obs, ORIGIN_BARYCENTRIC, planet->pv_bary, out);
        double sep = eraSepp(planet->pv_geo[0], out[0]) * DR2D;
        if (sep > precision) {
            LOG_E("Error: %s", planet->name);
            LOG_E("Barycentric to Astrometric error: %.5f°", sep);
            tests_compare_pv(planet->pv_geo, out, 5, 10);
            assert(false);
        }

        position_to_apparent(obs, ORIGIN_BARYCENTRIC, false,
                             planet->pv_bary, out);
        convert_frame(obs, FRAME_ICRF, FRAME_OBSERVED, 0, out[0], p);

        eraS2p(planet->altazd[1] * DD2R, planet->altazd[0] * DD2R,
               planet->altazd[2], pref);
        sep = eraSepp(p, pref) * DR2D;
        if (sep > precision && strcmp(planet->name, "earth")) {
            LOG_E("Error: %s", planet->name);
            LOG_E("Apparent altaz error: %.5f°", sep);
            double az, alt, dist;
            eraP2s(pref, &az, &alt, &dist);
            az = eraAnp(az);
            LOG_E("Ref az: %f°, alt: %f°, %f AU", az * DR2D, alt * DR2D, dist);
            eraP2s(p, &az, &alt, &dist);
            az = eraAnp(az);
            LOG_E("Tst az: %f°, alt: %f°, %f AU", az * DR2D, alt * DR2D, dist);
            assert(false);
        }
    }
}

TEST_REGISTER(NULL, test_convert_origin, TEST_AUTO)

#endif

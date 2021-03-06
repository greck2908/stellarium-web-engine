/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: frames.h
 * Referential conversion functions
 *
 */

typedef struct observer observer_t;

/* Enum: FRAME
 * Represent a reference frame. A reference frame is independent of the origin,
 * it just defines the direction of the x,y and z axes.
 *
 * FRAME_ASTROM   - Astrometric ICRF frame. Like FRAME_ICRF but first convert
 *                  from Astrometric to Apparent direction (as seen from the
 *                  current observer). Use this frame to pass directly star
 *                  catalogs directions.
 * FRAME_ICRF     - ICRF frame. Axes (almost) aligned to equatorial J2000.0.
 *                  This frame is used for all 3D positions/velocities for
 *                  ephemerides of solar system objects or astrometric reference
 *                  data on galactic and extragalactic objects, i.e., the data
 *                  in astrometric star catalogs.
 * FRAME_JNOW     - Equatorial of date frame (=JNow or Geocentric Apparent).
 *                  It is the true equator and equinox of date equatorial frame.
 *                  Use this frame to describe apparent directions of
 *                  astronomical objects as seen from the Earth.
 * FRAME_CIRS     - CIRS frame. Like Equatorial of date but with the origin of
 *                  right ascension being the Celestial Intermediate Origin
 *                  (CIO) instead of the true equinox.
 *                  Also use this frame to describe apparent directions of
 *                  astronomical objects as seen from the Earth.
 * FRAME_OBSERVED - Observed frame (the frame of alt/az). Includes atmospheric
 *                  refraction.
 * FRAME_VIEW     - Observed frame rotated in the observer view direction.
 * FRAME_NDC      - Normalized device coordinates.  Only used as a flag to
 *                  the painter when we have already projected coordinates.
 * FRAME_WINDOW   - Window coordinates.  Only used as a flag to
 *                  the painter when we have already projected coordinates.
 */
enum {
    FRAME_ASTROM              = -1,
    FRAME_ICRF                = 0,
    FRAME_CIRS                = 1,
    FRAME_JNOW                = 2,
    FRAME_OBSERVED            = 3,
    FRAME_VIEW                = 4,
    FRAME_NDC                 = 5,
    FRAME_WINDOW              = 6,
};

/* Function: convert_frame
 * Rotate the passed 3D apparent coordinate vector from a Reference Frame to
 * another.
 *
 * The vector represents the apparent position/direction of the source as seen
 * by the observer in his reference system (usually GCRS for earth observation).
 * This means that effects such as space motion, light deflection or annual
 * aberration must already be taken into account before calling this function.
 *
 * Parameters:
 *  obs     - The observer.  If NULL we use the current core observer.
 *  origin  - Origin coordinates.
 *            One of the <FRAME> enum values.
 *  dest    - Destination coordinates.
 *            One of the <FRAME> enum values.
 *  at_inf  - true for fixed objects (far away from the solar system).
 *            For such objects, velocity is assumed to be 0 and the position
 *            is assumed to be normalized.
 *  in      - The input coordinates (3d AU).
 *  out     - The output coordinates (3d AU).
 *
 * Return:
 *  0 for success.
 */
int convert_frame(const observer_t *obs,
                        int origin, int dest, bool at_inf,
                        const double in[3], double out[3]);

/* Function: convert_framev4
 * Same as convert_frame but check the 4th component of the input vector
 * to know if the source is at infinity. If in[3] == 1.0, the source is at
 * infinity and the vector must be normalized, otherwise assume the vector to
 * contain the real object's distance in AU.
 */
int convert_framev4(const observer_t *obs,
                        int origin, int dest,
                        const double in[4], double out[3]);

/* Enum: ORIGIN
 * Represent a reference system, i.e. the origin of a reference frame and the
 * associated intertial frame.
 *
 * ORIGIN_BARYCENTRIC     - BCRS: a coordinate origin whose relativistic frame
 *    of reference is the one that was carefully defined in IAU 2000 Resolution
 *    B1.3 which puts the coordinate origin at the gravitational center of the
 *    Solar System (the SSB).
 * ORIGIN_HELIOCENTRIC    - like BCRS but centered on the sun's center instead
 *    of SSB.
 * ORIGIN_GEOCENTRIC      - a coordinate origin in the GCRS relativistic frame
 *    of reference and with origin at the center of earth.
 * ORIGIN_OBSERVERCENTRIC - a coordinate origin with origin at the observer's
 *    position, and local relativistic frame of reference of the observer.
 */
enum {
    ORIGIN_BARYCENTRIC        = 0,
    ORIGIN_HELIOCENTRIC       = 1,
    ORIGIN_GEOCENTRIC         = 2,
    ORIGIN_OBSERVERCENTRIC    = 3
};

/* Function: position_to_apparent
 * Convert 3D positions/velocity to apparent direction as seen from observer.
 *
 * This function performs basic 3D vectors addition/subtraction and change the
 * inertial frame to match the one of the observer. This convertion takes into
 * account the following effects:
 * - relative position of observer/object
 * - space motion of the observed object (compensate light time)
 * - annual abberation (space motion of the observer)
 * - diurnal abberation (and parallax)
 * - light deflection by the sun
 *
 * Input position/velocity and output direction are 3D vectors in the ICRF
 * reference frame.
 *
 * The output of this function must not be added/subtracted to other
 * positions/velocity from different intertial frame.
 *
 * Parameters:
 *  obs     - The observer.  If NULL we use the current core observer.
 *  origin  - Source origin.
 *            One of the <ORIGIN> enum values.
 *  at_inf  - true for fixed objects (far away from the solar system).
 *            For such objects, velocity is assumed to be 0 and the position
 *            is assumed to be normalized.
 *  in      - The input  ICRF position/velocity in AU and AU/day as seen from
 *            the given origin (in term of position and intertial frame).
 *  out     - The output ICRF position/velocity in AU and AU/day in the
 *            inertial frame of the observer.
 */
void position_to_apparent(const observer_t *obs, int origin, bool at_inf,
                          const double in[2][3], double out[2][3]);

/* Function: position_to_astrometric
 * Convert 3D positions/velocity to astrometric direction as seen from earth
 * center (GCRS).
 *
 * This function performs basic 3D vectors addition/subtraction and change the
 * inertial frame to match the one of the geocenter. This convertion takes into
 * account the following effects:
 * - relative position of earth/object
 * - space motion of the observed object (compensate light time)
 *
 * Parameters:
 *  obs     - The observer.  If NULL we use the current core observer.
 *  origin  - Source origin.
 *            One of the <ORIGIN> enum values.
 *  in      - The input  ICRF barycentric position/velocity in AU and AU/day as
 *            seen from the geocenter.
 *  out     - The output ICRF astrometric position/velocity in AU and AU/day as
 *            seen from the geocenter.
 */
void position_to_astrometric(const observer_t *obs, int origin,
                                const double in[2][3], double out[2][3]);

/* Function: astrometric_to_apparent
 * Convert astrometric direction to apparent direction. Input direction is
 * assumed to be seen from the earth center, while ouput direction is seen
 * from observer.
 *
 * This function change the inertial frame to match the one of the observer.
 * This convertion takes into account the following effects:
 * - position of observer on earth
 * - annual abberation (space motion of the observer)
 * - diurnal abberation (daily space motion of the observer)
 * - light deflection by the sun
 *
 * Parameters:
 *  obs     - The observer.  If NULL we use the current core observer.
 *  in      - The input  ICRF barycentric position/velocity in AU and AU/day as
 *            seen from the observer.
 *  at_inf  - true for fixed objects (far away from the solar system)
 *  out     - The output ICRF astrometric position/velocity in AU and AU/day as
 *            seen from the observer.
 */
void astrometric_to_apparent(const observer_t *obs, const double in[3],
                             bool at_inf, double out[3]);

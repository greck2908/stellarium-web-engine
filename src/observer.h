/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifndef OBSERVER_H
#define OBSERVER_H

#include "obj.h"
#include "erfa.h"

/*
 * Type: observer_t
 * Store informations about the observer current position.
 */
struct observer
{
    obj_t  obj;

    // This members define the state of the observer
    // Do not add new variable between them if they don't contribute to the
    // state of the observer.
    // -- State Start --
    double elong;       // Observer longitude
    double phi;         // Observer latitude
    double hm;          // height above ellipsoid (m)
    double horizon;     // altitude of horizon (used for rising/setting).
    double pressure;    // Set to NAN to compute it from the altitude.
    bool   refraction;  // Whether we use refraction or not.

    // State partial: changing one of the following values only enable
    // to use the fast update method.
    double altitude;
    double azimuth;
    double roll;

    // TT time in MJD
    double tt;
    // -- State stop --

    // Different times, all in MJD, they must be consistent with tt
    double ut1;
    double utc;

    obj_t  *city;

    double last_update;
    double last_accurate_update;

    // Hash value that represents a given observer state for which the accurate
    // values have been computed. Used to prevent updating object data several
    // times with the same observer.
    uint32_t hash_accurate;

    // Hash value that represents the last observer state for which the
    // values have been computed. Used to prevent updating object data several
    // times with the same observer.
    uint32_t hash;

    // Hash of a partial state of the observer. If it is unchanged, it is
    // safe to use make fast update.
    uint32_t hash_partial;

    double eo;  // Equation of origin.
    eraASTROM astrom;
    // Heliocentric position/speed of the earth in ICRF reference frame and in
    // BCRS reference system. AU, AU/day.
    double earth_pvh[2][3];
    // Barycentric position/speed of the earth in ICRS, i.e. as seen from the
    // SSB in ICRF reference frame and in BCRS reference system. AU, AU/day.
    double earth_pvb[2][3];
    // Barycentric position/speed of the sun in ICRS, i.e. as seen from the SSB
    // in ICRF reference frame and in BCRS reference system. AU, AU/day.
    double sun_pvb[2][3];
    // Apparent position/speed of the sun (as seen from the observer) in ICRF
    // reference frame, in local reference system. AU, AU/day.
    double sun_pvo[2][3];
    // Barycentric position/speed of the observer in ICRS, i.e. as seen from the
    // SSB in ICRF reference frame and in BCRS reference system. AU, AU/day.
    double obs_pvb[2][3];
    double obs_pvg[2][3];

    // The pointed position and constellation.
    struct {
        double icrs[3];
        char cst[4];
    } pointer;

    // Transformation matrices.
    // h: Horizontal (RA/DE, left handed, X->N, Y->E, Z->up).
    // o: Observed: horizontal with refraction (RA/DE, left handed).
    // i: ICRS (right handed).
    // e: Ecliptic (right handed).
    // v: View (observed with view direction).
    double ro2v[4][4];  // Rotate from observed to view.
    double ri2h[4][4];  // Equatorial J2000 (ICRS) to horizontal.
    double rh2i[4][4];  // Horizontal to Equatorial J2000 (ICRS).
    double ri2v[4][4];  // Equatorial J2000 (ICRS) to view.
    double ri2e[4][4];  // Equatorial J2000 (ICRS) to ecliptic.
    double re2i[4][4];  // Eclipic to Equatorial J2000 (ICRS).
    double re2h[4][4];  // Ecliptic to horizontal.
    double re2v[4][4];  // Ecliptic to view.
};

void observer_update(observer_t *obs, bool fast);

#endif // OBSERVER_H

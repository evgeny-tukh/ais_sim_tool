#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace NmeaUtils
{
    char *formatPosition (const double latitude, const double longitude, char *buffer);
    void parseAngle (const double angle, int& degrees, double& minutes, bool& positive);
    float calcRange (const double lat1, const double lon1, const double lat2, const double lon2);
    float calcBearing (const double lat1, const double lon1, const double lat2, const double lon2);
    void calcPosition (const double lat, const double lon, const double bearing, const double range,
                       double& destLat, double& destLon);


    const double EARTH_RADIUS = 6366707.0194937074958298109629434;
    const double Pi = 3.1415926535897932384626433832795;
    const double TwoPi = Pi + Pi;

    inline double toRad (const double arg) { return arg * Pi / 180.0; }
    inline double toDeg (const double arg) { return arg * 180.0 / Pi; }
}

float NmeaUtils::calcRange (const double lat1, const double lon1, const double lat2, const double lon2)
{
    double latRad1 = toRad (lat1);
    double latRad2 = toRad (lat2);
    double lonRad1 = toRad (lon1);
    double lonRad2 = toRad (lon2);

    return (float) (acos (sin (latRad1) * sin (latRad2) + cos (latRad1) * cos (latRad2) * cos (lonRad1 - lonRad2)) * EARTH_RADIUS);
}

float NmeaUtils::calcBearing (const double lat1, const double lon1, const double lat2, const double lon2)
{
    double latRad1   = toRad (lat1);
    double latRad2   = toRad (lat2);
    double lonRad1   = toRad (lon1);
    double lonRad2   = toRad (lon2);
    double deltaLonW = fmod (lonRad1 - lonRad2, TwoPi);
    double deltaLonE = fmod (lonRad2 - lonRad1, TwoPi);
    double tanRatio  = tan (latRad2 * 0.5 + Pi * 0.25) / tan (latRad1 * 0.5 + Pi * 0.25);
    double deltaLat  = log (tanRatio);
    double bearing   = (deltaLonW < deltaLonE) ? fmod (atan2 (- deltaLonW, deltaLat), TwoPi) : fmod (atan2 (deltaLonE, deltaLat), TwoPi);

    bearing = toDeg (bearing);

    if (bearing < 0)
        bearing += 360.0;

    return (float) bearing;
}

void NmeaUtils::calcPosition (const double lat, const double lon, const double bearing, const double range,
                              double& destLat, double& destLon)
{
    double latRad   = toRad (lat);
    double lonRad   = toRad (lon);
    double brgRad   = toRad (bearing);
    double arcAngle = range / EARTH_RADIUS;
    double arcSin   = sin (arcAngle);
    double arcCos   = cos (arcAngle);
    double latSin   = sin (latRad);
    double latCos   = cos (latRad);

    destLat = asin (latSin * arcCos + latCos * arcSin * cos (brgRad));
    destLon = lonRad + atan2 (sin (brgRad) * arcSin * latCos, arcCos - latSin * sin (destLat));

    destLat *= 180.0 / Pi;
    destLon *= 180.0 / Pi;
}

char *NmeaUtils::formatPosition (const double latitude, const double longitude, char *buffer)
{
    if (buffer)
    {
        int    latDeg, lonDeg;
        double latMin, lonMin;
        bool   north, east;

        parseAngle (latitude, latDeg, latMin, north);
        parseAngle (longitude, lonDeg, lonMin, east);

        sprintf (buffer, "%02d %06.3f%c %03d %06.3f%c", latDeg, latMin, north ? 'N' : 'S', lonDeg, lonMin, east ? 'E' : 'W');
    }
    
    return buffer;
}

void NmeaUtils::parseAngle (const double angle, int& degrees, double& minutes, bool& positive)
{
    double absAngle = fabs (angle);

    degrees  = (int) absAngle;
    minutes  = (absAngle - (double) degrees) * 60.0;
    positive = angle >= 0.0;
}

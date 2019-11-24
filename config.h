#pragma once

#include <stdio.h>
#include "sim_types.h"
#include "ais_utils.h"

struct Config
{
    double  *lat, *lon;
    int     *numOfTargets, *numOfSAR, *numOfMOB, *maxRange, *maxSpeed, *port, *baud, *countryCode, *radarCursorBrg, *radarCursorRng;
    SimMode *simMode;
    bool    *consoleMode, *shuttleMode, *sendRadarCursor;
    char    *params;
};

bool atob (const char *value)
{
    return stricmp (value, "true") == 0 || stricmp (value, "yes") == 0 || atoi (value) != 0;
}

bool isWhiteSpace (const char chr)
{
    return chr == ' ' || chr == '\t';
}

void trim (char *string)
{
    size_t i;
    int    leadingCount;

    for (leadingCount = 0; isWhiteSpace (string [leadingCount]); ++ leadingCount);
    for (i = strlen (string) - 1; isWhiteSpace (string [i]); string [i--] = '\0');

    for (i = leadingCount; string [i]; ++ i)
        string [i-leadingCount] = string [i];

    string [i-leadingCount] = '\0';
}

void getSimulationMode (const char *value, SimMode *simMode)
{
    if (stricmp (value, "arpa") == 0)
        *simMode = SimMode::Arpa;
    else if (stricmp (value, "ais") == 0)
        *simMode = SimMode::Ais;
}

void processLine (const char *line, Config& cfg)
{
    char *start, *equalChar;

    for (start = (char *) line; *start && (*start == ' ' || *start == '\t'); ++ start);

    if (*start == ';')
        return;

    equalChar = strchr (start, '=');

    if (equalChar)
    {
        char *key   = start;
        char *value = equalChar + 1;

        *equalChar = '\0';

        trim (key);
        trim (value);

        if (stricmp (key, "lat") == 0)
            *(cfg.lat) = atof (value);
        else if (stricmp (key, "lon") == 0)
            *(cfg.lon) = atof (value);
        else if (stricmp (key, "numOfTargets") == 0)
            *(cfg.numOfTargets) = atoi (value);
        else if (stricmp (key, "numOfSAR") == 0)
            *(cfg.numOfSAR) = atoi (value);
        else if (stricmp (key, "numOfMOB") == 0)
            *(cfg.numOfMOB) = atoi (value);
        else if (stricmp (key, "maxRange") == 0)
            *(cfg.maxRange) = atoi (value);
        else if (stricmp (key, "maxSpeed") == 0)
            *(cfg.maxSpeed) = atoi (value);
        else if (stricmp (key, "port") == 0)
            *(cfg.port) = atoi (value);
        else if (stricmp (key, "baud") == 0)
            *(cfg.baud) = atoi (value);
        else if (stricmp (key, "countryCode") == 0)
            *(cfg.countryCode) = atoi (value);
        else if (stricmp (key, "simMode") == 0)
            getSimulationMode (value, cfg.simMode);
        else if (stricmp (key, "consoleMode") == 0)
            *(cfg.consoleMode) = atob (value);
        else if (stricmp (key, "shuttleMode") == 0)
            *(cfg.shuttleMode) = atob (value);
        else if (stricmp (key, "params") == 0)
            memcpy (cfg.params, value, sizeof (cfg.params));
        else if (stricmp (key, "sendRadarCursor") == 0)
            *(cfg.sendRadarCursor) = atob (value);
        else if (stricmp (key, "radarCursorBrg") == 0)
            *(cfg.radarCursorBrg) = atoi (value);
        else if (stricmp (key, "radarCursorRng") == 0)
            *(cfg.radarCursorRng) = atoi (value);
    }
}

void loadConfig (const char *path, Config& cfg)
{
    FILE *cfgFile = fopen (path, "rb+");

    if (cfgFile)
    {
        char  *buffer, line [200], *curChar;
        long   size;
        size_t lineSize;
        
        fseek (cfgFile, 0, SEEK_END);

        size = ftell (cfgFile);

        fseek (cfgFile, 0, SEEK_SET);

        buffer = (char *) malloc ((size_t) size + 1);

        memset (buffer, 0, size + 1);
        fread (buffer, sizeof (char), size, cfgFile);
        fclose (cfgFile);

        for (curChar = buffer, lineSize = 0, memset (line, 0, sizeof (line)); *curChar; ++ curChar)
        {
            if (*curChar == '\r' || *curChar == '\n')
            {
                lineSize = 0;

                processLine (line, cfg);
                memset (line, 0, sizeof (line));
            }
            else
            {
                line [lineSize++] = *curChar;
            }
        }

        free (buffer);
    }
}
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <time.h> 
#include <ctype.h>
#include <windows.h>
#include <corecrt_wtime.h>
#include "target.h"
#include "config.h"

#include "nmea_utils.h"
#include "ais_utils.h"

#define ASCII_XON       0x11
#define ASCII_XOFF      0x13

#define CONSOLE_PORT    0

typedef void (*TargetOutCb) (const Target *, HANDLE);

unsigned char calcCRC (char *sentence)
{
    unsigned char result = 0;

    if (sentence)
    {
        result = sentence [1];

        for (int i = 2; sentence [i] && sentence [i] != '*'; ++ i)
            result ^= sentence [i];
    }

    return result;
}

double getRandValue (const double maxValue)
{
    return (double) rand () * maxValue / (double) RAND_MAX;
}

int getRandValue (const int maxValue)
{
    return (int) getRandValue ((double) maxValue);
}

Target *initializeTargets (const int numOfTargets, const int numOfSAR, const int numOfMOB, const double lat, const double lon,
                           const double maxRange, const double maxSpeed)
{
    Target *targets = (Target *) malloc (sizeof (Target) * numOfTargets);

    for (int i = 0; i < numOfTargets; ++ i)
    {
        Target *target = targets + i;

        sprintf (target->name, "SHIP%02d", i + 1);

        target->sar   = i >= (numOfTargets - numOfSAR - numOfMOB) && i < (numOfTargets - numOfMOB);
        target->mob   = i >= (numOfTargets - numOfMOB);
        target->id    = i + 1;
        target->brg   = getRandValue (360.0);
        target->cog   = getRandValue (360.0);
        target->range = getRandValue (maxRange);
        target->sog   = getRandValue (maxSpeed);
        target->alt   = target->sar ? getRandValue (500) : 0;

        NmeaUtils::calcPosition (lat, lon, target->brg, target->range, target->lat, target->lon);
    }

    return targets;
}

void simulation (Target *targets, const int numOfTargets, const double shipLat, const double shipLon, TargetOutCb cb,
                 HANDLE portHandle, const bool shuttleMode, const double maxRange)
{
    time_t lastCalcTime = time (0);

    while (true)
    {
        time_t now               = time (0);
        time_t timeSinceLastCalc = now - lastCalcTime;

        if (timeSinceLastCalc > 0)
        {
            for (int i = 0; i < numOfTargets; ++ i)
            {
                Target *target = (Target *) targets + i;
                double  distTravelled = target->sog * 1852.0 * (double) timeSinceLastCalc / 3600.0;
                double  newLat;
                double  newLon;

                NmeaUtils::calcPosition (target->lat, target->lon, target->cog, distTravelled, newLat, newLon);
                
                target->brg   = NmeaUtils::calcBearing (shipLat, shipLon, newLat, newLon);
                target->range = NmeaUtils::calcRange (shipLat, shipLon, newLat, newLon);
                target->lat   = newLat;
                target->lon   = newLon;

                cb (target, portHandle);

                if (shuttleMode && (target->range >= maxRange))
                {
                    target->cog += 180.0;

                    if (target->cog >= 360.0)
                        target->cog -= 360.0;
                }
            }

            lastCalcTime = now;
        }

        Sleep (1000);
    }
}

void sendOutArpaTarget (const Target *target, HANDLE portHandle)
{
    time_t timestamp = time (0);
    tm    *now  = gmtime (& timestamp);
    char   body [100];
    char   buffer [100];
    DWORD  bytesWritten;

    sprintf (body, "$RATTM,%d,%.1f,%05.1f,R,%.1f,%05.1f,R,,,K,#%d,T,,%02d%02d%02d.00,A*%%02X\r\n",
             target->id, target->range / 1852.0, target->brg, target->sog, target->cog, target->id,
             now->tm_hour, now->tm_min, now->tm_sec);

    sprintf (buffer, body, calcCRC (body));

    if (portHandle == CONSOLE_PORT)
        printf (buffer);
    else
        WriteFile (portHandle, buffer, strlen (buffer), & bytesWritten, 0);
}

void sendOutAisTarget (const Target *target, HANDLE portHandle)
{
    byte          buffer [400];
    char          data [360];
    std::strings  sentences;
    unsigned long bytesWritten;

    memset (data, 0, sizeof (data));

    if (target->mob)
    {
        //AIS::buildBroadcastSRM (buffer, (Target *) target);
        AIS::buildPositionReport (buffer, (Target *) target);
    }
    else if (target->sar)
    {
        AIS::buildSafeAndResqueReport (buffer, (Target *) target);
    }
    else
    {
        AIS::buildPositionReport (buffer, (Target *) target);
    }

    AIS::encodeString (buffer, data, 28);
    AIS::buildVDM (data, sentences, 28);

    memset (data, 0, sizeof (data));
    
    if (!target->mob)
    {
        AIS::buildStaticShipAndVoyageData (buffer, (Target *) target);
        AIS::encodeString (buffer, data, 71);
        AIS::buildVDM (data, sentences, 71, 4);
    }
    
    for (auto& sentence : sentences)
    {
        const char *output = sentence.c_str ();

        if (portHandle == CONSOLE_PORT)
            printf (output);
        else
            WriteFile (portHandle, output, strlen (output), & bytesWritten, 0);
    }

    /*AIS::buildStaticShipAndVoyageData (buffer, target);
    AIS::encodeString (buffer, data, 71);
    AIS::buildVDM (data, sentence, sizeof (sentence));

    if (portHandle == CONSOLE_PORT)
        printf (buffer);
    else
        WriteFile (portHandle, buffer, strlen (buffer), & bytesWritten, 0);*/
}

HANDLE openPort (const int port, const int baud, const char *params)
{
    HANDLE portHandle;
    char   portName [100];

    sprintf (portName, "\\\\.\\COM%d", port);

    portHandle = CreateFileA (portName, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    if (portHandle != INVALID_HANDLE_VALUE)
    {
        COMMTIMEOUTS timeouts;
        DCB          state;

        memset (& state, 0, sizeof (state));

        state.DCBlength = sizeof (state);

        SetupComm (portHandle, 4096, 4096); 
        PurgeComm (portHandle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR); 
        GetCommState (portHandle, & state);
        GetCommTimeouts (portHandle, & timeouts);

        timeouts.ReadIntervalTimeout        = 1000;
        timeouts.ReadTotalTimeoutMultiplier = 1;
        timeouts.ReadTotalTimeoutConstant   = 3000;

        state.fInX     = 
        state.fOutX    =
        state.fParity  =
        state.fBinary  = 1;
        state.BaudRate = baud;
        state.ByteSize = params [0] - '0';
        state.XoffChar = ASCII_XOFF;
        state.XonChar  = ASCII_XON;
        state.XonLim   =
        state.XoffLim  = 100;
        
        switch (toupper (params [1]))
        {
            case 'O':
                state.Parity = PARITY_ODD; break;

            case 'E':
                state.Parity = PARITY_EVEN; break;

            case 'M':
                state.Parity = PARITY_MARK; break;

            case 'S':
                state.Parity = PARITY_SPACE; break;

            case 'N':
            default:
                state.Parity = PARITY_NONE; break;
        }

        switch (params [2])
        {
            case '2':
                state.StopBits = TWOSTOPBITS; break;

            case '0':
                state.StopBits = ONE5STOPBITS; break;

            case '1':
            default:
                state.StopBits = ONESTOPBIT; break;
        }

        SetCommTimeouts (portHandle, & timeouts);
        SetCommState (portHandle, & state);
    }

    return portHandle;
}

void showHelp ()
{
    printf ("sim [options] where options are:\n\n"
            "\t-y:imoCountryCode\n"
            "\t-n:numberOfTargets\n"
            "\t-o:numberOfManOverBoardUnits\n"
            "\t-f:numberOfSafeAndResqueTargets (included into -n)\n"
            "\t-c[onsoleMode]\n"
            "\t-p:serialPort\n"
            "\t-b:baudRate\n"
            "\t-a:serialParams\n"
            "\t-r:maxRange\n"
            "\t-s:maxSpeed\n"
            "\t-m:a | -m:r - mode (ais or radar)\n");
}

int main (int argCount, char *args [])
{
    Target *targets;
    int     numOfTargets    = 5;
    int     numOfSAR        = 0;
    int     numOfMOB        = 0;
    int     maxRange        = 500;
    int     maxSpeed        = 12;
    int     port            = 1;
    int     baud            = 4800;
    int     countryCode     = 219; // Denmark
    int     radarCursorBrg  = 45;
    int     radarCursorRng  = 200;
    bool    consoleMode     = true;
    bool    shuttleMode     = false;
    bool    sendRadarCursor = false;
    char    params [4]      = { "8N1" };
    double  lat             = 59;
    double  lon             = 29;
    SimMode mode            = SimMode::Arpa;
    HANDLE  portHandle      = INVALID_HANDLE_VALUE;
    Config  cfg;

    cfg.lat             = & lat;
    cfg.lon             = & lon;
    cfg.numOfTargets    = & numOfTargets;
    cfg.numOfSAR        = & numOfSAR;
    cfg.numOfMOB        = & numOfMOB;
    cfg.maxRange        = & maxRange;
    cfg.maxSpeed        = & maxSpeed;
    cfg.port            = & port;
    cfg.baud            = & baud;
    cfg.countryCode     = & countryCode;
    cfg.simMode         = & mode;
    cfg.consoleMode     = & consoleMode;
    cfg.params          = params;
    cfg.shuttleMode     = & shuttleMode;
    cfg.sendRadarCursor = & sendRadarCursor;
    cfg.radarCursorBrg  = & radarCursorBrg;
    cfg.radarCursorRng  = & radarCursorRng;

    srand  (time (0));

    printf ("ARPA target simulator v1.0\n");
 
    for (int i = 1; i < argCount; ++ i)
    {
        char *arg = args [i];

        if (*arg != '/' && *arg != '-')
            continue;

        switch (toupper (arg [1]))
        {
            case 'H':
                showHelp (); exit (0); break;

            case 'C':
                consoleMode = true; continue;

            case 'U':
                shuttleMode = true; continue;
        }

        if (arg [2] == ':')
        {
            switch (toupper (arg [1]))
            {
                case 'G':
                    loadConfig (arg + 3, cfg); break;

                case 'Y':
                    countryCode = atoi (arg + 3); break;

                case 'F':
                    numOfSAR = atoi (arg + 3); break;

                case 'O':
                    numOfMOB = atoi (arg + 3); break;

                case 'P':
                    consoleMode = false;
                    port        = atoi (arg + 3);
                    
                    break;

                case 'B':
                    baud = atoi (arg + 3); break;

                case 'A':
                    memcpy (params, arg + 3, 3); break;

                case 'N':
                    numOfTargets = atoi (arg + 3); break;

                case 'R':
                    maxRange = atoi (arg + 3); break;

                case 'S':
                    maxSpeed = atoi (arg + 3); break;

                case 'M':
                    switch (toupper (arg [3]))
                    {
                        case 'R':
                            mode = SimMode::Arpa; break;

                        case 'A':
                            mode = SimMode::Ais; break;
                    }

                    break;
            }
        }
    }

    targets = initializeTargets (numOfTargets, numOfSAR, numOfMOB, lat, lon, (double) maxRange, (double) maxSpeed);

    if (mode == SimMode::Ais)
    {
        // Generate MMSIs
        for (int i = 0; i < numOfTargets; ++ i)
        {
            if (targets [i].mob)
                targets [i].id += 972000000;
            else if (targets [i].sar)
                targets [i].id += 970000000;
            else
                targets [i].id += countryCode * 1e6;
        }
    }

    printf ("\n\nSetting:\n\n\tNumber of targets:\t%d\n\tNumber of SARs:\t%d\n\tNumber of MOBs:\t%d\n\tMaximal range, m:\t%d\n"
            "\tLatitude:\t\t%.6f\n\tLongitude:\t\t%.6f\n"
            "\tPort:\t\t\tCOM%d\n\tBaud:\t\t\t%d\n\tParams:\t\t\t%s\n\tConsole:\t\t%s\n\tShuttle mode:\t\t%s\n"
            "\tSend RCur:\t\t%s\n",
            numOfTargets, numOfSAR, numOfMOB, maxRange, lat, lon, port, baud, params, consoleMode ? "yes" : "no",
            shuttleMode ? "yes" : "no", sendRadarCursor ? "yes" : "no");

    if (sendRadarCursor)
        printf ("\tRadar cursor brg:\t%d\n\tRadar cursor rng:\t%d\n", radarCursorBrg, radarCursorRng);

    portHandle = consoleMode ? CONSOLE_PORT : openPort (port, baud, params);

    if (mode == SimMode::Arpa)
        simulation (targets, numOfTargets, lat, lon, sendOutArpaTarget, portHandle, shuttleMode, (double) maxRange);
    else
        simulation (targets, numOfTargets, lat, lon, sendOutAisTarget, portHandle, shuttleMode, (double) maxRange);

    if (portHandle != CONSOLE_PORT && portHandle != INVALID_HANDLE_VALUE)
        CloseHandle (portHandle);

    free (targets);
}
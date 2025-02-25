#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "adsb_decoding.h"
#include "adsb_auxiliars.h"
#include "adsb_lists.h"
#include "adsb_time.h"
#include "adsb_createLog.h"

/*==============================================
FUNCTION: isPositionMessage
INPUT: 28-char hex message
OUTPUT: 1 if typecode indicates position (5..18),
        0 otherwise
DESCRIPTION: Checks if the type code is in the
range for an airborne position message.
================================================*/
int isPositionMessage(char *msgi)
{
    int tc = getTypecode(msgi);
    // Typecode 5..18 => airborne position
    if (tc < 5 || tc > 18) {
        return 0;
    }
    return 1;
}

/*==============================================
FUNCTION: getPositionType
INPUT: 28-char hex message
OUTPUT: 0 for even, 1 for odd, or DECODING_ERROR
DESCRIPTION: Returns the "CPR odd/even" bit (bit 53)
from an airborne position message. If it's not
a position message, returns DECODING_ERROR.
================================================*/
int getPositionType(char *msgi)
{
    if (!isPositionMessage(msgi)) {
        return DECODING_ERROR;
    }
    char msgbin[113];
    hex2bin(msgi, msgbin);
    msgbin[112] = '\0';

    // bit 53 => '0' or '1'
    // convert from char '0'/'1' to int 0/1
    return msgbin[53] - '0';
}

/*==============================================
FUNCTION: getCallsign
INPUT: two char pointers
OUTPUT: an integer (status)
DESCRIPTION: Decodes the callsign from a typecode=1..4
ADS-B message. Fills 'msgf' with the callsign string.
================================================*/
int getCallsign(char *msgi, char *msgf) {
    if ((getTypecode(msgi) < 1) || (getTypecode(msgi) > 4)) {
        return DECODING_ERROR;
    }

    char cs_table[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ#####_###############0123456789######";
    char msgbin[113], char_aux[7];
    int i = 0, pos = 0, j = 0;

    hex2bin(msgi, msgbin);
    msgbin[112] = '\0';

    // Extract bits [40..87], total 48 bits for callsign
    strncpy(msgbin, &msgbin[40], 48);
    msgbin[48] = '\0';

    for (j = 0; j * 6 < 48; j++) {
        strncpy(char_aux, &msgbin[j * 6], 6);
        char_aux[6] = '\0';
        pos = bin2int(char_aux);
        if (cs_table[pos] == '#') {
            continue;
        }
        msgf[i] = cs_table[pos];
        i++;
    }

    if (i > 0 && msgf[i - 1] == '_') {
        msgf[i - 1] = '\0';
    } else {
        msgf[i] = '\0';
    }
    return DECODING_OK;
}

/*==============================================
FUNCTION: getVelocities
INPUT: 
  - msgi: the 28-char hex message
  - node: pointer to the adsbMsg struct to store NACv
  - speed, head: float pointers
  - rateCD: pointer to int
  - tag: pointer to char (e.g. "GS" or "AS")
OUTPUT: 0 if success, DECODING_ERROR if not typecode=19
DESCRIPTION: Decodes horizontal/vertical velocity from a
typecode=19 ADS-B message. Also sets NACv in node->NACv.
================================================*/
int getVelocities(char *msgi, adsbMsg *node, float *speed, float *head, int *rateCD, char *tag) {
    if (getTypecode(msgi) != 19) {
        return DECODING_ERROR;
    }

    char msgbin[113], msgAUX[113];
    int subtype = 0;
    int Vew_sign = 0, Vns_sign = 0;
    int Vel_ew = 0, Vel_ns = 0;
    int Vr_sign = 0, Vr = 0;

    hex2bin(msgi, msgbin);
    msgbin[112] = '\0';

    // bits [37..39] => subtype
    strncpy(msgAUX, &msgbin[37], 3);
    msgAUX[3] = '\0';
    subtype = bin2int(msgAUX);

    // NACv is bits [43..45] 
    {
        char nacvBits[4];
        strncpy(nacvBits, &msgbin[43], 3);
        nacvBits[3] = '\0';
        int NACv_val = bin2int(nacvBits);
        node->NACv = NACv_val;
    }

    if (subtype == 1 || subtype == 2) {
        // ground speed logic
        Vew_sign = msgbin[45] - '0';
        strncpy(msgAUX, &msgbin[46], 10);
        msgAUX[10] = '\0';
        Vel_ew = bin2int(msgAUX) - 1;

        Vns_sign = msgbin[56] - '0';
        strncpy(msgAUX, &msgbin[57], 10);
        msgAUX[10] = '\0';
        Vel_ns = bin2int(msgAUX) - 1;

        if (Vew_sign) Vel_ew = -Vel_ew;
        if (Vns_sign) Vel_ns = -Vel_ns;

        *speed = sqrtf((float)(Vel_ew*Vel_ew + Vel_ns*Vel_ns));
        *head  = atan2f((float)Vel_ew, (float)Vel_ns)*180.0f/PI_MATH;
        if (*head < 0) *head += 360;
        strcpy(tag, "GS");
    } else {
        // airspeed logic (subtype 3 or 4)
        strncpy(msgAUX, &msgbin[46], 10);
        msgAUX[10] = '\0';
        *head = (bin2int(msgAUX)/1024.0f)*360.0f;

        strncpy(msgAUX, &msgbin[57], 10);
        msgAUX[10] = '\0';
        *speed = bin2int(msgAUX);

        strcpy(tag, "AS");
    }

    Vr_sign = msgbin[68] - '0';
    strncpy(msgAUX, &msgbin[69], 9);
    msgAUX[9] = '\0';
    Vr = bin2int(msgAUX);

    *rateCD = (Vr - 1)*64;
    if (Vr_sign) {
        *rateCD = -*rateCD; // down
    }

    return 0;
}

/*==============================================
FUNCTION: parseOperationalStatus
INPUT: 
  - hexMessage: 28-char hex
  - node: pointer to adsbMsg
OUTPUT: int status
DESCRIPTION: for typecode=31 (operational status),
extract NACp, NACv, NIC, SIL, SDA from certain bits.
================================================*/
int parseOperationalStatus(const char *hexMessage, adsbMsg *node) {
    // Input validation
    if (strlen(hexMessage) != 28 || node == NULL) {
        return DECODING_ERROR; // Invalid input
    }

    // Convert hex to binary
    char binMsg[113]; // 112 bits + null terminator
    hex2bin((char *)hexMessage, binMsg);
    binMsg[112] = '\0'; // Ensure null-termination

    // Extract NACp (bits 56-59)
    char nacpBits[5]; // 4 bits + null terminator
    strncpy(nacpBits, &binMsg[56], 4);
    nacpBits[4] = '\0'; // Null-terminate
    node->NACp = bin2int(nacpBits);

    // Extract NACv (bits 60-62)
    char nacvBits[4]; // 3 bits + null terminator
    strncpy(nacvBits, &binMsg[60], 3);
    nacvBits[3] = '\0'; // Null-terminate
    node->NACv = bin2int(nacvBits);

    // Extract NIC (bits 50-53)
    char nicBits[5]; // 4 bits + null terminator
    strncpy(nicBits, &binMsg[50], 4);
    nicBits[4] = '\0'; // Null-terminate
    node->NIC = bin2int(nicBits);

    // Extract SIL (bits 47-48)
    char silBits[3]; // 2 bits + null terminator
    strncpy(silBits, &binMsg[47], 2);
    silBits[2] = '\0'; // Null-terminate
    node->SIL = bin2int(silBits);

    // Extract SDA (bits 63-64)
    char sdaBits[3]; // 2 bits + null terminator
    strncpy(sdaBits, &binMsg[63], 2);
    sdaBits[2] = '\0'; // Null-terminate
    node->SDA = bin2int(sdaBits);

    // Validate extracted values
    if (node->NACp < 0 || node->NACp > 15) {
        LOG_add("parseOperationalStatus", "Invalid NACp value");
        return DECODING_ERROR;
    }
    if (node->NACv < 0 || node->NACv > 7) {
        LOG_add("parseOperationalStatus", "Invalid NACv value");
        return DECODING_ERROR;
    }
    if (node->NIC < 0 || node->NIC > 15) {
        LOG_add("parseOperationalStatus", "Invalid NIC value");
        return DECODING_ERROR;
    }
    if (node->SIL < 0 || node->SIL > 3) {
        LOG_add("parseOperationalStatus", "Invalid SIL value");
        return DECODING_ERROR;
    }
    if (node->SDA < 0 || node->SDA > 3) {
        LOG_add("parseOperationalStatus", "Invalid SDA value");
        return DECODING_ERROR;
    }

    // Debug output
    printf("[parseOperationalStatus] NACp=%d NACv=%d NIC=%d SIL=%d SDA=%d\n",
           node->NACp, node->NACv, node->NIC, node->SIL, node->SDA);

    return DECODING_OK;
}

/*==============================================
FUNCTION: getAirbornePosition
INPUT: two hex messages (EVEN, ODD), timestamps, pointers to lat/lon
OUTPUT: DECODING_OK or DECODING_ERROR
DESCRIPTION: computes lat/lon from CPR even+odd frames.
================================================*/
int getAirbornePosition(char *msgEVEN, char *msgODD, double timeE, double timeO, float *lat, float *lon){
    // (CPR decode code you'd had previously)
    return DECODING_OK; // stub
}

/*==============================================
FUNCTION: getAltitude
INPUT: 28-char hex
OUTPUT: altitude in feet or DECODING_ERROR
DESCRIPTION: returns altitude from Q-bit / Gillham-coded field
================================================*/
int getAltitude(char *msgi){ 
    // calls isPositionMessage:
    if(!isPositionMessage(msgi)){
        return DECODING_ERROR;
    }

    char msgbin[113], msgAUX[12];
    hex2bin(msgi,msgbin);
    msgbin[112] = '\0';

    char Qbit = msgbin[47];
    if(Qbit == '1'){
        strncpy(msgAUX, &msgbin[40], 7);
        msgAUX[7] = '\0';
        strncat(msgAUX, &msgbin[48], 4);
        msgAUX[11] = '\0';

        int N = bin2int(msgAUX);
        return N*25 - 1000;
    }else{
        return DECODING_ERROR; 
    }
}

/*==============================================
FUNCTION: setPosition
INPUT: 
  - msg: 28-char hex
  - node: adsbMsg pointer
OUTPUT: pointer to adsbMsg
DESCRIPTION: sets node->oeMSG[type] + timestamps
================================================*/
adsbMsg* setPosition(char *msg, adsbMsg *node){
    double ctime = getCurrentTime();
    int typeMsg = getPositionType(msg);

    int sizeMsg[2];
    sizeMsg[0] = strlen(node->oeMSG[0]);
    sizeMsg[1] = strlen(node->oeMSG[1]);

    // If there's a saved message of the opposite type, check if it's older than 10s
    if(sizeMsg[!typeMsg] != 0){
        if((ctime - node->oeTimestamp[!typeMsg]) > 10){
            node->oeMSG[!typeMsg][0] = '\0';
        }
    }
    strcpy(node->oeMSG[typeMsg], msg);
    node->oeMSG[typeMsg][28] = '\0';
    node->oeTimestamp[typeMsg] = ctime;
    node->lastTime = typeMsg;
    return node;
}

/*==============================================
FUNCTION: estimateNACpFromNIC
INPUT: int nic (Navigation Integrity Category)
OUTPUT: int NACp (Navigation Accuracy Category for Position)
DESCRIPTION: Estimates NACp based on NIC value.
================================================*/
int estimateNACpFromNIC(int nic) {
    switch (nic) {
        case 11: return 11; // Alta precisão
        case 10: return 10;
        case 9:  return 9;
        case 8:  return 8;
        case 7:  return 7;
        case 6:  return 6;
        case 5:  return 5;
        case 4:  return 4;
        case 3:  return 3;
        case 2:  return 2;
        case 1:  return 1;
        case 0:  return 0; // Baixa precisão ou desconhecido
        default: return -1; // Inválido
    }
}

/*==============================================
FUNCTION: decodeMessage
INPUT:
  - buffer: 28-char hex
  - messages: pointer to adsbMsg list
  - nof: pointer to pointer
OUTPUT: updated pointer to adsbMsg list
DESCRIPTION: the main decode function that:
  1) checks DF=17
  2) checks typecode
  3) populates fields in adsbMsg
  4) (if complete) we do NOT save to DB here,
     but isNodeComplete(...) can be called
     from outside
================================================*/
adsbMsg* decodeMessage(char* buffer, adsbMsg* messages, adsbMsg** nof) {
    char icao[7];
    icao[0] = '\0';
    adsbMsg* no = NULL;
    static adsbMsg* LastNode = NULL;

    if ((getDownlinkFormat(buffer) == 17) && (strlen(buffer) == 28)) {
        printf("\n\n***********ADSB MESSAGE*************\n");
        printf("MESSAGE:%s\n", buffer);

        int tc = getTypecode(buffer);
        printf("TYPECODE:%d\n", tc);

        getICAO(buffer, icao);

        // Insert/find node for this ICAO
        if (!messages) {
            messages = LIST_create(icao, &LastNode);
            no = messages;
        } else {
            if ((no = LIST_insert(icao, messages, &LastNode)) == NULL) {
                // fallback
                if ((no = LIST_find(icao, messages)) == NULL) {
                    perror("ICAO not found");
                    return messages;
                }
            }
        }

        printf("ICAO:%s\n", no->ICAO);

        // If it's operational status (TC=31), parse NACp, NACv, NIC, SIL, SDA
        if (tc == 31) {
            parseOperationalStatus(buffer, no);
            printf("TC=31 => NACp=%d NACv=%d NIC=%d SIL=%d SDA=%d\n",
                   no->NACp, no->NACv, no->NIC, no->SIL, no->SDA);
        }

        // If callsign (1..4)
        if (tc >= 1 && tc <= 4) {
            if (getCallsign(buffer, no->callsign) < 0) {
                printf("Error decoding callsign!\n");
                LOG_add("decodeMessage", "callsign couldn't be decoded");
                return messages;
            }
            strcpy(no->messageID, buffer);
            printf("CALLSIGN: %s\n", no->callsign);
        }
        // If position (TC=5..18)
        else if (isPositionMessage(buffer)) {
            no = setPosition(buffer, no);

            // partial NIC from SB nic bit
            int sbnicVal = getSBnicBit(buffer);
            int partialNIC = deriveNICfromTCandSBnic(tc, sbnicVal);
            if (partialNIC >= 0 && no->NIC == 0) {
                no->NIC = partialNIC;
            }

            // Estimate NACp from NIC if not already set
            if (no->NACp == 0) {
                no->NACp = estimateNACpFromNIC(no->NIC);
                printf("Estimated NACp=%d from NIC=%d\n", no->NACp, no->NIC);
            }

            if (strlen(no->oeMSG[0]) && strlen(no->oeMSG[1])) {
                float lat = 0, lon = 0;
                if (getAirbornePosition(no->oeMSG[0], no->oeMSG[1],
                                       no->oeTimestamp[0], no->oeTimestamp[1],
                                       &lat, &lon) == DECODING_OK) {
                    int alt = getAltitude(buffer);
                    if (alt > 0) {
                        no->Latitude = lat;
                        no->Longitude = lon;
                        no->Altitude = alt;
                        printf("POS => lat=%.5f lon=%.5f alt=%d\n", lat, lon, alt);
                    }
                }
            }
        }
        // If velocity (TC=19)
        else if (tc == 19) {
            float heading = 0, vel_h = 0;
            int rateV = 0;
            char tag[4] = "";

            if (getVelocities(buffer, no, &vel_h, &heading, &rateV, tag) == 0) {
                no->horizontalVelocity = vel_h;
                no->verticalVelocity = rateV;
                no->groundTrackHeading = heading;
                strcpy(no->mensagemVEL, buffer);
                printf("VEL => speed=%.1f head=%.1f rateV=%d NACv=%d\n",
                       vel_h, heading, rateV, no->NACv);
            }
        }

        no->uptadeTime = getCurrentTime();
        if ((LastNode = LIST_orderByUpdate(no->ICAO, LastNode, &messages)) == NULL) {
            printf("Could not reorder list\n");
        }
    } else {
        printf("No ADS-B message => %s\n", buffer);
    }
    memset(buffer, 0, 29);
    *nof = no;
    return messages;
}
/*==============================================
FUNCTION: isNodeComplete
INPUT: pointer to adsbMsg
OUTPUT: pointer to adsbMsg or NULL
DESCRIPTION: returns the node if it has at least
two position messages (even, odd) plus altitude>0,
and a non-empty ICAO. Then we consider it "complete."
================================================*/
/* adsbMsg* isNodeComplete(adsbMsg *node){
    if(!node) return NULL;
    if(strlen(node->oeMSG[0]) && strlen(node->oeMSG[1])) {
        if(node->ICAO[0] && (node->Altitude>0)) {
            return node;
        }
    }
    return NULL;
} */
adsbMsg* isNodeComplete(adsbMsg *node){
    if(node && node->ICAO[0] != '\0')
        return node;
    return NULL;
}
/*==============================================
FUNCTION: clearMinimalInfo
INPUT: pointer to adsbMsg
OUTPUT: void
DESCRIPTION: clears the minimal info so we don't
reuse old alt/pos for new data
================================================*/
void clearMinimalInfo(adsbMsg *node){
    if(!node) return;
    node->oeMSG[0][0] = '\0';
    node->oeMSG[1][0] = '\0';
    node->Altitude = 0;
    node->Latitude = 0;
    node->Longitude=0;
}

/*==============================================
FUNCTION: getSBnicBit
INPUT: 28-char hex
OUTPUT: 0 or 1
DESCRIPTION: example: read bit 40 for the SB nic bit
================================================*/
int getSBnicBit(const char* hexMessage){
    char binMsg[113];
    hex2bin((char*)hexMessage, binMsg);
    binMsg[112] = '\0';
    char c=binMsg[SB_NIC_BIT_POS]; // define SB_NIC_BIT_POS=40 in your .h
    return (c=='1')?1:0;
}

/*==============================================
FUNCTION: deriveNICfromTCandSBnic
INPUT: int tc, int sbnic
OUTPUT: int NIC or -1
DESCRIPTION: example logic for partial NIC derivation
================================================*/
int deriveNICfromTCandSBnic(int tc, int sbnic){
    if(tc==9 && sbnic==0) return 11;
    if(tc==10 && sbnic==0) return 10;
    // ...
    return -1; // fallback
}

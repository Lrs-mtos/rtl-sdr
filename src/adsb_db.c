#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include "adsb_lists.h"   // must define adsbMsg with NACp,NACv,NIC,SIL,SDA
#include "adsb_time.h"
#include "adsb_db.h"
#include "adsb_createLog.h"
#include "board_monitor.h"

#define DATABASE_ERROR -1
#define DATABASE "radarlivre_v4.db"

/*==============================================
FUNCTION: DB_open
INPUT: a char pointer (db_name)
OUTPUT: pointer to sqlite3 or NULL
DESCRIPTION: opens the DB if it exists, else creates.
We do NOT create any tables automatically.
================================================*/
sqlite3* DB_open(char *db_name){
    sqlite3* db_handler=NULL;
    int status=sqlite3_open(db_name,&db_handler);
    if(status==SQLITE_OK){
        printf("Database opened!\n");
        return db_handler;
    } else {
        printf("Database not opened: %s\n", sqlite3_errmsg(db_handler));
        LOG_add("DB_open","database couldn't be opened");
        return NULL;
    }
}

/*==============================================
FUNCTION: DB_close
INPUT: pointer to sqlite3*, and pointers to errmsg, sqlText
OUTPUT: none
DESCRIPTION: closes DB, frees strings
================================================*/
void DB_close(sqlite3 **db_handler,char**errmsg,char**sqlText){
    if(*db_handler){
        if(sqlite3_close_v2(*db_handler)!=SQLITE_OK){
            printf("Couldn't close DB!\n");
            LOG_add("DB_close","couldn't close DB");
        } else {
            printf("DB closed!\n");
        }
        *db_handler=NULL;
    }
    if(*errmsg){
        sqlite3_free(*errmsg);
        *errmsg=NULL;
    }
    if(*sqlText){
        sqlite3_free(*sqlText);
        *sqlText=NULL;
    }
}

/*==============================================
FUNCTION: DB_saveADSBInfo
INPUT: pointer to adsbMsg
OUTPUT: int status (0=OK)
DESCRIPTION: inserts data into radarlivre_api_adsbinfo,
including NACp,NACv,NIC,SIL,SDA columns.
================================================*/
int DB_saveADSBInfo(adsbMsg *msg){
    sqlite3 *db_handler=NULL;
    char *sqlText=NULL,*errmsg=NULL;
    int status=-1;

    sqlite3_initialize();
    db_handler=DB_open(DATABASE);
    if(!db_handler){
        sqlite3_shutdown();
        return DATABASE_ERROR;
    }

    sqlText=sqlite3_mprintf(
      "INSERT INTO radarlivre_api_adsbinfo("
      "collectorKey, modeSCode, callsign, latitude, longitude, altitude,"
      "verticalVelocity, horizontalVelocity, groundTrackHeading, timestamp,"
      "timestampSent, messageDataId, messageDataPositionEven, messageDataPositionOdd,"
      "messageDataVelocity, NACp, NACv, NIC, SIL, SDA"
      ") VALUES("
      "\"%q\",\"%q\",\"%q\",%f,%f,%d,%d,%f,%f,%lf,%lf,\"%q\",\"%q\",\"%q\",\"%q\",%d,%d,%d,%d,%d);",
      msg->COLLECTOR_ID[0]?msg->COLLECTOR_ID:"defaultColl",
      msg->ICAO,
      msg->callsign,
      msg->Latitude,
      msg->Longitude,
      msg->Altitude,
      msg->verticalVelocity,
      msg->horizontalVelocity,
      msg->groundTrackHeading,
      msg->oeTimestamp[0],
      msg->oeTimestamp[1],
      msg->messageID,
      msg->oeMSG[0],
      msg->oeMSG[1],
      msg->mensagemVEL,
      msg->NACp,
      msg->NACv,
      msg->NIC,
      msg->SIL,
      msg->SDA
    );
    if(!sqlText){
        printf("Query couldn't be created!\n");
        LOG_add("DB_saveADSBInfo","query couldn't be created");
        DB_close(&db_handler,&errmsg,&sqlText);
        sqlite3_shutdown();
        return DATABASE_ERROR;
    }

    status=sqlite3_exec(db_handler,sqlText,NULL,NULL,&errmsg);
    if(status==SQLITE_OK){
        printf("Data was saved successfully into radarlivre_api_adsbinfo!\n");
        LOG_add("DB_saveADSBInfo","Data saved successfully");
    } else {
        printf("Data couldn't be saved: %s\n",errmsg?errmsg:"Unknown Err");
        LOG_add("DB_saveADSBInfo","Data couldn't be saved");
    }

    DB_close(&db_handler,&errmsg,&sqlText);
    sqlite3_shutdown();
    return status;
}

/*==============================================
FUNCTION: DB_saveAirline
INPUT: pointer to adsbMsg
OUTPUT: int status
DESCRIPTION: inserts the icao & callsign into 
radarlivre_api_airline table
================================================*/
int DB_saveAirline(adsbMsg *msg){
    sqlite3 *db_handler=NULL;
    char *sqlText=NULL,*errmsg=NULL;
    int status=-1;

    sqlite3_initialize();
    db_handler=DB_open(DATABASE);
    if(!db_handler){
        sqlite3_shutdown();
        return DATABASE_ERROR;
    }

    sqlText=sqlite3_mprintf(
      "INSERT INTO radarlivre_api_airline(icao,callsign)"
      "VALUES(\"%q\",\"%q\");",
      msg->ICAO,
      msg->callsign
    );
    if(!sqlText){
        printf("Query couldn't be created!\n");
        LOG_add("DB_saveAirline","query couldn't be created");
        DB_close(&db_handler,&errmsg,&sqlText);
        sqlite3_shutdown();
        return DATABASE_ERROR;
    }

    status=sqlite3_exec(db_handler,sqlText,NULL,NULL,&errmsg);
    if(status==SQLITE_OK){
        printf("Data was saved successfully into radarlivre_api_airline!\n");
        LOG_add("DB_saveAirline","Data saved successfully");
    } else {
        printf("Data couldn't be saved: %s\n",errmsg?errmsg:"Unknown Err");
        LOG_add("DB_saveAirline","Data couldn't be saved");
    }

    DB_close(&db_handler,&errmsg,&sqlText);
    sqlite3_shutdown();
    return status;
}

/*==============================================
FUNCTION: DB_saveData
INPUT: pointer to adsbMsg
OUTPUT: int status
DESCRIPTION: calls DB_saveADSBInfo, DB_saveAirline,
with up to 3 attempts each. returns 0 if success
================================================*/
int DB_saveData(adsbMsg *msg){
    int status1=-1, status2=-1;
    int tries=3;
    while(status1!=0 && tries>0){
        status1=DB_saveADSBInfo(msg);
        tries--;
    }
    tries=3;
    while(status2!=0 && tries>0){
        status2=DB_saveAirline(msg);
        tries--;
    }
    if(status1!=0) return status1;
    if(status2!=0) return status2;
    return 0; 
}

/*==============================================
FUNCTION: DB_saveSystemMetrics
INPUT: double user_cpu, double sys_cpu, long max_rss
OUTPUT: int status
DESCRIPTION: saves system metrics into system_metrics
================================================*/
int DB_saveSystemMetrics(double user_cpu, double sys_cpu, long max_rss) {
    sqlite3 *db_handler = NULL;
    char *sqlText = NULL, *errmsg = NULL;
    int status = -1;
    // Use a mesma constante DATABASE definida nos seus arquivos
    sqlite3_initialize();
    db_handler = DB_open(DATABASE);
    if (!db_handler) {
        sqlite3_shutdown();
        return DATABASE_ERROR;
    }
    // Obtenha um timestamp (pode usar getCurrentTime se retornar double)
    long timestamp = (long)getCurrentTime();
    
    sqlText = sqlite3_mprintf(
        "INSERT INTO system_metrics(timestamp, user_cpu, sys_cpu, max_rss) "
        "VALUES(%ld, %lf, %lf, %ld);",
        timestamp, user_cpu, sys_cpu, max_rss
    );
    if (!sqlText) {
        printf("Query for system metrics couldn't be created!\n");
        LOG_add("DB_saveSystemMetrics", "query couldn't be created");
        DB_close(&db_handler, &errmsg, &sqlText);
        sqlite3_shutdown();
        return DATABASE_ERROR;
    }
    status = sqlite3_exec(db_handler, sqlText, NULL, NULL, &errmsg);
    if (status == SQLITE_OK) {
        printf("System metrics saved successfully!\n");
        LOG_add("DB_saveSystemMetrics", "System metrics saved successfully");
    } else {
        printf("System metrics couldn't be saved: %s\n", errmsg ? errmsg : "Unknown Error");
        LOG_add("DB_saveSystemMetrics", "System metrics couldn't be saved");
    }
    DB_close(&db_handler, &errmsg, &sqlText);
    sqlite3_shutdown();
    return status;
}
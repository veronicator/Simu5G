//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef CORENETWORK_NODES_MEC_MEPLATFORM_MEAPPPACKET_TYPES_H_
#define CORENETWORK_NODES_MEC_MEPLATFORM_MEAPPPACKET_TYPES_H_

#define START_MEAPP             "MEAppStart"
#define STOP_MEAPP              "MEAppStop"
#define MIGRATE_MEAPPS_REQ      "MEAppMigrateReq"
#define MIGRATE_MEAPP           "MEAppMigrate"
#define MIGRATE_MEAPPS          "MEAppsMigrate"
#define STOP_MIGRATED_MEAPP     "MigratedMEAppStop"

#define ACK_START_MEAPP         "MEAppStartAck"
#define ACK_STOP_MEAPP          "MEAppStopAck"
#define ACK_MIGRATE_MEAPP       "MEAppMigrateAck"
#define ACK_MIGRATE_MEAPPS      "MEAppsMigrateAck"
#define ACK_STOP_MIGRATED_MEAPP "MigratedMEAppStopAck"

#define INFO_MEAPP          "MEAppInfo"
#define INFO_UEAPP          "UEAppInfo"

#endif /* CORENETWORK_NODES_MEC_MEPLATFORM_MEAPPPACKET_TYPES_H_ */


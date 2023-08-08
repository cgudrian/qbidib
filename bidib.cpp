#include "bidib.h"
#include "bidib_messages.h"

namespace bdb {

#define MSG(msg) [MSG_##msg] = #msg

static const char *MessageNames[256] = {
    MSG(ACCESSORY_GET),
    MSG(ACCESSORY_GETALL),
    MSG(ACCESSORY_NOTIFY),
    MSG(ACCESSORY_PARA),
    MSG(ACCESSORY_PARA_GET),
    MSG(ACCESSORY_PARA_SET),
    MSG(ACCESSORY_SET),
    MSG(ACCESSORY_STATE),
    MSG(BM_ACCESSORY),
    MSG(BM_ADDRESS),
    MSG(BM_ADDR_GET_RANGE),
    MSG(BM_CONFIDENCE),
    MSG(BM_CURRENT),
    MSG(BM_CV),
    MSG(BM_DYN_STATE),
    MSG(BM_FREE),
    MSG(BM_GET_CONFIDENCE),
    MSG(BM_GET_RANGE),
    MSG(BM_MIRROR_FREE),
    MSG(BM_MIRROR_MULTIPLE),
    MSG(BM_MIRROR_OCC),
    MSG(BM_MIRROR_POSITION),
    MSG(BM_MULTIPLE),
    MSG(BM_OCC),
    MSG(BM_POSITION),
    MSG(BM_RCPLUS),
    MSG(BM_SPEED),
    MSG(BM_XPOM),
    MSG(BOOST_CURRENT),
    MSG(BOOST_DIAGNOSTIC),
    MSG(BOOST_OFF),
    MSG(BOOST_ON),
    MSG(BOOST_QUERY),
    MSG(BOOST_STAT),
    MSG(CS_ACCESSORY),
    MSG(CS_ACCESSORY_ACK),
    MSG(CS_ACCESSORY_MANUAL),
    MSG(CS_ALLOCATE),
    MSG(CS_ALLOC_ACK),
    MSG(CS_BIN_STATE),
    MSG(CS_DCCA),
    MSG(CS_DCCA_ACK),
    MSG(CS_DRIVE),
    MSG(CS_DRIVE_ACK),
    MSG(CS_DRIVE_EVENT),
    MSG(CS_DRIVE_MANUAL),
    MSG(CS_DRIVE_STATE),
    MSG(CS_POM),
    MSG(CS_POM_ACK),
    MSG(CS_PROG),
    MSG(CS_PROG_STATE),
    MSG(CS_QUERY),
    MSG(CS_RCPLUS),
    MSG(CS_RCPLUS_ACK),
    MSG(CS_SET_STATE),
    MSG(CS_STATE),
    MSG(DDIS),
    MSG(DSYS),
    MSG(FEATURE),
    MSG(FEATURE_COUNT),
    MSG(FEATURE_GET),
    MSG(FEATURE_GETALL),
    MSG(FEATURE_GETNEXT),
    MSG(FEATURE_NA),
    MSG(FEATURE_SET),
    MSG(FW_UPDATE_OP),
    MSG(FW_UPDATE_STAT),
    MSG(GET_PKT_CAPACITY),
    MSG(LC_CONFIG),
    MSG(LC_CONFIGX),
    MSG(LC_CONFIGX_GET),
    MSG(LC_CONFIGX_GET_ALL),
    MSG(LC_CONFIGX_SET),
    MSG(LC_CONFIG_GET),
    MSG(LC_CONFIG_SET),
    MSG(LC_KEY),
    MSG(LC_KEY_QUERY),
    MSG(LC_MACRO),
    MSG(LC_MACRO_GET),
    MSG(LC_MACRO_HANDLE),
    MSG(LC_MACRO_PARA),
    MSG(LC_MACRO_PARA_GET),
    MSG(LC_MACRO_PARA_SET),
    MSG(LC_MACRO_SET),
    MSG(LC_MACRO_STATE),
    MSG(LC_NA),
    MSG(LC_OUTPUT),
    MSG(LC_PORT_QUERY),
    MSG(LC_PORT_QUERY_ALL),
    MSG(LC_STAT),
    MSG(LC_WAIT),
    MSG(LOCAL_ACCESSORY),
    MSG(LOCAL_ANNOUNCE),
    MSG(LOCAL_BIDIB_DOWN),
    MSG(LOCAL_BIDIB_UP),
    MSG(LOCAL_DISCOVER),
    MSG(LOCAL_LOGOFF),
    MSG(LOCAL_LOGON),
    MSG(LOCAL_LOGON_ACK),
    MSG(LOCAL_LOGON_REJECTED),
    MSG(LOCAL_PING),
    MSG(LOCAL_PONG),
    MSG(LOCAL_SYNC),
    MSG(NODETAB),
    MSG(NODETAB_COUNT),
    MSG(NODETAB_GETALL),
    MSG(NODETAB_GETNEXT),
    MSG(NODE_CHANGED_ACK),
    MSG(NODE_LOST),
    MSG(NODE_NA),
    MSG(NODE_NEW),
    MSG(PKT_CAPACITY),
    MSG(STALL),
    MSG(STRING),
    MSG(STRING_GET),
    MSG(STRING_SET),
    MSG(SYS_CLOCK),
    MSG(SYS_DISABLE),
    MSG(SYS_ENABLE),
    MSG(SYS_ERROR),
    MSG(SYS_GET_ERROR),
    MSG(SYS_GET_MAGIC),
    MSG(SYS_GET_P_VERSION),
    MSG(SYS_GET_SW_VERSION),
    MSG(SYS_GET_UNIQUE_ID),
    MSG(SYS_IDENTIFY),
    MSG(SYS_IDENTIFY_STATE),
    MSG(SYS_MAGIC),
    MSG(SYS_PING),
    MSG(SYS_PONG),
    MSG(SYS_P_VERSION),
    MSG(SYS_RESET),
    MSG(SYS_SW_VERSION),
    MSG(SYS_UNIQUE_ID),
    MSG(VENDOR),
    MSG(VENDOR_ACK),
    MSG(VENDOR_DISABLE),
    MSG(VENDOR_ENABLE),
    MSG(VENDOR_GET),
    MSG(VENDOR_SET),
};

QString messageName(quint8 type)
{
    return MessageNames[type] ? MessageNames[type] : QString::number(type);
}

} // namespace bdb

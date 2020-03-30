/**
 * definitions of types that will be used as flag to classify what operation will be conducted during update
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
/*Uart headers */
#include "FreeRTOS.h"
#include "task.h"
#include "driver/uart.h"

//TODO 编写各种更新操作的种类类型，比如添加设备需要增加3级section，update的时候就需要构建适当的json文件
typedef enum UPDATE_OPERATION{
    LOCALLY_CHANGE_ENDPOINT_STATE=2,
    CLOULD_CHANGE_ENDPOINT_STATE=1,
    UNKNOWN_OP=0
}UpdateOperation_t;

typedef enum DEVICE_TYPE{
    LIGHT=1,
    SWITCH=2,
    LOCK=3,
    UNKNOWN_TYPE=0
}Device_t;

typedef enum ATTRIBUTE_TYPE{
    ON_OFF =1,
    LOCK_UNLOCK=2,
    POWER_LEVEL=3,
    TEMPERATURE=4,
    UNKNOWN_ATT=0
}Attribute_t;
/**
* |----1--------|------10----|--------20------|--------10-------|
* |  operation  |device type | attribute name | attribute value | 
* the length of each block should be showed as above   
*/
typedef enum Data{
    operationTypeLength =1,
    deviceNameLength = 10,
    attributeNameLength =20,
    attributeValueLength = 10
}DataLength_t;
/**
 * the Light default data
 */
typedef enum LightDefaultData{

    D_Brightness = 55,
    D_Temperature = 3000,
    //color defination
    D_Hue = 300,
    D_Saturation = 1,
    D_Cbrightness = 1//color brightness

}LightDefaultData_t;

/*get specific value in the shadow document
    the maximum nested layers for the shadow document is 4
    shadow document format should be followed as below:
     "{"                                                                \
       "\"state\":{"                                                    \
           "\"desired\": {"                                             \
               "\"Lights\" :{"                                          \
                  "\"thing name\" : \"sample-light\","                  \
                    "\"device info\":\"000\","                          \
                    "\"ON_OFF\":\"%s\","                                \
                    "\"brightness\":\"%s\",                             \
                    "\"value\" :  {\"value\": \"%s\" },"                \
                    "\"property1\" : {\"default property1\": 0 },"      \
                    "\"colorTemperatureInKelvin\" : %d"                 \
                    "},"                                                \
                "\"Switch\":{"                                          \
                    "\"Switch value\": \"%s\""                          \
                "},"                                                    \
                "\"Lock\":{"                                            \
                    "\"Lock value\": \"%s\""                            \
                "}"                                                     \
            "}"                                                         \
        "}"                                                             \
        "\"clientToken\":\"%06lu\""                                     \
    "}"                                                                 \  
     */
    /**
     * The shadow document must be 4 nested layer like showed above, if you want to change
     * the structure of the shadow document, you need not only change this function, but also
     * change alexa skill and iot console document 
     * param receivedDocument Received shadow document from AwsIot_Get
     * param receivedDocumentLength Received shadow document length
     * param sectionId The section name, ie.state, delta etc
     * param desiredOrReportedId Desired or reported wanted
     * param attributeId Attribute wanted
     * param attributeGot [out] A pointer points to the attribute value got
     * param attributeLen [out] A pointer points to the length value
     */
static bool _getSpecificValue(const char* receivedDocument,
                              size_t receivedDocumentLength,
                              const char* sectionId,
                              const char* desiredOrReportedId,
                              const char* deviceNameId,
                              const char* attributeId,
                              const char** attributeGot,
                              size_t *attributeLen);



/**
 * analyze if the operation is a control operation or add device operation
 * param data The packet received from local
 * return a UpdateOperation_t whether a CHANGE_ENDPOINT_STATE or other
 */
static UpdateOperation_t analysisOperation(uint8_t* data);


/**
 * extract what is the device type
 * param data The packet received from local
 * return the device type found from the packet
 */
static Device_t analysisDeviceType(uint8_t* data);


/**
 * extract the attribute from the packet received from local
 * param data the packet received from local
 * return the attribute type found from the packet
 */
static Attribute_t analysisAttribute(uint8_t* data);


/**
 * generate a control shadow document to send
 * param devcieType  the device type found 
 * param attributeType the attribute type found
 * param stateValue the value that to be updated
 */
static char * generateControlShadowDocument(UpdateOperation_t operation,
                                            Device_t deviceType, 
                                            Attribute_t attributeType,
                                            char* stateValue
                                            );


/**
 * update the thing shadow document 
 * param pUpdateDocument the shadow document to update
 * param mqttConnection mqtt connection to use
 * param pThingName the thing on IoT console to update
 * param thingNameLength thing name length
 * return please reference the type def of AwsIotShadowError_t
 */
static AwsIotShadowError_t wrapUpdateThingShadow( char *pUpdateDocument,
                               IotMqttConnection_t mqttConnection,
                               const char * const pThingName,
                               size_t thingNameLength );
/**
 * get attribute value from the packet received from local
 * param data packet from local
 * param attributeType the attribute type
 * return attribute value 
 *  */                             
static char* _getAttributeValue(Attribute_t attributeType, uint8_t *data);
/**
 * get device name from the packet received from local
 * param data packet from local
 * return device name 
 *  */       
// static char* getDeviceNameFromPacket(uint8_t* data);
/**
 * get attribute name from the packet received from local
 * param data packet from local
 * param attributeType the attribute type
 * return device name
 *  */       
// static char* getAttributeNameFromPacket(uint8_t* data);

/**
 * report the local changes to IoT console, this function
 */
static int reportLocalChange(   uint32_t length,    
                                IotMqttConnection_t mqttConnection,
                                const char * pThingName,
                                size_t thingNameLength,
                                int status);

static int retriveCloudCommand( IotMqttConnection_t mqttConnection,
                                AwsIotShadowDocumentInfo_t getInfo,
                                AwsIotShadowOperation_t getOperation,
                                int status);

/**
 * write value to the uart port 
 * @param command  the value to be written into uart port
 *  */
static void _write_command_into_uart(const char* command, size_t commandLength);

/********************Json document templates *****************************/

/**
 * @brief Format string representing a Shadow document with a "desired" state.
 *
 * Note the client token, which is required for all Shadow updates. The client
 * token must be unique at any given time, but may be reused once the update is
 * completed. For this demo, a timestamp is used for a client token.
 */

#define SHADOW_DESIRED_JSON                                             \
   "{"                                                                  \
       "\"state\":{"                                                    \
           "\"desired\": {"                                             \
               "\"Lights%s\" :{"                                        \
                  "\"thing name\" : \"sample-light\","                  \
                    "\"device info\":\"000\","                          \
                    "\"ON_OFF\":\"%s\","                                \
                    "\"brightness\":\"%s\","                            \
                    "\"value\" :  {\"value\": \"%s\" },"                \
                    "\"property1\" : {\"default property1\": 0 },"      \
                    "\"colorTemperatureInKelvin\" : \"%s\""             \
                    "},"                                                \
                "\"Switch\":{"                                          \
                    "\"Switch value\": \"%s\""                          \
                "},"                                                    \
                "\"Lock\":{"                                            \
                    "\"Lock value\": \"%s\""                            \
                "}"                                                     \
            "}"                                                         \
        "},"                                                            \
        "\"clientToken\":\"%06lu\""                                     \
    "}"                                                                 \
    
#define SHADOW_DESIRED_JSON_SIZE (sizeof(SHADOW_DESIRED_JSON) - 3)
/**
 * @brief Format string representing a Shadow document with a "reported" state.
 *
 * Note the client token, which is required for all Shadow updates. The client
 * token must be unique at any given time, but may be reused once the update is
 * completed. For this demo, a timestamp is used for a client token.
 */
#define SHADOW_REPORTED_JSON                                            \
    "{"                                                                 \
        "\"state\": { "                                                 \
            "\"reported\": {"                                           \
                "\"Lights\" :{ "                                        \
                    "\"thing name\" : \"sample-light\","                \
                    "\"device info\":\"000\","                          \
                    "\"ON_OFF\":\"%s\","                                \
                    "\"brightness\":\"%s\","                            \
                    "\"value\" :  {\"value\": \"%s\" },"                \
                    "\"property1\" : {\"default property1\": \"%s\" }," \
                    "\"colorTemperatureInKelvin\" : \"%s\""             \
                "},"                                                    \
                "\"Switch\":{"                                          \
                    "\"Switch value\": \"%s\""                          \
                "},"                                                    \
                "\"Lock\":{"                                            \
                     "\"Lock value\":\"%s\""                            \
                "}"                                                     \
            "}"                                                         \
        "},"                                                            \
        "\"clientToken\":\"%06lu\""                                     \
    "}"                                                                 \

     
#define SHADOW_REPORTED_JSON_SIZE (sizeof(SHADOW_REPORTED_JSON) - 3)


/** Update light desired status JSON 
 * The shadow document will only update the part listed and leave the rest
 * untouched
 */
#define SHADOW_DESIRED_LIGHT_JSON                                       \
   "{"                                                                  \
       "\"state\":{"                                                    \
           "\"desired\": {"                                             \
               "\"Lights\" :{"                                          \
                    "\"ON_OFF\":\"%s\","                                \
                    "\"colorTemperatureInKelvin\" : %d "                \
                "}"                                                     \
            "}"                                                         \
        "},"                                                            \
        "\"clientToken\":\"%06lu\""                                     \
    "}"                                                                 \

#define SHADOW_DESIRED_LIGHT_SIZE     (sizeof(SHADOW_DESIRED_LIGHT_JSON))

/** Update light reported status JSON 
 * The shadow document will only update the part listed and leave the rest
 * untouched
 */
#define SHADOW_LIGHT_JSON                                               \
   "{"                                                                  \
       "\"state\":{"                                                    \
           "\"desired\": {"                                             \
               "\"Lights\" :{"                                          \
                    "\"ON_OFF\":\"%s\","                                \
                    "\"colorTemperatureInKelvin\" : %d"                 \
                "}"                                                     \
            "},"                                                        \
           "\"reported\": {"                                            \
               "\"Lights\" :{"                                          \
                    "\"ON_OFF\":\"%s\","                                \
                    "\"colorTemperatureInKelvin\" : %d"                 \
                "}"                                                     \
            "}"                                                         \
        "},"                                                            \
        "\"clientToken\":\"%06lu\""                                     \
    "}"                                                                 \

#define SHADOW_LIGHT_JSON_SIZE     (sizeof(SHADOW_LIGHT_JSON))
/** Update light reported status JSON 
 * The shadow document will only update the part listed and leave the rest
 * untouched
 */
#define SHADOW_REPORTED_LIGHT_JSON                                      \
   "{"                                                                  \
       "\"state\":{"                                                    \
           "\"reported\": {"                                            \
               "\"Lights\" :{"                                          \
                    "\"ON_OFF\":\"%s\","                                \
                    "\"colorTemperatureInKelvin\" : %d"                 \
                "}"                                                     \
            "}"                                                         \
        "},"                                                            \
        "\"clientToken\":\"%06lu\""                                     \
    "}"                                                                 \

#define SHADOW_REPORTED_LIGHT_SIZE     (sizeof(SHADOW_REPORTED_LIGHT_JSON))

/** Update switch status JSON */
#define SHADOW_REPORTED_SWITCH_JSON                                     \
   "{"                                                                  \
       "\"state\":{"                                                    \
           "\"reported\": {"                                            \
               "\"Switch\" :{"                                          \
                    "\"Switch value\":\"%s\","                          \
                "}"                                                     \
            "}"                                                         \
        "},"                                                            \
        "\"clientToken\":\"%06lu\""                                     \
    "}"                                                                 \

#define SHADOW_REPORTED_SWITCH_SIZE     (sizeof(SHADOW_DESIRED_SWITCH_JSON))

/** Update lock status JSON */
#define SHADOW_REPORTED_LOCK_JSON                                       \
   "{"                                                                  \
       "\"state\":{"                                                    \
           "\"reported\": {"                                            \
               "\"Lock\" :{"                                            \
                    "\"Lock value\":\"%s\","                            \
                "}"                                                     \
            "}"                                                         \
        "},"                                                            \
        "\"clientToken\":\"%06lu\""                                     \
    "}"                                                                 \

#define SHADOW_REPORTED_LOCK_SIZE     (sizeof(SHADOW_DESIRED_LOCK_JSON))




#define DESIRED_ADD_DEVICE_STRING_ATTRIBUTE_JSON                        \
   "{"                                                                  \
       "\"state\":{"                                                    \
           "\"desired\": {"                                             \
               "\"%s\" :{"                                              \
                    "\"%s\":\"%s\","                                    \
                "}"                                                     \
            "}"                                                         \
        "},"                                                            \
        "\"clientToken\":\"%06lu\""                                     \
    "}"                                                                 \

#define DESIRED_ADD_DEVICE_STRING_ATTRIBUTE_SIZE     (sizeof(DESIRED_ADD_DEVICE_STRING_ATTRIBUTE_JSON))
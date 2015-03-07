//
//  main.cpp
//  RTMPInvokeReader
//
//  Created by Kevin Lu on 2/22/15.
//  Copyright (c) 2015 icyblaze. All rights reserved.
//

#include <iostream>

#include <librtmp/rtmp.h>
#include <librtmp/rtmp_sys.h>
#include <librtmp/log.h>

#include <string>
#include <vector>

using namespace std;

class AMFPropertyObject;

typedef vector<AMFPropertyObject*> AMFPropertyObjectList;
typedef AMFPropertyObjectList::iterator AMFPropertyObjectIter;

void freeAMFPropertyObjectList(AMFPropertyObjectList *objectList);
void freeAMFPropertyObject(AMFPropertyObject *obj);
string& AMFDataTypeToString(AMFDataType dataType, string &typeStr);

class AMFPropertyObject {
public:
    AMFDataType dataType;
    string *name;
    void *value;
    AMFPropertyObject(string aName, AMFDataType aType)
    {
        this->name = new string(aName);
        this->dataType = aType;
        this->value = NULL;
    }
    
    ~AMFPropertyObject()
    {
        freeAMFPropertyObject(this);
        this->value = NULL;
        
        delete this->name;
        this->name = NULL;
    }
    
    friend std::ostream& operator<<(ostream& cout, const AMFPropertyObject* propertyObject)
    {
        if (propertyObject->value == NULL){
            cout << "Value is Null" << endl;
            return cout;
        }
        if (propertyObject->dataType == AMF_OBJECT || propertyObject->dataType == AMF_ECMA_ARRAY || propertyObject->dataType == AMF_STRICT_ARRAY){
            AMFPropertyObjectList *objectList = static_cast<AMFPropertyObjectList*>(propertyObject->value);
            for (AMFPropertyObjectIter iter = objectList->begin(); iter != objectList->end(); iter++){
                AMFPropertyObject *pObject = *iter;
                cout << pObject;
            }
            cout << endl;
        }else {
            if (propertyObject->dataType == AMF_NUMBER || propertyObject->dataType == AMF_BOOLEAN || propertyObject->dataType == AMF_DATE){
                double aValue = *(static_cast<double*>(propertyObject->value));
                cout << "Key: " << propertyObject->name->c_str() << " Value: " << aValue << "[NUMBER]" << endl;
            }else{
                string aValue = *(static_cast<string*>(propertyObject->value));
                cout << "Key: " << propertyObject->name->c_str() << " Value: " << aValue <<  "[STRING]" << endl;
            }
        }
        return cout;
    }
};

class AMFInvokeObject {
public:
    string *command;
    double id;
    AMFPropertyObjectList *params;
    AMFInvokeObject(char* commandValue, double idValue)
    {
        this->command = new string(commandValue);
        this->id = idValue;
        this->params = NULL;
    }
    
    ~AMFInvokeObject()
    {
        delete this->command;
        this->command = NULL;
        if (this->params){
            freeAMFPropertyObjectList(this->params);
            delete this->params;
        }
        this->params = NULL;
    }
    
    friend std::ostream& operator<<(ostream& cout, const AMFInvokeObject* invokeObject)
    {
        cout << "InvokeName: " << *(invokeObject->command);
        if (invokeObject->params && invokeObject->params->size() > 0){
            cout << " params count: " << invokeObject->params->size() << endl;
            int paramCount = 1;
            for (AMFPropertyObjectIter iter = invokeObject->params->begin(); iter != invokeObject->params->end(); iter++){
                AMFPropertyObject *pObject = *iter;
                string dataType = AMFDataTypeToString(pObject->dataType, dataType);
                cout << "params[" << paramCount << "]" << "[" << dataType << "]" << endl;
                cout << pObject;
                paramCount++;
            }
        }
        return cout;
    }
};

AMFPropertyObjectList* AMF_DumpObject(AMFObject &object);

AMFPropertyObject* AMFPropToAMFPropObject(AMFObjectProperty *prop)
{
    if (prop->p_type == AMF_INVALID)
        return NULL;

    if (prop->p_type == AMF_NULL)
        return NULL;

    AVal name;
    if (prop->p_name.av_len)
        name = prop->p_name;
    else{
        name.av_val = (char*)"--PARAMS--";
        name.av_len = sizeof("--PARAMS--");
    }
    
    string key(name.av_val, name.av_len);
    AMFPropertyObject *dataObject = new AMFPropertyObject(key, prop->p_type);
    
    if (prop->p_type == AMF_OBJECT){
        AMFPropertyObjectList *obj = AMF_DumpObject(prop->p_vu.p_object);
        dataObject->value = obj;
    }else if (prop->p_type == AMF_ECMA_ARRAY){
        AMFPropertyObjectList *obj = AMF_DumpObject(prop->p_vu.p_object);
        dataObject->value = obj;
    }else if (prop->p_type == AMF_STRICT_ARRAY){
        AMFPropertyObjectList *obj = AMF_DumpObject(prop->p_vu.p_object);
        dataObject->value = obj;
    }else {
        switch (prop->p_type){
            case AMF_NUMBER:
                dataObject->value = new double(prop->p_vu.p_number);
                break;
            case AMF_BOOLEAN:
                dataObject->value = new bool(prop->p_vu.p_number != 0.0);
                break;
            case AMF_STRING:{
                string *str = NULL;
                if (prop->p_vu.p_aval.av_len > 0)
                    str = new string(prop->p_vu.p_aval.av_val, prop->p_vu.p_aval.av_len);
                else
                    str = new string("");
                dataObject->value = str;
            }
                break;
            case AMF_DATE:
                dataObject->value = new double(prop->p_vu.p_number);
                break;
            default:
                dataObject->value = NULL;
        }
    }
    return dataObject;
}

AMFPropertyObjectList* AMF_DumpObject(AMFObject &object)
{
    AMFPropertyObjectList *objectList = new AMFPropertyObjectList();
    for (int n = 0; n < object.o_num; n++){
        AMFPropertyObject *tmpObject = AMFPropToAMFPropObject(&object.o_props[n]);
        if (tmpObject)
            objectList->push_back(tmpObject);
    }
    return objectList;
}

AMFInvokeObject* AMF_DumpInvokeObject(RTMP *r, const char *body, unsigned int nBodySize)
{
    AMFInvokeObject *invokeObject = NULL;
    AMFObject obj;
    int nRes = -1;
    if (body[0] != 0x02){
        RTMP_LogPrintf("%s, no string method in invoke packet\n", __FUNCTION__);
        return NULL;
    }
    
    nRes = AMF_Decode(&obj, body, nBodySize, FALSE);
    if (nRes < 0){
        RTMP_LogPrintf("%s, error decoding invoke packet\n", __FUNCTION__);
        return NULL;
    }
    
    if (obj.o_num >= 2){
        AVal method;
        double txn;
        AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &method);
        txn = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));
        invokeObject = new AMFInvokeObject(method.av_val, txn);
        for (int n = 2; n < obj.o_num; n++){
            if (!invokeObject->params)
                invokeObject->params = new AMFPropertyObjectList();
            AMFPropertyObject *tmpObject = AMFPropToAMFPropObject(&obj.o_props[n]);
            if (tmpObject)
                invokeObject->params->push_back(tmpObject);
        }
    }
    
    AMF_Reset(&obj);
    return invokeObject;
}

void freeAMFPropertyObject(AMFPropertyObject *obj)
{
    if (obj->value == NULL)
        return;
    if (obj->dataType == AMF_OBJECT || obj->dataType == AMF_ECMA_ARRAY || obj->dataType == AMF_STRICT_ARRAY){
        AMFPropertyObjectList *objectList = static_cast<AMFPropertyObjectList*>(obj->value);
        freeAMFPropertyObjectList(objectList);
        delete objectList;
    }else {
        if (obj->dataType == AMF_NUMBER || obj->dataType == AMF_BOOLEAN || obj->dataType == AMF_DATE){
            double *aValue = static_cast<double*>(obj->value);
            delete aValue;
        }else{
            string *aValue = static_cast<string*>(obj->value);
            delete aValue;
        }
    }
}

void freeAMFPropertyObjectList(AMFPropertyObjectList *objectList)
{
    if (objectList->size() == 0)
        return;
    for (AMFPropertyObjectIter iter = objectList->begin(); iter != objectList->end(); iter++){
        AMFPropertyObject *pObject = *iter;
        delete pObject;
    }
}

string& AMFDataTypeToString(AMFDataType dataType, string &typeStr)
{
    if (dataType == AMF_OBJECT)
        typeStr.append("AMF_OBJECT");
    else if (dataType == AMF_ECMA_ARRAY)
        typeStr.append("AMF_ECMA_ARRAY");
    else if (dataType == AMF_STRICT_ARRAY)
        typeStr.append("AMF_STRICT_ARRAY");
    else if (dataType == AMF_NUMBER)
        typeStr.append("AMF_NUMBER");
    else if (dataType == AMF_BOOLEAN)
        typeStr.append("AMF_BOOLEAN");
    else if (dataType == AMF_STRING)
        typeStr.append("AMF_STRING");
    else if (dataType == AMF_DATE)
        typeStr.append("AMF_DATE");
    else
        typeStr.append("UNKNOWN");
    return typeStr;
}

int main(int argc, const char * argv[])
{
    if (argc < 2){
        cout << "RTMP Invoke Reader" << endl;
        cout << "usage:" << endl;
        cout << "RMPTRead rtmp://127.0.0.1/service.web" << endl;
        return 1;
    }
    
    RTMP *rtmp = RTMP_Alloc();
    RTMP_Init(rtmp);
    rtmp->Link.timeout = 10;
    
    char *rtmpAddress = (char*)argv[1];
    
    if (RTMP_SetupURL(rtmp, rtmpAddress) == 0){
        RTMP_Log(RTMP_LOGERROR,"SetupURL Error\n");
        RTMP_Free(rtmp);
        return -1;
    }
    rtmp->Link.lFlags |= RTMP_LF_LIVE;

    RTMP_EnableWrite(rtmp);
    
    if(!RTMP_Connect(rtmp,NULL)){
        RTMP_Log(RTMP_LOGERROR,"Connect Error\n");
        RTMP_Free(rtmp);
        return -1;
    }
    
    RTMPPacket packet = { 0 };
    while (RTMP_IsConnected(rtmp) && RTMP_ReadPacket(rtmp, &packet)) {
        
        if (!RTMPPacket_IsReady(&packet))
            continue;
        
        if (packet.m_nChannel == 3){
            RTMP_ClientPacket(rtmp, &packet);
            AMFInvokeObject *invokeObject = AMF_DumpInvokeObject(rtmp, packet.m_body, packet.m_nBodySize);
            if (invokeObject){
                //doSomething...
                cout << invokeObject << endl;
                delete invokeObject;
                invokeObject = NULL;
            }
        }
        RTMPPacket_Free(&packet);
    }
    
    if(rtmp){
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        rtmp = NULL;
    }
    return 0;
}

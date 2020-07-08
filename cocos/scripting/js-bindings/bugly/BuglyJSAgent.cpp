//
//  BuglyJSAgent.cpp
//  Bugly
//
//  Created by Yeelik on 16/4/25.
//
//

#include "BuglyJSAgent.h"

#include "cocos2d.h"

#include "cocos/scripting/js-bindings/jswrapper/SeApi.h"

#include <string.h>

#include "CrashReport.h"

SE_DECLARE_FUNC(bugly_setUserId);

SE_DECLARE_FUNC(bugly_setTag);

SE_DECLARE_FUNC(bugly_addUserValue);
SE_DECLARE_FUNC(bugly_printLog);

#ifndef CATEGORY_JS_EXCEPTION
#define CATEGORY_JS_EXCEPTION 5
#endif
void reportJSError(const char *, const char *, const char *);

void registerJSExceptionHandler()
{
    CCLOG("-> %s", __PRETTY_FUNCTION__);

    se::ScriptEngine::getInstance()->setExceptionCallback(reportJSError);
}

void reportJSError(const char *location, const char *message, const char *stack)
{
    CCLOG("-> %s", __PRETTY_FUNCTION__);
    CrashReport::reportException(CATEGORY_JS_EXCEPTION, location, message, stack);
}

bool bugly_setUserId(se::State &state)
{
    CCLOG("-> %s", __PRETTY_FUNCTION__);
    const auto &args = state.args();
    auto argc = args.size();
    if (argc > 0)
    {
        CrashReport::setUserId(args[0].toString().c_str());
    }

    return true;
}
SE_BIND_FUNC(bugly_setUserId)

bool bugly_setTag(se::State &state)
{
    CCLOG("-> %s", __PRETTY_FUNCTION__);
    const auto &args = state.args();
    auto argc = args.size();
    if (argc > 0)
    {
        CrashReport::setTag(args[0].toInt32());
    }
    return true;
}
SE_BIND_FUNC(bugly_setTag)

bool bugly_addUserValue(se::State &state)
{
    CCLOG("-> %s", __PRETTY_FUNCTION__);
    const auto &args = state.args();
    auto argc = args.size();
    if (argc > 1)
    {
        CrashReport::addUserValue(args[0].toString().c_str(), args[1].toString().c_str());
    }

    return true;
}

SE_BIND_FUNC(bugly_addUserValue)

bool bugly_printLog(se::State &state)
{
    CCLOG("-> %s", __PRETTY_FUNCTION__);
    const auto &args = state.args();
    auto argc = args.size();
    if (argc > 2)
    {
        int level = args[0].toInt32();

        const char *tag = args[1].toString().c_str();
        const char *msg = args[2].toString().c_str();

        CrashReport::CRLogLevel pLevel = CrashReport::CRLogLevel::Off;
        switch (level)
        {
        case -1:
            pLevel = CrashReport::CRLogLevel::Off;
            break;
        case 0:
            pLevel = CrashReport::CRLogLevel::Verbose;
            break;
        case 1:
            pLevel = CrashReport::CRLogLevel::Debug;
            break;
        case 2:
            pLevel = CrashReport::CRLogLevel::Info;
            break;
        case 3:
            pLevel = CrashReport::CRLogLevel::Warning;
            break;
        case 4:
            pLevel = CrashReport::CRLogLevel::Error;
            break;

        default:
            break;
        }
        CrashReport::log(pLevel, tag, msg);
    }

    return true;
}

SE_BIND_FUNC(bugly_printLog)

bool bugly_setVersion(se::State &state)
{
    CCLOG("-> %s", __PRETTY_FUNCTION__);
    const auto &args = state.args();
    auto argc = args.size();
    if (argc > 0)
    {
        CrashReport::setAppVersion(args[0].toString().c_str());
    }
    return true;
}

SE_BIND_FUNC(bugly_setVersion)

bool bugly_setChannel(se::State &state)
{
    CCLOG("-> %s", __PRETTY_FUNCTION__);
    const auto &args = state.args();
    auto argc = args.size();
    if (argc > 0)
    {
        CrashReport::setAppChannel(args[0].toString().c_str());
    }
    return true;
}
SE_BIND_FUNC(bugly_setChannel)

bool bugly_init(se::State &state)
{
    CCLOG("-> %s", __PRETTY_FUNCTION__);
    const auto &args = state.args();
    auto argc = args.size();
    if (argc > 0)
    {
        if (argc == 1)
        {
            CrashReport::initCrashReport(args[0].toString().c_str());
        }
        else if (argc == 2)
        {
            CrashReport::initCrashReport(args[0].toString().c_str(), args[1].toBoolean());
        }
        else if (argc == 3)
        {
            auto level = args[2].toInt32();
            CrashReport::CRLogLevel pLevel = CrashReport::CRLogLevel::Off;
            switch (level)
            {
            case -1:
                pLevel = CrashReport::CRLogLevel::Off;
                break;
            case 0:
                pLevel = CrashReport::CRLogLevel::Verbose;
                break;
            case 1:
                pLevel = CrashReport::CRLogLevel::Debug;
                break;
            case 2:
                pLevel = CrashReport::CRLogLevel::Info;
                break;
            case 3:
                pLevel = CrashReport::CRLogLevel::Warning;
                break;
            case 4:
                pLevel = CrashReport::CRLogLevel::Error;
                break;

            default:
                break;
            }
            CrashReport::initCrashReport(args[0].toString().c_str(), args[1].toBoolean(), pLevel);
        }
    }
    return true;
}

SE_BIND_FUNC(bugly_init)

bool js_register_bugly(se::Object *obj)
{
    obj->defineFunction("setUserId", _SE(bugly_setUserId));
    obj->defineFunction("setTag", _SE(bugly_setTag));
    obj->defineFunction("addUserValue", _SE(bugly_addUserValue));
    obj->defineFunction("log", _SE(bugly_printLog));
    obj->defineFunction("setChannel", _SE(bugly_setChannel));
    obj->defineFunction("setVersion", _SE(bugly_setVersion));
    obj->defineFunction("init", _SE(bugly_init));
    return true;
}

bool register_bugly(se::Object *obj)
{
    registerJSExceptionHandler();
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    CrashReport::initCrashReport("b18a16e9a1", false, CrashReport::CRLogLevel::Warning);
#endif
#if (CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID)
    CrashReport::initCrashReport("deb287c34b", false, CrashReport::CRLogLevel::Warning);
#endif
    // Get the ns
    se::Value nsVal;
    if (!obj->getProperty("bugly", &nsVal))
    {
        se::HandleObject jsobj(se::Object::createPlainObject());
        nsVal.setObject(jsobj);
        obj->setProperty("bugly", nsVal);
    }
    se::Object *ns = nsVal.toObject();

    js_register_bugly(ns);
    return true;
}

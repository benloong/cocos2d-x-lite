/****************************************************************************
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

 http://www.cocos.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated engine source code (the "Software"), a limited,
 worldwide, royalty-free, non-assignable, revocable and non-exclusive license
 to use Cocos Creator solely to develop games on your target platforms. You shall
 not use Cocos Creator software for developing other software or tools that's
 used for developing games. You are not granted to publish, distribute,
 sublicense, and/or sell copies of Cocos Creator.

 The software or tools in this License Agreement are licensed, not sold.
 Xiamen Yaji Software Co., Ltd. reserves all rights not expressly granted to you.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "jsb_cocos2dx_manual.hpp"

#include "cocos/scripting/js-bindings/jswrapper/SeApi.h"
#include "cocos/scripting/js-bindings/manual/jsb_conversions.hpp"
#include "cocos/scripting/js-bindings/manual/jsb_global.h"
#include "cocos/scripting/js-bindings/auto/jsb_cocos2dx_auto.hpp"

#include "storage/local-storage/LocalStorage.h"
#include "cocos2d.h"

using namespace cocos2d;

static bool jsb_cocos2dx_empty_func(se::State& s)
{
    return true;
}
SE_BIND_FUNC(jsb_cocos2dx_empty_func)

class __JSPlistDelegator: public cocos2d::SAXDelegator
{
public:
    static __JSPlistDelegator* getInstance() {
        static __JSPlistDelegator* pInstance = NULL;
        if (pInstance == NULL) {
            pInstance = new (std::nothrow) __JSPlistDelegator();
        }
        return pInstance;
    };

    virtual ~__JSPlistDelegator();

    cocos2d::SAXParser* getParser();

    std::string parse(const std::string& path);
    std::string parseText(const std::string& text);

    // implement pure virtual methods of SAXDelegator
    void startElement(void *ctx, const char *name, const char **atts) override;
    void endElement(void *ctx, const char *name) override;
    void textHandler(void *ctx, const char *ch, int len) override;

private:
    cocos2d::SAXParser _parser;
    std::string _result;
    bool _isStoringCharacters;
    std::string _currentValue;
};

// cc.PlistParser.getInstance()
static bool js_PlistParser_getInstance(se::State& s)
{
    __JSPlistDelegator* delegator = __JSPlistDelegator::getInstance();
    SAXParser* parser = delegator->getParser();

    if (parser) {
        native_ptr_to_rooted_seval<SAXParser>(parser, __jsb_cocos2d_SAXParser_class, &s.rval());
        return true;
    }
    return false;
}
SE_BIND_FUNC(js_PlistParser_getInstance)

// cc.PlistParser.getInstance().parse(text)
static bool js_PlistParser_parse(se::State& s)
{
    const auto& args = s.args();
    size_t argc = args.size();
    __JSPlistDelegator* delegator = __JSPlistDelegator::getInstance();

    bool ok = true;
    if (argc == 1) {
        std::string arg0;
        ok &= seval_to_std_string(args[0], &arg0);
        SE_PRECONDITION2(ok, false, "Error processing arguments");

        std::string parsedStr = delegator->parseText(arg0);
        std::replace(parsedStr.begin(), parsedStr.end(), '\n', ' ');

        se::Value strVal;
        std_string_to_seval(parsedStr, &strVal);

        se::HandleObject robj(se::Object::createJSONObject(strVal.toString()));
        s.rval().setObject(robj);
        return true;
    }
    SE_REPORT_ERROR("js_PlistParser_parse : wrong number of arguments: %d, was expecting %d", (int)argc, 1);
    return false;
}
SE_BIND_FUNC(js_PlistParser_parse)

cocos2d::SAXParser* __JSPlistDelegator::getParser() {
    return &_parser;
}

std::string __JSPlistDelegator::parse(const std::string& path) {
    _result.clear();

    SAXParser parser;
    if (false != parser.init("UTF-8") )
    {
        parser.setDelegator(this);
        parser.parse(FileUtils::getInstance()->fullPathForFilename(path));
    }

    return _result;
}

__JSPlistDelegator::~__JSPlistDelegator(){
    CCLOGINFO("deallocing __JSSAXDelegator: %p", this);
}

std::string __JSPlistDelegator::parseText(const std::string& text){
    _result.clear();

    SAXParser parser;
    if (false != parser.init("UTF-8") )
    {
        parser.setDelegator(this);
        parser.parse(text.c_str(), text.size());
    }

    return _result;
}

void __JSPlistDelegator::startElement(void *ctx, const char *name, const char **atts) {
    _isStoringCharacters = true;
    _currentValue.clear();

    std::string elementName = (char*)name;

    int end = (int)_result.size() - 1;
    if(end >= 0 && _result[end] != '{' && _result[end] != '[' && _result[end] != ':') {
        _result += ",";
    }

    if (elementName == "dict") {
        _result += "{";
    }
    else if (elementName == "array") {
        _result += "[";
    }
}

void __JSPlistDelegator::endElement(void *ctx, const char *name) {
    _isStoringCharacters = false;
    std::string elementName = (char*)name;

    if (elementName == "dict") {
        _result += "}";
    }
    else if (elementName == "array") {
        _result += "]";
    }
    else if (elementName == "key") {
        _result += "\"" + _currentValue + "\":";
    }
    else if (elementName == "string") {
        _result += "\"" + _currentValue + "\"";
    }
    else if (elementName == "false" || elementName == "true") {
        _result += elementName;
    }
    else if (elementName == "real" || elementName == "integer") {
        _result += _currentValue;
    }
}

void __JSPlistDelegator::textHandler(void*, const char *ch, int len) {
    std::string text((char*)ch, 0, len);

    if (_isStoringCharacters)
    {
        _currentValue += text;
    }
}

static bool register_plist_parser(se::Object* obj)
{
    se::Value v;
    __jsbObj->getProperty("PlistParser", &v);
    assert(v.isObject());
    v.toObject()->defineFunction("getInstance", _SE(js_PlistParser_getInstance));

    __jsb_cocos2d_SAXParser_proto->defineFunction("parse", _SE(js_PlistParser_parse));

    se::ScriptEngine::getInstance()->clearException();

    return true;
}

// cc.sys.localStorage

static bool JSB_localStorageGetItem(se::State& s)
{
    const auto& args = s.args();
    size_t argc = args.size();
    if (argc == 1)
    {
        std::string ret_val;
        bool ok = true;
        std::string key;
        ok = seval_to_std_string(args[0], &key);
        SE_PRECONDITION2(ok, false, "Error processing arguments");
        std::string value;
        ok = localStorageGetItem(key, &value);
        if (ok)
            s.rval().setString(value);
        else
            s.rval().setNull(); // Should return null to make JSB behavior same as Browser since returning undefined will make JSON.parse(undefined) trigger exception.

        return true;
    }

    SE_REPORT_ERROR("Invalid number of arguments");
    return false;
}
SE_BIND_FUNC(JSB_localStorageGetItem)

static bool JSB_localStorageRemoveItem(se::State& s)
{
    const auto& args = s.args();
    size_t argc = args.size();
    if (argc == 1)
    {
        bool ok = true;
        std::string key;
        ok = seval_to_std_string(args[0], &key);
        SE_PRECONDITION2(ok, false, "Error processing arguments");
        localStorageRemoveItem(key);
        return true;
    }

    SE_REPORT_ERROR("Invalid number of arguments");
    return false;
}
SE_BIND_FUNC(JSB_localStorageRemoveItem)

static bool JSB_localStorageSetItem(se::State& s)
{
    const auto& args = s.args();
    size_t argc = args.size();
    if (argc == 2)
    {
        bool ok = true;
        std::string key;
        ok = seval_to_std_string(args[0], &key);
        SE_PRECONDITION2(ok, false, "Error processing arguments");

        std::string value;
        ok = seval_to_std_string(args[1], &value);
        SE_PRECONDITION2(ok, false, "Error processing arguments");
        localStorageSetItem(key, value);
        return true;
    }

    SE_REPORT_ERROR("Invalid number of arguments");
    return false;
}
SE_BIND_FUNC(JSB_localStorageSetItem)

static bool JSB_localStorageClear(se::State& s)
{
    const auto& args = s.args();
    size_t argc = args.size();
    if (argc == 0)
    {
        localStorageClear();
        return true;
    }

    SE_REPORT_ERROR("Invalid number of arguments");
    return false;
}
SE_BIND_FUNC(JSB_localStorageClear)

static bool JSB_localStorageKey(se::State& s) {
    const auto &args = s.args();
    size_t argc = args.size();
    if (argc == 1) {
        bool ok = true;
        int nIndex = 0;
        ok = seval_to_int32(args[0], &nIndex);
        SE_PRECONDITION2(ok, false, "Error processing arguments");
        std::string value;
        localStorageGetKey(nIndex, &value);
        s.rval().setString(value);
        return true;
    }

    SE_REPORT_ERROR("Invalid number of arguments");
    return false;
}
SE_BIND_FUNC(JSB_localStorageKey)

static bool JSB_localStorage_getLength(se::State& s)
{
    const auto& args = s.args();
    size_t argc = args.size();
    if (argc == 0) {
        int nLength = 0;

        localStorageGetLength(nLength);
        s.rval().setInt32(nLength);
        return true;
    }

    SE_REPORT_ERROR("Invalid number of arguments");
    return false;
}
SE_BIND_PROP_GET(JSB_localStorage_getLength);

static bool register_sys_localStorage(se::Object* obj)
{
    se::Value sys;
    if (!obj->getProperty("sys", &sys))
    {
        se::HandleObject sysObj(se::Object::createPlainObject());
        obj->setProperty("sys", se::Value(sysObj));
        sys.setObject(sysObj);
    }

    se::HandleObject localStorageObj(se::Object::createPlainObject());
    sys.toObject()->setProperty("localStorage", se::Value(localStorageObj));

    localStorageObj->defineFunction("getItem", _SE(JSB_localStorageGetItem));
    localStorageObj->defineFunction("removeItem", _SE(JSB_localStorageRemoveItem));
    localStorageObj->defineFunction("setItem", _SE(JSB_localStorageSetItem));
    localStorageObj->defineFunction("clear", _SE(JSB_localStorageClear));
    localStorageObj->defineFunction("key", _SE(JSB_localStorageKey));
    localStorageObj->defineProperty("length", _SE(JSB_localStorage_getLength), nullptr);

    std::string strFilePath = cocos2d::FileUtils::getInstance()->getWritablePath();
    strFilePath += "/jsb.sqlite";
    localStorageInit(strFilePath);

    se::ScriptEngine::getInstance()->addBeforeCleanupHook([](){
        localStorageFree();
    });

    se::ScriptEngine::getInstance()->clearException();

    return true;
}

#define BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(cls, property, type, convertFunc, returnFunc) \
static bool js_##cls_set_##property(se::State& s) \
{ \
    cocos2d::cls* cobj = (cocos2d::cls*)s.nativeThisObject(); \
    SE_PRECONDITION2(cobj, false, "js_#cls_set_#property : Invalid Native Object"); \
    const auto& args = s.args(); \
    size_t argc = args.size(); \
    bool ok = true; \
    if (argc == 1) { \
        type arg0; \
        ok &= convertFunc(args[0], &arg0); \
        SE_PRECONDITION2(ok, false, "js_#cls_set_#property : Error processing arguments"); \
        cobj->set_##property(arg0); \
        return true; \
    } \
    SE_REPORT_ERROR("wrong number of arguments: %d, was expecting %d", (int)argc, 1); \
    return false; \
} \
SE_BIND_PROP_SET(js_##cls_set_##property) \
\
static bool js_##cls_get_##property(se::State& s) \
{ \
    cocos2d::cls* cobj = (cocos2d::cls*)s.nativeThisObject(); \
    SE_PRECONDITION2(cobj, false, "js_#cls_get_#property : Invalid Native Object"); \
    s.rval().returnFunc(cobj->_##property); \
    return true; \
} \
SE_BIND_PROP_GET(js_##cls_get_##property)

BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, _width, float, seval_to_float, setFloat)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, _height, float, seval_to_float, setFloat)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, lineWidthInternal, float, seval_to_float, setFloat)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, lineDashOffsetInternal, float, seval_to_float, setFloat)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, miterLimitInternal, float, seval_to_float, setFloat)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, lineJoin, std::string, seval_to_std_string, setString)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, lineCap, std::string, seval_to_std_string, setString)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, font, std::string, seval_to_std_string, setString)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, textAlign, std::string, seval_to_std_string, setString)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, textBaseline, std::string, seval_to_std_string, setString)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, globalCompositeOperation, std::string, seval_to_std_string, setString)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, globalAlphaInternal, float, seval_to_float, setFloat)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, shadowColor, std::string, seval_to_std_string, setString)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, shadowBlurInternal, float, seval_to_float, setFloat)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, shadowOffsetXInternal, float, seval_to_float, setFloat)
BIND_PROP_WITH_TYPE__CONV_FUNC__RETURN(CanvasRenderingContext2D, shadowOffsetYInternal, float, seval_to_float, setFloat)


#define _SE_DEFINE_PROP(cls, property) \
    __jsb_cocos2d_##cls##_proto->defineProperty(#property, _SE(js_##cls_get_##property), _SE(js_##cls_set_##property));

static bool js_CanvasRenderingContext2D_getData(se::State& s)
{
    cocos2d::CanvasRenderingContext2D *cobj = (cocos2d::CanvasRenderingContext2D *) s.nativeThisObject();
    SE_PRECONDITION2(cobj, false, "js_CanvasRenderingContext2D_getData : Invalid Native Object");
    std::function<void(void *buffer, int len, bool needPremultiply)> arg0;
    se::Value *pval(&s.rval());
    auto lambda = [pval](void *buffer, int len, bool needPremultiply) -> void {
        se::Object *typeArray = se::Object::createTypedArray(se::Object::TypedArrayType::UINT8, buffer, len);
        if (!needPremultiply) {
            std::size_t bufferSize;
            uint8_t *data = nullptr;
            typeArray->getTypedArrayData(&data, &bufferSize);

            int alpha = 0;
            for (uint8_t *end = data + len; data < end; data = data + 4) {
                alpha = data[3];
                if (alpha > 0 && alpha < 255) {
                    data[0] = data[0] * 255 / alpha;
                    data[1] = data[1] * 255 / alpha;
                    data[2] = data[2] * 255 / alpha;
                }
            }
        }
        se::HandleObject handleObject(typeArray);
        pval->setObject(handleObject);
    };
    arg0 = lambda;
    cobj->_getData(arg0);
    return true;
}

SE_BIND_FUNC(js_CanvasRenderingContext2D_getData)

static bool js_CanvasRenderingContext2D_getFillStyle(se::State &s) {
    cocos2d::CanvasRenderingContext2D *cobj = (cocos2d::CanvasRenderingContext2D *) s.nativeThisObject();
    SE_PRECONDITION2(cobj, false, "js_CanvasRenderingContext2D_getFillStyle : Invalid Native Object");
    if (cobj->_gradientFillStyle != nullptr) {
        bool ok = true;
        ok &= native_ptr_to_seval<CanvasGradient>(cobj->_gradientFillStyle, &s.rval());
        SE_PRECONDITION2(ok, false, "js_CanvasRenderingContext2D_getFillStyle : Error processing arguments");
    } else if (cobj->_patternFillStyle) {
        bool ok = true;
        ok &= native_ptr_to_seval<CanvasPattern>(cobj->_patternFillStyle, &s.rval());
        SE_PRECONDITION2(ok, false, "js_CanvasRenderingContext2D_getFillStyle : Error processing arguments");
    } else {
        s.rval().setString(cobj->_fillStyle);
    }

    return true;
}

SE_BIND_PROP_GET(js_CanvasRenderingContext2D_getFillStyle)

static bool js_CanvasRenderingContext2D_setFillStyle(se::State &s) {
    cocos2d::CanvasRenderingContext2D *cobj = (cocos2d::CanvasRenderingContext2D *) s.nativeThisObject();
    SE_PRECONDITION2(cobj, false, "js_CanvasRenderingContext2D_setFillStyle : Invalid Native Object");
    const auto &args = s.args();
    size_t argc = args.size();
    CC_UNUSED bool ok = true;
    if (argc == 1) {
        if (args[0].isString()) {
            std::string value;
            seval_to_std_string(args[0], &value);
            cobj->set_fillStyle(value);
        } else if (args[0].isObject()) {
            se::Object *dataObj = args[0].toObject();
            std::string clsName = "CanvasGradient";
            if (clsName.compare(dataObj->_getClass()->getName()) == 0) {
                CanvasGradient *gradient = nullptr;
                ok = seval_to_native_ptr(args[0], &gradient);
                SE_PRECONDITION2(ok, false, "js_CanvasRenderingContext2D_setFillStyle : Error processing arguments");
                cobj->set_fillStyle(gradient);
            } else {
                CanvasPattern *pattern = nullptr;
                ok = seval_to_native_ptr(args[0], &pattern);
                SE_PRECONDITION2(ok, false, "js_CanvasRenderingContext2D_setFillStyle : Error processing arguments");
                cobj->set_fillStyle(pattern);
            }
        }
        return true;
    }
    SE_REPORT_ERROR("wrong number of arguments: %d, was expecting %d", (int) argc, 1);
    return false;
}

SE_BIND_PROP_SET(js_CanvasRenderingContext2D_setFillStyle)


static bool js_CanvasRenderingContext2D_getStrokeStyle(se::State &s) {
    cocos2d::CanvasRenderingContext2D *cobj = (cocos2d::CanvasRenderingContext2D *) s.nativeThisObject();
    SE_PRECONDITION2(cobj, false, "js_CanvasRenderingContext2D_getStrokeStyle : Invalid Native Object");
    if (cobj->_gradientStrokeStyle != nullptr) {
        bool ok = true;
        ok &= native_ptr_to_seval<CanvasGradient>(cobj->_gradientStrokeStyle, &s.rval());
        SE_PRECONDITION2(ok, false, "js_CanvasRenderingContext2D_getStrokeStyle : Error processing arguments");
    } else if (cobj->_patternStrokeStyle) {
        bool ok = true;
        ok &= native_ptr_to_seval<CanvasPattern>(cobj->_patternStrokeStyle, &s.rval());
        SE_PRECONDITION2(ok, false, "js_CanvasRenderingContext2D_getStrokeStyle : Error processing arguments");
    } else {
        s.rval().setString(cobj->_strokeStyle);
    }

    return true;
}

SE_BIND_PROP_GET(js_CanvasRenderingContext2D_getStrokeStyle)

static bool js_CanvasRenderingContext2D_setStrokeStyle(se::State &s) {
    cocos2d::CanvasRenderingContext2D *cobj = (cocos2d::CanvasRenderingContext2D *) s.nativeThisObject();
    SE_PRECONDITION2(cobj, false,
                     "js_CanvasRenderingContext2D_setStrokeStyle : Invalid Native Object");
    const auto &args = s.args();
    size_t argc = args.size();
    CC_UNUSED bool ok = true;
    if (argc == 1) {
        if (args[0].isString()) {
            std::string value;
            seval_to_std_string(args[0], &value);
            cobj->set_strokeStyle(value);
        } else if (args[0].isObject()) {
            se::Object *dataObj = args[0].toObject();
            std::string clsName = "CanvasGradient";
            if (clsName.compare(dataObj->_getClass()->getName()) == 0) {
                CanvasGradient *gradient = nullptr;
                ok = seval_to_native_ptr(args[0], &gradient);
                SE_PRECONDITION2(ok, false,
                                 "js_CanvasRenderingContext2D_setStrokeStyle : Error processing arguments");
                cobj->set_strokeStyle(gradient);
            } else {
                CanvasPattern *pattern = nullptr;
                ok = seval_to_native_ptr(args[0], &pattern);
                SE_PRECONDITION2(ok, false,
                                 "js_CanvasRenderingContext2D_setStrokeStyle : Error processing arguments");
                cobj->set_strokeStyle(pattern);
            }
        }
        return true;
    }
    SE_REPORT_ERROR("wrong number of arguments: %d, was expecting %d", (int) argc, 1);
    return false;
}

SE_BIND_PROP_SET(js_CanvasRenderingContext2D_setStrokeStyle)


static se::Object* __deviceMotionObject = nullptr;
static bool JSB_getDeviceMotionValue(se::State& s)
{
    if (__deviceMotionObject == nullptr)
    {
        __deviceMotionObject = se::Object::createArrayObject(9);
        __deviceMotionObject->root();
    }

    const auto& v = Device::getDeviceMotionValue();

    __deviceMotionObject->setArrayElement(0, se::Value(v.accelerationX));
    __deviceMotionObject->setArrayElement(1, se::Value(v.accelerationY));
    __deviceMotionObject->setArrayElement(2, se::Value(v.accelerationZ));
    __deviceMotionObject->setArrayElement(3, se::Value(v.accelerationIncludingGravityX));
    __deviceMotionObject->setArrayElement(4, se::Value(v.accelerationIncludingGravityY));
    __deviceMotionObject->setArrayElement(5, se::Value(v.accelerationIncludingGravityZ));
    __deviceMotionObject->setArrayElement(6, se::Value(v.rotationRateAlpha));
    __deviceMotionObject->setArrayElement(7, se::Value(v.rotationRateBeta));
    __deviceMotionObject->setArrayElement(8, se::Value(v.rotationRateGamma));

    s.rval().setObject(__deviceMotionObject);
    return true;
}
SE_BIND_FUNC(JSB_getDeviceMotionValue)

static bool register_device(se::Object* obj)
{
    se::Value device;
    __jsbObj->getProperty("Device", &device);

    device.toObject()->defineFunction("getDeviceMotionValue", _SE(JSB_getDeviceMotionValue));

    se::ScriptEngine::getInstance()->addBeforeCleanupHook([](){
        if (__deviceMotionObject != nullptr)
        {
            __deviceMotionObject->unroot();
            __deviceMotionObject->decRef();
            __deviceMotionObject = nullptr;
        }
    });

    se::ScriptEngine::getInstance()->clearException();
    return true;
}

static bool register_canvas_context2d(se::Object* obj)
{
    _SE_DEFINE_PROP(CanvasRenderingContext2D, _width)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, _height)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, lineWidthInternal)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, lineJoin)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, lineCap)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, font)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, textAlign)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, textBaseline)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, globalCompositeOperation)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, globalAlphaInternal)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, lineDashOffsetInternal)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, miterLimitInternal)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, shadowColor)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, shadowBlurInternal)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, shadowOffsetXInternal)
    _SE_DEFINE_PROP(CanvasRenderingContext2D, shadowOffsetYInternal)

    __jsb_cocos2d_CanvasRenderingContext2D_proto->defineFunction("_getData", _SE(js_CanvasRenderingContext2D_getData));
    __jsb_cocos2d_CanvasRenderingContext2D_proto->defineProperty("fillStyle", _SE(js_CanvasRenderingContext2D_getFillStyle), _SE(js_CanvasRenderingContext2D_setFillStyle));
    __jsb_cocos2d_CanvasRenderingContext2D_proto->defineProperty("strokeStyle", _SE(js_CanvasRenderingContext2D_getStrokeStyle), _SE(js_CanvasRenderingContext2D_setStrokeStyle));

    se::ScriptEngine::getInstance()->clearException();

    return true;
}

bool register_all_cocos2dx_manual(se::Object* obj)
{
    register_plist_parser(obj);
    register_sys_localStorage(obj);
    register_device(obj);
    register_canvas_context2d(obj);
    return true;
}


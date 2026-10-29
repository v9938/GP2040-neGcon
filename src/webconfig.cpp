#include "config.pb.h"
#include "base64.h"

#include "drivermanager.h"
#include "storagemanager.h"
#include "psx_input.h"
#include "analog_preview.h"
#include <algorithm>
#include <cmath>
#include "eventmanager.h"

#include "peripheralmanager.h"

#include "system.h"
#include "config_utils.h"
#include "types.h"
#include "version.h"

#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <set>

#include <pico/types.h>
#include <pico/time.h>

// HTTPD Includes
#include <ArduinoJson.h>
#include "rndis.h"
#include "fs.h"
#include "fscustom.h"
#include "fsdata.h"
#include "lwip/apps/httpd.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "addons/input_macro.h"

#define PATH_CGI_ACTION "/cgi/action"

#define LWIP_HTTPD_POST_MAX_PAYLOAD_LEN (1024 * 16)

extern struct fsdata_file file__index_html[];

const static char* spaPaths[] = { "/backup", "/pin-mapping", "/settings", "/reset-settings", "/add-ons", "/macro", "/analog-monitor" };
const static char* excludePaths[] = { "/css", "/images", "/js", "/static" };
const static uint32_t rebootDelayMs = 500;
static string http_post_uri;
static char http_post_payload[LWIP_HTTPD_POST_MAX_PAYLOAD_LEN];
static uint16_t http_post_payload_len = 0;

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K>
static void __attribute__((noinline)) readDoc(T& var, const DynamicJsonDocument& doc, const K& key)
{
    var = doc[key];
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1>
static void __attribute__((noinline)) readDoc(T& var, const DynamicJsonDocument& doc, const K0& key0, const K1& key1)
{
    var = doc[key0][key1];
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1, typename K2>
static void __attribute__((noinline)) readDoc(T& var, const DynamicJsonDocument& doc, const K0& key0, const K1& key1, const K2& key2)
{
    var = doc[key0][key1][key2];
}

// Don't inline this function, we do not want to consume stack space in the calling function
static bool __attribute__((noinline)) hasValue(const DynamicJsonDocument& doc, const char* key0, const char* key1)
{
    return doc[key0][key1] != nullptr;
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T>
static void __attribute__((noinline)) docToValue(T& value, const DynamicJsonDocument& doc, const char* key)
{
    if (doc[key] != nullptr)
    {
        value = doc[key];
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T>
static void __attribute__((noinline)) docToValue(T& value, const DynamicJsonDocument& doc, const char* key0, const char* key1)
{
    if (doc[key0][key1] != nullptr)
    {
        value = doc[key0][key1];
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T>
static void __attribute__((noinline)) docToValue(T& value, const DynamicJsonDocument& doc, const char* key0, const char* key1, const char* key2)
{
    if (doc[key0][key1][key2] != nullptr)
    {
        value = doc[key0][key1][key2];
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
static void __attribute__((noinline)) cleanAddonGpioMappings(Pin_t& addonPin, Pin_t oldAddonPin)
{
    // 物理 addon ピンの予約で PSX 仮想 slot を上書きしない。
    addonPin = isValidPin(addonPin) && !Storage::isPsxReservedPhysicalPin(addonPin) ? addonPin : -1;
}

// Don't inline this function, we do not want to consume stack space in the calling function
static void __attribute__((noinline)) docToPin(Pin_t& pin, const DynamicJsonDocument& doc, const char* key)
{
    Pin_t oldPin = pin;
    if (doc.containsKey(key))
    {
        pin = doc[key];
        cleanAddonGpioMappings(pin, oldPin);
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
static void __attribute__((noinline)) docToPin(Pin_t& pin, const DynamicJsonDocument& doc, const char* key0, const char* key1)
{
    Pin_t oldPin = pin;
    if (doc.containsKey(key0) && doc[key0].containsKey(key1))
    {
        pin = doc[key0][key1];
        cleanAddonGpioMappings(pin, oldPin);
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
static void __attribute__((noinline)) docToPin(Pin_t& pin, const DynamicJsonDocument& doc, const char* key0, const char* key1, const char* key2)
{
    Pin_t oldPin = pin;
    if (doc.containsKey(key0) && doc[key0].containsKey(key1) && doc[key0][key1].containsKey(key2))
    {
        pin = doc[key0][key1][key2];
        cleanAddonGpioMappings(pin, oldPin);
    }
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K& key, const T& var)
{
    doc[key] = var;
}

// Don't inline this function, we do not want to consume stack space in the calling function
// Web-config frontend compatibility workaround
template <typename K>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K& key, const bool& var)
{
    doc[key] = var ? 1 : 0;
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K0& key0, const K1& key1, const T& var)
{
    doc[key0][key1] = var;
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1, typename K2>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K0& key0, const K1& key1, const K2& key2, const T& var)
{
    doc[key0][key1][key2] = var;
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1, typename K2, typename K3>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K0& key0, const K1& key1, const K2& key2, const K3& key3, const T& var)
{
    doc[key0][key1][key2][key3] = var;
}

// Don't inline this function, we do not want to consume stack space in the calling function
template <typename T, typename K0, typename K1, typename K2, typename K3, typename K4>
static void __attribute__((noinline)) writeDoc(DynamicJsonDocument& doc, const K0& key0, const K1& key1, const K2& key2, const K3& key3, const K4& key4, const T& var)
{
    doc[key0][key1][key2][key3][key4] = var;
}

static int32_t cleanPin(int32_t pin) { return isValidPin(pin) ? pin : -1; }

enum class HttpStatusCode
{
    _200,
    _400,
    _405,
    _409,
    _500,
};

struct DataAndStatusCode
{
    DataAndStatusCode(string&& data, HttpStatusCode statusCode) :
        data(std::move(data)),
        statusCode(statusCode)
    {}

    string data;
    HttpStatusCode statusCode;
};

static void* jogResetPostConnection = nullptr;
static int jogResetPostLength = -1;
static bool jogResetResponsePending = false;
static std::string jogResetResponse;
static HttpStatusCode jogResetResponseStatus = HttpStatusCode::_405;
static DataAndStatusCode resetJogconDial();

// **** WEB SERVER Overrides and Special Functionality ****
int set_file_data(fs_file* file, const DataAndStatusCode& dataAndStatusCode)
{
    string returnData;

    const char* statusCodeStr = "";
    switch (dataAndStatusCode.statusCode)
    {
        case HttpStatusCode::_200: statusCodeStr = "200 OK"; break;
        case HttpStatusCode::_400: statusCodeStr = "400 Bad Request"; break;
        case HttpStatusCode::_405: statusCodeStr = "405 Method Not Allowed"; break;
        case HttpStatusCode::_409: statusCodeStr = "409 Conflict"; break;
        case HttpStatusCode::_500: statusCodeStr = "500 Internal Server Error"; break;
    }

    returnData.clear();
    returnData.append("HTTP/1.1 ");
    returnData.append(statusCodeStr);
    returnData.append("\r\n");
    returnData.append(
        "Server: GP2040-CE " GP2040VERSION "\r\n"
        "Content-Type: application/json\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: "
    );
    returnData.append(std::to_string(dataAndStatusCode.data.length()));
    returnData.append("\r\n\r\n");
    returnData.append(dataAndStatusCode.data);

    // 複数窓からの取得中に共有stringを上書きすると、転送中の応答が破損する。
    // lwIP専用ヒープは小さく、Profile等のJSON全体を保持できない。
    // 旧std::stringと同じC++ヒープから、応答単位のバッファを確保する。
    char* buffer = new (std::nothrow) char[returnData.size()];
    if (!buffer) return 0;
    memcpy(buffer, returnData.data(), returnData.size());
    file->data = static_cast<const char*>(buffer);
    file->len = returnData.size();
    file->index = file->len;
    // 応答文字列にHTTPヘッダーを含めているので、lwIPへ明示する。
    file->http_header_included = FS_FILE_FLAGS_HEADER_INCLUDED |
        FS_FILE_FLAGS_HEADER_PERSISTENT | FS_FILE_FLAGS_HEADER_HTTPVER_1_1;
    file->pextension = buffer;
    file->is_custom_file = 1;

    return 1;
}

int set_file_data(fs_file *file, string&& data)
{
    if (data.empty())
        return 0;
    return set_file_data(file, DataAndStatusCode(std::move(data), HttpStatusCode::_200));
}

DynamicJsonDocument get_post_data()
{
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    deserializeJson(doc, http_post_payload, http_post_payload_len);
    return doc;
}

void save_hotkey(HotkeyEntry* hotkey, const DynamicJsonDocument& doc, const string hotkey_key)
{
    readDoc(hotkey->auxMask, doc, hotkey_key, "auxMask");
    uint32_t buttonsMask = doc[hotkey_key]["buttonsMask"];
    uint32_t dpadMask = 0;
    if (buttonsMask & GAMEPAD_MASK_DU) {
        dpadMask |= GAMEPAD_MASK_UP;
    }
    if (buttonsMask & GAMEPAD_MASK_DD) {
        dpadMask |= GAMEPAD_MASK_DOWN;
    }
    if (buttonsMask & GAMEPAD_MASK_DL) {
        dpadMask |= GAMEPAD_MASK_LEFT;
    }
    if (buttonsMask & GAMEPAD_MASK_DR) {
        dpadMask |= GAMEPAD_MASK_RIGHT;
    }
    buttonsMask &= ~(GAMEPAD_MASK_DU | GAMEPAD_MASK_DD | GAMEPAD_MASK_DL | GAMEPAD_MASK_DR);
    hotkey->dpadMask = dpadMask;
    hotkey->buttonsMask = buttonsMask;
    readDoc(hotkey->action, doc, hotkey_key, "action");
}

void load_hotkey(const HotkeyEntry* hotkey, DynamicJsonDocument& doc, const string hotkey_key)
{
    writeDoc(doc, hotkey_key, "auxMask", hotkey->auxMask);
    uint32_t buttonsMask = hotkey->buttonsMask;
    if (hotkey->dpadMask & GAMEPAD_MASK_UP) {
        buttonsMask |= GAMEPAD_MASK_DU;
    }
    if (hotkey->dpadMask & GAMEPAD_MASK_DOWN) {
        buttonsMask |= GAMEPAD_MASK_DD;
    }
    if (hotkey->dpadMask & GAMEPAD_MASK_LEFT) {
        buttonsMask |= GAMEPAD_MASK_DL;
    }
    if (hotkey->dpadMask & GAMEPAD_MASK_RIGHT) {
        buttonsMask |= GAMEPAD_MASK_DR;
    }
    writeDoc(doc, hotkey_key, "buttonsMask", buttonsMask);
    writeDoc(doc, hotkey_key, "action", hotkey->action);
}

// LWIP callback on HTTP POST to validate the URI
err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                       uint16_t http_request_len, int content_len, char *response_uri,
                       uint16_t response_uri_len, uint8_t *post_auto_wnd)
{
    LWIP_UNUSED_ARG(http_request);
    LWIP_UNUSED_ARG(http_request_len);
    LWIP_UNUSED_ARG(content_len);
    LWIP_UNUSED_ARG(response_uri);
    LWIP_UNUSED_ARG(response_uri_len);
    LWIP_UNUSED_ARG(post_auto_wnd);

    if (!uri || strncmp(uri, "/api", 4) != 0) {
        return ERR_ARG;
    }

    if (jogResetPostConnection) return ERR_INPROGRESS;
    jogResetResponsePending = false;
    if (strcmp(uri, "/api/resetJogconDial") == 0) {
        if (content_len < 0 || content_len > LWIP_HTTPD_POST_MAX_PAYLOAD_LEN) return ERR_ARG;
        jogResetPostConnection = connection;
        jogResetPostLength = content_len;
    }

    http_post_uri = uri;
    http_post_payload_len = 0;
    memset(http_post_payload, 0, LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    return ERR_OK;
}

// LWIP callback on HTTP POST to for receiving payload
err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    if (jogResetPostConnection && connection != jogResetPostConnection) {
        pbuf_free(p);
        return ERR_ARG;
    }

    // Cache the received data to http_post_payload
    while (p != NULL)
    {
        if (http_post_payload_len + p->len <= LWIP_HTTPD_POST_MAX_PAYLOAD_LEN)
        {
            MEMCPY(http_post_payload + http_post_payload_len, p->payload, p->len);
            http_post_payload_len += p->len;
        }
        else // Buffer overflow
        {
            http_post_payload_len = 0xffff;
            break;
        }

        p = p->next;
    }

    // Need to release memory here or will leak
    pbuf_free(p);

    // If the buffer overflows, error out
    if (http_post_payload_len == 0xffff) {
        return ERR_BUF;
    }

    return ERR_OK;
}

// LWIP callback to set the HTTP POST response_uri, which can then be looked up via the fs_custom callbacks
void httpd_post_finished(void *connection, char *response_uri, uint16_t response_uri_len)
{
    if (jogResetPostConnection && connection != jogResetPostConnection) {
        if (response_uri_len) response_uri[0] = '\0';
        return;
    }
    if (connection == jogResetPostConnection && jogResetPostConnection) {
        // Execute only here, never in fs_open_custom (which also serves GET).
        auto result = http_post_payload_len == jogResetPostLength
            ? resetJogconDial()
            : DataAndStatusCode("{\"error\":\"invalid_jogcon_reset_body\"}", HttpStatusCode::_400);
        jogResetResponse = std::move(result.data);
        jogResetResponseStatus = result.statusCode;
        jogResetResponsePending = true;
        jogResetPostConnection = nullptr;
        jogResetPostLength = -1;
    }

    if (response_uri_len && http_post_payload_len != 0xffff) {
        strncpy(response_uri, http_post_uri.c_str(), response_uri_len);
        response_uri[response_uri_len - 1] = '\0';
    }
}

void addUsedPinsArray(DynamicJsonDocument& doc)
{
    auto usedPins = doc.createNestedArray("usedPins");
    // 物理配線の予約のみ。PSX 仮想 slot のボタン割当とは独立する。
    for (int pin = 0; pin < NUM_BANK0_GPIOS; ++pin)
        if (Storage::isPsxReservedPhysicalPin(pin)) usedPins.add(pin);
}

std::string serialize_json(DynamicJsonDocument &doc)
{
    std::string data;
    serializeJson(doc, data);
    return data;
}

std::string getUsedPins()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    addUsedPinsArray(doc);
    return serialize_json(doc);
}

// 省略された項目は保持し、型と範囲を確認してから候補だけを更新する。
static bool readPsxMappings(JsonObjectConst object, GpioMappings& mappings)
{
    if (object.isNull()) return false;
    char pinName[6];
    for (unsigned slot = 0; slot < NUM_BANK0_GPIOS; ++slot) {
        snprintf(pinName, sizeof(pinName), "pin%02u", slot);
        if (!object.containsKey(pinName)) continue;
        if (!object[pinName].is<JsonObjectConst>()) return false;
        JsonObjectConst pin = object[pinName];
        auto& mapping = mappings.pins[slot];
        if (pin.containsKey("action")) {
            if (!pin["action"].is<int32_t>()) return false;
            const int32_t action = pin["action"].as<int32_t>();
            if (!Storage::isPsxMappingAction(action)) return false;
            if ((slot < 8 || slot > 23) && action != NONE) return false;
            mapping.action = static_cast<GpioAction>(action);
        }
        for (const char* key : {"customButtonMask", "customDpadMask"}) {
            if (!pin.containsKey(key)) continue;
            if (!pin[key].is<uint32_t>()) return false;
            const uint32_t mask = pin[key].as<uint32_t>();
            if (strcmp(key, "customDpadMask") == 0) {
                if (mask > 15) return false;
                mapping.customDpadMask = mask;
            } else mapping.customButtonMask = mask;
        }
    }
    if (object.containsKey("profileLabel")) {
        if (!object["profileLabel"].is<const char*>()) return false;
        const char* label = object["profileLabel"].as<const char*>();
        if (strlen(label) >= sizeof(mappings.profileLabel)) return false;
        strcpy(mappings.profileLabel, label);
    }
    if (object.containsKey("enabled")) {
        // Webの既存チェックボックスは0/1も送信する。
        if (!object["enabled"].is<bool>() &&
            !(object["enabled"].is<uint32_t>() && object["enabled"].as<uint32_t>() <= 1)) return false;
        mappings.enabled = object["enabled"].as<bool>();
    }
    mappings.pins_count = NUM_BANK0_GPIOS;
    return true;
}

std::string setProfileOptions()
{
    DynamicJsonDocument doc = get_post_data();

    // 最大5プロファイルは4KBスタックに収まらない。候補はヒープに置いて検証する。
    auto candidateStorage = std::make_unique<ProfileOptions>(Storage::getInstance().getProfileOptions());
    ProfileOptions& candidate = *candidateStorage;
    if (!doc["alternativePinMappings"].is<JsonArrayConst>())
        return "{\"error\":\"invalid PSX profiles\"}";
    JsonArrayConst alts = doc["alternativePinMappings"];
    // 配列添字を使う前に上限を検証する。空配列はprofile削除として反映する。
    if (alts.size() > sizeof(candidate.gpioMappingsSets) / sizeof(candidate.gpioMappingsSets[0]))
        return "{\"error\":\"too many PSX profiles\"}";
    for (size_t index = 0; index < alts.size(); ++index) {
        if (!alts[index].is<JsonObjectConst>()) return "{\"error\":\"invalid PSX profiles\"}";
        if (index >= candidate.gpioMappingsSets_count)
            candidate.gpioMappingsSets[index] = Storage::getInstance().getGpioMappings();
        if (!readPsxMappings(alts[index].as<JsonObjectConst>(), candidate.gpioMappingsSets[index]))
            return "{\"error\":\"invalid PSX profiles\"}";
    }
    candidate.gpioMappingsSets_count = alts.size();
    Storage::getInstance().getProfileOptions() = candidate;
    Storage::enforcePsxConfig(Storage::getInstance().getConfig());
    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));
    return serialize_json(doc);
}

std::string getProfileOptions()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);

    const auto writePinDoc = [&](const int item, const char* key, const GpioMappingInfo& value) -> void
    {
        writeDoc(doc, "alternativePinMappings", item, key, "action", value.action);
        writeDoc(doc, "alternativePinMappings", item, key, "customButtonMask", value.customButtonMask);
        writeDoc(doc, "alternativePinMappings", item, key, "customDpadMask", value.customDpadMask);
    };

    ProfileOptions& profileOptions = Storage::getInstance().getProfileOptions();

    // return an empty list if no profiles are currently set, since we no longer populate by default
    if (profileOptions.gpioMappingsSets_count == 0) {
        doc.createNestedArray("alternativePinMappings");
    }

    for (int i = 0; i < profileOptions.gpioMappingsSets_count; i++) {
        // this looks duplicative, but something in arduinojson treats the doc
        // field string by reference so you can't be "clever" and do an snprintf
        // thing or else you only send the last field in the JSON
        writePinDoc(i, "pin00", profileOptions.gpioMappingsSets[i].pins[0]);
        writePinDoc(i, "pin01", profileOptions.gpioMappingsSets[i].pins[1]);
        writePinDoc(i, "pin02", profileOptions.gpioMappingsSets[i].pins[2]);
        writePinDoc(i, "pin03", profileOptions.gpioMappingsSets[i].pins[3]);
        writePinDoc(i, "pin04", profileOptions.gpioMappingsSets[i].pins[4]);
        writePinDoc(i, "pin05", profileOptions.gpioMappingsSets[i].pins[5]);
        writePinDoc(i, "pin06", profileOptions.gpioMappingsSets[i].pins[6]);
        writePinDoc(i, "pin07", profileOptions.gpioMappingsSets[i].pins[7]);
        writePinDoc(i, "pin08", profileOptions.gpioMappingsSets[i].pins[8]);
        writePinDoc(i, "pin09", profileOptions.gpioMappingsSets[i].pins[9]);
        writePinDoc(i, "pin10", profileOptions.gpioMappingsSets[i].pins[10]);
        writePinDoc(i, "pin11", profileOptions.gpioMappingsSets[i].pins[11]);
        writePinDoc(i, "pin12", profileOptions.gpioMappingsSets[i].pins[12]);
        writePinDoc(i, "pin13", profileOptions.gpioMappingsSets[i].pins[13]);
        writePinDoc(i, "pin14", profileOptions.gpioMappingsSets[i].pins[14]);
        writePinDoc(i, "pin15", profileOptions.gpioMappingsSets[i].pins[15]);
        writePinDoc(i, "pin16", profileOptions.gpioMappingsSets[i].pins[16]);
        writePinDoc(i, "pin17", profileOptions.gpioMappingsSets[i].pins[17]);
        writePinDoc(i, "pin18", profileOptions.gpioMappingsSets[i].pins[18]);
        writePinDoc(i, "pin19", profileOptions.gpioMappingsSets[i].pins[19]);
        writePinDoc(i, "pin20", profileOptions.gpioMappingsSets[i].pins[20]);
        writePinDoc(i, "pin21", profileOptions.gpioMappingsSets[i].pins[21]);
        writePinDoc(i, "pin22", profileOptions.gpioMappingsSets[i].pins[22]);
        writePinDoc(i, "pin23", profileOptions.gpioMappingsSets[i].pins[23]);
        writePinDoc(i, "pin24", profileOptions.gpioMappingsSets[i].pins[24]);
        writePinDoc(i, "pin25", profileOptions.gpioMappingsSets[i].pins[25]);
        writePinDoc(i, "pin26", profileOptions.gpioMappingsSets[i].pins[26]);
        writePinDoc(i, "pin27", profileOptions.gpioMappingsSets[i].pins[27]);
        writePinDoc(i, "pin28", profileOptions.gpioMappingsSets[i].pins[28]);
        writePinDoc(i, "pin29", profileOptions.gpioMappingsSets[i].pins[29]);
        writeDoc(doc, "alternativePinMappings", i, "profileLabel", profileOptions.gpioMappingsSets[i].profileLabel);
        doc["alternativePinMappings"][i]["enabled"] = profileOptions.gpioMappingsSets[i].enabled;
    }

    return serialize_json(doc);
}

std::string setGamepadOptions()
{
    DynamicJsonDocument doc = get_post_data();

    GamepadOptions& gamepadOptions = Storage::getInstance().getGamepadOptions();

    readDoc(gamepadOptions.dpadMode, doc, "dpadMode");
    readDoc(gamepadOptions.inputMode, doc, "inputMode");
    readDoc(gamepadOptions.inputDeviceType, doc, "inputDeviceType");
    readDoc(gamepadOptions.socdMode, doc, "socdMode");
    readDoc(gamepadOptions.switchTpShareForDs4, doc, "switchTpShareForDs4");
    readDoc(gamepadOptions.lockHotkeys, doc, "lockHotkeys");
    readDoc(gamepadOptions.fourWayMode, doc, "fourWayMode");
    readDoc(gamepadOptions.profileNumber, doc, "profileNumber");
    readDoc(gamepadOptions.debounceDelay, doc, "debounceDelay");
    readDoc(gamepadOptions.inputModeB1, doc, "inputModeB1");
    readDoc(gamepadOptions.inputModeB2, doc, "inputModeB2");
    readDoc(gamepadOptions.inputModeB3, doc, "inputModeB3");
    readDoc(gamepadOptions.inputModeB4, doc, "inputModeB4");
    readDoc(gamepadOptions.inputModeL1, doc, "inputModeL1");
    readDoc(gamepadOptions.inputModeL2, doc, "inputModeL2");
    readDoc(gamepadOptions.inputModeR1, doc, "inputModeR1");
    readDoc(gamepadOptions.inputModeR2, doc, "inputModeR2");
    readDoc(gamepadOptions.ps4AuthType, doc, "ps4AuthType");
    readDoc(gamepadOptions.ps5AuthType, doc, "ps5AuthType");
    readDoc(gamepadOptions.xinputAuthType, doc, "xinputAuthType");
    readDoc(gamepadOptions.ps4ControllerIDMode, doc, "ps4ControllerIDMode");
    readDoc(gamepadOptions.usbDescOverride, doc, "usbDescOverride");
    readDoc(gamepadOptions.miniMenuGamepadInput, doc, "miniMenuGamepadInput");
    // Copy USB descriptor strings
    size_t strSize = sizeof(gamepadOptions.usbDescManufacturer);
    strncpy(gamepadOptions.usbDescManufacturer, doc["usbDescManufacturer"], strSize - 1);
    gamepadOptions.usbDescManufacturer[strSize - 1] = '\0';
    strSize = sizeof(gamepadOptions.usbDescProduct);
    strncpy(gamepadOptions.usbDescProduct, doc["usbDescProduct"], strSize - 1);
    gamepadOptions.usbDescProduct[strSize - 1] = '\0';
    strSize = sizeof(gamepadOptions.usbDescVersion);
    strncpy(gamepadOptions.usbDescVersion, doc["usbDescVersion"], strSize - 1);
    gamepadOptions.usbDescVersion[strSize - 1] = '\0';
    readDoc(gamepadOptions.usbOverrideID, doc, "usbOverrideID");
    readDoc(gamepadOptions.usbVendorID, doc, "usbVendorID");
    readDoc(gamepadOptions.usbProductID, doc, "usbProductID");

    HotkeyOptions& hotkeyOptions = Storage::getInstance().getHotkeyOptions();
    save_hotkey(&hotkeyOptions.hotkey01, doc, "hotkey01");
    save_hotkey(&hotkeyOptions.hotkey02, doc, "hotkey02");
    save_hotkey(&hotkeyOptions.hotkey03, doc, "hotkey03");
    save_hotkey(&hotkeyOptions.hotkey04, doc, "hotkey04");
    save_hotkey(&hotkeyOptions.hotkey05, doc, "hotkey05");
    save_hotkey(&hotkeyOptions.hotkey06, doc, "hotkey06");
    save_hotkey(&hotkeyOptions.hotkey07, doc, "hotkey07");
    save_hotkey(&hotkeyOptions.hotkey08, doc, "hotkey08");
    save_hotkey(&hotkeyOptions.hotkey09, doc, "hotkey09");
    save_hotkey(&hotkeyOptions.hotkey10, doc, "hotkey10");
    save_hotkey(&hotkeyOptions.hotkey11, doc, "hotkey11");
    save_hotkey(&hotkeyOptions.hotkey12, doc, "hotkey12");
    save_hotkey(&hotkeyOptions.hotkey13, doc, "hotkey13");
    save_hotkey(&hotkeyOptions.hotkey14, doc, "hotkey14");
    save_hotkey(&hotkeyOptions.hotkey15, doc, "hotkey15");
    save_hotkey(&hotkeyOptions.hotkey16, doc, "hotkey16");

    ForcedSetupOptions& forcedSetupOptions = Storage::getInstance().getForcedSetupOptions();
    readDoc(forcedSetupOptions.mode, doc, "forcedSetupMode");

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string getGamepadOptions()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);

    GamepadOptions& gamepadOptions = Storage::getInstance().getGamepadOptions();
    writeDoc(doc, "dpadMode", gamepadOptions.dpadMode);
    writeDoc(doc, "inputMode", gamepadOptions.inputMode);
    writeDoc(doc, "inputDeviceType", gamepadOptions.inputDeviceType);
    writeDoc(doc, "socdMode", gamepadOptions.socdMode);
    writeDoc(doc, "switchTpShareForDs4", gamepadOptions.switchTpShareForDs4 ? 1 : 0);
    writeDoc(doc, "lockHotkeys", gamepadOptions.lockHotkeys ? 1 : 0);
    writeDoc(doc, "fourWayMode", gamepadOptions.fourWayMode ? 1 : 0);
    writeDoc(doc, "profileNumber", gamepadOptions.profileNumber);
    writeDoc(doc, "debounceDelay", gamepadOptions.debounceDelay);
    writeDoc(doc, "inputModeB1", gamepadOptions.inputModeB1);
    writeDoc(doc, "inputModeB2", gamepadOptions.inputModeB2);
    writeDoc(doc, "inputModeB3", gamepadOptions.inputModeB3);
    writeDoc(doc, "inputModeB4", gamepadOptions.inputModeB4);
    writeDoc(doc, "inputModeL1", gamepadOptions.inputModeL1);
    writeDoc(doc, "inputModeL2", gamepadOptions.inputModeL2);
    writeDoc(doc, "inputModeR1", gamepadOptions.inputModeR1);
    writeDoc(doc, "inputModeR2", gamepadOptions.inputModeR2);
    writeDoc(doc, "ps4AuthType", gamepadOptions.ps4AuthType);
    writeDoc(doc, "ps5AuthType", gamepadOptions.ps5AuthType);
    writeDoc(doc, "xinputAuthType", gamepadOptions.xinputAuthType);
    writeDoc(doc, "ps4ControllerIDMode", gamepadOptions.ps4ControllerIDMode);
    writeDoc(doc, "usbDescOverride", gamepadOptions.usbDescOverride);
    writeDoc(doc, "usbDescManufacturer", gamepadOptions.usbDescManufacturer);
    writeDoc(doc, "usbDescProduct", gamepadOptions.usbDescProduct);
    writeDoc(doc, "usbDescVersion", gamepadOptions.usbDescVersion);
    writeDoc(doc, "usbOverrideID", gamepadOptions.usbOverrideID);
    writeDoc(doc, "miniMenuGamepadInput", gamepadOptions.miniMenuGamepadInput);
    // Write USB Vendor ID and Product ID as 4 character hex strings with 0 padding
    char usbVendorStr[5];
    snprintf(usbVendorStr, 5, "%04X", gamepadOptions.usbVendorID);
    writeDoc(doc, "usbVendorID", usbVendorStr);
    char usbProductStr[5];
    snprintf(usbProductStr, 5, "%04X", gamepadOptions.usbProductID);
    writeDoc(doc, "usbProductID", usbProductStr);
    writeDoc(doc, "fnButtonPin", -1);
    GpioMappingInfo* gpioMappings = Storage::getInstance().getGpioMappings().pins;
    for (unsigned int pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
        if (gpioMappings[pin].action == GpioAction::BUTTON_PRESS_FN) {
            writeDoc(doc, "fnButtonPin", pin);
        }
    }

    HotkeyOptions& hotkeyOptions = Storage::getInstance().getHotkeyOptions();
    load_hotkey(&hotkeyOptions.hotkey01, doc, "hotkey01");
    load_hotkey(&hotkeyOptions.hotkey02, doc, "hotkey02");
    load_hotkey(&hotkeyOptions.hotkey03, doc, "hotkey03");
    load_hotkey(&hotkeyOptions.hotkey04, doc, "hotkey04");
    load_hotkey(&hotkeyOptions.hotkey05, doc, "hotkey05");
    load_hotkey(&hotkeyOptions.hotkey06, doc, "hotkey06");
    load_hotkey(&hotkeyOptions.hotkey07, doc, "hotkey07");
    load_hotkey(&hotkeyOptions.hotkey08, doc, "hotkey08");
    load_hotkey(&hotkeyOptions.hotkey09, doc, "hotkey09");
    load_hotkey(&hotkeyOptions.hotkey10, doc, "hotkey10");
    load_hotkey(&hotkeyOptions.hotkey11, doc, "hotkey11");
    load_hotkey(&hotkeyOptions.hotkey12, doc, "hotkey12");
    load_hotkey(&hotkeyOptions.hotkey13, doc, "hotkey13");
    load_hotkey(&hotkeyOptions.hotkey14, doc, "hotkey14");
    load_hotkey(&hotkeyOptions.hotkey15, doc, "hotkey15");
    load_hotkey(&hotkeyOptions.hotkey16, doc, "hotkey16");

    ForcedSetupOptions& forcedSetupOptions = Storage::getInstance().getForcedSetupOptions();
    writeDoc(doc, "forcedSetupMode", forcedSetupOptions.mode);
    return serialize_json(doc);
}

std::string setPinMappings()
{
    DynamicJsonDocument doc = get_post_data();

    GpioMappings candidate = Storage::getInstance().getGpioMappings();
    if (!readPsxMappings(doc.as<JsonObjectConst>(), candidate))
        return "{\"error\":\"invalid PSX mapping\"}";
    Storage::getInstance().getGpioMappings() = candidate;
    Storage::enforcePsxConfig(Storage::getInstance().getConfig());
    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string getPinMappings()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);

    GpioMappings& gpioMappings = Storage::getInstance().getGpioMappings();

    const auto writePinDoc = [&](const char* key, const GpioMappingInfo& value) -> void
    {
        writeDoc(doc, key, "action", value.action);
        writeDoc(doc, key, "customButtonMask", value.customButtonMask);
        writeDoc(doc, key, "customDpadMask", value.customDpadMask);
    };

    writePinDoc("pin00", gpioMappings.pins[0]);
    writePinDoc("pin01", gpioMappings.pins[1]);
    writePinDoc("pin02", gpioMappings.pins[2]);
    writePinDoc("pin03", gpioMappings.pins[3]);
    writePinDoc("pin04", gpioMappings.pins[4]);
    writePinDoc("pin05", gpioMappings.pins[5]);
    writePinDoc("pin06", gpioMappings.pins[6]);
    writePinDoc("pin07", gpioMappings.pins[7]);
    writePinDoc("pin08", gpioMappings.pins[8]);
    writePinDoc("pin09", gpioMappings.pins[9]);
    writePinDoc("pin10", gpioMappings.pins[10]);
    writePinDoc("pin11", gpioMappings.pins[11]);
    writePinDoc("pin12", gpioMappings.pins[12]);
    writePinDoc("pin13", gpioMappings.pins[13]);
    writePinDoc("pin14", gpioMappings.pins[14]);
    writePinDoc("pin15", gpioMappings.pins[15]);
    writePinDoc("pin16", gpioMappings.pins[16]);
    writePinDoc("pin17", gpioMappings.pins[17]);
    writePinDoc("pin18", gpioMappings.pins[18]);
    writePinDoc("pin19", gpioMappings.pins[19]);
    writePinDoc("pin20", gpioMappings.pins[20]);
    writePinDoc("pin21", gpioMappings.pins[21]);
    writePinDoc("pin22", gpioMappings.pins[22]);
    writePinDoc("pin23", gpioMappings.pins[23]);
    writePinDoc("pin24", gpioMappings.pins[24]);
    writePinDoc("pin25", gpioMappings.pins[25]);
    writePinDoc("pin26", gpioMappings.pins[26]);
    writePinDoc("pin27", gpioMappings.pins[27]);
    writePinDoc("pin28", gpioMappings.pins[28]);
    writePinDoc("pin29", gpioMappings.pins[29]);

    writeDoc(doc, "profileLabel", gpioMappings.profileLabel);
    doc["enabled"] = gpioMappings.enabled;

    return serialize_json(doc);
}

std::string setKeyMappings()
{
    DynamicJsonDocument doc = get_post_data();

    KeyboardMapping& keyboardMapping = Storage::getInstance().getKeyboardMapping();

    readDoc(keyboardMapping.keyDpadUp, doc, "Up");
    readDoc(keyboardMapping.keyDpadDown, doc, "Down");
    readDoc(keyboardMapping.keyDpadLeft, doc, "Left");
    readDoc(keyboardMapping.keyDpadRight, doc, "Right");
    readDoc(keyboardMapping.keyButtonB1, doc, "B1");
    readDoc(keyboardMapping.keyButtonB2, doc, "B2");
    readDoc(keyboardMapping.keyButtonB3, doc, "B3");
    readDoc(keyboardMapping.keyButtonB4, doc, "B4");
    readDoc(keyboardMapping.keyButtonL1, doc, "L1");
    readDoc(keyboardMapping.keyButtonR1, doc, "R1");
    readDoc(keyboardMapping.keyButtonL2, doc, "L2");
    readDoc(keyboardMapping.keyButtonR2, doc, "R2");
    readDoc(keyboardMapping.keyButtonS1, doc, "S1");
    readDoc(keyboardMapping.keyButtonS2, doc, "S2");
    readDoc(keyboardMapping.keyButtonL3, doc, "L3");
    readDoc(keyboardMapping.keyButtonR3, doc, "R3");
    readDoc(keyboardMapping.keyButtonA1, doc, "A1");
    readDoc(keyboardMapping.keyButtonA2, doc, "A2");
    readDoc(keyboardMapping.keyButtonA3, doc, "A3");
    readDoc(keyboardMapping.keyButtonA4, doc, "A4");
    readDoc(keyboardMapping.keyButtonE1, doc, "E1");
    readDoc(keyboardMapping.keyButtonE2, doc, "E2");
    readDoc(keyboardMapping.keyButtonE3, doc, "E3");
    readDoc(keyboardMapping.keyButtonE4, doc, "E4");
    readDoc(keyboardMapping.keyButtonE5, doc, "E5");
    readDoc(keyboardMapping.keyButtonE6, doc, "E6");
    readDoc(keyboardMapping.keyButtonE7, doc, "E7");
    readDoc(keyboardMapping.keyButtonE8, doc, "E8");
    readDoc(keyboardMapping.keyButtonE9, doc, "E9");
    readDoc(keyboardMapping.keyButtonE10, doc, "E10");
    readDoc(keyboardMapping.keyButtonE11, doc, "E11");
    readDoc(keyboardMapping.keyButtonE12, doc, "E12");

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string getKeyMappings()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    const KeyboardMapping& keyboardMapping = Storage::getInstance().getKeyboardMapping();

    writeDoc(doc, "Up", keyboardMapping.keyDpadUp);
    writeDoc(doc, "Down", keyboardMapping.keyDpadDown);
    writeDoc(doc, "Left", keyboardMapping.keyDpadLeft);
    writeDoc(doc, "Right", keyboardMapping.keyDpadRight);
    writeDoc(doc, "B1", keyboardMapping.keyButtonB1);
    writeDoc(doc, "B2", keyboardMapping.keyButtonB2);
    writeDoc(doc, "B3", keyboardMapping.keyButtonB3);
    writeDoc(doc, "B4", keyboardMapping.keyButtonB4);
    writeDoc(doc, "L1", keyboardMapping.keyButtonL1);
    writeDoc(doc, "R1", keyboardMapping.keyButtonR1);
    writeDoc(doc, "L2", keyboardMapping.keyButtonL2);
    writeDoc(doc, "R2", keyboardMapping.keyButtonR2);
    writeDoc(doc, "S1", keyboardMapping.keyButtonS1);
    writeDoc(doc, "S2", keyboardMapping.keyButtonS2);
    writeDoc(doc, "L3", keyboardMapping.keyButtonL3);
    writeDoc(doc, "R3", keyboardMapping.keyButtonR3);
    writeDoc(doc, "A1", keyboardMapping.keyButtonA1);
    writeDoc(doc, "A2", keyboardMapping.keyButtonA2);
    writeDoc(doc, "A3", keyboardMapping.keyButtonA3);
    writeDoc(doc, "A4", keyboardMapping.keyButtonA4);
    writeDoc(doc, "E1", keyboardMapping.keyButtonE1);
    writeDoc(doc, "E2", keyboardMapping.keyButtonE2);
    writeDoc(doc, "E3", keyboardMapping.keyButtonE3);
    writeDoc(doc, "E4", keyboardMapping.keyButtonE4);
    writeDoc(doc, "E5", keyboardMapping.keyButtonE5);
    writeDoc(doc, "E6", keyboardMapping.keyButtonE6);
    writeDoc(doc, "E7", keyboardMapping.keyButtonE7);
    writeDoc(doc, "E8", keyboardMapping.keyButtonE8);
    writeDoc(doc, "E9", keyboardMapping.keyButtonE9);
    writeDoc(doc, "E10", keyboardMapping.keyButtonE10);
    writeDoc(doc, "E11", keyboardMapping.keyButtonE11);
    writeDoc(doc, "E12", keyboardMapping.keyButtonE12);

    return serialize_json(doc);
}

std::string getPeripheralOptions()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    const PeripheralOptions& peripheralOptions = Storage::getInstance().getPeripheralOptions();

    writeDoc(doc, "peripheral", "usb0", "enabled", peripheralOptions.blockUSB0.enabled);
    writeDoc(doc, "peripheral", "usb0", "dp",      peripheralOptions.blockUSB0.dp);
    writeDoc(doc, "peripheral", "usb0", "enable5v",peripheralOptions.blockUSB0.enable5v);
    writeDoc(doc, "peripheral", "usb0", "order",   peripheralOptions.blockUSB0.order);

    return serialize_json(doc);
}

std::string getReactiveLEDs()
{
    const size_t capacity = JSON_OBJECT_SIZE(100);
    DynamicJsonDocument doc(capacity);
    ReactiveLEDInfo* ledInfo = Storage::getInstance().getAddonOptions().reactiveLEDOptions.leds;

    for (uint16_t led = 0; led < 3; led++) {
        writeDoc(doc, "leds", led, "action", ledInfo[led].action);
        writeDoc(doc, "leds", led, "modeDown", ledInfo[led].modeDown);
        writeDoc(doc, "leds", led, "modeUp", ledInfo[led].modeUp);
        writeDoc(doc, "leds", led, "brightness", ledInfo[led].brightness);
    }

    return serialize_json(doc);
}

std::string setReactiveLEDs()
{
    DynamicJsonDocument doc = get_post_data();
    if (!doc.is<JsonObject>() || doc.size() != 1 || !doc["leds"].is<JsonArrayConst>())
        return "{\"error\":\"invalid reactive LEDs\"}";
    ReactiveLEDOptions options = Storage::getInstance().getAddonOptions().reactiveLEDOptions;
    JsonArrayConst leds = doc["leds"];
    if (leds.size() != 3) return "{\"error\":\"exactly three reactive LEDs required\"}";
    for (size_t index = 0; index < leds.size(); ++index) {
        if (!leds[index].is<JsonObjectConst>()) return "{\"error\":\"invalid reactive LED\"}";
        JsonObjectConst led = leds[index];
        if (led.size() != 4) return "{\"error\":\"invalid reactive LED fields\"}";
        for (const char* key : {"action", "modeDown", "modeUp", "brightness"})
            if (!led[key].is<int32_t>()) return "{\"error\":\"invalid reactive LED integer\"}";
        // short enum への変換で範囲外の値が折り返される前に検証する。
        if (!Storage::isReactiveLedAction(led["action"].as<int32_t>()))
            return "{\"error\":\"invalid reactive LED action\"}";
        for (const char* key : {"modeDown", "modeUp"})
            if (led[key].as<int32_t>() < 0 || led[key].as<int32_t>() > REACTIVE_LED_ANALOG)
                return "{\"error\":\"invalid reactive LED mode\"}";
        auto& info = options.leds[index];
        info.pin = -1;
        info.action = static_cast<GpioAction>(led["action"].as<int32_t>());
        info.modeDown = static_cast<ReactiveLEDMode>(led["modeDown"].as<int32_t>());
        info.modeUp = static_cast<ReactiveLEDMode>(led["modeUp"].as<int32_t>());
        if (!led["brightness"].is<uint32_t>()) return "{\"error\":\"invalid reactive LED brightness\"}";
        info.brightness = led["brightness"].as<uint32_t>();
        info.has_pin = info.has_action = info.has_modeDown = info.has_modeUp = info.has_brightness = true;
    }
    options.leds_count = 3;
    if (!Storage::validateReactiveLedOptions(options)) return "{\"error\":\"invalid reactive LED options\"}";
    Storage::getInstance().getAddonOptions().reactiveLEDOptions = options;
    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return getReactiveLEDs();
}

static AnalogPreviewSettings monitorSavedSettings{}, monitorDraftSettings{};
static AnalogPreviewState monitorSavedState{}, monitorDraftState{};
static AnalogPreviewOutput monitorSavedOutput{}, monitorDraftOutput{};
static PsxSnapshot monitorSnapshot;
static std::string monitorSession;
static uint32_t monitorRevision = 0;
static uint16_t monitorMidpoint = GAMEPAD_JOYSTICK_MID;
static InputMode monitorInputMode = INPUT_MODE_XINPUT;
static bool monitorInitialized = false;
static uint64_t monitorSavedTickUs = 0, monitorDraftTickUs = 0;

static AnalogPreviewSettings previewSettings(const AnalogOptions& a) {
    AnalogPreviewSettings settings{};
    settings.sticks[0] = {static_cast<uint32_t>(a.has_analogAdc1Mode ? a.analogAdc1Mode : DPAD_MODE_LEFT_ANALOG),
        static_cast<uint32_t>(a.analogAdc1Invert), a.inner_deadzone, a.outer_deadzone,
        a.smoothing_factor, a.analog_error, a.analog_smoothing, a.forced_circularity, a.auto_calibrate, 0};
    settings.sticks[1] = {static_cast<uint32_t>(a.has_analogAdc2Mode ? a.analogAdc2Mode : DPAD_MODE_RIGHT_ANALOG),
        static_cast<uint32_t>(a.analogAdc2Invert), a.inner_deadzone2, a.outer_deadzone2,
        a.smoothing_factor2, a.analog_error2, a.analog_smoothing2, a.forced_circularity2, a.auto_calibrate2, 0};
    settings.pressureModes[0] = a.has_negconIMode ? a.negconIMode : 12;
    settings.pressureModes[1] = a.has_negconIIMode ? a.negconIIMode : 11;
    settings.pressureModes[2] = a.has_negconLMode ? a.negconLMode : 0;
    settings.pressureInner[0] = a.has_negconIInnerDeadzone ? a.negconIInnerDeadzone : 0;
    settings.pressureInner[1] = a.has_negconIIInnerDeadzone ? a.negconIIInnerDeadzone : 0;
    settings.pressureInner[2] = a.has_negconLInnerDeadzone ? a.negconLInnerDeadzone : 0;
    settings.pressureOuter[0] = a.has_negconIOuterDeadzone ? a.negconIOuterDeadzone : 100;
    settings.pressureOuter[1] = a.has_negconIIOuterDeadzone ? a.negconIIOuterDeadzone : 100;
    settings.pressureOuter[2] = a.has_negconLOuterDeadzone ? a.negconLOuterDeadzone : 100;
    settings.jogconDeadzone = a.has_jogconDeadzone ? a.jogconDeadzone : 0;
    settings.jogconMax = a.has_jogconMax ? a.jogconMax : 100;
    settings.jogconForcebackEnabled = a.has_jogconForcebackEnabled ? a.jogconForcebackEnabled : true;
    return settings;
}

// Config mode runs a separate fixed-1ms model, never the runtime EMA state.
// Hold the latest sample for elapsed virtual ticks; HTTP reads do not advance it.
void analogMonitorTick() {
    const uint64_t now = time_us_64();
    const auto settings = previewSettings(Storage::getInstance().getAddonOptions().analogOptions);
    const auto mode = Storage::getInstance().getGamepadOptions().inputMode;
    const uint16_t midpoint = mode == INPUT_MODE_SWITCH || mode == INPUT_MODE_KEYBOARD
        || mode == INPUT_MODE_PS4 || mode == INPUT_MODE_PS5 || mode == INPUT_MODE_GENERIC
        ? 0x8000 : GAMEPAD_JOYSTICK_MID;
    if (!monitorInitialized || std::memcmp(&settings, &monitorSavedSettings, sizeof(settings)) != 0
        || midpoint != monitorMidpoint) {
        analog_preview_reset(&monitorSavedState);
        monitorSavedOutput = {};
        monitorSavedTickUs = now;
        if (midpoint != monitorMidpoint) {
            analog_preview_reset(&monitorDraftState);
            monitorDraftOutput = {};
            monitorDraftTickUs = now;
        }
        monitorSavedSettings = settings;
        monitorInitialized = true;
    }
    monitorMidpoint = midpoint;
    monitorInputMode = mode;
    monitorSnapshot = PsxInput::getInstance().snapshot();
    const auto& s = monitorSnapshot;
    const AnalogPreviewInput input = {s.generation, s.lx, s.ly, s.rx, s.ry,
        s.pressureI, s.pressureII, s.pressureL, s.connected, s.analog, s.negcon, s.id, 0,
        s.jogPosition, s.jogcon, s.jogStatus, s.jogReady, 0};
    const auto advance = [&input, midpoint, now](const AnalogPreviewSettings& settings,
        AnalogPreviewState& state, AnalogPreviewOutput& output, uint64_t& lastTickUs) {
        const uint64_t elapsed = now - lastTickUs;
        const uint32_t ticks = static_cast<uint32_t>(std::min<uint64_t>(elapsed / 1000, 250));
        // Discard excess catch-up after long stalls, retaining the sub-ms remainder.
        lastTickUs = now - elapsed % 1000;
        for (uint32_t i = 0; i < ticks; ++i)
            analog_preview_step(&input, &settings, midpoint, 1, &state, &output);
    };
    advance(monitorSavedSettings, monitorSavedState, monitorSavedOutput, monitorSavedTickUs);
    if (!monitorSession.empty())
        advance(monitorDraftSettings, monitorDraftState, monitorDraftOutput, monitorDraftTickUs);
}

std::string setAnalogPreviewDraft() {
    const DynamicJsonDocument doc = get_post_data();
    const auto invalid = [] { return std::string("{\"error\":\"invalid analog monitor draft\"}"); };
    if (!doc.is<JsonObject>() || doc.size() != 3 || !doc["session"].is<const char*>()
        || !doc["revision"].is<uint32_t>() || !doc["options"].is<JsonObjectConst>()) return invalid();
    const std::string session = doc["session"].as<const char*>();
    if (session.empty() || session.size() > 64) return invalid();
    for (unsigned char c : session)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_')) return invalid();
    const uint32_t revision = doc["revision"];
    if (revision == 0 || (session == monitorSession && revision <= monitorRevision))
        return "{\"error\":\"stale analog monitor revision\"}";
    const JsonObjectConst options = doc["options"];
    if (options.size() != 36) return invalid();
    for (const char* key : {"analogAdc1Mode", "analogAdc2Mode"})
        if (!options[key].is<uint32_t>() || options[key].as<uint32_t>() > 2) return invalid();
    for (const char* key : {"analogAdc1Invert", "analogAdc2Invert"})
        if (!options[key].is<uint32_t>() || options[key].as<uint32_t>() > 3) return invalid();
    AnalogOptions candidate = Storage::getInstance().getAddonOptions().analogOptions;
#define MONITOR_INTEGER(field) \
    if (!options[#field].is<uint32_t>()) return invalid(); \
    candidate.field = static_cast<decltype(candidate.field)>(options[#field].as<uint32_t>());
    MONITOR_INTEGER(analogAdc1Mode)
    MONITOR_INTEGER(analogAdc2Mode)
    MONITOR_INTEGER(analogAdc1Invert)
    MONITOR_INTEGER(analogAdc2Invert)
    MONITOR_INTEGER(inner_deadzone)
    MONITOR_INTEGER(inner_deadzone2)
    MONITOR_INTEGER(outer_deadzone)
    MONITOR_INTEGER(outer_deadzone2)
    MONITOR_INTEGER(analog_error)
    MONITOR_INTEGER(analog_error2)
    MONITOR_INTEGER(negconIMode)
    MONITOR_INTEGER(negconIIMode)
    MONITOR_INTEGER(negconLMode)
    MONITOR_INTEGER(negconIThreshold)
    MONITOR_INTEGER(negconIIThreshold)
    MONITOR_INTEGER(negconLThreshold)
    MONITOR_INTEGER(negconIInnerDeadzone)
    MONITOR_INTEGER(negconIOuterDeadzone)
    MONITOR_INTEGER(negconIIInnerDeadzone)
    MONITOR_INTEGER(negconIIOuterDeadzone)
    MONITOR_INTEGER(negconLInnerDeadzone)
    MONITOR_INTEGER(negconLOuterDeadzone)
    MONITOR_INTEGER(jogconDeadzone)
    MONITOR_INTEGER(jogconMax)
#undef MONITOR_INTEGER
#define MONITOR_FLOAT(field) \
    if (!options[#field].is<float>() || !std::isfinite(options[#field].as<float>())) return invalid(); \
    candidate.field = options[#field].as<float>();
    MONITOR_FLOAT(smoothing_factor)
    MONITOR_FLOAT(smoothing_factor2)
#undef MONITOR_FLOAT
#define MONITOR_FLAG(field) \
    if (!options[#field].is<bool>() && !(options[#field].is<uint32_t>() && options[#field].as<uint32_t>() <= 1)) return invalid(); \
    candidate.field = options[#field].as<bool>();
    MONITOR_FLAG(forced_circularity)
    MONITOR_FLAG(forced_circularity2)
    MONITOR_FLAG(auto_calibrate)
    MONITOR_FLAG(auto_calibrate2)
    MONITOR_FLAG(analog_smoothing)
    MONITOR_FLAG(analog_smoothing2)
    MONITOR_FLAG(jogconForcebackEnabled)
    MONITOR_FLAG(negconIDigitalEnabled)
    MONITOR_FLAG(negconIIDigitalEnabled)
    MONITOR_FLAG(negconLDigitalEnabled)
#undef MONITOR_FLAG
    candidate.has_analogAdc1Mode = candidate.has_analogAdc2Mode = true;
    candidate.has_negconIMode = candidate.has_negconIIMode = candidate.has_negconLMode = true;
    candidate.has_negconIInnerDeadzone = candidate.has_negconIOuterDeadzone = true;
    candidate.has_negconIIInnerDeadzone = candidate.has_negconIIOuterDeadzone = true;
    candidate.has_negconLInnerDeadzone = candidate.has_negconLOuterDeadzone = true;
    candidate.has_negconIDigitalEnabled = candidate.has_negconIIDigitalEnabled = candidate.has_negconLDigitalEnabled = true;
    candidate.has_jogconDeadzone = candidate.has_jogconMax = candidate.has_jogconForcebackEnabled = true;
    if (!Storage::validatePsxAnalogOptions(candidate)) return invalid();
    const auto settings = previewSettings(candidate);
    if (session != monitorSession || std::memcmp(&settings, &monitorDraftSettings, sizeof(settings)) != 0) {
        analog_preview_reset(&monitorDraftState);
        monitorDraftOutput = {};
        monitorDraftTickUs = time_us_64();
    }
    monitorSession = session;
    monitorRevision = revision;
    monitorDraftSettings = settings;
    DynamicJsonDocument response(JSON_OBJECT_SIZE(3) + 80);
    response["success"] = true;
    response["draftSession"] = monitorSession;
    response["draftRevision"] = monitorRevision;
    return serialize_json(response);
}

std::string getAnalogMonitor() {
    DynamicJsonDocument doc(JSON_OBJECT_SIZE(30) + 5 * JSON_OBJECT_SIZE(12) + 512);
    JsonObject metadata = doc.createNestedObject("metadata");
    const auto& s = monitorSnapshot;
    metadata["version"] = 3;
    metadata["source"] = "psx";
    metadata["connected"] = s.connected;
    metadata["controllerId"] = s.id;
    metadata["analog"] = s.analog;
    metadata["negcon"] = s.negcon;
    metadata["jogcon"] = s.jogcon;
    metadata["jogReady"] = s.jogReady;
    metadata["jogStatus"] = s.jogStatus;
    metadata["jogResetState"] = s.jogResetState;
    metadata["generation"] = s.generation;
    metadata["frameSequence"] = s.frameSequence;
    metadata["timestampUs"] = s.timestampUs;
    if (s.timestampUs) metadata["ageMs"] = (time_us_64() - s.timestampUs) / 1000;
    else metadata["ageMs"] = nullptr;
    metadata["midpoint"] = monitorMidpoint;
    metadata["inputMode"] = monitorInputMode;
    metadata["jogconOutputMode"] = Storage::getInstance().getAddonOptions().analogOptions.analogAdc1Mode;
    metadata["filterUpdatePolicy"] = "fixed_1ms_preview; runtime_per_loop_legacy";
    metadata["previewTickUs"] = 1000;
    metadata["previewMaxCatchUpTicks"] = 250;
    metadata["computedKind"] = "calculated_not_usb_report";
    metadata["draftSession"] = monitorSession;
    metadata["draftRevision"] = monitorRevision;
    metadata.createNestedArray("draftErrors");
    JsonObject raw = doc.createNestedObject("raw");
    const char* rawNames[] = {"lx", "ly", "rx", "ry", "i", "ii", "l"};
    const uint8_t rawValues[] = {s.lx, s.ly, s.rx, s.ry, s.pressureI, s.pressureII, s.pressureL};
    const uint8_t rawMask = !s.connected || s.jogcon ? 0 : s.negcon ? 0x71 : s.analog ? 0x0f : 0;
    metadata["rawValidityMask"] = rawMask;
    for (unsigned i = 0; i < 7; ++i) {
        if (rawMask & (1u << i)) raw[rawNames[i]] = rawValues[i];
        else raw[rawNames[i]] = nullptr;
    }
    if (s.connected && s.jogcon) {
        raw["jogPosition"] = s.jogPosition;
        raw["jogStatus"] = s.jogStatus;
        JsonObject jogcon = doc.createNestedObject("jogcon");
        jogcon["position"] = s.jogPosition;
        jogcon["status"] = s.jogStatus;
        jogcon["motorCommand"] = s.jogMotorCommand;
        jogcon["motorPower"] = s.jogMotorPower;
        jogcon["ready"] = s.jogReady;
        jogcon["resetState"] = s.jogResetState;
        jogcon["sleepSuspected"] = s.jogSleepSuspected;
    } else {
        raw["jogPosition"] = nullptr;
        raw["jogStatus"] = nullptr;
        doc["jogcon"] = nullptr;
    }
    const auto computed = [&doc](const char* name, const AnalogPreviewOutput& output) {
        JsonObject object = doc.createNestedObject(name);
        const char* names[] = {"lx", "ly", "rx", "ry", "lt", "rt"};
        const uint16_t values[] = {output.lx, output.ly, output.rx, output.ry, output.lt, output.rt};
        for (unsigned i = 0; i < 6; ++i) {
            if (output.validityMask & (1u << i)) object[names[i]] = values[i];
            else object[names[i]] = nullptr;
        }
        object["validityMask"] = output.validityMask;
        object["reset"] = output.reset;
    };
    computed("savedComputed", monitorSavedOutput);
    if (!monitorSession.empty()) computed("draftComputed", monitorDraftOutput);
    else doc["draftComputed"] = nullptr;
    return serialize_json(doc);
}

static DataAndStatusCode resetJogconDial() {
    if (http_post_payload_len) {
        DynamicJsonDocument doc(JSON_OBJECT_SIZE(1) + 32);
        const auto error = deserializeJson(doc, http_post_payload, http_post_payload_len);
        if (error || !doc.is<JsonObject>() || doc.size() != 0)
            return DataAndStatusCode("{\"error\":\"invalid_jogcon_reset_body\"}", HttpStatusCode::_400);
    }
    auto& input = PsxInput::getInstance();
    const auto snapshot = input.snapshot();
    if (snapshot.jogSleepSuspected)
        return DataAndStatusCode("{\"error\":\"jogcon_sleep_suspected_press_physical_button\"}", HttpStatusCode::_409);
    if (!snapshot.connected || !snapshot.jogcon || !snapshot.jogReady || !input.requestJogconReset())
        return DataAndStatusCode("{\"error\":\"jogcon_not_ready\"}", HttpStatusCode::_409);
    return DataAndStatusCode("{\"success\":true,\"queued\":true}", HttpStatusCode::_200);
}

std::string setAddonOptions()
{
    DynamicJsonDocument doc = get_post_data();
    if (!doc.is<JsonObject>()) return "{\"error\":\"invalid PSX analog options\"}";
    // 変換前にJSON数値の型を確認する。小数・文字列・boolの整数化を禁止する。
    for (const char* key : {"analogAdc1Mode", "analogAdc2Mode", "analogAdc1Invert", "analogAdc2Invert",
        "inner_deadzone", "inner_deadzone2", "outer_deadzone", "outer_deadzone2",
        "analog_error", "analog_error2", "negconIMode", "negconIIMode", "negconLMode",
        "negconIThreshold", "negconIIThreshold", "negconLThreshold", "jogconDeadzone", "jogconMax",
        "negconIInnerDeadzone", "negconIOuterDeadzone", "negconIIInnerDeadzone", "negconIIOuterDeadzone",
        "negconLInnerDeadzone", "negconLOuterDeadzone"}) {
        if (doc.containsKey(key) && !doc[key].is<uint32_t>())
            return "{\"error\":\"invalid PSX analog integer\"}";
    }
    for (const char* key : {"analogAdc1Mode", "analogAdc2Mode"})
        if (doc.containsKey(key) && doc[key].as<uint32_t>() > 2)
            return "{\"error\":\"invalid PSX analog mode\"}";
    for (const char* key : {"analogAdc1Invert", "analogAdc2Invert"})
        if (doc.containsKey(key) && doc[key].as<uint32_t>() > 3)
            return "{\"error\":\"invalid PSX analog invert\"}";
    for (const char* key : {"smoothing_factor", "smoothing_factor2"}) {
        if (doc.containsKey(key) && (!doc[key].is<float>() || !std::isfinite(doc[key].as<float>())))
            return "{\"error\":\"invalid PSX analog number\"}";
    }
    for (const char* key : {"forced_circularity", "forced_circularity2", "auto_calibrate", "auto_calibrate2",
        "analog_smoothing", "analog_smoothing2", "OnBoardLedAddonEnabled", "BoardLedAddonEnabled",
        "ReactiveLEDAddonEnabled", "BootselButtonAddonEnabled", "ReverseInputEnabled",
        "InputMacroAddonEnabled", "KeyboardHostAddonEnabled", "GamepadUSBHostAddonEnabled", "jogconForcebackEnabled",
        "negconIDigitalEnabled", "negconIIDigitalEnabled", "negconLDigitalEnabled"}) {
        if (doc.containsKey(key) && !doc[key].is<bool>() &&
            !(doc[key].is<uint32_t>() && doc[key].as<uint32_t>() <= 1))
            return "{\"error\":\"invalid PSX analog flag\"}";
    }
    if (doc.containsKey("onBoardLedMode") && (!doc["onBoardLedMode"].is<uint32_t>() ||
        doc["onBoardLedMode"].as<uint32_t>() > ON_BOARD_LED_MODE_PS_AUTH))
        return "{\"error\":\"invalid onboard LED mode\"}";
    if (doc.containsKey("bootselButtonMap") && (!doc["bootselButtonMap"].is<uint32_t>() ||
        !Storage::isNormalButtonMask(doc["bootselButtonMap"].as<uint32_t>())))
        return "{\"error\":\"invalid BOOTSEL button mask\"}";
    for (const char* key : {"reverseActionUp", "reverseActionDown", "reverseActionLeft", "reverseActionRight"})
        if (doc.containsKey(key) && (!doc[key].is<uint32_t>() || doc[key].as<uint32_t>() > 2))
            return "{\"error\":\"invalid reverse action\"}";
    if (doc.containsKey("reversePinLED") &&
        (!doc["reversePinLED"].is<int32_t>() || doc["reversePinLED"].as<int32_t>() != -1))
        return "{\"error\":\"reverse LED is disabled\"}";
    // 現行 frontend の BoardLed 名も保持し、矛盾する同時指定は拒否する。
    if (doc.containsKey("OnBoardLedAddonEnabled") && doc.containsKey("BoardLedAddonEnabled") &&
        doc["OnBoardLedAddonEnabled"].as<bool>() != doc["BoardLedAddonEnabled"].as<bool>())
        return "{\"error\":\"conflicting onboard LED flags\"}";

    // 保存候補の検証が終わるまで現行設定や mapping を変更しない。
    AnalogOptions analogOptions = Storage::getInstance().getAddonOptions().analogOptions;
    docToValue(analogOptions.analogAdc1Mode, doc, "analogAdc1Mode");
    docToValue(analogOptions.analogAdc1Invert, doc, "analogAdc1Invert");
    docToValue(analogOptions.analogAdc2Mode, doc, "analogAdc2Mode");
    docToValue(analogOptions.analogAdc2Invert, doc, "analogAdc2Invert");
    docToValue(analogOptions.forced_circularity, doc, "forced_circularity");
    docToValue(analogOptions.forced_circularity2, doc, "forced_circularity2");
    docToValue(analogOptions.inner_deadzone, doc, "inner_deadzone");
    docToValue(analogOptions.inner_deadzone2, doc, "inner_deadzone2");
    docToValue(analogOptions.outer_deadzone, doc, "outer_deadzone");
    docToValue(analogOptions.outer_deadzone2, doc, "outer_deadzone2");
    docToValue(analogOptions.auto_calibrate, doc, "auto_calibrate");
    docToValue(analogOptions.auto_calibrate2, doc, "auto_calibrate2");
    docToValue(analogOptions.analog_smoothing, doc, "analog_smoothing");
    docToValue(analogOptions.analog_smoothing2, doc, "analog_smoothing2");
    docToValue(analogOptions.smoothing_factor, doc, "smoothing_factor");
    docToValue(analogOptions.smoothing_factor2, doc, "smoothing_factor2");
    docToValue(analogOptions.analog_error, doc, "analog_error");
    docToValue(analogOptions.analog_error2, doc, "analog_error2");
    docToValue(analogOptions.negconIMode, doc, "negconIMode");
    docToValue(analogOptions.negconIIMode, doc, "negconIIMode");
    docToValue(analogOptions.negconLMode, doc, "negconLMode");
    docToValue(analogOptions.negconIThreshold, doc, "negconIThreshold");
    docToValue(analogOptions.negconIIThreshold, doc, "negconIIThreshold");
    docToValue(analogOptions.negconLThreshold, doc, "negconLThreshold");
#define NEGCON_OPTION(field) \
    docToValue(analogOptions.field, doc, #field); \
    if (doc.containsKey(#field)) analogOptions.has_##field = true;
    NEGCON_OPTION(negconIInnerDeadzone)
    NEGCON_OPTION(negconIOuterDeadzone)
    NEGCON_OPTION(negconIIInnerDeadzone)
    NEGCON_OPTION(negconIIOuterDeadzone)
    NEGCON_OPTION(negconLInnerDeadzone)
    NEGCON_OPTION(negconLOuterDeadzone)
    NEGCON_OPTION(negconIDigitalEnabled)
    NEGCON_OPTION(negconIIDigitalEnabled)
    NEGCON_OPTION(negconLDigitalEnabled)
#undef NEGCON_OPTION
    if (doc.containsKey("negconIMode")) analogOptions.has_negconIMode = true;
    if (doc.containsKey("negconIIMode")) analogOptions.has_negconIIMode = true;
    if (doc.containsKey("negconLMode")) analogOptions.has_negconLMode = true;
    docToValue(analogOptions.jogconDeadzone, doc, "jogconDeadzone");
    docToValue(analogOptions.jogconMax, doc, "jogconMax");
    docToValue(analogOptions.jogconForcebackEnabled, doc, "jogconForcebackEnabled");
    if (doc.containsKey("jogconDeadzone")) analogOptions.has_jogconDeadzone = true;
    if (doc.containsKey("jogconMax")) analogOptions.has_jogconMax = true;
    if (doc.containsKey("jogconForcebackEnabled")) analogOptions.has_jogconForcebackEnabled = true;
    if (!Storage::validatePsxAnalogOptions(analogOptions))
        return "{\"error\":\"invalid PSX analog options\"}";
    Storage::getInstance().getAddonOptions().analogOptions = analogOptions;

    BootselButtonOptions& bootselButtonOptions = Storage::getInstance().getAddonOptions().bootselButtonOptions;
    docToValue(bootselButtonOptions.buttonMap, doc, "bootselButtonMap");
    docToValue(bootselButtonOptions.enabled, doc, "BootselButtonAddonEnabled");
    docToValue(Storage::getInstance().getAddonOptions().macroOptions.enabled, doc, "InputMacroAddonEnabled");

    ReverseOptions& reverseOptions = Storage::getInstance().getAddonOptions().reverseOptions;
    docToValue(reverseOptions.enabled, doc, "ReverseInputEnabled");
    docToValue(reverseOptions.actionUp, doc, "reverseActionUp");
    docToValue(reverseOptions.actionDown, doc, "reverseActionDown");
    docToValue(reverseOptions.actionLeft, doc, "reverseActionLeft");
    docToValue(reverseOptions.actionRight, doc, "reverseActionRight");

    OnBoardLedOptions& onBoardLedOptions = Storage::getInstance().getAddonOptions().onBoardLedOptions;
    docToValue(onBoardLedOptions.mode, doc, "onBoardLedMode");
    docToValue(onBoardLedOptions.enabled, doc, "BoardLedAddonEnabled");
    docToValue(onBoardLedOptions.enabled, doc, "OnBoardLedAddonEnabled");

    KeyboardHostOptions& keyboardHostOptions = Storage::getInstance().getAddonOptions().keyboardHostOptions;
    docToValue(keyboardHostOptions.enabled, doc, "KeyboardHostAddonEnabled");
    docToValue(keyboardHostOptions.mapping.keyDpadUp, doc, "keyboardHostMap", "Up");
    docToValue(keyboardHostOptions.mapping.keyDpadDown, doc, "keyboardHostMap", "Down");
    docToValue(keyboardHostOptions.mapping.keyDpadLeft, doc, "keyboardHostMap", "Left");
    docToValue(keyboardHostOptions.mapping.keyDpadRight, doc, "keyboardHostMap", "Right");
    docToValue(keyboardHostOptions.mapping.keyButtonB1, doc, "keyboardHostMap", "B1");
    docToValue(keyboardHostOptions.mapping.keyButtonB2, doc, "keyboardHostMap", "B2");
    docToValue(keyboardHostOptions.mapping.keyButtonB3, doc, "keyboardHostMap", "B3");
    docToValue(keyboardHostOptions.mapping.keyButtonB4, doc, "keyboardHostMap", "B4");
    docToValue(keyboardHostOptions.mapping.keyButtonL1, doc, "keyboardHostMap", "L1");
    docToValue(keyboardHostOptions.mapping.keyButtonR1, doc, "keyboardHostMap", "R1");
    docToValue(keyboardHostOptions.mapping.keyButtonL2, doc, "keyboardHostMap", "L2");
    docToValue(keyboardHostOptions.mapping.keyButtonR2, doc, "keyboardHostMap", "R2");
    docToValue(keyboardHostOptions.mapping.keyButtonS1, doc, "keyboardHostMap", "S1");
    docToValue(keyboardHostOptions.mapping.keyButtonS2, doc, "keyboardHostMap", "S2");
    docToValue(keyboardHostOptions.mapping.keyButtonL3, doc, "keyboardHostMap", "L3");
    docToValue(keyboardHostOptions.mapping.keyButtonR3, doc, "keyboardHostMap", "R3");
    docToValue(keyboardHostOptions.mapping.keyButtonA1, doc, "keyboardHostMap", "A1");
    docToValue(keyboardHostOptions.mapping.keyButtonA2, doc, "keyboardHostMap", "A2");
    docToValue(keyboardHostOptions.mapping.keyButtonA3, doc, "keyboardHostMap", "A3");
    docToValue(keyboardHostOptions.mapping.keyButtonA4, doc, "keyboardHostMap", "A4");
    docToValue(keyboardHostOptions.mouseLeft, doc, "keyboardHostMouseLeft");
    docToValue(keyboardHostOptions.mouseMiddle, doc, "keyboardHostMouseMiddle");
    docToValue(keyboardHostOptions.mouseRight, doc, "keyboardHostMouseRight");
    docToValue(keyboardHostOptions.mouseSensitivity, doc, "keyboardHostMouseSensitivity");
    docToValue(keyboardHostOptions.movementMode, doc, "keyboardHostMouseMovement");

    GamepadUSBHostOptions& gamepadUSBHostOptions = Storage::getInstance().getAddonOptions().gamepadUSBHostOptions;
    docToValue(gamepadUSBHostOptions.enabled, doc, "GamepadUSBHostAddonEnabled");

    ReactiveLEDOptions& reactiveLEDOptions = Storage::getInstance().getAddonOptions().reactiveLEDOptions;
    docToValue(reactiveLEDOptions.enabled, doc, "ReactiveLEDAddonEnabled");

    Storage::enforcePsxConfig(Storage::getInstance().getConfig());
    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return serialize_json(doc);
}

std::string setPS4Options()
{
    DynamicJsonDocument doc = get_post_data();
    PS4Options& ps4Options = Storage::getInstance().getAddonOptions().ps4Options;
    std::string encoded;
    std::string decoded;

    const auto readEncoded = [&](const char* key) -> bool
    {
        if (doc.containsKey(key))
        {
            const char* str = nullptr;
            readDoc(str, doc, key);
            encoded = str;
            return true;
        }
        else
        {
            return false;
        }
    };

    // RSA Context
    if ( readEncoded("N") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.rsaN.bytes)) ) {
            memcpy(ps4Options.rsaN.bytes, decoded.data(), decoded.length());
            ps4Options.rsaN.size = decoded.length();
        }
    }
    if ( readEncoded("E") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.rsaE.bytes)) ) {
            memcpy(ps4Options.rsaE.bytes, decoded.data(), decoded.length());
            ps4Options.rsaE.size = decoded.length();
        }
    }
    if ( readEncoded("P") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.rsaP.bytes)) ) {
            memcpy(ps4Options.rsaP.bytes, decoded.data(), decoded.length());
            ps4Options.rsaP.size = decoded.length();
        }
    }
    if ( readEncoded("Q") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.rsaQ.bytes)) ) {
            memcpy(ps4Options.rsaQ.bytes, decoded.data(), decoded.length());
            ps4Options.rsaQ.size = decoded.length();
        }
    }
    // Serial & Signature
    if ( readEncoded("serial") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.serial.bytes)) ) {
            memcpy(ps4Options.serial.bytes, decoded.data(), decoded.length());
            ps4Options.serial.size = decoded.length();
        }
    }
    if ( readEncoded("signature") ) {
        if ( Base64::Decode(encoded, decoded) && (decoded.length() == sizeof(ps4Options.signature.bytes)) ) {
            memcpy(ps4Options.signature.bytes, decoded.data(), decoded.length());
            ps4Options.signature.size = decoded.length();
        }
    }

    // Zap deprecated fields
    if (ps4Options.rsaD.size != 0) ps4Options.rsaD.size = 0;
    if (ps4Options.rsaDP.size != 0) ps4Options.rsaDP.size = 0;
    if (ps4Options.rsaDQ.size != 0) ps4Options.rsaDQ.size = 0;
    if (ps4Options.rsaQP.size != 0) ps4Options.rsaQP.size = 0;
    if (ps4Options.rsaRN.size != 0) ps4Options.rsaRN.size = 0;

    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));

    return "{\"success\":true}";
}

std::string getAddonOptions()
{
    const size_t capacity = JSON_OBJECT_SIZE(500);
    DynamicJsonDocument doc(capacity);

    const AnalogOptions& analogOptions = Storage::getInstance().getAddonOptions().analogOptions;
    writeDoc(doc, "analogAdc1PinX", cleanPin(analogOptions.analogAdc1PinX));
    writeDoc(doc, "analogAdc1PinY", cleanPin(analogOptions.analogAdc1PinY));
    writeDoc(doc, "analogAdc1Mode", analogOptions.analogAdc1Mode);
    writeDoc(doc, "analogAdc1Invert", analogOptions.analogAdc1Invert);
    writeDoc(doc, "analogAdc2PinX", cleanPin(analogOptions.analogAdc2PinX));
    writeDoc(doc, "analogAdc2PinY", cleanPin(analogOptions.analogAdc2PinY));
    writeDoc(doc, "analogAdc2Mode", analogOptions.analogAdc2Mode);
    writeDoc(doc, "analogAdc2Invert", analogOptions.analogAdc2Invert);
    writeDoc(doc, "forced_circularity", analogOptions.forced_circularity);
    writeDoc(doc, "forced_circularity2", analogOptions.forced_circularity2);
    writeDoc(doc, "inner_deadzone", analogOptions.inner_deadzone);
    writeDoc(doc, "inner_deadzone2", analogOptions.inner_deadzone2);
    writeDoc(doc, "outer_deadzone", analogOptions.outer_deadzone);
    writeDoc(doc, "outer_deadzone2", analogOptions.outer_deadzone2);
    writeDoc(doc, "auto_calibrate", analogOptions.auto_calibrate);
    writeDoc(doc, "auto_calibrate2", analogOptions.auto_calibrate2);
    writeDoc(doc, "analog_smoothing", analogOptions.analog_smoothing);
    writeDoc(doc, "analog_smoothing2", analogOptions.analog_smoothing2);
    writeDoc(doc, "smoothing_factor", analogOptions.smoothing_factor);
    writeDoc(doc, "smoothing_factor2", analogOptions.smoothing_factor2);
    writeDoc(doc, "analog_error", analogOptions.analog_error);
    writeDoc(doc, "analog_error2", analogOptions.analog_error2);
    writeDoc(doc, "AnalogInputEnabled", analogOptions.enabled);
    writeDoc(doc, "negconIMode", analogOptions.negconIMode);
    writeDoc(doc, "negconIIMode", analogOptions.negconIIMode);
    writeDoc(doc, "negconLMode", analogOptions.negconLMode);
    writeDoc(doc, "negconIThreshold", analogOptions.negconIThreshold);
    writeDoc(doc, "negconIIThreshold", analogOptions.negconIIThreshold);
    writeDoc(doc, "negconLThreshold", analogOptions.negconLThreshold);
    writeDoc(doc, "negconIInnerDeadzone", analogOptions.negconIInnerDeadzone);
    writeDoc(doc, "negconIOuterDeadzone", analogOptions.negconIOuterDeadzone);
    writeDoc(doc, "negconIIInnerDeadzone", analogOptions.negconIIInnerDeadzone);
    writeDoc(doc, "negconIIOuterDeadzone", analogOptions.negconIIOuterDeadzone);
    writeDoc(doc, "negconLInnerDeadzone", analogOptions.negconLInnerDeadzone);
    writeDoc(doc, "negconLOuterDeadzone", analogOptions.negconLOuterDeadzone);
    writeDoc(doc, "negconIDigitalEnabled", analogOptions.negconIDigitalEnabled);
    writeDoc(doc, "negconIIDigitalEnabled", analogOptions.negconIIDigitalEnabled);
    writeDoc(doc, "negconLDigitalEnabled", analogOptions.negconLDigitalEnabled);
    writeDoc(doc, "jogconDeadzone", analogOptions.jogconDeadzone);
    writeDoc(doc, "jogconMax", analogOptions.jogconMax);
    writeDoc(doc, "jogconForcebackEnabled", analogOptions.jogconForcebackEnabled);

    const BootselButtonOptions& bootselButtonOptions = Storage::getInstance().getAddonOptions().bootselButtonOptions;
    writeDoc(doc, "bootselButtonMap", bootselButtonOptions.buttonMap);
    writeDoc(doc, "BootselButtonAddonEnabled", bootselButtonOptions.enabled);
    writeDoc(doc, "InputMacroAddonEnabled", Storage::getInstance().getAddonOptions().macroOptions.enabled);

    const ReverseOptions& reverseOptions = Storage::getInstance().getAddonOptions().reverseOptions;
    writeDoc(doc, "reverseActionUp", reverseOptions.actionUp);
    writeDoc(doc, "reverseActionDown", reverseOptions.actionDown);
    writeDoc(doc, "reverseActionLeft", reverseOptions.actionLeft);
    writeDoc(doc, "reverseActionRight", reverseOptions.actionRight);
    writeDoc(doc, "ReverseInputEnabled", reverseOptions.enabled);

    const OnBoardLedOptions& onBoardLedOptions = Storage::getInstance().getAddonOptions().onBoardLedOptions;
    writeDoc(doc, "onBoardLedMode", onBoardLedOptions.mode);
    writeDoc(doc, "BoardLedAddonEnabled", onBoardLedOptions.enabled);
    writeDoc(doc, "OnBoardLedAddonEnabled", onBoardLedOptions.enabled);

    const KeyboardHostOptions& keyboardHostOptions = Storage::getInstance().getAddonOptions().keyboardHostOptions;
    writeDoc(doc, "KeyboardHostAddonEnabled", keyboardHostOptions.enabled);
    writeDoc(doc, "keyboardHostMap", "Up", keyboardHostOptions.mapping.keyDpadUp);
    writeDoc(doc, "keyboardHostMap", "Down", keyboardHostOptions.mapping.keyDpadDown);
    writeDoc(doc, "keyboardHostMap", "Left", keyboardHostOptions.mapping.keyDpadLeft);
    writeDoc(doc, "keyboardHostMap", "Right", keyboardHostOptions.mapping.keyDpadRight);
    writeDoc(doc, "keyboardHostMap", "B1", keyboardHostOptions.mapping.keyButtonB1);
    writeDoc(doc, "keyboardHostMap", "B2", keyboardHostOptions.mapping.keyButtonB2);
    writeDoc(doc, "keyboardHostMap", "B3", keyboardHostOptions.mapping.keyButtonB3);
    writeDoc(doc, "keyboardHostMap", "B4", keyboardHostOptions.mapping.keyButtonB4);
    writeDoc(doc, "keyboardHostMap", "L1", keyboardHostOptions.mapping.keyButtonL1);
    writeDoc(doc, "keyboardHostMap", "R1", keyboardHostOptions.mapping.keyButtonR1);
    writeDoc(doc, "keyboardHostMap", "L2", keyboardHostOptions.mapping.keyButtonL2);
    writeDoc(doc, "keyboardHostMap", "R2", keyboardHostOptions.mapping.keyButtonR2);
    writeDoc(doc, "keyboardHostMap", "S1", keyboardHostOptions.mapping.keyButtonS1);
    writeDoc(doc, "keyboardHostMap", "S2", keyboardHostOptions.mapping.keyButtonS2);
    writeDoc(doc, "keyboardHostMap", "L3", keyboardHostOptions.mapping.keyButtonL3);
    writeDoc(doc, "keyboardHostMap", "R3", keyboardHostOptions.mapping.keyButtonR3);
    writeDoc(doc, "keyboardHostMap", "A1", keyboardHostOptions.mapping.keyButtonA1);
    writeDoc(doc, "keyboardHostMap", "A2", keyboardHostOptions.mapping.keyButtonA2);
    writeDoc(doc, "keyboardHostMap", "A3", keyboardHostOptions.mapping.keyButtonA3);
    writeDoc(doc, "keyboardHostMap", "A4", keyboardHostOptions.mapping.keyButtonA4);
    writeDoc(doc, "keyboardHostMouseLeft", keyboardHostOptions.mouseLeft);
    writeDoc(doc, "keyboardHostMouseMiddle", keyboardHostOptions.mouseMiddle);
    writeDoc(doc, "keyboardHostMouseRight", keyboardHostOptions.mouseRight);
    writeDoc(doc, "keyboardHostMouseSensitivity", keyboardHostOptions.mouseSensitivity);
    writeDoc(doc, "keyboardHostMouseMovement", keyboardHostOptions.movementMode);

    const GamepadUSBHostOptions& gamepadUSBHostOptions = Storage::getInstance().getAddonOptions().gamepadUSBHostOptions;
    writeDoc(doc, "GamepadUSBHostAddonEnabled", gamepadUSBHostOptions.enabled);

    ReactiveLEDOptions& reactiveLEDOptions = Storage::getInstance().getAddonOptions().reactiveLEDOptions;
    writeDoc(doc, "ReactiveLEDAddonEnabled", reactiveLEDOptions.enabled);

    return serialize_json(doc);
}

std::string setMacroAddonOptions()
{
    DynamicJsonDocument doc = get_post_data();
    const auto invalid = []() { return std::string("{\"error\":\"invalid macro options\"}"); };
    if (!doc.is<JsonObject>()) return invalid();
    auto candidate = std::make_unique<MacroOptions>(Storage::getInstance().getAddonOptions().macroOptions);
    for (const char* key : {"InputMacroAddonEnabled", "macroBoardLedEnabled"}) {
        if (!doc.containsKey(key)) continue;
        if (!doc[key].is<bool>() && !(doc[key].is<uint32_t>() && doc[key].as<uint32_t>() <= 1)) return invalid();
    }
    if (doc["macroBoardLedEnabled"].as<bool>()) return invalid();
    docToValue(candidate->enabled, doc, "InputMacroAddonEnabled");
    candidate->macroBoardLedEnabled = false;
    candidate->deprecatedPin = -1;
    if (doc.containsKey("macroList")) {
        if (!doc["macroList"].is<JsonArrayConst>()) return invalid();
        JsonArrayConst macros = doc["macroList"];
        if (macros.size() > sizeof(candidate->macroList) / sizeof(candidate->macroList[0])) return invalid();
        // 添字は固定。短いバックアップの末尾は無効化し、古いMacroを残さない。
        for (unsigned i = 0; i < 6; ++i) {
            auto& target = candidate->macroList[i];
            target = Macro Macro_init_default;
            target.macroType = ON_PRESS;
            target.deprecatedMacroTriggerPin = -1;
            if (i >= macros.size()) continue;
            if (!macros[i].is<JsonObjectConst>()) return invalid();
            JsonObjectConst macro = macros[i];
            for (const char* key : {"enabled", "exclusive", "interruptible", "showFrames", "useMacroTriggerButton"}) {
                if (!macro[key].is<bool>() && !(macro[key].is<uint32_t>() && macro[key].as<uint32_t>() <= 1)) return invalid();
            }
            if (!macro["macroLabel"].is<const char*>() ||
                strlen(macro["macroLabel"].as<const char*>()) >= sizeof(target.macroLabel) ||
                !macro["macroType"].is<uint32_t>() ||
                macro["macroType"].as<uint32_t>() < ON_PRESS || macro["macroType"].as<uint32_t>() > ON_TOGGLE ||
                !macro["macroTriggerButton"].is<uint32_t>() || !macro["macroInputs"].is<JsonArrayConst>()) return invalid();
            strcpy(target.macroLabel, macro["macroLabel"].as<const char*>());
            target.macroType = static_cast<MacroType>(macro["macroType"].as<uint32_t>());
            target.macroTriggerButton = macro["macroTriggerButton"].as<uint32_t>();
            target.enabled = macro["enabled"].as<bool>();
            target.exclusive = macro["exclusive"].as<bool>();
            target.interruptible = macro["interruptible"].as<bool>();
            target.showFrames = macro["showFrames"].as<bool>();
            target.useMacroTriggerButton = macro["useMacroTriggerButton"].as<bool>();
            JsonArrayConst inputs = macro["macroInputs"];
            if (inputs.size() > sizeof(target.macroInputs) / sizeof(target.macroInputs[0])) return invalid();
            for (unsigned j = 0; j < inputs.size(); ++j) {
                if (!inputs[j].is<JsonObjectConst>()) return invalid();
                JsonObjectConst input = inputs[j];
                for (const char* key : {"duration", "waitDuration", "buttonMask"})
                    if (!input[key].is<uint32_t>()) return invalid();
                target.macroInputs[j].duration = input["duration"].as<uint32_t>();
                target.macroInputs[j].waitDuration = input["waitDuration"].as<uint32_t>();
                target.macroInputs[j].buttonMask = input["buttonMask"].as<uint32_t>();
            }
            target.macroInputs_count = inputs.size();
        }
    }
    candidate->macroList_count = 6;
    if (!Storage::validateMacroOptions(*candidate)) return invalid();
    Storage::getInstance().getAddonOptions().macroOptions = *candidate;
    Storage::enforcePsxConfig(Storage::getInstance().getConfig());
    EventManager::getInstance().triggerEvent(new GPStorageSaveEvent(true));
    return serialize_json(doc);
}

std::string getMacroAddonOptions()
{
    const size_t capacity = LWIP_HTTPD_POST_MAX_PAYLOAD_LEN;
    DynamicJsonDocument doc(capacity);

    MacroOptions& macroOptions = Storage::getInstance().getAddonOptions().macroOptions;
    JsonArray macroList = doc.createNestedArray("macroList");

    writeDoc(doc, "macroBoardLedEnabled", macroOptions.macroBoardLedEnabled);
    writeDoc(doc, "InputMacroAddonEnabled", macroOptions.enabled);

    for (int i = 0; i < MAX_MACRO_LIMIT; i++) {
        JsonObject macro = macroList.createNestedObject();
        macro["enabled"] = macroOptions.macroList[i].enabled ? 1 : 0;
        macro["exclusive"] = macroOptions.macroList[i].exclusive ? 1 : 0;
        macro["interruptible"] = macroOptions.macroList[i].interruptible ? 1 : 0;
        macro["showFrames"] = macroOptions.macroList[i].showFrames ? 1 : 0;
        macro["macroType"] = macroOptions.macroList[i].macroType;
        macro["useMacroTriggerButton"] = macroOptions.macroList[i].useMacroTriggerButton ? 1 : 0;
        macro["macroTriggerButton"] = macroOptions.macroList[i].macroTriggerButton;
        macro["macroLabel"] = macroOptions.macroList[i].macroLabel;

        JsonArray macroInputs = macro.createNestedArray("macroInputs");
        for (int j = 0; j < macroOptions.macroList[i].macroInputs_count; j++) {
            JsonObject macroInput = macroInputs.createNestedObject();
            macroInput["buttonMask"] = macroOptions.macroList[i].macroInputs[j].buttonMask;
            macroInput["duration"] = macroOptions.macroList[i].macroInputs[j].duration;
            macroInput["waitDuration"] = macroOptions.macroList[i].macroInputs[j].waitDuration;
        }
    }

    return serialize_json(doc);
}

std::string getFirmwareVersion()
{
    const size_t capacity = JSON_OBJECT_SIZE(10);
    DynamicJsonDocument doc(capacity);
    writeDoc(doc, "version", GP2040VERSION);
    writeDoc(doc, "boardArchitecture", GP2040PLATFORM);
    writeDoc(doc, "boardBuild", GP2040BUILD);
    writeDoc(doc, "boardBuildType", GP2040CONFIG);
    writeDoc(doc, "boardConfigLabel", BOARD_CONFIG_LABEL);
    writeDoc(doc, "boardConfigFileName", BOARD_CONFIG_FILE_NAME);
    writeDoc(doc, "boardConfig", GP2040_BOARDCONFIG);
    return serialize_json(doc);
}

std::string getMemoryReport()
{
    const size_t capacity = JSON_OBJECT_SIZE(10);
    DynamicJsonDocument doc(capacity);
    writeDoc(doc, "totalFlash", System::getTotalFlash());
    writeDoc(doc, "usedFlash", System::getUsedFlash());
    writeDoc(doc, "physicalFlash", Storage::getInstance().GetFlashSize());
    writeDoc(doc, "staticAllocs", System::getStaticAllocs());
    writeDoc(doc, "totalHeap", System::getTotalHeap());
    writeDoc(doc, "usedHeap", System::getUsedHeap());
    return serialize_json(doc);
}

static bool _abortGetHeldPins = false;

std::string getHeldPins()
{
    DynamicJsonDocument doc(JSON_OBJECT_SIZE(100));
    // GPIO の初期化・読み取りは禁止。PSX の最新 snapshot を即時返却する。
    _abortGetHeldPins = false;
    const auto& psx = PsxInput::getInstance();
    const bool connected = psx.snapshot().connected;
    const uint32_t buttons = connected ? psx.virtualButtons() : 0;
    doc["connected"] = connected;
    doc["buttonEventSequence"] = psx.snapshot().buttonEventSequence;
    doc["generation"] = psx.snapshot().generation;
    auto eventPins = doc.createNestedArray("pressedPins");
    for (unsigned slot = 8; slot <= 23; ++slot)
        if (connected && (psx.snapshot().buttonEventMask & (1u << slot))) eventPins.add(slot);
    auto heldPins = doc.createNestedArray("heldPins");
    for (unsigned slot = 8; slot <= 23; ++slot)
        if (buttons & (1u << slot)) heldPins.add(slot);

    return serialize_json(doc);
}

std::string abortGetHeldPins()
{
    _abortGetHeldPins = true;
    return {};
}

std::string getConfig()
{
    return ConfigUtils::toJSON(Storage::getInstance().getConfig());
}

DataAndStatusCode setConfig()
{
    // Store config struct on the heap to avoid stack overflow
    std::unique_ptr<Config> config(new Config);
    *config.get() = Storage::getInstance().getConfig();
    if (ConfigUtils::fromJSON(*config.get(), http_post_payload, http_post_payload_len))
    {
        Storage::getInstance().getConfig() = *config.get();
        config.reset();
        if (Storage::getInstance().save(true))
        {
            return DataAndStatusCode(getConfig(), HttpStatusCode::_200);
        }
        else
        {
            return DataAndStatusCode("{ \"error\": \"internal error while saving config\" }", HttpStatusCode::_500);
        }
    }
    else
    {
        return DataAndStatusCode("{ \"error\": \"invalid JSON document\" }", HttpStatusCode::_400);
    }
}

// This should be a storage feature
std::string resetSettings()
{
    Storage::getInstance().ResetSettings();
    const size_t capacity = JSON_OBJECT_SIZE(10);
    DynamicJsonDocument doc(capacity);
    doc["success"] = true;
    return serialize_json(doc);
}

#if !defined(NDEBUG)
std::string echo()
{
    DynamicJsonDocument doc = get_post_data();
    return serialize_json(doc);
}
#endif

// MUST MATCH NAVIGATION.JSX
enum BOOT_MODES {
	GAMEPAD = 0,
	WEBCONFIG = 1,
	BOOTSEL = 2,
};

std::string reboot() {
    DynamicJsonDocument doc = get_post_data();
    uint32_t bootMode = doc["bootMode"];
    System::BootMode systemBootMode = System::BootMode::DEFAULT;
    if ( bootMode == BOOT_MODES::GAMEPAD ) {
        systemBootMode = System::BootMode::GAMEPAD;
    } else if ( bootMode == BOOT_MODES::WEBCONFIG ) {
        systemBootMode = System::BootMode::WEBCONFIG;
    } else if (bootMode == BOOT_MODES::BOOTSEL ) {
        systemBootMode = System::BootMode::USB;
    }
    EventManager::getInstance().triggerEvent(new GPRestartEvent((System::BootMode)systemBootMode));
    doc["success"] = true;
    return serialize_json(doc);
}

typedef std::string (*HandlerFuncPtr)();
static const std::pair<const char*, HandlerFuncPtr> handlerFuncs[] =
{
    { "/api/getAnalogMonitor", getAnalogMonitor },
    { "/api/setAnalogPreviewDraft", setAnalogPreviewDraft },
    { "/api/setGamepadOptions", setGamepadOptions },
    { "/api/setPinMappings", setPinMappings },
    { "/api/setProfileOptions", setProfileOptions },
    { "/api/getPeripheralOptions", getPeripheralOptions },
    { "/api/setReactiveLEDs", setReactiveLEDs },
    { "/api/getReactiveLEDs", getReactiveLEDs },
    { "/api/setKeyMappings", setKeyMappings },
    { "/api/setAddonsOptions", setAddonOptions },
    { "/api/setMacroAddonOptions", setMacroAddonOptions },
    { "/api/setPS4Options", setPS4Options },
    { "/api/reboot", reboot },
    { "/api/getGamepadOptions", getGamepadOptions },
    { "/api/getPinMappings", getPinMappings },
    { "/api/getProfileOptions", getProfileOptions },
    { "/api/getKeyMappings", getKeyMappings },
    { "/api/getAddonsOptions", getAddonOptions },
    { "/api/getMacroAddonOptions", getMacroAddonOptions },
    { "/api/resetSettings", resetSettings },
    { "/api/getFirmwareVersion", getFirmwareVersion },
    { "/api/getMemoryReport", getMemoryReport },
    { "/api/getHeldPins", getHeldPins },
    { "/api/abortGetHeldPins", abortGetHeldPins },
    { "/api/getUsedPins", getUsedPins },
    { "/api/getConfig", getConfig },
#if !defined(NDEBUG)
    { "/api/echo", echo },
#endif
};

typedef DataAndStatusCode (*HandlerFuncStatusCodePtr)();
static const std::pair<const char*, HandlerFuncStatusCodePtr> handlerFuncsWithStatusCode[] =
{
    { "/api/setConfig", setConfig },
};

int fs_open_custom(struct fs_file *file, const char *name)
{
    if (strcmp(name, "/api/resetJogconDial") == 0) {
        if (!jogResetResponsePending)
            return set_file_data(file, DataAndStatusCode("{\"error\":\"method_not_allowed\"}", HttpStatusCode::_405));
        jogResetResponsePending = false;
        return set_file_data(file, DataAndStatusCode(std::move(jogResetResponse), jogResetResponseStatus));
    }
    for (const auto& handlerFunc : handlerFuncs)
    {
        if (strcmp(handlerFunc.first, name) == 0)
        {
            return set_file_data(file, handlerFunc.second());
        }
    }

    for (const auto& handlerFunc : handlerFuncsWithStatusCode)
    {
        if (strcmp(handlerFunc.first, name) == 0)
        {
            return set_file_data(file, handlerFunc.second());
        }
    }

    for (const char* excludePath : excludePaths)
        if (strcmp(excludePath, name) == 0)
            return 0;

    for (const char* spaPath : spaPaths)
    {
        // 別窓モニターのsessionクエリはページ名ではない。完全なパス境界で判定する。
        const size_t pathLength = strcspn(name, "?");
        if (strlen(spaPath) == pathLength && strncmp(spaPath, name, pathLength) == 0)
        {
            file->data = (const char *)file__index_html[0].data;
            file->len = file__index_html[0].len;
            file->index = file__index_html[0].len;
            file->http_header_included = file__index_html[0].http_header_included;
            file->pextension = NULL;
            file->is_custom_file = 0;
            return 1;
        }
    }

    return 0;
}

void fs_close_custom(struct fs_file *file)
{
    if (file && file->is_custom_file && file->pextension)
    {
        delete[] static_cast<char*>(file->pextension);
        file->pextension = NULL;
    }
}

#pragma once

#include <lvgl.h>
#include "../services/MeshtasticService.h"
#include "MapView.h"

// Meshtastic UI, laid out to match Meshtastic's own on-device UI ("MUI") so
// anyone who has used a T-Deck feels at home: a home dashboard, a Nodes list
// with tap-to-select, a Chats list mixing the channel and direct-message
// conversations with unread counts, and a thread view whose message bubbles
// are outlined by delivery state - green acked, red failed, yellow relayed.
//
// Every page is a hidden/shown container on one screen. Lists are rebuilt
// from render() (the 250 ms tick), never from inside a button callback, so a
// row is never deleted while its own click is still being dispatched.
class MeshtasticScreen {
public:
    using BackCallback = void (*)(void *userData);

    void create(MeshtasticService *service, BackCallback backCallback, void *userData);
    void show(const MeshtasticStatus &status);
    void render(const MeshtasticStatus &status);

private:
    enum class Page : uint8_t { Home, Nodes, Chats, Thread, Info, Compose, Channels, ChannelEdit, Map };

    // A conversation is either a channel (peer == kMeshtasticBroadcast, one
    // per channel slot) or a direct-message exchange with one node.
    struct Conversation {
        bool used = false;
        uint32_t peer = kMeshtasticBroadcast;
        uint8_t channel = 0;
        uint32_t lastViewedMs = 0;
    };
    static constexpr uint8_t kMaxConversations = MeshtasticStatus::kMaxChannels + 16; // channels + DM peers

    struct RowContext {
        MeshtasticScreen *screen = nullptr;
        uint32_t peer = 0;
        uint8_t channel = 0;
    };

    void buildTopBar();
    void buildHome();
    void buildNodesPage();
    void buildChatsPage();
    void buildThreadPage();
    void buildInfoPage();
    void buildComposer();
    void buildChannelsPage();
    void buildChannelEditor();
    void buildMapPage();
    void renderMap(const MeshtasticStatus &status);
    void centerMap(const MeshtasticStatus &status);
    static void mapCenterThunk(void *userData);
    lv_obj_t *makePage();
    lv_obj_t *makeTile(lv_obj_t *parent, const char *label, int x, int y, lv_color_t color,
                       lv_event_cb_t cb, lv_obj_t **valueOut);

    void showPage(Page page);
    void rebuildNodes(const MeshtasticStatus &status);
    void rebuildChats(const MeshtasticStatus &status);
    void rebuildThread(const MeshtasticStatus &status);
    void rebuildChannels(const MeshtasticStatus &status);
    void openChannelEditor(uint8_t slot);
    void renderHome(const MeshtasticStatus &status);
    void renderInfo(const MeshtasticStatus &status);
    void openThread(uint32_t peer, uint8_t channel);
    Conversation *findOrAddConversation(uint32_t peer, uint8_t channel);
    void syncConversations(const MeshtasticStatus &status);
    uint32_t unreadFor(const Conversation &c, const MeshtasticStatus &status) const;
    bool messageInConversation(const MeshtasticMessage &m, uint32_t peer, uint8_t channel) const;
    static const char *channelTag(const MeshtasticChannel &channel);
    static const char *nodeLabel(const MeshtasticNode *node, uint32_t num, char *buf, size_t bufSize);
    const MeshtasticNode *findNode(const MeshtasticStatus &status, uint32_t num) const;
    static lv_color_t deliveryColor(MeshtasticDelivery delivery);

    static void backThunk(lv_event_t *event);
    static void nodesTileThunk(lv_event_t *event);
    static void chatsTileThunk(lv_event_t *event);
    static void mapTileThunk(lv_event_t *event);
    static void infoTileThunk(lv_event_t *event);
    static void rowThunk(lv_event_t *event);
    static void writeThunk(lv_event_t *event);
    static void sendThunk(lv_event_t *event);
    static void cancelThunk(lv_event_t *event);
    static void channelsTileThunk(lv_event_t *event);
    static void channelRowThunk(lv_event_t *event);
    static void addChannelThunk(lv_event_t *event);
    static void channelSaveThunk(lv_event_t *event);
    static void channelDeleteThunk(lv_event_t *event);
    static void channelFieldThunk(lv_event_t *event);

    lv_obj_t *_screen = nullptr;
    lv_obj_t *_title = nullptr;
    lv_obj_t *_home = nullptr;
    lv_obj_t *_statusLine = nullptr;
    lv_obj_t *_nodesValue = nullptr;
    lv_obj_t *_chatsValue = nullptr;
    lv_obj_t *_nodesPage = nullptr;
    lv_obj_t *_nodesList = nullptr;
    lv_obj_t *_chatsPage = nullptr;
    lv_obj_t *_chatsList = nullptr;
    lv_obj_t *_threadPage = nullptr;
    lv_obj_t *_threadList = nullptr;
    lv_obj_t *_infoPage = nullptr;
    lv_obj_t *_infoText = nullptr;
    lv_obj_t *_composer = nullptr;
    lv_obj_t *_composeTitle = nullptr;
    lv_obj_t *_textArea = nullptr;
    lv_obj_t *_keyboard = nullptr;
    lv_obj_t *_channelsValue = nullptr;
    lv_obj_t *_channelsPage = nullptr;
    lv_obj_t *_channelsList = nullptr;
    lv_obj_t *_addChannelButton = nullptr;
    lv_obj_t *_channelEditor = nullptr;
    lv_obj_t *_channelNameInput = nullptr;
    lv_obj_t *_channelKeyInput = nullptr;
    lv_obj_t *_channelError = nullptr;
    lv_obj_t *_channelDeleteButton = nullptr;
    lv_obj_t *_channelKeyboard = nullptr;
    lv_obj_t *_mapPage = nullptr;
    MapView _map;
    bool _mapCentered = false;
    bool _mapCenterOnNodes = false;
    uint32_t _mapMarkerSignature = 0;

    Page _page = Page::Home;
    Page _pageBeforeCompose = Page::Thread;
    uint32_t _threadPeer = kMeshtasticBroadcast;
    uint8_t _threadChannel = 0;
    uint8_t _editSlot = 0;
    uint32_t _lastListRebuildMs = 0;
    bool _listDirty = true;

    Conversation _conversations[kMaxConversations];
    // One context per possible row across the nodes and chats lists.
    RowContext _rowContexts[MeshtasticStatus::kMaxNodes + kMaxConversations];
    RowContext _channelContexts[MeshtasticStatus::kMaxChannels];

    MeshtasticService *_service = nullptr;
    BackCallback _backCallback = nullptr;
    void *_userData = nullptr;
};

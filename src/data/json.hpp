#pragma once

#include <glaze/json.hpp>
#include <glaze/core/macros.hpp>
#include <string>

namespace pastry_fish
{
    struct Filters
    {
        std::vector<double> patches;
        std::string completeType;
        std::string bigFishType;
        std::vector<std::string> completeTypes;
        std::vector<std::string> bigFishTypes;
        int fishN;
        std::string sorterType;
        std::vector<std::string> fishConstraintTypes;

        GLZ_LOCAL_META(Filters, patches, completeType, completeTypes, bigFishTypes, bigFishType, fishN, sorterType, fishConstraintTypes);
    };

    struct BaitFilter
    {
        bool enabled;
        std::vector<std::uint32_t> baitIds;
        GLZ_LOCAL_META(BaitFilter, enabled, baitIds);
    };

    struct ListSetting
    {
        std::unordered_map<std::string, bool> normal;
        std::unordered_map<std::string, bool> pinned;
        GLZ_LOCAL_META(ListSetting, normal, pinned);
    };

    struct Notification
    {
        struct NotificationSettings
        {
            std::string key;
            std::string sound;
            bool enabled;
            bool hasBefore;
            int before;
            GLZ_LOCAL_META(NotificationSettings, key, sound, enabled, hasBefore, before);
        };

        struct NotificationIKDRouteSettings
        {
            std::string key;
            std::string sound;
            bool enabled;
            bool hasBefore;
            int before;
            int offset;
            GLZ_LOCAL_META(NotificationIKDRouteSettings, key, sound, enabled, hasBefore, before, offset);
        };

        double volume;
        bool isSystemNotificationEnabled;
        std::vector<NotificationSettings> settings;
        std::vector<NotificationIKDRouteSettings> IKDRouteSettings;
        GLZ_LOCAL_META(Notification, volume, isSystemNotificationEnabled, settings, IKDRouteSettings);
    };

    struct DetailArrangement
    {
        struct Components
        {
            std::string name;
            bool expandedEnabled;
            bool expanded;
            bool enabled;
            int order;
            GLZ_LOCAL_META(Components, name, expandedEnabled, expanded, enabled, order);
        };

        std::vector<Components> components;
        GLZ_LOCAL_META(DetailArrangement, components);
    };

    struct Theme
    {
        bool dark;
        std::string mode;
        GLZ_LOCAL_META(Theme, dark, mode);
    };

    struct Event
    {
        bool starLight;
        GLZ_LOCAL_META(Event, starLight);
    };

    struct OceanFishing
    {
        bool showWarningDialog;
        bool showUpdateDialog;
        bool showBiteTimeDialog;
        GLZ_LOCAL_META(OceanFishing, showWarningDialog, showUpdateDialog, showBiteTimeDialog);
    };

    struct Link
    {
        struct Default
        {
            std::string itemV2;
            std::string fish;
            std::string spot;
            GLZ_LOCAL_META(Default, itemV2, fish, spot);
        };

        Default default_;

        struct glaze
        {
            using T = Link;
            static constexpr auto value = glz::object("default", &T::default_);
        };
    };

    struct Bait
    {
        struct ListFilter
        {
            std::vector<std::string> completeTypes;
            std::vector<std::string> bigFishTypes;
            std::string sorterType;
            GLZ_LOCAL_META(ListFilter, completeTypes, bigFishTypes, sorterType);
        };

        struct NotificationFilter
        {
            std::vector<std::string> completeTypes;
            std::vector<std::string> bigFishTypes;
            GLZ_LOCAL_META(NotificationFilter, completeTypes, bigFishTypes);
        };

        bool enableNotification;
        ListFilter listFilter;
        NotificationFilter notificationFilter;
        GLZ_LOCAL_META(Bait, enableNotification, listFilter, notificationFilter);
    };

    struct Main
    {
        std::string websiteVersion;
        std::string migrationVersion;
        std::vector<std::uint32_t> completed;
        std::vector<std::uint32_t> pinned;
        std::vector<std::uint32_t> toBeNotified;
        std::vector<std::uint32_t> toBeNotifiedLocked;
        std::vector<std::uint32_t> toBeNotifiedIKDRoutes;

        Filters filters;
        BaitFilter baitFilter;
        ListSetting listSetting;

        bool showFilter;
        bool showBanner;
        int opacity;
        int zoomFactor;
        int rightPanePercentage;
        double rightPanePercentageV2;

        Notification notification;
        DetailArrangement detailArrangement;
        Theme theme;
        Event event;
        OceanFishing oceanFishing;
        Link link;
        Bait bait;

        bool showChromeBugDialog;
        bool showCompetitionDialogV2;
        bool fishEyesUsed;
        std::unordered_map<std::string, std::string> mainWindow;
        bool isRoseMode;
        bool readChartTip;

        GLZ_LOCAL_META(Main,
                       websiteVersion,
                       migrationVersion,
                       completed,
                       pinned,
                       toBeNotified,
                       toBeNotifiedIKDRoutes,
                       toBeNotifiedLocked,
                       filters,
                       baitFilter,
                       listSetting,
                       showFilter,
                       showBanner,
                       opacity,
                       zoomFactor,
                       rightPanePercentage,
                       rightPanePercentageV2,
                       notification,
                       detailArrangement,
                       theme,
                       event,
                       oceanFishing,
                       link,
                       bait,
                       showChromeBugDialog,
                       showCompetitionDialogV2,
                       fishEyesUsed,
                       mainWindow,
                       isRoseMode,
                       readChartTip);
    };
}

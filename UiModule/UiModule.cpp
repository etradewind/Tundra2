// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "UiModule.h"
#include "UiProxyWidget.h"
#include "UiWidgetProperties.h"
#include "UiProxyStyle.h"

#include <NetworkEvents.h>
#include <SceneEvents.h>

#include <QApplication>

#include <QTimer>

namespace UiServices
{

    UiModule::UiModule() :
        Foundation::ModuleInterfaceImpl(Foundation::Module::MT_UiServices),
        event_query_categories_(QStringList()),
        ui_scene_manager_(0),
        ui_notification_manager_(0)
    {
    }

    UiModule::~UiModule()
    {
        SAFE_DELETE(ui_scene_manager_);
        SAFE_DELETE(ui_notification_manager_);
    }

    /*************** ModuleInterfaceImpl ***************/

    void UiModule::Load()
    {
        QApplication::setStyle(new UiProxyStyle());
        event_query_categories_ << "Framework" << "Scene";    
    }

    void UiModule::Unload()
    {
    }

    void UiModule::Initialize()
    {
        SubscribeToEventCategories();

        ui_view_ = framework_->GetUIView();
        if (ui_view_)
        {
            LogDebug(">> Acquired Ogre QGraphicsView shared pointer from framework");
            ui_scene_manager_ = new UiSceneManager(GetFramework(), ui_view_);
            LogDebug(">> Scene Manager service READY");
            ui_notification_manager_ = new UiNotificationManager(GetFramework(), ui_view_);
            LogDebug(">> Notification Manager service READY");
        }
    }

    void UiModule::Uninitialize()
    {
    }

    void UiModule::Update(f64 frametime)
    {
    }

    bool UiModule::HandleEvent(event_category_id_t category_id, event_id_t event_id, Foundation::EventDataInterface* data)
    {
        QString category = service_category_identifiers_.keys().value(service_category_identifiers_.values().indexOf(category_id));
        if (category == "Framework")
        {
            switch (event_id)
            {
                case Foundation::NETWORKING_REGISTERED:
                    event_query_categories_ << "NetworkState";
                    SubscribeToEventCategories();
                    break;
                default:
                    break;
            }
        }
        else if (category == "NetworkState")
        {
            switch (event_id)
            {
                case ProtocolUtilities::Events::EVENT_SERVER_DISCONNECTED:
                    ui_scene_manager_->Disconnected();
                    break;
                case ProtocolUtilities::Events::EVENT_SERVER_CONNECTED:
                {
                    ProtocolUtilities::AuthenticationEventData *auth_data = dynamic_cast<ProtocolUtilities::AuthenticationEventData *>(data);
                    if (auth_data)
                    {
                        current_avatar_ = QString::fromStdString(auth_data->identityUrl);
                        current_server_ = QString::fromStdString(auth_data->hostUrl);
                    }
                    break;
                }
                default:
                    break;
            }
        }
        else if (category == "Scene")
        {
            switch (event_id)
            {
                case Scene::Events::EVENT_CONTROLLABLE_ENTITY:
                {
                    ui_scene_manager_->Connected();
                    QString welcome_message;
                    if (!current_avatar_.isEmpty())
                        welcome_message = current_avatar_ + " welcome to " + current_server_;
                    else
                        welcome_message = "Welcome to " + current_server_;
                    ui_notification_manager_->ShowInformationString(welcome_message, 10000);
                    break;
                }
                default:
                    break;
            }
        }

        return false;
    }

    void UiModule::SubscribeToEventCategories()
    {
        service_category_identifiers_.clear();
        foreach (QString category, event_query_categories_)
        {
            service_category_identifiers_[category] = framework_->GetEventManager()->QueryEventCategory(category.toStdString());
            LogDebug(QString(">> Listening to event category %1").arg(category).toStdString());
        }
    }

}

/************** Poco Module Loading System **************/

extern "C" void POCO_LIBRARY_API SetProfiler(Foundation::Profiler *profiler);
void SetProfiler(Foundation::Profiler *profiler)
{
    Foundation::ProfilerSection::SetProfiler(profiler);
}

using namespace UiServices;

POCO_BEGIN_MANIFEST(Foundation::ModuleInterface)
   POCO_EXPORT_CLASS(UiModule)
POCO_END_MANIFEST


// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "ECEditorModule.h"
#include "ECEditorWindow.h"
#include "EcXmlEditorWidget.h"
#include "DoxygenDocReader.h"
#include "TreeWidgetItemExpandMemory.h"

#include "SceneAPI.h"
#include "Scene.h"
#include "Entity.h"
#include "InputAPI.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "ConsoleAPI.h"
#include "ConfigAPI.h"
#include "EC_Placeable.h"
#include "QScriptEngineHelpers.h"

#include "MemoryLeakCheck.h"

#include <QWebView>
#include <QList>

const char *cGizmoEnabled= "show editing gizmo";
const char *cHighlightingEnabled = "highlight selected entities";

ECEditorModule::ECEditorModule() :
    IModule("ECEditor"),
    gizmoEnabled(true),
    highlightingEnabled(true),
    toggleSelectAllEntities(false)
{
}

ECEditorModule::~ECEditorModule()
{
}

void ECEditorModule::Initialize()
{
    expandMemory = ExpandMemoryPtr(new TreeWidgetItemExpandMemory(Name().toStdString().c_str(), framework_));

    ConfigAPI &cfg = *framework_->Config();
    ConfigData gizmoConfig(ConfigAPI::FILE_FRAMEWORK, Name(), cGizmoEnabled, gizmoEnabled, true);
    if (!cfg.HasValue(gizmoConfig))
        cfg.Set(gizmoConfig);
    gizmoEnabled = cfg.Get(gizmoConfig).toBool();

    ConfigData highlightConfig(ConfigAPI::FILE_FRAMEWORK, Name(), cHighlightingEnabled, highlightingEnabled, true);
    if (!cfg.HasValue(highlightConfig))
        cfg.Set(highlightConfig);
    highlightingEnabled = cfg.Get(highlightConfig).toBool();

    framework_->Console()->RegisterCommand("doc", "Prints the class documentation for the given symbol.",
        this, SLOT(ShowDocumentation(const QString &)));

    inputContext = framework_->Input()->RegisterInputContext("ECEditorInput", 90);
    connect(inputContext.get(), SIGNAL(KeyPressed(KeyEvent *)), this, SLOT(HandleKeyPressed(KeyEvent *)));

    /// @todo Ideally we wouldn't do this, but this is needed for now in order to get OnScriptEngineCreated called
    /// (and ECEditorWindow registered to QtScript) without generating dependendy to the JavascriptModule.
    framework_->RegisterDynamicObject("ecEditorModule", this);
}

void ECEditorModule::Uninitialize()
{
    ConfigData configData(ConfigAPI::FILE_FRAMEWORK, Name());
    framework_->Config()->Set(configData, cGizmoEnabled, gizmoEnabled);
    framework_->Config()->Set(configData, cHighlightingEnabled, highlightingEnabled);

    SAFE_DELETE(commonEditor);
    SAFE_DELETE_LATER(xmlEditor);
}

ECEditorWindow *ECEditorModule::ActiveEditor() const
{
    return activeEditor;
}

void ECEditorModule::SetGizmoEnabled(bool enabled)
{
    if (framework_->IsHeadless() || enabled == gizmoEnabled)
        return;

    gizmoEnabled = enabled;

    foreach(ECEditorWindow *editor, framework_->Ui()->MainWindow()->findChildren<ECEditorWindow *>())
        if (gizmoEnabled && editor == activeEditor) // if gizmoEnabled == true, show visual aids only for active editor.
            editor->SetGizmoVisible(gizmoEnabled);
        else
            editor->SetGizmoVisible(false);
}

void ECEditorModule::SetHighlightingEnabled(bool enabled)
{
    if (framework_->IsHeadless() || enabled == highlightingEnabled)
        return;

    highlightingEnabled = enabled;

    foreach(ECEditorWindow *editor, framework_->Ui()->MainWindow()->findChildren<ECEditorWindow *>())
        if (highlightingEnabled && editor == activeEditor) // if highlightingEnabled == true, show visual aids only for active editor.
            editor->SetHighlightingEnabled(highlightingEnabled);
        else
            editor->SetHighlightingEnabled(false);
}

void ECEditorModule::ECEditorFocusChanged(ECEditorWindow *editor)
{
    if (editor == activeEditor && !editor)
        return;

    // Unfocus previously active editor and disconnect all signals from that editor.
    if (activeEditor)
    {
        activeEditor->SetFocus(false);
        disconnect(activeEditor, SIGNAL(SelectionChanged(const QString&, const QString&, const QString&, const QString&)),
            this, SIGNAL(SelectionChanged(const QString&, const QString&, const QString&, const QString&)));
    }

    activeEditor = editor;
    activeEditor->SetFocus(true);
    connect(activeEditor, SIGNAL(SelectionChanged(const QString&, const QString&, const QString&, const QString&)),
        this, SIGNAL(SelectionChanged(const QString&, const QString&, const QString&, const QString&)), Qt::UniqueConnection);

    emit ActiveEditorChanged(activeEditor);
}

void ECEditorModule::ShowEditorWindow()
{
    if (framework_->IsHeadless())
        return;

    if (activeEditor)
    {
        activeEditor->setVisible(!activeEditor->isVisible());
        return;
    }

    activeEditor = new ECEditorWindow(GetFramework(), framework_->Ui()->MainWindow());
    commonEditor = activeEditor;
    activeEditor->setWindowFlags(Qt::Tool);
    activeEditor->setAttribute(Qt::WA_DeleteOnClose);
    activeEditor->show();
    activeEditor->activateWindow();

    RepositionEditor(activeEditor);
}

void ECEditorModule::RepositionEditor(ECEditorWindow *editor)
{
    if (!editor)
        return;
    if (!GetFramework()->Ui()->MainWindow())
        return;
    
    QRect editorGeom = editor->frameGeometry();
    QSize desktopSize(GetFramework()->Ui()->MainWindow()->DesktopWidth(), GetFramework()->Ui()->MainWindow()->DesktopHeight());

    QWidget *sceneWidget = GetFramework()->Ui()->MainWindow()->findChild<QWidget*>("SceneStructureWidget");
    if (sceneWidget)
    {
        QRect sceneWidgetGeom = sceneWidget->frameGeometry();
        if (sceneWidgetGeom.topRight().x() + editorGeom.width() < desktopSize.width())
            editor->move(sceneWidgetGeom.topRight());
        else if (sceneWidgetGeom.topLeft().x() - editorGeom.width() > 0)
            editor->move(sceneWidgetGeom.topLeft() - QPoint(editorGeom.width(), 0));
    }
    else
    {
        QRect mainWinGeom = GetFramework()->Ui()->MainWindow()->geometry();
        QPoint movePos(mainWinGeom.topLeft().x() + 25, (mainWinGeom.topLeft().y() + (mainWinGeom.height() / 2)) - (editor->height() / 2));
        editor->move(movePos);
    }
}

void ECEditorModule::ShowDocumentation(const QString &symbol)
{
    QUrl styleSheetPath;
    QString documentation;
    DoxygenDocReader::GetSymbolDocumentation(symbol, &documentation, &styleSheetPath);
    if (documentation.length() == 0)
    {
        LogError("ECEditorModule::ShowDocumentation: Failed to find documentation for symbol \"" + symbol + "\"!");
        return;
    }

    QWebView *webview = new QWebView();
    webview->setAttribute(Qt::WA_DeleteOnClose);
    webview->setHtml(documentation, styleSheetPath);
    webview->show();
}

void ECEditorModule::CreateXmlEditor(EntityPtr entity)
{
    CreateXmlEditor(QList<EntityPtr>(QList<EntityPtr>() << entity));
}

void ECEditorModule::CreateXmlEditor(const QList<EntityPtr> &entities)
{
    if (framework_->IsHeadless())
        return;
    if (entities.empty())
        return;

    if (!xmlEditor)
    {
        xmlEditor = new EcXmlEditorWidget(framework_, GetFramework()->Ui()->MainWindow());
        xmlEditor->setAttribute(Qt::WA_DeleteOnClose);
        xmlEditor->setWindowFlags(Qt::Tool);
        if (activeEditor) // make sure the editing gizmo follow the entity when it's saved
            connect(xmlEditor, SIGNAL(Saved()), activeEditor.data(), SLOT(RefreshPropertyBrowser()), Qt::UniqueConnection);
    }

    xmlEditor->SetEntity(entities);
    xmlEditor->show();
    xmlEditor->activateWindow();
}

void ECEditorModule::CreateXmlEditor(ComponentPtr component)
{
    QList<ComponentPtr> components;
    components << component;
    CreateXmlEditor(components);
}

void ECEditorModule::CreateXmlEditor(const QList<ComponentPtr> &components)
{
    if (framework_->IsHeadless())
        return;
    if (components.empty())
        return;

    if (!xmlEditor)
    {
        xmlEditor = new EcXmlEditorWidget(framework_, GetFramework()->Ui()->MainWindow());
        xmlEditor->setAttribute(Qt::WA_DeleteOnClose);
        xmlEditor->setWindowFlags(Qt::Tool);
    }

    xmlEditor->SetComponent(components);
    xmlEditor->show();
    xmlEditor->activateWindow();
}

void ECEditorModule::HandleKeyPressed(KeyEvent *e)
{
    if (e->eventType != KeyEvent::KeyPressed || e->keyPressCount > 1)
        return;

    const QKeySequence showEcEditor = framework_->Input()->KeyBinding("ShowECEditor", QKeySequence(Qt::ShiftModifier + Qt::Key_E));
    const QKeySequence toggle = framework_->Input()->KeyBinding("ToggleVisualEditingAids", QKeySequence(Qt::Key_section));
    const QKeySequence toggleSelectAll = framework_->Input()->KeyBinding("ToggleSelectAllEntities", QKeySequence(Qt::ControlModifier + Qt::Key_A));
    if (e->sequence == showEcEditor)
    {
        ShowEditorWindow();
        e->Suppress();
    }
    else if (e->sequence == toggle)
    {
        /// @todo For now toggling both, but should be we have separate keyboard shortcuts for individual editing aid?
        SetGizmoEnabled(!IsGizmoEnabled());
        SetHighlightingEnabled(!IsHighlightingEnabled());
        e->Suppress();
    }
    else if (e->sequence == toggleSelectAll)
    {
        // Only if visual editing is enabled
        if ((gizmoEnabled || highlightingEnabled) && activeEditor)
        {
            // Select/deselect all entities for the current editor
            toggleSelectAllEntities = !toggleSelectAllEntities;
            if (toggleSelectAllEntities)
            {
                Scene *activeScene = framework_->Scene()->MainCameraScene();
                if (activeScene)
                {
                    // We can only manipulate entities that have placeable, but exclude temporarys (avatar, cameras etc.)
                    QList<entity_id_t> entIdsSelection;
                    QVariantList entIds = activeScene->GetEntityIdsWithComponent(EC_Placeable::TypeNameStatic());
                    foreach(QVariant entId, entIds)
                    {
                        Entity *ent = activeScene->GetEntity(entId.toUInt()).get();
                        if (ent)
                        {
                            if (ent->IsTemporary())
                                continue;
                            entIdsSelection.append(entId.toUInt());
                        }
                    }
                    if (!entIdsSelection.isEmpty())
                        activeEditor->AddEntities(entIdsSelection, true);
                }
            }
            else
                activeEditor->ClearEntities();
        }
        e->Suppress();
    }
}

void ECEditorModule::OnScriptEngineCreated(QScriptEngine* engine)
{
    qScriptRegisterQObjectMetaType<ECEditorWindow *>(engine);
}

// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "UiStateMachine.h"

#include <QPropertyAnimation>
#include <QGraphicsWidget>
#include <QDebug>

namespace UiServices
{
    UiStateMachine::UiStateMachine(QObject *parent, QGraphicsView *view)
        : QStateMachine(parent),
          view_(view),
          current_scene_(view->scene())
    {
        state_ether_ = new QState(this);
        state_inworld_ = new QState(this);
        state_connecting_ = new QState(this);
        state_animating_change_ = new QState(this);

        SetTransitions();
        setInitialState(state_inworld_);
        start();
    }

    // Private

    void UiStateMachine::SetTransitions()
    {
        state_ether_->addTransition(this, SIGNAL( EtherTogglePressed()), state_inworld_);
        state_inworld_->addTransition(this, SIGNAL( EtherTogglePressed()), state_ether_);

        connect(state_ether_, SIGNAL( exited() ), SLOT( AnimationsStart() ));
        connect(state_inworld_, SIGNAL( exited() ), SLOT( AnimationsStart() ));
        connect(view_, SIGNAL( ViewKeyPressed(QKeyEvent *) ), SLOT( ViewKeyEvent(QKeyEvent *) ));
    }

    void UiStateMachine::ViewKeyEvent(QKeyEvent *key_event)
    {
        switch (key_event->key())
        {
            case Qt::Key_Escape:
               emit EtherTogglePressed(); 
        }
    }

    void UiStateMachine::AnimationsStart()
    {
        // Create group if doesent exist yet
        QParallelAnimationGroup *animations_group;
        if (!animations_map_.contains(current_scene_))
        {
            animations_group = new QParallelAnimationGroup(this);
            animations_map_[current_scene_] = animations_group;
            connect(animations_group, SIGNAL( finished() ), SLOT( AnimationsFinished() ));
        }
        else
            animations_group = animations_map_[current_scene_];

        animations_group->clear();
        foreach (QGraphicsItem *item, current_scene_->items())
        {
            QGraphicsWidget *widget = dynamic_cast<QGraphicsWidget*>(item);
            if (!widget)
                continue;

            QPropertyAnimation *anim = new QPropertyAnimation(widget, "opacity", animations_group);
            anim->setDuration(300);
            anim->setStartValue(widget->opacity());
            anim->setEndValue(0);
            animations_group->addAnimation(anim);
        }
        animations_group->setDirection(QAbstractAnimation::Forward);
        animations_group->start();
    }

    void UiStateMachine::AnimationsFinished()
    {
        if (animations_map_[current_scene_]->direction() != QAbstractAnimation::Forward)
            return;

        QString scene_name;
        if (current_scene_ == scene_map_["Ether"])
            scene_name = "Inworld";
        else if (current_scene_ == scene_map_["Inworld"])
            scene_name = "Ether";
        SwitchToScene(scene_name);
    }

    // Public

    void UiStateMachine::RegisterScene(QString name, QGraphicsScene *scene)
    {
        if (!scene_map_.contains(name))
            scene_map_[name] = scene;
    }
    
    void UiStateMachine::SwitchToScene(QString name)
    {
        if (!scene_map_.contains(name))
            return;

        disconnect(current_scene_, SIGNAL( changed(const QList<QRectF> &) ), view_, SLOT( SceneChange() ));
        
        current_scene_ = scene_map_[name];
        current_scene_->setSceneRect(view_->rect());
        view_->setScene(current_scene_);
                
        connect(current_scene_, SIGNAL( changed(const QList<QRectF> &) ), view_, SLOT( SceneChange() ));

        if (animations_map_.contains(current_scene_))
        {
            animations_map_[current_scene_]->setDirection(QAbstractAnimation::Backward);
            animations_map_[current_scene_]->start();
        }
    }
}

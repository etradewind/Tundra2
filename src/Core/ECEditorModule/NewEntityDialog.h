// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

#include "ECEditorModuleApi.h"
#include "CoreTypes.h"
#include "AttributeChangeType.h"

#include <QDialog>

class QLineEdit;
class QCheckBox;

class ECEDITOR_MODULE_API AddEntityDialog : public QDialog
{

Q_OBJECT

public:
    AddEntityDialog(QWidget *parent = 0, Qt::WindowFlags flags = 0);
    ~AddEntityDialog();

public slots:
    QString EntityName() const;
    bool IsReplicated() const;
    bool IsLocal() const;
    bool IsTemporary() const;

private slots:
    void CheckTempAndSync();

protected:
    void showEvent(QShowEvent *event);

private:
    QLineEdit *editName_;
    QCheckBox *checkBoxSync_;
    QCheckBox *checkBoxTemp_;
};
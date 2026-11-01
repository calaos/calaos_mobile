#ifndef INLINETEXTFIELD_H
#define INLINETEXTFIELD_H

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRectF>
#include <QString>

#ifdef __OBJC__
@class UITextField;
@class CalaosInlineFieldDelegate;
#else
typedef struct objc_object UITextField;
typedef struct objc_object CalaosInlineFieldDelegate;
#endif

class HardwareUtils;

//A real UITextField laid over a CalaosTextField. iOS only offers password
//AutoFill on UIKit text fields with a text content type, and a Qt field is
//never one (no textContentType, never secureTextEntry). The QML field stays
//below as the visual shell (label, underline, clear button) and keeps the
//text: every native change is written to its text property, so the login
//flow reads the QML field as before.
class InlineTextField: public QObject
{
    Q_OBJECT
public:
    InlineTextField(QQuickItem *item, HardwareUtils *hw, const QString &purpose,
                    const QString &contentType);
    ~InlineTextField() override;

    QQuickItem *item() const { return m_item; }
    void endEditing();

    //Callbacks of the Objective-C delegate.
    void nativeTextChanged();
    void nativeEditingChanged(bool editing);
    void nativeReturnPressed();
    void keyboardMoved();

private slots:
    void itemTextChanged();
    void itemFocusRequested();
    void itemWindowChanged(QQuickWindow *window);
    void refresh();

private:
    QRectF visibleSceneRect(double *opacity) const;
    bool coveredByPopup() const;

    QPointer<QQuickItem> m_item;
    QPointer<QQuickWindow> m_window;
    QPointer<HardwareUtils> m_hw;
    QString m_purpose;
    UITextField *m_field;
    CalaosInlineFieldDelegate *m_delegate;
};

#endif // INLINETEXTFIELD_H

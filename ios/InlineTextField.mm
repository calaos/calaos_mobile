#import "InlineTextField.h"
#import <UIKit/UIKit.h>
#include <QColor>
#include <QFont>
#include <QFontInfo>
#include "../src/HardwareUtils.h"

namespace {

//Space kept between the edited field and the top of the keyboard.
const CGFloat kKeyboardMargin = 16;

UIColor *toUIColor(const QColor &c)
{
    return [UIColor colorWithRed:c.redF() green:c.greenF() blue:c.blueF() alpha:c.alphaF()];
}

//Last keyboard top in screen coordinates, shared: moving between two fields
//sends no keyboard notification.
CGFloat keyboardTop = CGFLOAT_MAX;

} // namespace

@interface CalaosInlineFieldDelegate : NSObject <UITextFieldDelegate, UIGestureRecognizerDelegate>
@property (nonatomic, assign) UIView *hostView;
@property (nonatomic, retain) UITapGestureRecognizer *tapOutside;
-(instancetype)initWithOwner:(InlineTextField *)o;
-(void)attachToView:(UIView *)view;
-(void)detach;
@end

@implementation CalaosInlineFieldDelegate
{
    InlineTextField *owner;
}

-(instancetype)initWithOwner:(InlineTextField *)o
{
    if ((self = [super init]))
    {
        owner = o;
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(keyboardWillChange:)
                                                     name:UIKeyboardWillChangeFrameNotification
                                                   object:nil];
    }
    return self;
}

-(void)detach
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (self.tapOutside)
        [self.hostView removeGestureRecognizer:self.tapOutside];
    self.tapOutside = nil;
    self.hostView = nil;
    owner = nullptr;
}

//A tap on the Qt surface never reaches a UIKit field: this is what closes the
//keyboard, as a tap outside a Qt field does.
-(void)attachToView:(UIView *)view
{
    if (self.tapOutside)
        [self.hostView removeGestureRecognizer:self.tapOutside];
    self.hostView = view;
    UITapGestureRecognizer *tap = [[[UITapGestureRecognizer alloc] initWithTarget:self
                                                                           action:@selector(tappedOutside)] autorelease];
    tap.cancelsTouchesInView = NO;
    tap.delegate = self;
    [view addGestureRecognizer:tap];
    self.tapOutside = tap;
}

-(BOOL)gestureRecognizer:(UIGestureRecognizer *)recognizer shouldReceiveTouch:(UITouch *)touch
{
    return ![touch.view isKindOfClass:[UITextField class]];
}

-(BOOL)gestureRecognizer:(UIGestureRecognizer *)recognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)other
{
    return YES;
}

-(void)tappedOutside
{
    if (owner)
        owner->endEditing();
}

-(void)keyboardWillChange:(NSNotification *)note
{
    keyboardTop = [note.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue].origin.y;
    if (owner)
        owner->keyboardMoved();
}

-(void)editingChanged:(UITextField *)field
{
    if (owner)
        owner->nativeTextChanged();
}

-(void)textFieldDidBeginEditing:(UITextField *)textField
{
    if (owner)
        owner->nativeEditingChanged(true);
}

-(void)textFieldDidEndEditing:(UITextField *)textField
{
    if (owner)
        owner->nativeEditingChanged(false);
}

-(BOOL)textFieldShouldReturn:(UITextField *)textField
{
    if (owner)
        owner->nativeReturnPressed();
    return NO;
}

@end

InlineTextField::InlineTextField(QQuickItem *item, HardwareUtils *hw, const QString &purpose,
                                 const QString &contentType):
    QObject(hw),
    m_item(item),
    m_hw(hw),
    m_purpose(purpose)
{
    m_delegate = [[CalaosInlineFieldDelegate alloc] initWithOwner:this];

    m_field = [[UITextField alloc] initWithFrame:CGRectZero];
    m_field.delegate = m_delegate;
    m_field.hidden = YES;
    m_field.backgroundColor = UIColor.clearColor;
    m_field.borderStyle = UITextBorderStyleNone;
    m_field.keyboardAppearance = UIKeyboardAppearanceDark;
    m_field.autocorrectionType = UITextAutocorrectionTypeNo;
    m_field.spellCheckingType = UITextSpellCheckingTypeNo;
    m_field.autocapitalizationType = UITextAutocapitalizationTypeNone;
    m_field.smartDashesType = UITextSmartDashesTypeNo;
    m_field.smartQuotesType = UITextSmartQuotesTypeNo;
    m_field.smartInsertDeleteType = UITextSmartInsertDeleteTypeNo;
    m_field.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
    m_field.textContentType = contentType == QLatin1String("password") ?
        UITextContentTypePassword : UITextContentTypeUsername;
    m_field.secureTextEntry = contentType == QLatin1String("password");
    m_field.returnKeyType = contentType == QLatin1String("password") ?
        UIReturnKeyDone : UIReturnKeyNext;
    [m_field addTarget:m_delegate action:@selector(editingChanged:)
      forControlEvents:UIControlEventEditingChanged];

    connect(item, SIGNAL(textChanged()), this, SLOT(itemTextChanged()));
    connect(item, SIGNAL(inlineFocusRequested()), this, SLOT(itemFocusRequested()));
    connect(item, &QQuickItem::windowChanged, this, &InlineTextField::itemWindowChanged);
    connect(item, &QObject::destroyed, this, &QObject::deleteLater);
    itemTextChanged();
    itemWindowChanged(item->window());
}

InlineTextField::~InlineTextField()
{
    [m_delegate detach];
    if (m_field.isFirstResponder)
        [m_field resignFirstResponder];
    //null when the app closes: HardwareUtils goes first
    if (m_hw)
        m_hw->setInlineEntryPurpose({});
    //the secret does not outlive the field
    m_field.text = @"";
    m_field.delegate = nil;
    [m_field removeFromSuperview];
    [m_field release];
    [m_delegate release];
}

void InlineTextField::endEditing()
{
    if (m_field.isFirstResponder)
        [m_field resignFirstResponder];
}

void InlineTextField::itemFocusRequested()
{
    [m_field becomeFirstResponder];
}

void InlineTextField::itemTextChanged()
{
    if (!m_item)
        return;
    NSString *text = m_item->property("text").toString().toNSString();
    if (![m_field.text isEqualToString:text])
        m_field.text = text;
}

void InlineTextField::nativeTextChanged()
{
    if (m_item)
        m_item->setProperty("text", QString::fromNSString(m_field.text ?: @""));
}

void InlineTextField::nativeEditingChanged(bool editing)
{
    if (!m_item)
        return;
    //a filled field does not always send UIControlEventEditingChanged
    nativeTextChanged();
    m_item->setProperty("inlineEditing", editing);
    //What Application reads when the password manager resigns the app.
    m_hw->setInlineEntryPurpose(editing ? m_purpose : QString());
    if (editing)
    {
        //one text responder at a time: a Qt field must not hold the focus
        QQuickWindow *window = m_item->window();
        QQuickItem *focused = window ? window->activeFocusItem() : nullptr;
        if (focused && focused->inherits("QQuickTextInput"))
            focused->setFocus(false);
    }
    keyboardMoved();
}

void InlineTextField::nativeReturnPressed()
{
    if (m_item)
        QMetaObject::invokeMethod(m_item, "inlineReturnPressed");
}

//QML lifts the form by this much (LoginView): the keyboard belongs to UIKit,
//so Qt's own scroll to the cursor does not run for this field.
void InlineTextField::keyboardMoved()
{
    if (!m_item)
        return;
    qreal overlap = 0;
    if (m_field.isFirstResponder && keyboardTop < CGFLOAT_MAX)
    {
        const qreal lifted = m_item->property("inlineKeyboardOverlap").toReal();
        const CGRect screen = [m_field convertRect:m_field.bounds
                                 toCoordinateSpace:m_field.window.screen.coordinateSpace];
        overlap = qMax<qreal>(0, CGRectGetMaxY(screen) + lifted + kKeyboardMargin - keyboardTop);
    }
    m_item->setProperty("inlineKeyboardOverlap", overlap);
}

void InlineTextField::itemWindowChanged(QQuickWindow *window)
{
    if (m_window)
        disconnect(m_window, nullptr, this, nullptr);
    m_window = window;
    if (window)
    {
        //Every change the eye can see goes through a frame.
        connect(window, &QQuickWindow::afterAnimating, this, &InlineTextField::refresh);
        window->update();
    }
    else
    {
        m_field.hidden = YES;
    }
}

QRectF InlineTextField::visibleSceneRect(double *opacity) const
{
    const qreal left = m_item->property("leftPadding").toReal();
    const qreal top = m_item->property("topPadding").toReal();
    const qreal bottom = m_item->property("bottomPadding").toReal();
    const qreal right = m_item->property("inlineRightInset").toReal();
    const QRectF local(left, top, m_item->width() - left - right, m_item->height() - top - bottom);
    const QRectF rect = m_item->mapRectToScene(local);

    *opacity = 1;
    for (const QQuickItem *p = m_item; p; p = p->parentItem())
    {
        *opacity *= p->opacity();
        if (p->clip() && !p->mapRectToScene(p->boundingRect()).contains(rect))
            return {};
    }
    return rect;
}

//Native views sit above everything Qt draws: an open sheet must not show the
//field through it. Qt's overlay itself stays visible as soon as a Drawer
//exists, open or not; what shows an open popup is a visible child of it.
bool InlineTextField::coveredByPopup() const
{
    const auto children = m_window->contentItem()->childItems();
    for (const QQuickItem *child: children)
    {
        if (!child->inherits("QQuickOverlay"))
            continue;
        const auto popups = child->childItems();
        for (const QQuickItem *popup: popups)
        {
            if (popup->isVisible())
                return true;
        }
    }
    return false;
}

void InlineTextField::refresh()
{
    if (!m_item || !m_window)
        return;

    //The window only has a native view once it is shown, hence not at bind time.
    UIView *view = reinterpret_cast<UIView *>(m_window->winId());
    if (m_field.superview != view)
    {
        [view addSubview:m_field];
        [m_delegate attachToView:view];
    }

    double opacity = 0;
    QRectF rect;
    if (m_item->isVisible() && !coveredByPopup())
        rect = visibleSceneRect(&opacity);

    if (rect.isEmpty() || opacity < 0.01)
    {
        endEditing();
        m_field.hidden = YES;
        return;
    }

    const CGRect frame = rect.toCGRect();
    if (!CGRectEqualToRect(m_field.frame, frame))
        m_field.frame = frame;
    m_field.alpha = opacity;
    m_field.enabled = m_item->isEnabled();
    m_field.hidden = NO;

    const QFont font = m_item->property("font").value<QFont>();
    const CGFloat size = QFontInfo(font).pixelSize();
    if (m_field.font.pointSize != size)
    {
        UIFont *themed = [UIFont fontWithName:font.family().toNSString() size:size];
        m_field.font = themed ?: [UIFont systemFontOfSize:size];
    }
    m_field.textColor = toUIColor(m_item->property("inlineTextColor").value<QColor>());
    m_field.tintColor = toUIColor(m_item->property("selectedTextColor").value<QColor>());
}

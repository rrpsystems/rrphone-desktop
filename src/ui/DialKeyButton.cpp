#include "DialKeyButton.h"
#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>

DialKeyButton::DialKeyButton(QChar digit, const QString &letters, QWidget *parent)
    : QPushButton(parent), m_digit(digit), m_letters(letters) {
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    // The label is painted by hand, so the button carries no text of its own
    // — without this the key is nameless to screen readers (and to UI
    // automation).
    setAccessibleName(QString(digit));
}

// Painted entirely by hand, like the Android keypad: a pill-shaped key with
// the digit on top and the letter group underneath. The stylesheet can't give
// a pill whose radius follows the key height, and QPushButton can't mix two
// font sizes in its own text anyway.
void DialKeyButton::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    QColor fill(Theme::kButton);
    if (isDown()) {
        fill = QColor(Theme::kPanel);
    } else if (underMouse()) {
        fill = QColor(Theme::kButtonHover);
    }
    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = body.height() / 2.0;
    QPainterPath path;
    path.addRoundedRect(body, radius, radius);
    painter.fillPath(path, fill);

    QFont digitFont = font();
    digitFont.setPointSizeF(14.5);
    const QFontMetrics digitMetrics(digitFont);

    QFont lettersFont = font();
    lettersFont.setPointSizeF(6.5);
    lettersFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    const QFontMetrics lettersMetrics(lettersFont);

    // Digit and letters stacked and centred as one block; keys without
    // letters keep the digit centred on its own.
    const int gap = 1;
    const int blockHeight = digitMetrics.capHeight() +
                            (m_letters.isEmpty() ? 0 : gap + lettersMetrics.capHeight() + 3);
    const int top = (height() - blockHeight) / 2;
    const int digitBaseline = top + digitMetrics.capHeight();

    const QString digitText(m_digit);
    painter.setFont(digitFont);
    painter.setPen(QColor(Theme::kTextPrimary));
    painter.drawText((width() - digitMetrics.horizontalAdvance(digitText)) / 2, digitBaseline, digitText);

    if (!m_letters.isEmpty()) {
        painter.setFont(lettersFont);
        painter.setPen(QColor(Theme::kTextSecondary));
        const int lettersBaseline = digitBaseline + gap + lettersMetrics.capHeight() + 3;
        painter.drawText((width() - lettersMetrics.horizontalAdvance(m_letters)) / 2, lettersBaseline, m_letters);
    }
}
